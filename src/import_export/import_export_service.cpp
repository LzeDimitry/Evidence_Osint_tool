#include "import_export/import_export_service.hpp"

#include "database/migrations.hpp"
#include "domain/types.hpp"
#include "import_export/json.hpp"

#include <zip.h>

#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace evidence_trace::import_export {

namespace {

using json::Json;

constexpr std::int64_t max_archive_entry_size = 100 * 1024 * 1024;
constexpr std::int64_t max_archive_total_size = 1024 * 1024 * 1024;
constexpr std::size_t max_archive_entries = 10000;

Json optional_json(const std::optional<std::string>& value) {
    return value.has_value() ? Json(*value) : Json(nullptr);
}

std::string required_string(const Json::Object& object, const std::string& key) {
    const auto& value = object.at(std::string(key));
    if (!value.is_string()) throw std::invalid_argument("Import rejected: field '" + key + "' must be a string");
    return value.as_string();
}

std::string optional_string(const Json::Object& object, const std::string& key, const std::string& fallback = {}) {
    const auto iterator = object.find(key);
    if (iterator == object.end() || iterator->second.is_null()) return fallback;
    if (!iterator->second.is_string()) throw std::invalid_argument("Import rejected: field '" + key + "' must be a string or null");
    return iterator->second.as_string();
}

std::optional<std::string> optional_value(const Json::Object& object, const std::string& key) {
    const auto iterator = object.find(key);
    if (iterator == object.end() || iterator->second.is_null()) return std::nullopt;
    if (!iterator->second.is_string()) throw std::invalid_argument("Import rejected: field '" + key + "' must be a string or null");
    return iterator->second.as_string();
}

std::int64_t required_integer(const Json::Object& object, const std::string& key) {
    const auto& value = object.at(key);
    if (!value.is_number()) throw std::invalid_argument("Import rejected: field '" + key + "' must be an integer");
    try { return value.as_int64(); }
    catch (const std::exception&) { throw std::invalid_argument("Import rejected: field '" + key + "' must be an integer"); }
}

const Json::Array& required_array(const Json::Object& object, std::string_view key) {
    const auto& value = object.at(std::string(key));
    if (!value.is_array()) throw std::invalid_argument("Import rejected: field '" + std::string(key) + "' must be an array");
    return value.as_array();
}

const Json::Object& as_object(const Json& value, std::string_view context) {
    if (!value.is_object()) throw std::invalid_argument("Import rejected: " + std::string(context) + " must be an object");
    return value.as_object();
}

bool safe_archive_path(const std::string& name) {
    if (name.empty() || name.front() == '/' || name.front() == '\\' || name.find('\0') != std::string::npos) return false;
    if (name.size() >= 2 && ((name[0] >= 'A' && name[0] <= 'Z') || (name[0] >= 'a' && name[0] <= 'z')) && name[1] == ':') return false;
    std::string component;
    for (std::size_t i = 0; i <= name.size(); ++i) {
        const char character = i == name.size() ? '/' : name[i];
        if (character == '/' || character == '\\') {
            if (component.empty() || component == "." || component == "..") return false;
            component.clear();
        } else {
            component.push_back(character);
        }
    }
    return true;
}

std::string zip_error(zip_t* archive, const std::string& prefix) {
    return prefix + ": " + zip_strerror(archive);
}

struct ArchiveData {
    std::map<std::string, std::vector<unsigned char>> entries;
};

ArchiveData read_archive(const std::filesystem::path& path) {
    int error_code = 0;
    zip_t* archive = zip_open(path.string().c_str(), ZIP_RDONLY, &error_code);
    if (archive == nullptr) throw std::invalid_argument("Import rejected: unable to open ZIP archive");
    bool discard = true;
    try {
        const auto count = zip_get_num_entries(archive, 0);
        if (count < 0) throw std::invalid_argument(zip_error(archive, "Unable to inspect archive"));
        if (static_cast<std::size_t>(count) > max_archive_entries) throw std::invalid_argument("Import rejected: archive has too many entries");
        ArchiveData result;
        std::int64_t total_size = 0;
        for (zip_uint64_t index = 0; index < static_cast<zip_uint64_t>(count); ++index) {
            zip_stat_t info{};
            if (zip_stat_index(archive, index, 0, &info) != 0 || info.name == nullptr) throw std::invalid_argument("Import rejected: invalid ZIP entry");
            const std::string name(info.name);
            if (!safe_archive_path(name)) throw std::invalid_argument("Import rejected: unsafe archive path '" + name + "'");
            if (result.entries.contains(name)) throw std::invalid_argument("Import rejected: duplicate archive path '" + name + "'");
            if ((info.valid & ZIP_STAT_SIZE) == 0 || info.size > static_cast<zip_uint64_t>(max_archive_entry_size)) throw std::invalid_argument("Import rejected: archive entry is too large '" + name + "'");
            if (total_size > max_archive_total_size - static_cast<std::int64_t>(info.size)) throw std::invalid_argument("Import rejected: archive is too large");
            total_size += static_cast<std::int64_t>(info.size);
            zip_uint8_t operating_system = 0;
            zip_uint32_t external_attributes = 0;
            if (zip_file_get_external_attributes(archive, index, 0, &operating_system, &external_attributes) == 0 && operating_system == ZIP_OPSYS_UNIX) {
                const auto mode = static_cast<mode_t>((external_attributes >> 16) & 0xffffU);
                if ((mode & S_IFMT) == S_IFLNK) throw std::invalid_argument("Import rejected: symlink entry '" + name + "'");
            }
            zip_file_t* file = zip_fopen_index(archive, index, 0);
            if (file == nullptr) throw std::invalid_argument(zip_error(archive, "Unable to read archive entry"));
            std::vector<unsigned char> bytes(static_cast<std::size_t>(info.size));
            std::size_t offset = 0;
            while (offset < bytes.size()) {
                const auto read = zip_fread(file, bytes.data() + offset, bytes.size() - offset);
                if (read <= 0) {
                    zip_fclose(file);
                    throw std::invalid_argument(zip_error(archive, "Unable to read archive entry"));
                }
                offset += static_cast<std::size_t>(read);
            }
            if (zip_fclose(file) != 0) throw std::invalid_argument(zip_error(archive, "Unable to close archive entry"));
            result.entries.emplace(name, std::move(bytes));
        }
        if (zip_close(archive) != 0) throw std::invalid_argument("Import rejected: unable to close ZIP archive");
        discard = false;
        return result;
    } catch (...) {
        if (discard) zip_discard(archive);
        throw;
    }
}

std::string sha256_hex(const std::vector<unsigned char>& bytes) {
    return storage::sha256_bytes(bytes);
}

void validate_manifest(const ArchiveData& archive, const Json& manifest_value) {
    const auto& manifest = as_object(manifest_value, "manifest");
    if (required_string(manifest, "format") != "evidence-trace-manifest") throw std::invalid_argument("Import rejected: invalid manifest format");
    if (required_integer(manifest, "format_version") != 1) throw std::invalid_argument("Import rejected: unsupported manifest version");
    const auto& entries = required_array(manifest, "entries");
    std::set<std::string> listed;
    for (const auto& item : entries) {
        const auto& entry = as_object(item, "manifest entry");
        const auto path = required_string(entry, "path");
        if (!safe_archive_path(path) || path == "manifest.json") throw std::invalid_argument("Import rejected: unsafe manifest path '" + path + "'");
        if (!listed.insert(path).second) throw std::invalid_argument("Import rejected: duplicate manifest path '" + path + "'");
        const auto archive_entry = archive.entries.find(path);
        if (archive_entry == archive.entries.end()) throw std::invalid_argument("Import rejected: manifest entry is missing from archive: " + path);
        const auto expected_size = required_integer(entry, "size");
        if (expected_size < 0 || static_cast<std::size_t>(expected_size) != archive_entry->second.size()) throw std::invalid_argument("Import rejected: size mismatch for " + path);
        if (domain::lower_ascii(required_string(entry, "sha256")) != sha256_hex(archive_entry->second)) throw std::invalid_argument("Import rejected: SHA-256 mismatch for " + path);
        if (path != "case.json" && path.rfind("attachments/", 0) != 0) throw std::invalid_argument("Import rejected: unexpected manifest path " + path);
    }
    if (!listed.contains("case.json")) throw std::invalid_argument("Import rejected: manifest has no case.json entry");
    for (const auto& [path, bytes] : archive.entries) {
        (void)bytes;
        if (path != "manifest.json" && !listed.contains(path)) throw std::invalid_argument("Import rejected: archive entry is not listed in manifest: " + path);
    }
}

} // namespace

