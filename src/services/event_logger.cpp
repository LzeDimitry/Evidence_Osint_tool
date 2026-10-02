#include "services/event_logger.hpp"

#include "domain/types.hpp"

namespace evidence_trace::services {

domain::Id EventLogger::record(const domain::Id& case_id,
                               const std::string& action,
                               const std::string& object_type,
                               const std::optional<domain::Id>& object_id,
                               const std::string& description,
                               const std::string& payload_json) {
    const auto id = domain::new_id();
    auto statement = database_.prepare(
        "INSERT INTO log_events(id, case_id, occurred_at, action, object_type, object_id, description, payload_json) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?);");
    statement.bind(1, id);
    statement.bind(2, case_id);
    statement.bind(3, domain::utc_now());
    statement.bind(4, action);
    statement.bind(5, object_type);
    statement.bind(6, object_id);
    statement.bind(7, description);
    statement.bind(8, payload_json);
    statement.step();
    return id;
}

std::vector<domain::LogEvent> EventLogger::list(const domain::Id& case_id) const {
    std::vector<domain::LogEvent> events;
    auto statement = database_.prepare(
        "SELECT id, case_id, occurred_at, action, object_type, object_id, description, payload_json "
        "FROM log_events WHERE case_id = ? ORDER BY occurred_at, id;");
    statement.bind(1, case_id);
    while (statement.step()) {
        domain::LogEvent event;
        event.id = statement.column_text(0);
        event.case_id = statement.column_text(1);
        event.occurred_at = statement.column_text(2);
        event.action = statement.column_text(3);
        event.object_type = statement.column_text(4);
        event.object_id = statement.column_optional_text(5);
        event.description = statement.column_text(6);
        event.payload_json = statement.column_text(7);
        events.push_back(std::move(event));
    }
    return events;
}

} // namespace evidence_trace::services
