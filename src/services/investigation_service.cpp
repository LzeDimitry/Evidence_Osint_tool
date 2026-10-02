#include "services/investigation_service.hpp"

#include "domain/types.hpp"

#include <sstream>
#include <stdexcept>

namespace evidence_trace::services {

namespace {

std::string json_escape(const std::string& value) {
    std::ostringstream output;
    output << '"';
    for (const unsigned char character : value) {
        switch (character) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (character < 0x20) output << "\\u00" << std::hex << static_cast<int>(character);
            else output << static_cast<char>(character);
        }
    }
    output << '"';
    return output.str();
}

std::string like_pattern(const std::string& query) {
    return "%" + domain::lower_ascii(query) + "%";
}

std::filesystem::path expected_attachment_path(const domain::Id& evidence_id) {
    return std::filesystem::path("attachments") / (evidence_id + ".bin");
}

} // namespace

void InvestigationService::ensure_case(const domain::Id& case_id) const {
    auto statement = database_.prepare("SELECT 1 FROM cases WHERE id = ?;");
    statement.bind(1, case_id);
    if (!statement.step()) throw std::runtime_error("Case not found: " + case_id);
}

domain::Entity InvestigationService::get_entity_internal(const domain::Id& id) const {
    auto statement = database_.prepare(
        "SELECT id, case_id, type, label, original_value, canonical_value, details_json, tags_json, aliases_json, created_at, updated_at "
        "FROM entities WHERE id = ?;");
    statement.bind(1, id);
    if (!statement.step()) throw std::runtime_error("Entity not found: " + id);
    return read_entity(statement);
}

domain::Source InvestigationService::get_source_internal(const domain::Id& id) const {
    auto statement = database_.prepare(
        "SELECT id, case_id, type, locator, title, accessed_at, author, reliability_note, created_at "
        "FROM sources WHERE id = ?;");
    statement.bind(1, id);
    if (!statement.step()) throw std::runtime_error("Source not found: " + id);
    return read_source(statement);
}

domain::Evidence InvestigationService::get_evidence_internal(const domain::Id& id) const {
    auto statement = database_.prepare(
        "SELECT id, case_id, source_id, kind, relative_path, original_filename, mime_type, byte_size, sha256, "
        "text_content, source_url, captured_at, imported_at, note, quotation_location, created_at "
        "FROM evidence WHERE id = ?;");
    statement.bind(1, id);
    if (!statement.step()) throw std::runtime_error("Evidence not found: " + id);
    return read_evidence(statement);
}

domain::Claim InvestigationService::get_claim_internal(const domain::Id& id) const {
    auto statement = database_.prepare(
        "SELECT id, case_id, statement, kind, predicate, status, reasoning, recorded_by, created_at, updated_at "
        "FROM claims WHERE id = ?;");
    statement.bind(1, id);
    if (!statement.step()) throw std::runtime_error("Claim not found: " + id);
    return read_claim(statement);
}

void InvestigationService::ensure_source_in_case(const std::optional<domain::Id>& source_id,
                                                  const domain::Id& case_id) const {
    if (!source_id.has_value()) return;
    if (get_source_internal(*source_id).case_id != case_id) {
        throw std::invalid_argument("Source belongs to a different case");
    }
}

void InvestigationService::ensure_entity_links_in_case(const domain::Id& case_id,
                                                       const std::vector<EntityLinkInput>& links) const {
    if (links.empty()) throw std::invalid_argument("A claim must refer to at least one entity");
    for (const auto& [entity_id, role] : links) {
        (void)role;
        if (get_entity_internal(entity_id).case_id != case_id) {
            throw std::invalid_argument("Claim entity belongs to a different case");
        }
    }
}

void InvestigationService::ensure_evidence_links_in_case(const domain::Id& case_id,
                                                          const std::vector<EvidenceLinkInput>& links) const {
    for (const auto& [evidence_id, role, note] : links) {
        (void)role;
        (void)note;
        if (get_evidence_internal(evidence_id).case_id != case_id) {
            throw std::invalid_argument("Claim evidence belongs to a different case");
        }
    }
}

std::string InvestigationService::canonical_value(domain::EntityType type, const std::string& original_value) {
    const auto trimmed = domain::trim(original_value);
    if (type == domain::EntityType::Domain || type == domain::EntityType::Email) {
        return domain::lower_ascii(trimmed);
    }
    if (type == domain::EntityType::Url) return original_value;
    return trimmed;
}

std::string InvestigationService::tags_to_json(const std::vector<std::string>& tags) {
    std::ostringstream output;
    output << '[';
    for (std::size_t i = 0; i < tags.size(); ++i) {
        if (i != 0) output << ',';
        output << json_escape(domain::trim(tags[i]));
    }
    output << ']';
    return output.str();
}

domain::Entity InvestigationService::read_entity(database::Statement& statement) {
    domain::Entity value;
    value.id = statement.column_text(0);
    value.case_id = statement.column_text(1);
    value.type = domain::entity_type_from_string(statement.column_text(2));
    value.label = statement.column_text(3);
    value.original_value = statement.column_text(4);
    value.canonical_value = statement.column_text(5);
    value.details_json = statement.column_text(6);
    value.tags_json = statement.column_text(7);
    value.aliases_json = statement.column_text(8);
    value.created_at = statement.column_text(9);
    value.updated_at = statement.column_text(10);
    return value;
}

domain::Source InvestigationService::read_source(database::Statement& statement) {
    domain::Source value;
    value.id = statement.column_text(0);
    value.case_id = statement.column_text(1);
    value.type = domain::source_type_from_string(statement.column_text(2));
    value.locator = statement.column_text(3);
    value.title = statement.column_text(4);
    value.accessed_at = statement.column_text(5);
    value.author = statement.column_text(6);
    value.reliability_note = statement.column_text(7);
    value.created_at = statement.column_text(8);
    return value;
}

