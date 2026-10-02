#include "services/case_service.hpp"

#include "domain/types.hpp"
#include "storage/attachment_store.hpp"

#include <sstream>
#include <stdexcept>
#include <array>
#include <algorithm>

namespace evidence_trace::services {

namespace {

std::string json_escape(const std::string& value) {
    std::ostringstream output;
    output << '"';
    for (const unsigned char character : value) {
        switch (character) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (character < 0x20) {
                output << "\\u" << std::hex << static_cast<int>(character);
            } else {
                output << static_cast<char>(character);
            }
        }
    }
    output << '"';
    return output.str();
}

template <typename T>
void bind_case(T& statement, const domain::Case& value) {
    statement.bind(1, value.id);
    statement.bind(2, value.title);
    statement.bind(3, value.purpose);
    statement.bind(4, value.description);
    statement.bind(5, value.primary_target_type);
    statement.bind(6, value.scope);
    statement.bind(7, value.tags_json);
    statement.bind(8, domain::to_string(value.status));
    statement.bind(9, value.created_at);
    statement.bind(10, value.updated_at);
}

domain::Case read_case(database::Statement& statement, int offset = 0) {
    domain::Case value;
    value.id = statement.column_text(offset + 0);
    value.title = statement.column_text(offset + 1);
    value.purpose = statement.column_text(offset + 2);
    value.description = statement.column_text(offset + 3);
    value.primary_target_type = statement.column_optional_text(offset + 4);
    value.scope = statement.column_text(offset + 5);
    value.tags_json = statement.column_text(offset + 6);
    value.status = domain::case_status_from_string(statement.column_text(offset + 7));
    value.created_at = statement.column_text(offset + 8);
    value.updated_at = statement.column_text(offset + 9);
    return value;
}

} // namespace

std::string CaseService::tags_to_json(const std::vector<std::string>& tags) {
    std::ostringstream output;
    output << '[';
    for (std::size_t i = 0; i < tags.size(); ++i) {
        if (i != 0) output << ',';
        output << json_escape(domain::trim(tags[i]));
    }
    output << ']';
    return output.str();
}

domain::Case CaseService::create(const std::string& title,
                                 const std::string& purpose,
                                 const std::string& description,
                                 const std::optional<std::string>& primary_target_type,
                                 const std::string& scope,
                                 const std::vector<std::string>& tags) {
    if (domain::trim(title).empty()) throw std::invalid_argument("Case title is required");
    if (primary_target_type.has_value()) {
        static constexpr std::array<const char*, 7> allowed{"person", "company", "place", "domain", "username", "event", "other"};
        if (std::find(allowed.begin(), allowed.end(), *primary_target_type) == allowed.end()) throw std::invalid_argument("Unsupported primary target type");
    }
    const auto now = domain::utc_now();
    domain::Case value;
    value.id = domain::new_id();
    value.title = title;
    value.purpose = purpose;
    value.description = description;
    value.primary_target_type = primary_target_type;
    value.scope = scope;
    value.tags_json = tags_to_json(tags);
    value.status = domain::CaseStatus::Active;
    value.created_at = now;
    value.updated_at = now;

    database::Transaction transaction(database_);
    auto statement = database_.prepare(
        "INSERT INTO cases(id, title, purpose, description, primary_target_type, scope, tags_json, status, created_at, updated_at) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");
    bind_case(statement, value);
    statement.step();
    logger_.record(value.id, "created", "case", value.id, "Case created");
    transaction.commit();
    return value;
}

domain::Case CaseService::get_internal(const domain::Id& id) const {
    auto statement = database_.prepare(
        "SELECT id, title, purpose, description, primary_target_type, scope, tags_json, status, created_at, updated_at "
        "FROM cases WHERE id = ?;");
    statement.bind(1, id);
    if (!statement.step()) throw std::runtime_error("Case not found: " + id);
    return read_case(statement);
}

domain::Case CaseService::get(const domain::Id& id) const {
    return get_internal(id);
}

std::vector<domain::CaseSummary> CaseService::list(const std::optional<std::string>& search) const {
    std::vector<domain::CaseSummary> cases;
    const bool filtered = search.has_value() && !search->empty();
    const std::string sql =
        "SELECT c.id, c.title, c.purpose, c.description, c.primary_target_type, c.scope, c.tags_json, c.status, c.created_at, c.updated_at, "
        "(SELECT count(*) FROM entities e WHERE e.case_id = c.id), "
        "(SELECT count(*) FROM claims cl WHERE cl.case_id = c.id) "
        "FROM cases c "
        "WHERE (? = 0 OR lower(c.title) LIKE '%' || lower(?) || '%' OR lower(c.tags_json) LIKE '%' || lower(?) || '%') "
        "ORDER BY c.updated_at DESC, c.id;";
    auto statement = database_.prepare(sql);
    statement.bind(1, static_cast<std::int64_t>(filtered ? 1 : 0));
    statement.bind(2, filtered ? *search : "");
    statement.bind(3, filtered ? *search : "");
    while (statement.step()) {
        auto value = read_case(statement);
        domain::CaseSummary summary;
        static_cast<domain::Case&>(summary) = value;
        summary.entity_count = statement.column_int64(10);
        summary.claim_count = statement.column_int64(11);
        cases.push_back(std::move(summary));
    }
    return cases;
}

