#pragma once

#include "database/sqlite_database.hpp"
#include "domain/types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace evidence_trace::services {

class EventLogger {
public:
    explicit EventLogger(database::Database& database) : database_(database) {}

    domain::Id record(const domain::Id& case_id,
                      const std::string& action,
                      const std::string& object_type,
                      const std::optional<domain::Id>& object_id,
                      const std::string& description,
                      const std::string& payload_json = "{}");
    std::vector<domain::LogEvent> list(const domain::Id& case_id) const;

private:
    database::Database& database_;
};

} // namespace evidence_trace::services