domain::Evidence InvestigationService::read_evidence(database::Statement& statement) {
    domain::Evidence value;
    value.id = statement.column_text(0);
    value.case_id = statement.column_text(1);
    value.source_id = statement.column_optional_text(2);
    value.kind = domain::evidence_kind_from_string(statement.column_text(3));
    value.relative_path = statement.column_optional_text(4);
    value.original_filename = statement.column_optional_text(5);
    value.mime_type = statement.column_optional_text(6);
    if (!statement.column_is_null(7)) value.byte_size = statement.column_int64(7);
    value.sha256 = statement.column_optional_text(8);
    value.text_content = statement.column_text(9);
    value.source_url = statement.column_optional_text(10);
    value.captured_at = statement.column_text(11);
    value.imported_at = statement.column_text(12);
    value.note = statement.column_text(13);
    value.quotation_location = statement.column_text(14);
    value.created_at = statement.column_text(15);
    return value;
}

domain::Claim InvestigationService::read_claim(database::Statement& statement) {
    domain::Claim value;
    value.id = statement.column_text(0);
    value.case_id = statement.column_text(1);
    value.statement = statement.column_text(2);
    value.kind = domain::claim_kind_from_string(statement.column_text(3));
    value.predicate = statement.column_optional_text(4);
    value.status = domain::claim_status_from_string(statement.column_text(5));
    value.reasoning = statement.column_text(6);
    value.recorded_by = statement.column_text(7);
    value.created_at = statement.column_text(8);
    value.updated_at = statement.column_text(9);
    return value;
}

domain::Note InvestigationService::read_note(database::Statement& statement) {
    domain::Note value;
    value.id = statement.column_text(0);
    value.case_id = statement.column_text(1);
    value.title = statement.column_text(2);
    value.body = statement.column_text(3);
    value.created_at = statement.column_text(4);
    value.updated_at = statement.column_text(5);
    return value;
}

domain::Entity InvestigationService::create_entity(const domain::Id& case_id,
                                                    domain::EntityType type,
                                                    const std::string& label,
                                                    const std::string& original_value,
                                                    const std::string& details_json,
                                                    const std::vector<std::string>& tags,
                                                    const std::vector<std::string>& aliases) {
    ensure_case(case_id);
    if (domain::trim(label).empty()) throw std::invalid_argument("Entity label is required");
    const auto now = domain::utc_now();
    domain::Entity value;
    value.id = domain::new_id();
    value.case_id = case_id;
    value.type = type;
    value.label = label;
    value.original_value = original_value;
    value.canonical_value = canonical_value(type, original_value.empty() ? label : original_value);
    value.details_json = details_json;
    value.tags_json = tags_to_json(tags);
    value.aliases_json = tags_to_json(aliases);
    value.created_at = now;
    value.updated_at = now;
    database::Transaction transaction(database_);
    auto statement = database_.prepare(
        "INSERT INTO entities(id, case_id, type, label, original_value, canonical_value, details_json, tags_json, aliases_json, created_at, updated_at) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");
    statement.bind(1, value.id);
    statement.bind(2, value.case_id);
    statement.bind(3, domain::to_string(value.type));
    statement.bind(4, value.label);
    statement.bind(5, value.original_value);
    statement.bind(6, value.canonical_value);
    statement.bind(7, value.details_json);
    statement.bind(8, value.tags_json);
    statement.bind(9, value.aliases_json);
    statement.bind(10, value.created_at);
    statement.bind(11, value.updated_at);
    statement.step();
    logger_.record(case_id, "created", "entity", value.id, "Entity created: " + label);
    transaction.commit();
    return value;
}

void InvestigationService::update_entity(const domain::Id& id,
                                          const std::string& label,
                                          const std::string& original_value,
                                          const std::string& details_json,
                                          const std::vector<std::string>& tags,
                                          const std::vector<std::string>& aliases) {
    const auto current = get_entity_internal(id);
    if (domain::trim(label).empty()) throw std::invalid_argument("Entity label is required");
    const auto value = original_value.empty() ? label : original_value;
    auto statement = database_.prepare(
        "UPDATE entities SET label = ?, original_value = ?, canonical_value = ?, details_json = ?, tags_json = ?, aliases_json = ?, updated_at = ? WHERE id = ?;");
    statement.bind(1, label);
    statement.bind(2, original_value);
    statement.bind(3, canonical_value(current.type, value));
    statement.bind(4, details_json);
    statement.bind(5, tags_to_json(tags));
    statement.bind(6, tags_to_json(aliases));
    statement.bind(7, domain::utc_now());
    statement.bind(8, id);
    statement.step();
    logger_.record(current.case_id, "updated", "entity", id, "Entity details updated");
}

void InvestigationService::delete_entity(const domain::Id& id, bool explicit_confirmation) {
    const auto entity = get_entity_internal(id);

    auto claim_count_statement = database_.prepare("SELECT count(*) FROM claim_entities WHERE entity_id = ?;");
    claim_count_statement.bind(1, id);
    claim_count_statement.step();
    const auto claim_links = claim_count_statement.column_int64(0);
    if (claim_links > 0) {
        throw std::invalid_argument(
            "Entity is referenced by " + std::to_string(claim_links) +
            " claim(s); review those claims in Claims & Relations before deleting the entity");
    }

    auto note_count_statement = database_.prepare("SELECT count(*) FROM note_entities WHERE entity_id = ?;");
    note_count_statement.bind(1, id);
    note_count_statement.step();
    const auto note_links = note_count_statement.column_int64(0);
    if (note_links > 0 && !explicit_confirmation) {
        throw std::invalid_argument("Entity is linked to notes; explicit confirmation is required before deletion");
    }

    database::Transaction transaction(database_);
    logger_.record(entity.case_id, "deleted", "entity", id,
                   "Entity deleted after explicit confirmation",
                   "{\"note_links\":" + std::to_string(note_links) + "}");
    auto note_links_statement = database_.prepare("DELETE FROM note_entities WHERE entity_id = ?;");
    note_links_statement.bind(1, id);
    note_links_statement.step();
    auto delete_statement = database_.prepare("DELETE FROM entities WHERE id = ?;");
    delete_statement.bind(1, id);
    delete_statement.step();
    transaction.commit();
}