void CaseService::update(const domain::Id& id,
                         const std::string& title,
                         const std::string& purpose,
                         const std::string& description,
                         const std::optional<std::string>& primary_target_type,
                         const std::string& scope,
                         const std::optional<std::vector<std::string>>& tags) {
    if (domain::trim(title).empty()) throw std::invalid_argument("Case title is required");
    if (primary_target_type.has_value()) {
        static constexpr std::array<const char*, 7> allowed{"person", "company", "place", "domain", "username", "event", "other"};
        if (std::find(allowed.begin(), allowed.end(), *primary_target_type) == allowed.end()) throw std::invalid_argument("Unsupported primary target type");
    }
    const auto current = get_internal(id);
    auto statement = database_.prepare(
        "UPDATE cases SET title = ?, purpose = ?, description = ?, primary_target_type = ?, scope = ?, tags_json = ?, updated_at = ? "
        "WHERE id = ?;");
    statement.bind(1, title);
    statement.bind(2, purpose);
    statement.bind(3, description);
    statement.bind(4, primary_target_type);
    statement.bind(5, scope);
    statement.bind(6, tags.has_value() ? tags_to_json(*tags) : current.tags_json);
    statement.bind(7, domain::utc_now());
    statement.bind(8, id);
    statement.step();
    logger_.record(id, "updated", "case", id, "Case metadata updated");
}

void CaseService::archive(const domain::Id& id) {
    get_internal(id);
    auto statement = database_.prepare("UPDATE cases SET status = 'archived', updated_at = ? WHERE id = ?;");
    statement.bind(1, domain::utc_now());
    statement.bind(2, id);
    statement.step();
    logger_.record(id, "archived", "case", id, "Case archived");
}

void CaseService::restore(const domain::Id& id) {
    get_internal(id);
    auto statement = database_.prepare("UPDATE cases SET status = 'active', updated_at = ? WHERE id = ?;");
    statement.bind(1, domain::utc_now());
    statement.bind(2, id);
    statement.step();
    logger_.record(id, "restored", "case", id, "Case restored to active status");
}

void CaseService::delete_case(const domain::Id& id, storage::AttachmentStore& attachment_store) {
    get_internal(id);

    struct PendingAttachment {
        std::filesystem::path relative_path;
        storage::StagedRemoval staged;
    };
    std::vector<PendingAttachment> attachments;
    auto evidence = database_.prepare("SELECT id, relative_path FROM evidence WHERE case_id = ? AND relative_path IS NOT NULL;");
    evidence.bind(1, id);
    try {
        while (evidence.step()) {
            const auto evidence_id = evidence.column_text(0);
            const auto relative_path = std::filesystem::path(evidence.column_text(1));
            const auto expected_path = std::filesystem::path("attachments") / (evidence_id + ".bin");
            if (relative_path != expected_path) {
                throw std::invalid_argument("Refusing to delete case: evidence attachment path is not owned by its evidence record");
            }
            attachments.push_back({relative_path, attachment_store.stage_removal(relative_path)});
        }
    } catch (...) {
        std::string restore_error;
        for (auto iterator = attachments.rbegin(); iterator != attachments.rend(); ++iterator) {
            try { attachment_store.restore_removal(iterator->staged); }
            catch (const std::exception& error) { restore_error += std::string("; ") + error.what(); }
        }
        if (!restore_error.empty()) throw std::runtime_error("Case deletion was not started; attachment restore failed" + restore_error);
        throw;
    }

    const auto restore_attachments = [&] {
        std::string restore_error;
        for (auto iterator = attachments.rbegin(); iterator != attachments.rend(); ++iterator) {
            try { attachment_store.restore_removal(iterator->staged); }
            catch (const std::exception& error) { restore_error += std::string("; ") + error.what(); }
        }
        if (!restore_error.empty()) throw std::runtime_error("Case deletion rolled back, but attachment restore failed" + restore_error);
    };

    try {
        database::Transaction transaction(database_);
        const auto delete_for_case = [&](const std::string& sql) {
            auto statement = database_.prepare(sql);
            statement.bind(1, id);
            statement.step();
        };

        delete_for_case("DELETE FROM run_step_evidence WHERE run_step_id IN (SELECT rs.id FROM run_steps rs JOIN playbook_runs pr ON pr.id = rs.run_id WHERE pr.case_id = ?);");
        delete_for_case("DELETE FROM run_steps WHERE run_id IN (SELECT id FROM playbook_runs WHERE case_id = ?);");
        delete_for_case("DELETE FROM playbook_runs WHERE case_id = ?;");
        delete_for_case("DELETE FROM note_entities WHERE note_id IN (SELECT id FROM notes WHERE case_id = ?);");
        delete_for_case("DELETE FROM note_evidence WHERE note_id IN (SELECT id FROM notes WHERE case_id = ?);");
        delete_for_case("DELETE FROM notes WHERE case_id = ?;");
        delete_for_case("DELETE FROM claim_entities WHERE claim_id IN (SELECT id FROM claims WHERE case_id = ?);");
        delete_for_case("DELETE FROM claim_evidence WHERE claim_id IN (SELECT id FROM claims WHERE case_id = ?);");
        delete_for_case("DELETE FROM claims WHERE case_id = ?;");
        delete_for_case("DELETE FROM evidence WHERE case_id = ?;");
        delete_for_case("DELETE FROM sources WHERE case_id = ?;");
        delete_for_case("DELETE FROM entities WHERE case_id = ?;");
        delete_for_case("DELETE FROM log_events WHERE case_id = ?;");
        delete_for_case("DELETE FROM cases WHERE id = ?;");
        transaction.commit();
    } catch (...) {
        try { restore_attachments(); }
        catch (const std::exception& error) { throw std::runtime_error(std::string("Case deletion failed and recovery failed: ") + error.what()); }
        throw;
    }

    try {
        for (auto& attachment : attachments) attachment_store.discard_removal(attachment.staged);
    } catch (const std::exception& error) {
        throw std::runtime_error(std::string("Case data was deleted, but preserved attachment cleanup failed; quarantined files were retained for recovery: ") + error.what());
    }
}

} // namespace evidence_trace::services