namespace {

Json make_case_json(database::Database& database, const domain::Id& case_id) {
    auto case_statement = database.prepare("SELECT id, title, purpose, description, primary_target_type, scope, tags_json, status, created_at, updated_at FROM cases WHERE id = ?;");
    case_statement.bind(1, case_id);
    if (!case_statement.step()) throw std::runtime_error("Case not found: " + case_id);
    Json::Object case_object;
    case_object["id"] = case_statement.column_text(0); case_object["title"] = case_statement.column_text(1);
    case_object["purpose"] = case_statement.column_text(2); case_object["description"] = case_statement.column_text(3);
    case_object["primary_target_type"] = optional_json(case_statement.column_optional_text(4)); case_object["scope"] = case_statement.column_text(5);
    case_object["tags_json"] = case_statement.column_text(6); case_object["status"] = case_statement.column_text(7);
    case_object["created_at"] = case_statement.column_text(8); case_object["updated_at"] = case_statement.column_text(9);

    Json::Array entities;
    auto entities_statement = database.prepare("SELECT id, case_id, type, label, original_value, canonical_value, details_json, tags_json, aliases_json, created_at, updated_at FROM entities WHERE case_id = ? ORDER BY id;");
    entities_statement.bind(1, case_id);
    while (entities_statement.step()) {
        Json::Object object;
        object["id"] = entities_statement.column_text(0); object["case_id"] = entities_statement.column_text(1); object["type"] = entities_statement.column_text(2); object["label"] = entities_statement.column_text(3);
        object["original_value"] = entities_statement.column_text(4); object["canonical_value"] = entities_statement.column_text(5); object["details_json"] = entities_statement.column_text(6); object["tags_json"] = entities_statement.column_text(7); object["aliases_json"] = entities_statement.column_text(8);
        object["created_at"] = entities_statement.column_text(9); object["updated_at"] = entities_statement.column_text(10); entities.emplace_back(std::move(object));
    }
    Json::Array sources;
    auto sources_statement = database.prepare("SELECT id, case_id, type, locator, title, accessed_at, author, reliability_note, created_at FROM sources WHERE case_id = ? ORDER BY id;");
    sources_statement.bind(1, case_id);
    while (sources_statement.step()) {
        Json::Object object;
        object["id"] = sources_statement.column_text(0); object["case_id"] = sources_statement.column_text(1); object["type"] = sources_statement.column_text(2); object["locator"] = sources_statement.column_text(3); object["title"] = sources_statement.column_text(4);
        object["accessed_at"] = sources_statement.column_text(5); object["author"] = sources_statement.column_text(6); object["reliability_note"] = sources_statement.column_text(7); object["created_at"] = sources_statement.column_text(8); sources.emplace_back(std::move(object));
    }
    Json::Array evidence;
    auto evidence_statement = database.prepare("SELECT id, case_id, source_id, kind, relative_path, original_filename, mime_type, byte_size, sha256, text_content, source_url, captured_at, imported_at, note, quotation_location, created_at FROM evidence WHERE case_id = ? ORDER BY id;");
    evidence_statement.bind(1, case_id);
    while (evidence_statement.step()) {
        Json::Object object;
        object["id"] = evidence_statement.column_text(0); object["case_id"] = evidence_statement.column_text(1); object["source_id"] = optional_json(evidence_statement.column_optional_text(2)); object["kind"] = evidence_statement.column_text(3);
        object["relative_path"] = optional_json(evidence_statement.column_optional_text(4)); object["original_filename"] = optional_json(evidence_statement.column_optional_text(5)); object["mime_type"] = optional_json(evidence_statement.column_optional_text(6));
        if (evidence_statement.column_is_null(7)) object["byte_size"] = Json(nullptr); else object["byte_size"] = evidence_statement.column_int64(7);
        object["sha256"] = optional_json(evidence_statement.column_optional_text(8)); object["text_content"] = evidence_statement.column_text(9); object["source_url"] = optional_json(evidence_statement.column_optional_text(10));
        object["captured_at"] = evidence_statement.column_text(11); object["imported_at"] = evidence_statement.column_text(12); object["note"] = evidence_statement.column_text(13); object["quotation_location"] = evidence_statement.column_text(14); object["created_at"] = evidence_statement.column_text(15);
        evidence.emplace_back(std::move(object));
    }
    Json::Array claims;
    auto claims_statement = database.prepare("SELECT id, case_id, statement, kind, predicate, status, reasoning, recorded_by, created_at, updated_at FROM claims WHERE case_id = ? ORDER BY id;");
    claims_statement.bind(1, case_id);
    while (claims_statement.step()) {
        Json::Object object;
        object["id"] = claims_statement.column_text(0); object["case_id"] = claims_statement.column_text(1); object["statement"] = claims_statement.column_text(2); object["kind"] = claims_statement.column_text(3);
        object["predicate"] = optional_json(claims_statement.column_optional_text(4)); object["status"] = claims_statement.column_text(5); object["reasoning"] = claims_statement.column_text(6); object["recorded_by"] = claims_statement.column_text(7); object["created_at"] = claims_statement.column_text(8); object["updated_at"] = claims_statement.column_text(9); claims.emplace_back(std::move(object));
    }
    Json::Array claim_entities;
    auto claim_entities_statement = database.prepare("SELECT ce.claim_id, ce.entity_id, ce.role FROM claim_entities ce JOIN claims c ON c.id = ce.claim_id WHERE c.case_id = ? ORDER BY ce.claim_id, ce.entity_id, ce.role;");
    claim_entities_statement.bind(1, case_id);
    while (claim_entities_statement.step()) claim_entities.emplace_back(Json::Object{{"claim_id", claim_entities_statement.column_text(0)}, {"entity_id", claim_entities_statement.column_text(1)}, {"role", claim_entities_statement.column_text(2)}});
    Json::Array claim_evidence;
    auto claim_evidence_statement = database.prepare("SELECT ce.claim_id, ce.evidence_id, ce.role, ce.note FROM claim_evidence ce JOIN claims c ON c.id = ce.claim_id WHERE c.case_id = ? ORDER BY ce.claim_id, ce.evidence_id;");
    claim_evidence_statement.bind(1, case_id);
    while (claim_evidence_statement.step()) claim_evidence.emplace_back(Json::Object{{"claim_id", claim_evidence_statement.column_text(0)}, {"evidence_id", claim_evidence_statement.column_text(1)}, {"role", claim_evidence_statement.column_text(2)}, {"note", claim_evidence_statement.column_text(3)}});

    Json::Array notes;
    auto notes_statement = database.prepare("SELECT id, case_id, title, body, created_at, updated_at FROM notes WHERE case_id = ? ORDER BY id;");
    notes_statement.bind(1, case_id);
    while (notes_statement.step()) notes.emplace_back(Json::Object{{"id", notes_statement.column_text(0)}, {"case_id", notes_statement.column_text(1)}, {"title", notes_statement.column_text(2)}, {"body", notes_statement.column_text(3)}, {"created_at", notes_statement.column_text(4)}, {"updated_at", notes_statement.column_text(5)}});
    Json::Array note_entities;
    auto note_entities_statement = database.prepare("SELECT ne.note_id, ne.entity_id FROM note_entities ne JOIN notes n ON n.id = ne.note_id WHERE n.case_id = ? ORDER BY ne.note_id, ne.entity_id;");
    note_entities_statement.bind(1, case_id);
    while (note_entities_statement.step()) note_entities.emplace_back(Json::Object{{"note_id", note_entities_statement.column_text(0)}, {"entity_id", note_entities_statement.column_text(1)}});
    Json::Array note_evidence;
    auto note_evidence_statement = database.prepare("SELECT ne.note_id, ne.evidence_id FROM note_evidence ne JOIN notes n ON n.id = ne.note_id WHERE n.case_id = ? ORDER BY ne.note_id, ne.evidence_id;");
    note_evidence_statement.bind(1, case_id);
    while (note_evidence_statement.step()) note_evidence.emplace_back(Json::Object{{"note_id", note_evidence_statement.column_text(0)}, {"evidence_id", note_evidence_statement.column_text(1)}});

    Json::Array runs;
    auto runs_statement = database.prepare("SELECT id, case_id, name_snapshot, entity_id, started_at, updated_at FROM playbook_runs WHERE case_id = ? ORDER BY id;");
    runs_statement.bind(1, case_id);
    while (runs_statement.step()) runs.emplace_back(Json::Object{{"id", runs_statement.column_text(0)}, {"case_id", runs_statement.column_text(1)}, {"name_snapshot", runs_statement.column_text(2)}, {"entity_id", optional_json(runs_statement.column_optional_text(3))}, {"started_at", runs_statement.column_text(4)}, {"updated_at", runs_statement.column_text(5)}});
    Json::Array run_steps;
    auto run_steps_statement = database.prepare("SELECT rs.id, rs.run_id, rs.position, rs.name_snapshot, rs.instructions_snapshot, rs.state, rs.completed_at, rs.note FROM run_steps rs JOIN playbook_runs r ON r.id = rs.run_id WHERE r.case_id = ? ORDER BY rs.run_id, rs.position;");
    run_steps_statement.bind(1, case_id);
    while (run_steps_statement.step()) run_steps.emplace_back(Json::Object{{"id", run_steps_statement.column_text(0)}, {"run_id", run_steps_statement.column_text(1)}, {"position", run_steps_statement.column_int64(2)}, {"name_snapshot", run_steps_statement.column_text(3)}, {"instructions_snapshot", run_steps_statement.column_text(4)}, {"state", run_steps_statement.column_text(5)}, {"completed_at", optional_json(run_steps_statement.column_optional_text(6))}, {"note", run_steps_statement.column_text(7)}});
    Json::Array run_step_evidence;
    auto run_step_evidence_statement = database.prepare("SELECT rse.run_step_id, rse.evidence_id FROM run_step_evidence rse JOIN run_steps rs ON rs.id = rse.run_step_id JOIN playbook_runs r ON r.id = rs.run_id WHERE r.case_id = ? ORDER BY rse.run_step_id, rse.evidence_id;");
    run_step_evidence_statement.bind(1, case_id);
    while (run_step_evidence_statement.step()) run_step_evidence.emplace_back(Json::Object{{"run_step_id", run_step_evidence_statement.column_text(0)}, {"evidence_id", run_step_evidence_statement.column_text(1)}});
    Json::Array log_events;
    auto logs_statement = database.prepare("SELECT id, case_id, occurred_at, action, object_type, object_id, description, payload_json FROM log_events WHERE case_id = ? ORDER BY occurred_at, id;");
    logs_statement.bind(1, case_id);
    while (logs_statement.step()) log_events.emplace_back(Json::Object{{"id", logs_statement.column_text(0)}, {"case_id", logs_statement.column_text(1)}, {"occurred_at", logs_statement.column_text(2)}, {"action", logs_statement.column_text(3)}, {"object_type", logs_statement.column_text(4)}, {"object_id", optional_json(logs_statement.column_optional_text(5))}, {"description", logs_statement.column_text(6)}, {"payload_json", logs_statement.column_text(7)}});

    Json::Object root;
    root["format"] = "evidence-trace-case"; root["format_version"] = 1; root["schema_version"] = database::current_schema_version; root["exported_at"] = domain::utc_now();
    root["case"] = std::move(case_object); root["entities"] = std::move(entities); root["sources"] = std::move(sources); root["evidence"] = std::move(evidence);
    root["claims"] = std::move(claims); root["claim_entities"] = std::move(claim_entities); root["claim_evidence"] = std::move(claim_evidence); root["notes"] = std::move(notes); root["note_entities"] = std::move(note_entities); root["note_evidence"] = std::move(note_evidence);
    root["playbook_runs"] = std::move(runs); root["run_steps"] = std::move(run_steps); root["run_step_evidence"] = std::move(run_step_evidence); root["log_events"] = std::move(log_events);
    return Json(std::move(root));
}

void zip_add_bytes(zip_t* archive, const std::string& name, const std::string& bytes) {
    auto* source = zip_source_buffer(archive, bytes.data(), bytes.size(), 0);
    if (source == nullptr) throw std::runtime_error(zip_error(archive, "Unable to create ZIP source"));
    if (zip_file_add(archive, name.c_str(), source, ZIP_FL_OVERWRITE | ZIP_FL_ENC_UTF_8) < 0) { zip_source_free(source); throw std::runtime_error(zip_error(archive, "Unable to add ZIP entry")); }
}

void zip_add_file(zip_t* archive, const std::string& name, const std::filesystem::path& path) {
    auto* source = zip_source_file(archive, path.string().c_str(), 0, 0);
    if (source == nullptr) throw std::runtime_error(zip_error(archive, "Unable to create ZIP file source"));
    if (zip_file_add(archive, name.c_str(), source, ZIP_FL_OVERWRITE | ZIP_FL_ENC_UTF_8) < 0) { zip_source_free(source); throw std::runtime_error(zip_error(archive, "Unable to add ZIP file")); }
}

std::filesystem::path temporary_sibling(const std::filesystem::path& final_path) {
    const auto parent = final_path.parent_path().empty() ? std::filesystem::path(".") : final_path.parent_path();
    return parent / (final_path.filename().string() + ".tmp-" + domain::new_id());
}

void remove_temporary_file(const std::filesystem::path& path) {
    if (path.empty()) return;
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

} // namespace

void ImportExportService::export_case(const domain::Id& case_id, const std::filesystem::path& archive_path) {
    // Keep the success event in the exported case, but commit it only after
    // the temporary archive has been finalized and atomically installed.
    database::Transaction transaction(database_);
    logger_.record(case_id, "exported", "case", case_id, "Case archive exported");
    const auto case_json = make_case_json(database_, case_id);
    const auto case_text = case_json.dump();
    Json::Array manifest_entries;
    manifest_entries.emplace_back(Json::Object{{"path", "case.json"}, {"size", static_cast<std::int64_t>(case_text.size())}, {"sha256", sha256_hex(std::vector<unsigned char>(case_text.begin(), case_text.end()))}});
    std::vector<std::pair<std::string, std::filesystem::path>> files;
    auto evidence_statement = database_.prepare("SELECT id, relative_path, sha256, byte_size FROM evidence WHERE case_id = ? AND kind = 'file' ORDER BY id;");
    evidence_statement.bind(1, case_id);
    while (evidence_statement.step()) {
        const auto relative = evidence_statement.column_optional_text(1);
        if (!relative.has_value()) throw std::runtime_error("File evidence has no stored path");
        const auto absolute = attachment_store_.absolute_path(*relative);
        if (!attachment_store_.exists(*relative)) throw std::runtime_error("Cannot export missing evidence file: " + evidence_statement.column_text(0));
        const auto bytes = attachment_store_.read(*relative);
        if (evidence_statement.column_is_null(2) || storage::sha256_bytes(bytes) != evidence_statement.column_text(2) ||
            (!evidence_statement.column_is_null(3) && static_cast<std::int64_t>(bytes.size()) != evidence_statement.column_int64(3))) {
            throw std::runtime_error("Cannot export evidence with a failed integrity check: " + evidence_statement.column_text(0));
        }
        const auto archive_name = std::string("attachments/") + evidence_statement.column_text(0) + ".bin";
        manifest_entries.emplace_back(Json::Object{{"path", archive_name}, {"size", static_cast<std::int64_t>(bytes.size())}, {"sha256", sha256_hex(bytes)}});
        files.emplace_back(archive_name, absolute);
    }
    const Json manifest(Json::Object{{"format", "evidence-trace-manifest"}, {"format_version", 1}, {"entries", std::move(manifest_entries)}});
    const auto manifest_text = manifest.dump();
    std::filesystem::create_directories(archive_path.parent_path().empty() ? std::filesystem::path(".") : archive_path.parent_path());
    const auto temporary_path = temporary_sibling(archive_path);
    int error_code = 0;
    zip_t* archive = zip_open(temporary_path.string().c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error_code);
    if (archive == nullptr) {
        remove_temporary_file(temporary_path);
        throw std::runtime_error("Unable to create ZIP archive: " + archive_path.string());
    }
    bool discard = true;
    try {
        zip_add_bytes(archive, "case.json", case_text); zip_add_bytes(archive, "manifest.json", manifest_text);
        for (const auto& [name, path] : files) zip_add_file(archive, name, path);
        if (zip_close(archive) != 0) throw std::runtime_error("Unable to finalize ZIP archive");
        discard = false;
        std::filesystem::rename(temporary_path, archive_path);
    } catch (...) {
        if (discard) zip_discard(archive);
        remove_temporary_file(temporary_path);
        throw;
    }
    transaction.commit();
}

void ImportExportService::write_report(const domain::Id& case_id, const std::filesystem::path& report_path) {
    auto case_statement = database_.prepare("SELECT title, purpose, description, scope, status FROM cases WHERE id = ?;");
    case_statement.bind(1, case_id);
    if (!case_statement.step()) throw std::runtime_error("Case not found: " + case_id);
    std::filesystem::create_directories(report_path.parent_path().empty() ? std::filesystem::path(".") : report_path.parent_path());
    const auto temporary_path = temporary_sibling(report_path);
    try {
        std::ofstream output(temporary_path);
        if (!output) throw std::runtime_error("Unable to write report: " + report_path.string());
        output << "# Evidence Trace report\n\nGenerated (UTC): " << domain::utc_now() << "\n\n";
        output << "> Assessments in this report are manual investigator assessments, not guarantees of truth or legal admissibility.\n\n";
        output << "## Case\n\n**Title:** " << case_statement.column_text(0) << "  \n**Status:** " << case_statement.column_text(4) << "  \n**Purpose:** " << case_statement.column_text(1) << "  \n**Scope:** " << case_statement.column_text(3) << "\n\n" << case_statement.column_text(2) << "\n\n";
        output << "## Entities\n\n";
        auto entities = database_.prepare("SELECT type, label, original_value FROM entities WHERE case_id = ? ORDER BY label COLLATE NOCASE, id;");
        entities.bind(1, case_id);
        while (entities.step()) output << "- **" << entities.column_text(0) << "** — " << entities.column_text(1) << (entities.column_text(2).empty() ? "" : " (" + entities.column_text(2) + ")") << "\n";
        output << "\n## Confirmed conclusions\n\n";
        auto confirmed = database_.prepare("SELECT statement, reasoning FROM claims WHERE case_id = ? AND status = 'confirmed' ORDER BY updated_at, id;");
        confirmed.bind(1, case_id);
        while (confirmed.step()) output << "- " << confirmed.column_text(0) << (confirmed.column_text(1).empty() ? "" : " — " + confirmed.column_text(1)) << "\n";
        output << "\n## Other assessments\n\n";
        auto other = database_.prepare("SELECT status, statement, reasoning FROM claims WHERE case_id = ? AND status != 'confirmed' ORDER BY updated_at, id;");
        other.bind(1, case_id);
        while (other.step()) output << "- **" << other.column_text(0) << "** — " << other.column_text(1) << (other.column_text(2).empty() ? "" : " — " + other.column_text(2)) << "\n";
        output << "\n## Evidence references\n\n";
        auto evidence = database_.prepare("SELECT id, kind, coalesce(original_filename, source_url, 'text excerpt'), sha256, note FROM evidence WHERE case_id = ? ORDER BY imported_at, id;");
        evidence.bind(1, case_id);
        while (evidence.step()) output << "- " << evidence.column_text(0) << " [" << evidence.column_text(1) << "] — " << evidence.column_text(2) << (evidence.column_optional_text(3).has_value() ? " (SHA-256: " + *evidence.column_optional_text(3) + ")" : "") << (evidence.column_text(4).empty() ? "" : " — " + evidence.column_text(4)) << "\n";
        output << "\n## Brief chronology\n\n";
        auto logs = database_.prepare("SELECT occurred_at, description FROM log_events WHERE case_id = ? ORDER BY occurred_at, id;");
        logs.bind(1, case_id);
        while (logs.step()) output << "- " << logs.column_text(0) << " — " << logs.column_text(1) << "\n";
        output << "\n## Open questions\n\n";
        auto open = database_.prepare("SELECT status, statement FROM claims WHERE case_id = ? AND status IN ('unverified','possible','probable','refuted') ORDER BY status, updated_at, id;");
        open.bind(1, case_id);
        while (open.step()) output << "- **" << open.column_text(0) << "** — " << open.column_text(1) << "\n";
        output.flush();
        if (!output) throw std::runtime_error("Unable to finish report: " + report_path.string());
        output.close();
        if (!output) throw std::runtime_error("Unable to close report: " + report_path.string());
        std::filesystem::rename(temporary_path, report_path);
    } catch (...) {
        remove_temporary_file(temporary_path);
        throw;
    }
    logger_.record(case_id, "exported", "report", case_id, "Markdown report exported");
}

namespace {

void collect_id_map(const Json::Array& values,
                    std::unordered_map<std::string, std::string>& ids,
                    bool remap) {
    for (const auto& item : values) {
        const auto& object = as_object(item, "record");
        const auto old_id = required_string(object, "id");
        if (ids.contains(old_id)) throw std::invalid_argument("Import rejected: duplicate record ID " + old_id);
        ids.emplace(old_id, remap ? domain::new_id() : old_id);
    }
}

std::string mapped(const std::string& old_id,
                   const std::unordered_map<std::string, std::string>& ids) {
    const auto iterator = ids.find(old_id);
    if (iterator == ids.end()) throw std::invalid_argument("Import rejected: unknown reference " + old_id);
    return iterator->second;
}

std::string replace_ids(std::string text,
                        const std::unordered_map<std::string, std::string>& ids) {
    for (const auto& [old_id, new_id] : ids) {
        std::size_t position = 0;
        while ((position = text.find(old_id, position)) != std::string::npos) {
            text.replace(position, old_id.size(), new_id);
            position += new_id.size();
        }
    }
    return text;
}

bool id_exists(database::Database& database, const std::string& table, const std::string& id) {
    auto statement = database.prepare("SELECT 1 FROM " + table + " WHERE id = ?;");
    statement.bind(1, id);
    return statement.step();
}

void validate_import_records(const Json::Object& case_object,
                             const Json::Array& entities,
                             const Json::Array& sources,
                             const Json::Array& evidence,
                             const Json::Array& claims,
                             const Json::Array& claim_entities,
                             const Json::Array& claim_evidence,
                             const Json::Array& notes,
                             const Json::Array& note_entities,
                             const Json::Array& note_evidence,
                             const Json::Array& runs,
                             const Json::Array& run_steps,
                             const Json::Array& run_step_evidence,
                             const Json::Array& logs,
                             const ArchiveData& archive) {
    const auto case_id = required_string(case_object, "id");
    if (case_id.empty()) throw std::invalid_argument("Import rejected: case ID is empty");
    std::set<std::string> entity_ids;
    for (const auto& item : entities) {
        const auto& object = as_object(item, "entity");
        if (required_string(object, "case_id") != case_id) throw std::invalid_argument("Import rejected: entity case reference mismatch");
        entity_ids.insert(required_string(object, "id"));
    }
    std::set<std::string> source_ids;
    for (const auto& item : sources) {
        const auto& object = as_object(item, "source");
        if (required_string(object, "case_id") != case_id) throw std::invalid_argument("Import rejected: source case reference mismatch");
        source_ids.insert(required_string(object, "id"));
    }
    std::set<std::string> evidence_ids;
    std::set<std::string> file_paths;
    for (const auto& item : evidence) {
        const auto& object = as_object(item, "evidence");
        const auto evidence_id = required_string(object, "id");
        if (required_string(object, "case_id") != case_id) throw std::invalid_argument("Import rejected: evidence case reference mismatch");
        evidence_ids.insert(evidence_id);
        if (const auto source = optional_value(object, "source_id"); source && !source_ids.contains(*source)) throw std::invalid_argument("Import rejected: unknown source reference " + *source);
        const auto kind = required_string(object, "kind");
        const auto path = optional_value(object, "relative_path");
        if (kind == "file") {
            if (!path || !safe_archive_path(*path) || path->rfind("attachments/", 0) != 0) throw std::invalid_argument("Import rejected: invalid file evidence path");
            if (!archive.entries.contains(*path)) throw std::invalid_argument("Import rejected: missing attachment " + *path);
            const auto hash = optional_value(object, "sha256");
            if (!hash || required_integer(object, "byte_size") < 0) throw std::invalid_argument("Import rejected: file evidence hash or size is missing");
            if (domain::lower_ascii(*hash) != sha256_hex(archive.entries.at(*path))) throw std::invalid_argument("Import rejected: evidence SHA-256 mismatch for " + evidence_id);
            if (static_cast<std::int64_t>(archive.entries.at(*path).size()) != required_integer(object, "byte_size")) throw std::invalid_argument("Import rejected: evidence size mismatch for " + evidence_id);
            file_paths.insert(*path);
        } else if (path) {
            throw std::invalid_argument("Import rejected: non-file evidence has a stored path");
        }
    }
    for (const auto& [path, bytes] : archive.entries) {
        (void)bytes;
        if (path.rfind("attachments/", 0) == 0 && !file_paths.contains(path)) throw std::invalid_argument("Import rejected: unreferenced attachment " + path);
    }
    std::set<std::string> claim_ids;
    for (const auto& item : claims) {
        const auto& object = as_object(item, "claim");
        if (required_string(object, "case_id") != case_id) throw std::invalid_argument("Import rejected: claim case reference mismatch");
        claim_ids.insert(required_string(object, "id"));
    }
    std::set<std::string> note_ids;
    for (const auto& item : notes) {
        const auto& object = as_object(item, "note");
        if (required_string(object, "case_id") != case_id) throw std::invalid_argument("Import rejected: note case reference mismatch");
        note_ids.insert(required_string(object, "id"));
    }
    std::set<std::string> run_ids;
    for (const auto& item : runs) {
        const auto& object = as_object(item, "playbook run");
        if (required_string(object, "case_id") != case_id) throw std::invalid_argument("Import rejected: run case reference mismatch");
        run_ids.insert(required_string(object, "id"));
        if (const auto entity = optional_value(object, "entity_id"); entity && !entity_ids.contains(*entity)) throw std::invalid_argument("Import rejected: unknown run entity");
    }
    std::set<std::string> run_step_ids;
    for (const auto& item : run_steps) {
        const auto& object = as_object(item, "run step");
        if (!run_ids.contains(required_string(object, "run_id"))) throw std::invalid_argument("Import rejected: unknown run reference");
        run_step_ids.insert(required_string(object, "id"));
    }
    for (const auto& item : claim_entities) {
        const auto& object = as_object(item, "claim entity link");
        if (!claim_ids.contains(required_string(object, "claim_id")) || !entity_ids.contains(required_string(object, "entity_id"))) throw std::invalid_argument("Import rejected: invalid claim entity reference");
    }
    std::map<std::string, std::pair<int, int>> relation_roles;
    std::map<std::string, int> entity_counts;
    for (const auto& item : claim_entities) {
        const auto& object = as_object(item, "claim entity link");
        const auto claim_id = required_string(object, "claim_id");
        const auto role = required_string(object, "role");
        ++entity_counts[claim_id];
        if (role == "subject") ++relation_roles[claim_id].first;
        if (role == "object") ++relation_roles[claim_id].second;
    }
    for (const auto& item : claims) {
        const auto& object = as_object(item, "claim");
        const auto claim_id = required_string(object, "id");
        if (entity_counts[claim_id] < 1) throw std::invalid_argument("Import rejected: claim has no entity reference");
        if (const auto predicate = optional_value(object, "predicate"); predicate && (relation_roles[claim_id].first != 1 || relation_roles[claim_id].second != 1)) throw std::invalid_argument("Import rejected: relation does not have exactly one subject and object");
    }
    for (const auto& item : claim_evidence) {
        const auto& object = as_object(item, "claim evidence link");
        if (!claim_ids.contains(required_string(object, "claim_id")) || !evidence_ids.contains(required_string(object, "evidence_id"))) throw std::invalid_argument("Import rejected: invalid claim evidence reference");
    }
    for (const auto& item : note_entities) {
        const auto& object = as_object(item, "note entity link");
        if (!note_ids.contains(required_string(object, "note_id")) || !entity_ids.contains(required_string(object, "entity_id"))) throw std::invalid_argument("Import rejected: invalid note entity reference");
    }
    for (const auto& item : note_evidence) {
        const auto& object = as_object(item, "note evidence link");
        if (!note_ids.contains(required_string(object, "note_id")) || !evidence_ids.contains(required_string(object, "evidence_id"))) throw std::invalid_argument("Import rejected: invalid note evidence reference");
    }
    for (const auto& item : run_step_evidence) {
        const auto& object = as_object(item, "run step evidence link");
        if (!run_step_ids.contains(required_string(object, "run_step_id")) || !evidence_ids.contains(required_string(object, "evidence_id"))) throw std::invalid_argument("Import rejected: invalid run step evidence reference");
    }
    for (const auto& item : logs) if (required_string(as_object(item, "log event"), "case_id") != case_id) throw std::invalid_argument("Import rejected: log case reference mismatch");
}

} // namespace

ImportResult ImportExportService::import_case(const std::filesystem::path& archive_path, ImportMode mode) {
    const auto archive = read_archive(archive_path);
    if (!archive.entries.contains("case.json") || !archive.entries.contains("manifest.json")) throw std::invalid_argument("Import rejected: archive must contain case.json and manifest.json");
    Json root_value;
    Json manifest_value;
    try {
        root_value = Json::parse(std::string(archive.entries.at("case.json").begin(), archive.entries.at("case.json").end()));
        manifest_value = Json::parse(std::string(archive.entries.at("manifest.json").begin(), archive.entries.at("manifest.json").end()));
    } catch (const json::JsonError& error) {
        throw std::invalid_argument(std::string("Import rejected: invalid JSON: ") + error.what());
    }
    validate_manifest(archive, manifest_value);
    const auto& root = as_object(root_value, "case.json root");
    if (required_string(root, "format") != "evidence-trace-case") throw std::invalid_argument("Import rejected: invalid case format");
    if (required_integer(root, "format_version") != 1) throw std::invalid_argument("Import rejected: unsupported case format version");
    if (required_integer(root, "schema_version") != database::current_schema_version) throw std::invalid_argument("Import rejected: unsupported database schema version");
    const auto& case_object = as_object(root.at("case"), "case");
    const auto original_case_id = required_string(case_object, "id");
    const auto& entities = required_array(root, "entities");
    const auto& sources = required_array(root, "sources");
    const auto& evidence = required_array(root, "evidence");
    const auto& claims = required_array(root, "claims");
    const auto& claim_entities = required_array(root, "claim_entities");
    const auto& claim_evidence = required_array(root, "claim_evidence");
    const auto& notes = required_array(root, "notes");
    const auto& note_entities = required_array(root, "note_entities");
    const auto& note_evidence = required_array(root, "note_evidence");
    const auto& runs = required_array(root, "playbook_runs");
    const auto& run_steps = required_array(root, "run_steps");
    const auto& run_step_evidence = required_array(root, "run_step_evidence");
    const auto& logs = required_array(root, "log_events");
    validate_import_records(case_object, entities, sources, evidence, claims, claim_entities, claim_evidence, notes, note_entities, note_evidence, runs, run_steps, run_step_evidence, logs, archive);

    const bool conflict = id_exists(database_, "cases", original_case_id);
    if (conflict && mode == ImportMode::SkipExisting) return {false, true, original_case_id, "Existing case skipped"};
    const bool remap = conflict && mode == ImportMode::ImportAsCopy;
    std::unordered_map<std::string, std::string> ids;
    ids.emplace(original_case_id, remap ? domain::new_id() : original_case_id);
    collect_id_map(entities, ids, remap); collect_id_map(sources, ids, remap); collect_id_map(evidence, ids, remap);
    collect_id_map(claims, ids, remap); collect_id_map(notes, ids, remap); collect_id_map(runs, ids, remap); collect_id_map(run_steps, ids, remap); collect_id_map(logs, ids, remap);
    if (!remap) {
        const std::vector<std::pair<std::string, std::string>> tables{{"entities", "entity"}, {"sources", "source"}, {"evidence", "evidence"}, {"claims", "claim"}, {"notes", "note"}, {"playbook_runs", "playbook run"}, {"run_steps", "run step"}, {"log_events", "log event"}};
        for (const auto& [table, label] : tables) for (const auto& [old_id, new_id] : ids) if (old_id != original_case_id && id_exists(database_, table, old_id)) throw std::invalid_argument("Import rejected: existing " + label + " ID " + old_id);
    }
    std::vector<std::pair<std::filesystem::path, storage::StagedFile>> staged_files;
    try {
        for (const auto& item : evidence) {
            const auto& object = as_object(item, "evidence");
            if (required_string(object, "kind") != "file") continue;
            const auto path = optional_value(object, "relative_path");
            if (!path) throw std::invalid_argument("Import rejected: file evidence has no path");
            const auto new_id = ids.at(required_string(object, "id"));
            staged_files.emplace_back(std::filesystem::path("attachments") / (new_id + ".bin"), attachment_store_.stage_bytes(archive.entries.at(*path)));
        }
        database::Transaction transaction(database_);
        auto case_insert = database_.prepare("INSERT INTO cases(id, title, purpose, description, primary_target_type, scope, tags_json, status, created_at, updated_at) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");
        case_insert.bind(1, ids.at(original_case_id)); case_insert.bind(2, required_string(case_object, "title") + (remap ? " (imported copy)" : "")); case_insert.bind(3, optional_string(case_object, "purpose")); case_insert.bind(4, optional_string(case_object, "description")); case_insert.bind(5, optional_value(case_object, "primary_target_type")); case_insert.bind(6, optional_string(case_object, "scope")); case_insert.bind(7, optional_string(case_object, "tags_json", "[]")); case_insert.bind(8, optional_string(case_object, "status", "active")); case_insert.bind(9, required_string(case_object, "created_at")); case_insert.bind(10, required_string(case_object, "updated_at")); case_insert.step();
        for (const auto& item : entities) { const auto& o = as_object(item, "entity"); auto s = database_.prepare("INSERT INTO entities(id, case_id, type, label, original_value, canonical_value, details_json, tags_json, aliases_json, created_at, updated_at) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);"); s.bind(1, mapped(required_string(o, "id"), ids)); s.bind(2, mapped(required_string(o, "case_id"), ids)); s.bind(3, required_string(o, "type")); s.bind(4, required_string(o, "label")); s.bind(5, optional_string(o, "original_value")); s.bind(6, optional_string(o, "canonical_value")); s.bind(7, optional_string(o, "details_json", "{}")); s.bind(8, optional_string(o, "tags_json", "[]")); s.bind(9, optional_string(o, "aliases_json", "[]")); s.bind(10, required_string(o, "created_at")); s.bind(11, required_string(o, "updated_at")); s.step(); }
        for (const auto& item : sources) { const auto& o = as_object(item, "source"); auto s = database_.prepare("INSERT INTO sources(id, case_id, type, locator, title, accessed_at, author, reliability_note, created_at) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?);"); s.bind(1, mapped(required_string(o, "id"), ids)); s.bind(2, mapped(required_string(o, "case_id"), ids)); s.bind(3, required_string(o, "type")); s.bind(4, optional_string(o, "locator")); s.bind(5, optional_string(o, "title")); s.bind(6, required_string(o, "accessed_at")); s.bind(7, optional_string(o, "author")); s.bind(8, optional_string(o, "reliability_note")); s.bind(9, required_string(o, "created_at")); s.step(); }
        for (const auto& item : evidence) {
            const auto& o = as_object(item, "evidence"); const auto kind = required_string(o, "kind"); const auto old_id = required_string(o, "id");
            auto s = database_.prepare("INSERT INTO evidence(id, case_id, source_id, kind, relative_path, original_filename, mime_type, byte_size, sha256, text_content, source_url, captured_at, imported_at, note, quotation_location, created_at) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");
            s.bind(1, mapped(old_id, ids)); s.bind(2, mapped(required_string(o, "case_id"), ids));
            if (const auto source = optional_value(o, "source_id"); source) s.bind(3, mapped(*source, ids)); else s.bind_null(3);
            s.bind(4, kind);
            if (kind == "file") s.bind(5, std::string("attachments/") + mapped(old_id, ids) + ".bin"); else s.bind(5, optional_value(o, "relative_path"));
            s.bind(6, optional_value(o, "original_filename")); s.bind(7, optional_value(o, "mime_type"));
            if (const auto iterator = o.find("byte_size"); iterator != o.end() && !iterator->second.is_null()) s.bind(8, iterator->second.as_int64()); else s.bind_null(8);
            s.bind(9, optional_value(o, "sha256")); s.bind(10, optional_string(o, "text_content")); s.bind(11, optional_value(o, "source_url")); s.bind(12, required_string(o, "captured_at")); s.bind(13, required_string(o, "imported_at")); s.bind(14, optional_string(o, "note")); s.bind(15, optional_string(o, "quotation_location")); s.bind(16, required_string(o, "created_at")); s.step();
        }
        for (const auto& item : claims) { const auto& o = as_object(item, "claim"); auto s = database_.prepare("INSERT INTO claims(id, case_id, statement, kind, predicate, status, reasoning, recorded_by, created_at, updated_at) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?);"); s.bind(1, mapped(required_string(o, "id"), ids)); s.bind(2, mapped(required_string(o, "case_id"), ids)); s.bind(3, required_string(o, "statement")); s.bind(4, required_string(o, "kind")); s.bind(5, optional_value(o, "predicate")); s.bind(6, required_string(o, "status")); s.bind(7, optional_string(o, "reasoning")); s.bind(8, optional_string(o, "recorded_by")); s.bind(9, required_string(o, "created_at")); s.bind(10, required_string(o, "updated_at")); s.step(); }
        for (const auto& item : claim_entities) { const auto& o = as_object(item, "claim entity link"); auto s = database_.prepare("INSERT INTO claim_entities(claim_id, entity_id, role) VALUES(?, ?, ?);"); s.bind(1, mapped(required_string(o, "claim_id"), ids)); s.bind(2, mapped(required_string(o, "entity_id"), ids)); s.bind(3, required_string(o, "role")); s.step(); }
        for (const auto& item : claim_evidence) { const auto& o = as_object(item, "claim evidence link"); auto s = database_.prepare("INSERT INTO claim_evidence(claim_id, evidence_id, role, note) VALUES(?, ?, ?, ?);"); s.bind(1, mapped(required_string(o, "claim_id"), ids)); s.bind(2, mapped(required_string(o, "evidence_id"), ids)); s.bind(3, required_string(o, "role")); s.bind(4, optional_string(o, "note")); s.step(); }
        for (const auto& item : notes) { const auto& o = as_object(item, "note"); auto s = database_.prepare("INSERT INTO notes(id, case_id, title, body, created_at, updated_at) VALUES(?, ?, ?, ?, ?, ?);"); s.bind(1, mapped(required_string(o, "id"), ids)); s.bind(2, mapped(required_string(o, "case_id"), ids)); s.bind(3, optional_string(o, "title")); s.bind(4, required_string(o, "body")); s.bind(5, required_string(o, "created_at")); s.bind(6, required_string(o, "updated_at")); s.step(); }
        for (const auto& item : note_entities) { const auto& o = as_object(item, "note entity link"); auto s = database_.prepare("INSERT INTO note_entities(note_id, entity_id) VALUES(?, ?);"); s.bind(1, mapped(required_string(o, "note_id"), ids)); s.bind(2, mapped(required_string(o, "entity_id"), ids)); s.step(); }
        for (const auto& item : note_evidence) { const auto& o = as_object(item, "note evidence link"); auto s = database_.prepare("INSERT INTO note_evidence(note_id, evidence_id) VALUES(?, ?);"); s.bind(1, mapped(required_string(o, "note_id"), ids)); s.bind(2, mapped(required_string(o, "evidence_id"), ids)); s.step(); }
        for (const auto& item : runs) { const auto& o = as_object(item, "playbook run"); auto s = database_.prepare("INSERT INTO playbook_runs(id, case_id, playbook_id, entity_id, name_snapshot, started_at, updated_at) VALUES(?, ?, NULL, ?, ?, ?, ?);"); s.bind(1, mapped(required_string(o, "id"), ids)); s.bind(2, mapped(required_string(o, "case_id"), ids)); if (const auto entity = optional_value(o, "entity_id"); entity) s.bind(3, mapped(*entity, ids)); else s.bind_null(3); s.bind(4, required_string(o, "name_snapshot")); s.bind(5, required_string(o, "started_at")); s.bind(6, required_string(o, "updated_at")); s.step(); }
        for (const auto& item : run_steps) { const auto& o = as_object(item, "run step"); auto s = database_.prepare("INSERT INTO run_steps(id, run_id, source_step_id, position, name_snapshot, instructions_snapshot, state, completed_at, note) VALUES(?, ?, NULL, ?, ?, ?, ?, ?, ?);"); s.bind(1, mapped(required_string(o, "id"), ids)); s.bind(2, mapped(required_string(o, "run_id"), ids)); s.bind(3, required_integer(o, "position")); s.bind(4, required_string(o, "name_snapshot")); s.bind(5, optional_string(o, "instructions_snapshot")); s.bind(6, required_string(o, "state")); s.bind(7, optional_value(o, "completed_at")); s.bind(8, optional_string(o, "note")); s.step(); }
        for (const auto& item : run_step_evidence) { const auto& o = as_object(item, "run step evidence link"); auto s = database_.prepare("INSERT INTO run_step_evidence(run_step_id, evidence_id) VALUES(?, ?);"); s.bind(1, mapped(required_string(o, "run_step_id"), ids)); s.bind(2, mapped(required_string(o, "evidence_id"), ids)); s.step(); }
        for (const auto& item : logs) { const auto& o = as_object(item, "log event"); auto s = database_.prepare("INSERT INTO log_events(id, case_id, occurred_at, action, object_type, object_id, description, payload_json) VALUES(?, ?, ?, ?, ?, ?, ?, ?);"); s.bind(1, mapped(required_string(o, "id"), ids)); s.bind(2, mapped(required_string(o, "case_id"), ids)); s.bind(3, required_string(o, "occurred_at")); s.bind(4, required_string(o, "action")); s.bind(5, required_string(o, "object_type")); if (const auto object_id = optional_value(o, "object_id"); object_id) s.bind(6, mapped(*object_id, ids)); else s.bind_null(6); s.bind(7, required_string(o, "description")); s.bind(8, replace_ids(optional_string(o, "payload_json", "{}"), ids)); s.step(); }
        for (auto& [relative, staged] : staged_files) attachment_store_.move_to_final(staged, relative);
        logger_.record(ids.at(original_case_id), "imported", "case", ids.at(original_case_id), remap ? "Case imported as a copy" : "Case imported");
        transaction.commit();
        return {true, false, ids.at(original_case_id), remap ? "Case imported as a copy" : "Case imported"};
    } catch (...) {
        for (auto& [relative, staged] : staged_files) {
            if (staged.temporary_path.empty()) attachment_store_.remove(relative);
            else { std::error_code ignored; std::filesystem::remove(staged.temporary_path, ignored); }
        }
        throw;
    }
}

} // namespace evidence_trace::import_export