std::vector<domain::Entity> InvestigationService::list_entities(const domain::Id& case_id) const {
    ensure_case(case_id);
    std::vector<domain::Entity> values;
    auto statement = database_.prepare(
        "SELECT id, case_id, type, label, original_value, canonical_value, details_json, tags_json, aliases_json, created_at, updated_at "
        "FROM entities WHERE case_id = ? ORDER BY label COLLATE NOCASE, id;");
    statement.bind(1, case_id);
    while (statement.step()) values.push_back(read_entity(statement));
    return values;
}

domain::Entity InvestigationService::get_entity(const domain::Id& id) const {
    return get_entity_internal(id);
}

std::vector<domain::Entity> InvestigationService::possible_duplicates(const domain::Id& case_id,
                                                                       domain::EntityType type,
                                                                       const std::string& original_value) const {
    ensure_case(case_id);
    std::vector<domain::Entity> values;
    auto statement = database_.prepare(
        "SELECT id, case_id, type, label, original_value, canonical_value, details_json, tags_json, aliases_json, created_at, updated_at "
        "FROM entities WHERE case_id = ? AND type = ? AND canonical_value = ? ORDER BY created_at;");
    statement.bind(1, case_id);
    statement.bind(2, domain::to_string(type));
    statement.bind(3, canonical_value(type, original_value));
    while (statement.step()) values.push_back(read_entity(statement));
    return values;
}

std::vector<domain::Claim> InvestigationService::claims_for_entity(const domain::Id& entity_id) const {
    const auto entity = get_entity_internal(entity_id);
    std::vector<domain::Claim> values;
    auto statement = database_.prepare(
        "SELECT c.id, c.case_id, c.statement, c.kind, c.predicate, c.status, c.reasoning, c.recorded_by, c.created_at, c.updated_at "
        "FROM claims c JOIN claim_entities ce ON ce.claim_id = c.id WHERE ce.entity_id = ? "
        "ORDER BY c.updated_at DESC, c.id;");
    statement.bind(1, entity.id);
    while (statement.step()) values.push_back(read_claim(statement));
    return values;
}

domain::Source InvestigationService::create_source(const domain::Id& case_id,
                                                   domain::SourceType type,
                                                   const std::string& locator,
                                                   const std::string& title,
                                                   const std::string& accessed_at,
                                                   const std::string& author,
                                                   const std::string& reliability_note) {
    ensure_case(case_id);
    domain::Source value;
    value.id = domain::new_id();
    value.case_id = case_id;
    value.type = type;
    value.locator = locator;
    value.title = title;
    value.accessed_at = accessed_at.empty() ? domain::utc_now() : accessed_at;
    value.author = author;
    value.reliability_note = reliability_note;
    value.created_at = domain::utc_now();
    database::Transaction transaction(database_);
    auto statement = database_.prepare(
        "INSERT INTO sources(id, case_id, type, locator, title, accessed_at, author, reliability_note, created_at) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?);");
    statement.bind(1, value.id);
    statement.bind(2, value.case_id);
    statement.bind(3, domain::to_string(value.type));
    statement.bind(4, value.locator);
    statement.bind(5, value.title);
    statement.bind(6, value.accessed_at);
    statement.bind(7, value.author);
    statement.bind(8, value.reliability_note);
    statement.bind(9, value.created_at);
    statement.step();
    logger_.record(case_id, "created", "source", value.id, "Source created");
    transaction.commit();
    return value;
}

std::vector<domain::Source> InvestigationService::list_sources(const domain::Id& case_id) const {
    ensure_case(case_id);
    std::vector<domain::Source> values;
    auto statement = database_.prepare(
        "SELECT id, case_id, type, locator, title, accessed_at, author, reliability_note, created_at "
        "FROM sources WHERE case_id = ? ORDER BY accessed_at DESC, id;");
    statement.bind(1, case_id);
    while (statement.step()) values.push_back(read_source(statement));
    return values;
}

domain::Source InvestigationService::get_source(const domain::Id& id) const {
    return get_source_internal(id);
}

void InvestigationService::delete_source(const domain::Id& id, bool explicit_confirmation) {
    const auto source = get_source_internal(id);
    auto count_statement = database_.prepare("SELECT count(*) FROM evidence WHERE source_id = ?;");
    count_statement.bind(1, id);
    count_statement.step();
    const auto evidence_links = count_statement.column_int64(0);
    if (evidence_links > 0) {
        throw std::invalid_argument(
            "Source is referenced by " + std::to_string(evidence_links) +
            " evidence item(s); review or move those evidence records before deleting the source");
    }
    if (!explicit_confirmation) throw std::invalid_argument("Source deletion requires explicit confirmation");
    database::Transaction transaction(database_);
    logger_.record(source.case_id, "deleted", "source", id, "Source deleted after explicit confirmation");
    auto statement = database_.prepare("DELETE FROM sources WHERE id = ?;");
    statement.bind(1, id);
    statement.step();
    transaction.commit();
}

