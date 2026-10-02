#pragma once

#include "database/sqlite_database.hpp"
#include "domain/types.hpp"
#include "services/event_logger.hpp"

#include <optional>
#include <string>
#include <vector>

namespace evidence_trace::storage {
class AttachmentStore;
}

namespace evidence_trace::services {

class CaseService {
public:
    CaseService(database::Database& database, EventLogger& logger)
        : database_(database), logger_(logger) {}

    domain::Case create(const std::string& title,
                        const std::string& purpose = {},
                        const std::string& description = {},
                        const std::optional<std::string>& primary_target_type = std::nullopt,
                        const std::string& scope = {},
                        const std::vector<std::string>& tags = {});
    domain::Case get(const domain::Id& id) const;
    std::vector<domain::CaseSummary> list(const std::optional<std::string>& search = std::nullopt) const;
    void update(const domain::Id& id,
                const std::string& title,
                const std::string& purpose,
                const std::string& description,
                const std::optional<std::string>& primary_target_type,
                const std::string& scope,
                const std::optional<std::vector<std::string>>& tags);
    void archive(const domain::Id& id);
    void restore(const domain::Id& id);
    void delete_case(const domain::Id& id, storage::AttachmentStore& attachment_store);

    static std::string tags_to_json(const std::vector<std::string>& tags);

private:
    domain::Case get_internal(const domain::Id& id) const;
    database::Database& database_;
    EventLogger& logger_;
};

} // namespace evidence_trace::services