domain::Evidence InvestigationService::add_file_evidence(const domain::Id& case_id,
                                                          const std::filesystem::path& original_file,
                                                          const std::optional<domain::Id>& source_id,
                                                          const std::optional<std::string>& source_url,
                                                          const std::string& note,
                                                          const std::optional<std::string>& mime_type,
                                                          const std::string& captured_at) {
    ensure_case(case_id);
    ensure_source_in_case(source_id, case_id);
    auto staged = attachment_store_.stage_file(original_file);
    domain::Evidence value;
    value.id = domain::new_id();
    value.case_id = case_id;
    value.source_id = source_id;
    value.kind = domain::EvidenceKind::File;
    value.relative_path = std::filesystem::path("attachments") / (value.id + ".bin");
    value.original_filename = original_file.filename().string();
    value.mime_type = mime_type;
    value.byte_size = staged.byte_size;
    value.sha256 = staged.sha256;
    value.source_url = source_url;
    value.captured_at = captured_at.empty() ? domain::utc_now() : captured_at;
    value.imported_at = domain::utc_now();
    value.note = note;
    value.created_at = domain::utc_now();
    bool moved = false;
    try {
        database::Transaction transaction(database_);
        auto statement = database_.prepare(
            "INSERT INTO evidence(id, case_id, source_id, kind, relative_path, original_filename, mime_type, byte_size, sha256, "
            "text_content, source_url, captured_at, imported_at, note, quotation_location, created_at) "
            "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");
        statement.bind(1, value.id);
        statement.bind(2, value.case_id);
        statement.bind(3, value.source_id);
        statement.bind(4, domain::to_string(value.kind));
        statement.bind(5, *value.relative_path);
        statement.bind(6, value.original_filename);
        statement.bind(7, value.mime_type);
        statement.bind(8, value.byte_size);
        statement.bind(9, value.sha256);
        statement.bind(10, value.text_content);
        statement.bind(11, value.source_url);
        statement.bind(12, value.captured_at);
        statement.bind(13, value.imported_at);
        statement.bind(14, value.note);
        statement.bind(15, value.quotation_location);
        statement.bind(16, value.created_at);
        statement.step();
        attachment_store_.move_to_final(staged, *value.relative_path);
        moved = true;
        logger_.record(case_id, "added", "evidence", value.id, "File evidence added");
        transaction.commit();
        return value;
    } catch (...) {
        if (moved) attachment_store_.remove(*value.relative_path);
        if (!staged.temporary_path.empty()) {
            std::error_code ignored;
            std::filesystem::remove(staged.temporary_path, ignored);
        }
        throw;
    }
}

domain::Evidence InvestigationService::add_text_evidence(const domain::Id& case_id,
                                                          const std::string& text,
                                                          const std::optional<domain::Id>& source_id,
                                                          const std::optional<std::string>& source_url,
                                                          const std::string& note,
                                                          const std::string& quotation_location,
                                                          const std::string& captured_at) {
    ensure_case(case_id);
    ensure_source_in_case(source_id, case_id);
    if (text.empty()) throw std::invalid_argument("Text evidence cannot be empty");
    domain::Evidence value;
    value.id = domain::new_id();
    value.case_id = case_id;
    value.source_id = source_id;
    value.kind = domain::EvidenceKind::Text;
    value.text_content = text;
    value.source_url = source_url;
    value.captured_at = captured_at.empty() ? domain::utc_now() : captured_at;
    value.imported_at = domain::utc_now();
    value.note = note;
    value.quotation_location = quotation_location;
    value.created_at = domain::utc_now();
    database::Transaction transaction(database_);
    auto statement = database_.prepare(
        "INSERT INTO evidence(id, case_id, source_id, kind, relative_path, original_filename, mime_type, byte_size, sha256, "
        "text_content, source_url, captured_at, imported_at, note, quotation_location, created_at) "
        "VALUES(?, ?, ?, ?, NULL, NULL, NULL, NULL, NULL, ?, ?, ?, ?, ?, ?, ?);");
    statement.bind(1, value.id);
    statement.bind(2, value.case_id);
    statement.bind(3, value.source_id);
    statement.bind(4, domain::to_string(value.kind));
    statement.bind(5, value.text_content);
    statement.bind(6, value.source_url);
    statement.bind(7, value.captured_at);
    statement.bind(8, value.imported_at);
    statement.bind(9, value.note);
    statement.bind(10, value.quotation_location);
    statement.bind(11, value.created_at);
    statement.step();
    logger_.record(case_id, "added", "evidence", value.id, "Text evidence added");
    transaction.commit();
    return value;
}

domain::Evidence InvestigationService::add_url_evidence(const domain::Id& case_id,
                                                         const std::string& url,
                                                         const std::optional<domain::Id>& source_id,
                                                         const std::string& note,
                                                         const std::string& captured_at) {
    ensure_case(case_id);
    ensure_source_in_case(source_id, case_id);
    if (domain::trim(url).empty()) throw std::invalid_argument("URL evidence requires a URL");
    domain::Evidence value;
    value.id = domain::new_id();
    value.case_id = case_id;
    value.source_id = source_id;
    value.kind = domain::EvidenceKind::Url;
    value.source_url = url;
    value.captured_at = captured_at.empty() ? domain::utc_now() : captured_at;
    value.imported_at = domain::utc_now();
    value.note = note;
    value.created_at = domain::utc_now();
    database::Transaction transaction(database_);
    auto statement = database_.prepare(
        "INSERT INTO evidence(id, case_id, source_id, kind, relative_path, original_filename, mime_type, byte_size, sha256, "
        "text_content, source_url, captured_at, imported_at, note, quotation_location, created_at) "
        "VALUES(?, ?, ?, ?, NULL, NULL, NULL, NULL, NULL, '', ?, ?, ?, ?, '', ?);");
    statement.bind(1, value.id);
    statement.bind(2, value.case_id);
    statement.bind(3, value.source_id);
    statement.bind(4, domain::to_string(value.kind));
    statement.bind(5, value.source_url);
    statement.bind(6, value.captured_at);
    statement.bind(7, value.imported_at);
    statement.bind(8, value.note);
    statement.bind(9, value.created_at);
    statement.step();
    logger_.record(case_id, "added", "evidence", value.id, "External URL evidence added");
    transaction.commit();
    return value;
}

domain::Evidence InvestigationService::get_evidence(const domain::Id& id) const {
    return get_evidence_internal(id);
}

std::vector<domain::Evidence> InvestigationService::list_evidence(const domain::Id& case_id) const {
    ensure_case(case_id);
    std::vector<domain::Evidence> values;
    auto statement = database_.prepare(
        "SELECT id, case_id, source_id, kind, relative_path, original_filename, mime_type, byte_size, sha256, "
        "text_content, source_url, captured_at, imported_at, note, quotation_location, created_at "
        "FROM evidence WHERE case_id = ? ORDER BY imported_at DESC, id;");
    statement.bind(1, case_id);
    while (statement.step()) values.push_back(read_evidence(statement));
    return values;
}

domain::IntegrityResult InvestigationService::verify_evidence(const domain::Id& id) const {
    const auto evidence = get_evidence_internal(id);
    domain::IntegrityResult result;
    result.evidence_id = id;
    const auto finish = [&](domain::IntegrityResult value) {
        logger_.record(evidence.case_id, "integrity_checked", "evidence", id, value.message);
        return value;
    };
    if (evidence.kind != domain::EvidenceKind::File || !evidence.relative_path.has_value() || !evidence.sha256.has_value()) {
        result.message = "Evidence is external only; no preserved file to verify";
        return finish(result);
    }
    try {
        if (!attachment_store_.exists(*evidence.relative_path)) {
            result.message = "File changed or unavailable";
            return finish(result);
        }
        result.actual_sha256 = attachment_store_.hash(*evidence.relative_path);
        const auto actual_size = attachment_store_.size(*evidence.relative_path);
        result.ok = *result.actual_sha256 == *evidence.sha256 &&
                    (!evidence.byte_size.has_value() || actual_size == *evidence.byte_size);
        result.message = result.ok ? "Integrity verified" : "File changed or unavailable";
        return finish(result);
    } catch (const std::exception&) {
        result.message = "File changed or unavailable";
        return finish(result);
    }
}

std::vector<domain::Claim> InvestigationService::claims_for_evidence(const domain::Id& evidence_id) const {
    const auto evidence = get_evidence_internal(evidence_id);
    std::vector<domain::Claim> values;
    auto statement = database_.prepare(
        "SELECT c.id, c.case_id, c.statement, c.kind, c.predicate, c.status, c.reasoning, c.recorded_by, c.created_at, c.updated_at "
        "FROM claims c JOIN claim_evidence ce ON ce.claim_id = c.id WHERE ce.evidence_id = ? "
        "ORDER BY c.updated_at DESC, c.id;");
    statement.bind(1, evidence.id);
    while (statement.step()) values.push_back(read_claim(statement));
    return values;
}

void InvestigationService::delete_evidence(const domain::Id& id, bool explicit_confirmation) {
    const auto evidence = get_evidence_internal(id);
    auto count_statement = database_.prepare("SELECT count(*) FROM claim_evidence WHERE evidence_id = ?;");
    count_statement.bind(1, id);
    count_statement.step();
    const auto claim_links = count_statement.column_int64(0);
    auto note_count = database_.prepare("SELECT count(*) FROM note_evidence WHERE evidence_id = ?;");
    note_count.bind(1, id);
    note_count.step();
    const auto note_links = note_count.column_int64(0);
    auto run_count = database_.prepare("SELECT count(*) FROM run_step_evidence WHERE evidence_id = ?;");
    run_count.bind(1, id);
    run_count.step();
    const auto run_links = run_count.column_int64(0);
    if ((claim_links > 0 || note_links > 0 || run_links > 0) && !explicit_confirmation) {
        throw std::invalid_argument("Evidence is linked; explicit confirmation is required before deletion");
    }
    std::optional<storage::StagedRemoval> staged;
    if (evidence.relative_path.has_value()) {
        if (*evidence.relative_path != expected_attachment_path(id)) {
            throw std::invalid_argument("Refusing to delete evidence: attachment path is not owned by its evidence record");
        }
        staged = attachment_store_.stage_removal(*evidence.relative_path);
    }
    try {
        database::Transaction transaction(database_);
        logger_.record(evidence.case_id, "deleted", "evidence", id,
                       "Evidence deleted after explicit confirmation",
                       "{\"claim_links\":" + std::to_string(claim_links) + "}");
        for (const auto& sql : {"DELETE FROM claim_evidence WHERE evidence_id = ?;",
                                "DELETE FROM note_evidence WHERE evidence_id = ?;",
                                "DELETE FROM run_step_evidence WHERE evidence_id = ?;"}) {
            auto statement = database_.prepare(sql);
            statement.bind(1, id);
            statement.step();
        }
        auto statement = database_.prepare("DELETE FROM evidence WHERE id = ?;");
        statement.bind(1, id);
        statement.step();
        transaction.commit();
    } catch (...) {
        if (staged) {
            try { attachment_store_.restore_removal(*staged); }
            catch (const std::exception& error) { throw std::runtime_error(std::string("Evidence deletion failed and attachment recovery failed: ") + error.what()); }
        }
        throw;
    }
    if (staged) {
        try { attachment_store_.discard_removal(*staged); }
        catch (const std::exception& error) { throw std::runtime_error(std::string("Evidence record was deleted, but its quarantined attachment could not be cleaned up: ") + error.what()); }
    }
}

domain::Claim InvestigationService::create_claim(const domain::Id& case_id,
                                                 const std::string& statement_text,
                                                 domain::ClaimKind kind,
                                                 const std::vector<EntityLinkInput>& entities,
                                                 const std::optional<std::string>& predicate,
                                                 domain::ClaimStatus status,
                                                 const std::string& reasoning,
                                                 const std::string& recorded_by,
                                                 const std::vector<EvidenceLinkInput>& evidence_links) {
    ensure_case(case_id);
    ensure_entity_links_in_case(case_id, entities);
    ensure_evidence_links_in_case(case_id, evidence_links);
    if (domain::trim(statement_text).empty()) throw std::invalid_argument("Claim statement is required");
    if (kind == domain::ClaimKind::Inference && domain::trim(reasoning).empty()) {
        throw std::invalid_argument("Reasoning is required for an inference claim");
    }
    if (predicate.has_value() && domain::trim(*predicate).empty()) throw std::invalid_argument("Predicate cannot be empty");
    if (predicate.has_value()) {
        std::int64_t subjects = 0;
        std::int64_t objects = 0;
        for (const auto& [id, role] : entities) {
            (void)id;
            if (role == domain::ClaimEntityRole::Subject) ++subjects;
            if (role == domain::ClaimEntityRole::Object) ++objects;
        }
        if (subjects != 1 || objects != 1) throw std::invalid_argument("A relation requires exactly one subject and one object");
    }
    if (status == domain::ClaimStatus::Confirmed) {
        bool supports = false;
        for (const auto& [id, role, note] : evidence_links) {
            (void)id; (void)note;
            if (role == domain::EvidenceRole::Supports) supports = true;
        }
        if (!supports) throw std::invalid_argument("Attach at least one supporting item before marking this claim confirmed");
        if (domain::trim(reasoning).empty()) throw std::invalid_argument("Written reasoning is required before marking this claim confirmed");
    }

    const auto now = domain::utc_now();
    domain::Claim value;
    value.id = domain::new_id();
    value.case_id = case_id;
    value.statement = statement_text;
    value.kind = kind;
    value.predicate = predicate;
    value.status = status;
    value.reasoning = reasoning;
    value.recorded_by = recorded_by.empty() ? "local-investigator" : recorded_by;
    value.created_at = now;
    value.updated_at = now;

    database::Transaction transaction(database_);
    auto statement = database_.prepare(
        "INSERT INTO claims(id, case_id, statement, kind, predicate, status, reasoning, recorded_by, created_at, updated_at) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");
    statement.bind(1, value.id);
    statement.bind(2, value.case_id);
    statement.bind(3, value.statement);
    statement.bind(4, domain::to_string(value.kind));
    statement.bind(5, value.predicate);
    statement.bind(6, domain::to_string(value.status));
    statement.bind(7, value.reasoning);
    statement.bind(8, value.recorded_by);
    statement.bind(9, value.created_at);
    statement.bind(10, value.updated_at);
    statement.step();
    for (const auto& [entity_id, role] : entities) {
        auto link = database_.prepare("INSERT INTO claim_entities(claim_id, entity_id, role) VALUES(?, ?, ?);");
        link.bind(1, value.id);
        link.bind(2, entity_id);
        link.bind(3, domain::to_string(role));
        link.step();
    }
    for (const auto& [evidence_id, role, note] : evidence_links) {
        auto link = database_.prepare("INSERT INTO claim_evidence(claim_id, evidence_id, role, note) VALUES(?, ?, ?, ?);");
        link.bind(1, value.id);
        link.bind(2, evidence_id);
        link.bind(3, domain::to_string(role));
        link.bind(4, note);
        link.step();
    }
    logger_.record(case_id, "created", predicate.has_value() ? "relation" : "claim", value.id,
                   predicate.has_value() ? "Relation claim created" : "Claim created");
    transaction.commit();
    return value;
}

domain::Claim InvestigationService::create_relation(const domain::Id& case_id,
                                                    const domain::Id& subject_entity_id,
                                                    const std::string& predicate,
                                                    const domain::Id& object_entity_id,
                                                    const std::string& statement_text,
                                                    domain::ClaimStatus status,
                                                    const std::string& reasoning,
                                                    const std::string& recorded_by,
                                                    const std::vector<EvidenceLinkInput>& evidence_links,
                                                    domain::ClaimKind kind) {
    const auto subject = get_entity_internal(subject_entity_id);
    const auto object = get_entity_internal(object_entity_id);
    if (subject.case_id != case_id || object.case_id != case_id) throw std::invalid_argument("Relation entities belong to a different case");
    const auto text = statement_text.empty() ? subject.label + " " + predicate + " " + object.label : statement_text;
    return create_claim(case_id, text, kind,
                        {{subject_entity_id, domain::ClaimEntityRole::Subject},
                         {object_entity_id, domain::ClaimEntityRole::Object}},
                        predicate, status, reasoning, recorded_by, evidence_links);
}

domain::Claim InvestigationService::get_claim(const domain::Id& id) const {
    return get_claim_internal(id);
}

std::vector<domain::Claim> InvestigationService::list_claims(const domain::Id& case_id) const {
    ensure_case(case_id);
    std::vector<domain::Claim> values;
    auto statement = database_.prepare(
        "SELECT c.id, c.case_id, c.statement, c.kind, c.predicate, c.status, c.reasoning, c.recorded_by, c.created_at, c.updated_at "
        "FROM claims c WHERE c.case_id = ? ORDER BY c.updated_at DESC, c.id;");
    statement.bind(1, case_id);
    while (statement.step()) values.push_back(read_claim(statement));
    return values;
}

std::vector<domain::ClaimEntityLink> InvestigationService::claim_entities(const domain::Id& claim_id) const {
    const auto claim = get_claim_internal(claim_id);
    (void)claim;
    std::vector<domain::ClaimEntityLink> values;
    auto statement = database_.prepare("SELECT claim_id, entity_id, role FROM claim_entities WHERE claim_id = ? ORDER BY role, entity_id;");
    statement.bind(1, claim_id);
    while (statement.step()) {
        values.push_back({statement.column_text(0), statement.column_text(1), domain::claim_entity_role_from_string(statement.column_text(2))});
    }
    return values;
}

std::vector<domain::ClaimEvidenceLink> InvestigationService::claim_evidence(const domain::Id& claim_id) const {
    const auto claim = get_claim_internal(claim_id);
    (void)claim;
    std::vector<domain::ClaimEvidenceLink> values;
    auto statement = database_.prepare("SELECT claim_id, evidence_id, role, note FROM claim_evidence WHERE claim_id = ? ORDER BY evidence_id;");
    statement.bind(1, claim_id);
    while (statement.step()) {
        values.push_back({statement.column_text(0), statement.column_text(1), domain::evidence_role_from_string(statement.column_text(2)), statement.column_text(3)});
    }
    return values;
}

void InvestigationService::link_evidence(const domain::Id& claim_id,
                                         const domain::Id& evidence_id,
                                         domain::EvidenceRole role,
                                         const std::string& note) {
    const auto claim = get_claim_internal(claim_id);
    const auto evidence = get_evidence_internal(evidence_id);
    if (claim.case_id != evidence.case_id) throw std::invalid_argument("Claim and evidence belong to different cases");
    database::Transaction transaction(database_);
    auto statement = database_.prepare("INSERT INTO claim_evidence(claim_id, evidence_id, role, note) VALUES(?, ?, ?, ?);");
    statement.bind(1, claim_id);
    statement.bind(2, evidence_id);
    statement.bind(3, domain::to_string(role));
    statement.bind(4, note);
    statement.step();
    logger_.record(claim.case_id, "linked", "claim_evidence", claim_id, "Evidence linked to claim");
    transaction.commit();
}

void InvestigationService::unlink_evidence(const domain::Id& claim_id, const domain::Id& evidence_id) {
    const auto claim = get_claim_internal(claim_id);
    get_evidence_internal(evidence_id);
    database::Transaction transaction(database_);
    auto statement = database_.prepare("DELETE FROM claim_evidence WHERE claim_id = ? AND evidence_id = ?;");
    statement.bind(1, claim_id);
    statement.bind(2, evidence_id);
    statement.step();
    logger_.record(claim.case_id, "unlinked", "claim_evidence", claim_id, "Evidence unlinked from claim");
    transaction.commit();
}

void InvestigationService::change_claim_status(const domain::Id& claim_id,
                                                domain::ClaimStatus status,
                                                const std::string& explanation,
                                                const std::optional<std::string>& new_reasoning) {
    auto claim = get_claim_internal(claim_id);
    if (claim.status == status) throw std::invalid_argument("Claim already has that status");
    if (domain::trim(explanation).empty()) throw std::invalid_argument("A status change requires an explanation");
    const auto reasoning = new_reasoning.has_value() ? *new_reasoning : claim.reasoning;
    if (status == domain::ClaimStatus::Confirmed) {
        auto supports = database_.prepare("SELECT count(*) FROM claim_evidence WHERE claim_id = ? AND role = 'supports';");
        supports.bind(1, claim_id);
        supports.step();
        if (supports.column_int64(0) == 0) throw std::invalid_argument("Attach at least one supporting item before marking this claim confirmed");
        if (domain::trim(reasoning).empty()) throw std::invalid_argument("Written reasoning is required before marking this claim confirmed");
    }
    database::Transaction transaction(database_);
    auto statement = database_.prepare("UPDATE claims SET status = ?, reasoning = ?, updated_at = ? WHERE id = ?;");
    statement.bind(1, domain::to_string(status));
    statement.bind(2, reasoning);
    statement.bind(3, domain::utc_now());
    statement.bind(4, claim_id);
    statement.step();
    logger_.record(claim.case_id, "status_changed", "claim", claim_id,
                   "Claim status changed from " + domain::to_string(claim.status) + " to " + domain::to_string(status),
                   "{\"previous\":" + json_escape(domain::to_string(claim.status)) +
                   ",\"new\":" + json_escape(domain::to_string(status)) +
                   ",\"explanation\":" + json_escape(explanation) + "}");
    transaction.commit();
}

void InvestigationService::change_claim_statement(const domain::Id& claim_id, const std::string& statement_text) {
    const auto claim = get_claim_internal(claim_id);
    if (domain::trim(statement_text).empty()) throw std::invalid_argument("Claim statement is required");
    database::Transaction transaction(database_);
    auto statement = database_.prepare("UPDATE claims SET statement = ?, updated_at = ? WHERE id = ?;");
    statement.bind(1, statement_text);
    statement.bind(2, domain::utc_now());
    statement.bind(3, claim_id);
    statement.step();
    logger_.record(claim.case_id, "statement_changed", "claim", claim_id, "Claim statement changed");
    transaction.commit();
}

void InvestigationService::delete_claim(const domain::Id& id, bool explicit_confirmation) {
    const auto claim = get_claim_internal(id);
    auto count_statement = database_.prepare("SELECT count(*) FROM claim_evidence WHERE claim_id = ?;");
    count_statement.bind(1, id);
    count_statement.step();
    const auto evidence_links = count_statement.column_int64(0);
    if (evidence_links > 0 && !explicit_confirmation) {
        throw std::invalid_argument("Claim has evidence links; explicit confirmation is required before deletion");
    }
    database::Transaction transaction(database_);
    logger_.record(claim.case_id, "deleted", "claim", id,
                   "Claim deleted after explicit confirmation",
                   "{\"evidence_links\":" + std::to_string(evidence_links) + "}");
    for (const auto& sql : {"DELETE FROM claim_evidence WHERE claim_id = ?;",
                            "DELETE FROM claim_entities WHERE claim_id = ?;",
                            "DELETE FROM claims WHERE id = ?;"}) {
        auto statement = database_.prepare(sql);
        statement.bind(1, id);
        statement.step();
    }
    transaction.commit();
}

domain::Note InvestigationService::add_note(const domain::Id& case_id,
                                            const std::string& body,
                                            const std::string& title,
                                            const std::vector<domain::Id>& entity_ids,
                                            const std::vector<domain::Id>& evidence_ids) {
    ensure_case(case_id);
    if (domain::trim(body).empty()) throw std::invalid_argument("Note body is required");
    for (const auto& id : entity_ids) if (get_entity_internal(id).case_id != case_id) throw std::invalid_argument("Note entity belongs to a different case");
    for (const auto& id : evidence_ids) if (get_evidence_internal(id).case_id != case_id) throw std::invalid_argument("Note evidence belongs to a different case");
    domain::Note value;
    value.id = domain::new_id();
    value.case_id = case_id;
    value.title = title;
    value.body = body;
    value.created_at = domain::utc_now();
    value.updated_at = value.created_at;
    database::Transaction transaction(database_);
    auto statement = database_.prepare("INSERT INTO notes(id, case_id, title, body, created_at, updated_at) VALUES(?, ?, ?, ?, ?, ?);");
    statement.bind(1, value.id);
    statement.bind(2, value.case_id);
    statement.bind(3, value.title);
    statement.bind(4, value.body);
    statement.bind(5, value.created_at);
    statement.bind(6, value.updated_at);
    statement.step();
    for (const auto& id : entity_ids) {
        auto link = database_.prepare("INSERT INTO note_entities(note_id, entity_id) VALUES(?, ?);");
        link.bind(1, value.id); link.bind(2, id); link.step();
    }
    for (const auto& id : evidence_ids) {
        auto link = database_.prepare("INSERT INTO note_evidence(note_id, evidence_id) VALUES(?, ?);");
        link.bind(1, value.id); link.bind(2, id); link.step();
    }
    logger_.record(case_id, "created", "note", value.id, "Investigation note added");
    transaction.commit();
    return value;
}

std::vector<domain::Note> InvestigationService::list_notes(const domain::Id& case_id) const {
    ensure_case(case_id);
    std::vector<domain::Note> values;
    auto statement = database_.prepare("SELECT id, case_id, title, body, created_at, updated_at FROM notes WHERE case_id = ? ORDER BY updated_at DESC, id;");
    statement.bind(1, case_id);
    while (statement.step()) values.push_back(read_note(statement));
    return values;
}

std::vector<domain::SearchResult> InvestigationService::search(const domain::Id& case_id,
                                                               const std::string& query) const {
    ensure_case(case_id);
    if (domain::trim(query).empty()) throw std::invalid_argument("Search text is required");
    const auto pattern = like_pattern(query);
    std::vector<domain::SearchResult> values;
    {
        auto statement = database_.prepare(
            "SELECT 'entity', id, label, type || ': ' || coalesce(original_value, '') FROM entities "
            "WHERE case_id = ? AND (lower(label) LIKE ? OR lower(original_value) LIKE ? OR lower(canonical_value) LIKE ? OR lower(details_json) LIKE ? OR lower(tags_json) LIKE ? OR lower(aliases_json) LIKE ?); ");
        statement.bind(1, case_id); for (int i = 2; i <= 7; ++i) statement.bind(i, pattern);
        while (statement.step()) values.push_back({statement.column_text(0), statement.column_text(1), statement.column_text(2), statement.column_text(3)});
    }
    {
        auto statement = database_.prepare(
            "SELECT 'source', id, title, locator FROM sources WHERE case_id = ? AND (lower(locator) LIKE ? OR lower(title) LIKE ? OR lower(author) LIKE ? OR lower(reliability_note) LIKE ?); ");
        statement.bind(1, case_id); for (int i = 2; i <= 5; ++i) statement.bind(i, pattern);
        while (statement.step()) values.push_back({statement.column_text(0), statement.column_text(1), statement.column_text(2), statement.column_text(3)});
    }
    {
        auto statement = database_.prepare(
            "SELECT 'evidence', id, coalesce(original_filename, source_url, kind), coalesce(note, '') FROM evidence "
            "WHERE case_id = ? AND (lower(coalesce(original_filename, '')) LIKE ? OR lower(coalesce(source_url, '')) LIKE ? OR lower(text_content) LIKE ? OR lower(note) LIKE ?); ");
        statement.bind(1, case_id); for (int i = 2; i <= 5; ++i) statement.bind(i, pattern);
        while (statement.step()) values.push_back({statement.column_text(0), statement.column_text(1), statement.column_text(2), statement.column_text(3)});
    }
    {
        auto statement = database_.prepare(
            "SELECT 'claim', id, statement, coalesce(reasoning, '') FROM claims WHERE case_id = ? AND (lower(statement) LIKE ? OR lower(reasoning) LIKE ? OR lower(coalesce(predicate, '')) LIKE ?); ");
        statement.bind(1, case_id); for (int i = 2; i <= 4; ++i) statement.bind(i, pattern);
        while (statement.step()) values.push_back({statement.column_text(0), statement.column_text(1), statement.column_text(2), statement.column_text(3)});
    }
    {
        auto statement = database_.prepare(
            "SELECT 'note', id, title, body FROM notes WHERE case_id = ? AND (lower(title) LIKE ? OR lower(body) LIKE ?); ");
        statement.bind(1, case_id); statement.bind(2, pattern); statement.bind(3, pattern);
        while (statement.step()) values.push_back({statement.column_text(0), statement.column_text(1), statement.column_text(2), statement.column_text(3)});
    }
    {
        auto statement = database_.prepare(
            "SELECT 'run_step', rs.id, rs.name_snapshot, rs.instructions_snapshot FROM run_steps rs "
            "JOIN playbook_runs r ON r.id = rs.run_id WHERE r.case_id = ? AND (lower(rs.name_snapshot) LIKE ? OR lower(rs.instructions_snapshot) LIKE ? OR lower(rs.note) LIKE ?); ");
        statement.bind(1, case_id); for (int i = 2; i <= 4; ++i) statement.bind(i, pattern);
        while (statement.step()) values.push_back({statement.column_text(0), statement.column_text(1), statement.column_text(2), statement.column_text(3)});
    }
    return values;
}

std::vector<domain::LogEvent> InvestigationService::activity(const domain::Id& case_id) const {
    ensure_case(case_id);
    return logger_.list(case_id);
}

} // namespace evidence_trace::services
