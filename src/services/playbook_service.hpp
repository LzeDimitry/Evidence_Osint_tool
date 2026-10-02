#pragma once

#include "database/sqlite_database.hpp"
#include "domain/types.hpp"
#include "services/event_logger.hpp"

#include <optional>
#include <string>
#include <vector>

namespace evidence_trace::services {

class PlaybookService {
public:
    PlaybookService(database::Database& database, EventLogger& logger)
        : database_(database), logger_(logger) {}

    domain::Technique create_technique(const std::string& name,
                                       const std::string& objective = {},
                                       const std::string& steps = {},
                                       const std::string& example_queries_json = "[]",
                                       const std::string& tools_links_json = "[]",
                                       const std::string& limitations = {},
                                       const std::string& tags_json = "[]");
    domain::Technique get_technique(const domain::Id& id) const;
    std::vector<domain::Technique> list_techniques() const;
    void update_technique(const domain::Id& id,
                          const std::string& name,
                          const std::string& objective,
                          const std::string& steps,
                          const std::string& example_queries_json,
                          const std::string& tools_links_json,
                          const std::string& limitations,
                          const std::string& tags_json);
    void delete_technique(const domain::Id& id, bool explicit_confirmation);

    domain::Playbook create_playbook(const std::string& name,
                                     const std::string& scope = {},
                                     const std::string& description = {});
    domain::Playbook get_playbook(const domain::Id& id) const;
    std::vector<domain::Playbook> list_playbooks() const;
    void update_playbook(const domain::Id& id,
                         const std::string& name,
                         const std::string& scope,
                         const std::string& description);
    void delete_playbook(const domain::Id& id, bool explicit_confirmation);
    domain::PlaybookStep add_playbook_step(const domain::Id& playbook_id,
                                           const std::optional<domain::Id>& technique_id,
                                           const std::string& name = {},
                                           const std::string& instructions = {});
    std::vector<domain::PlaybookStep> list_playbook_steps(const domain::Id& playbook_id) const;
    void move_playbook_step(const domain::Id& step_id, std::int64_t new_position);

    domain::PlaybookRun start_run(const domain::Id& case_id,
                                  const domain::Id& playbook_id,
                                  const std::optional<domain::Id>& entity_id = std::nullopt);
    std::vector<domain::PlaybookRun> list_runs(const domain::Id& case_id) const;
    std::vector<domain::RunStep> list_run_steps(const domain::Id& run_id) const;
    domain::RunStep add_run_step(const domain::Id& run_id,
                                 const std::string& name,
                                 const std::string& instructions = {});
    void update_run_step(const domain::Id& run_step_id,
                         domain::RunStepState state,
                         const std::string& note = {});
    void link_step_evidence(const domain::Id& run_step_id, const domain::Id& evidence_id);
    std::vector<domain::Id> evidence_for_step(const domain::Id& run_step_id) const;

private:
    void ensure_case(const domain::Id& case_id) const;
    domain::Technique get_technique_internal(const domain::Id& id) const;
    domain::Playbook get_playbook_internal(const domain::Id& id) const;
    domain::PlaybookRun get_run_internal(const domain::Id& id) const;
    static domain::Technique read_technique(database::Statement& statement);
    static domain::Playbook read_playbook(database::Statement& statement);
    static domain::PlaybookStep read_playbook_step(database::Statement& statement);
    static domain::PlaybookRun read_run(database::Statement& statement);
    static domain::RunStep read_run_step(database::Statement& statement);

    database::Database& database_;
    EventLogger& logger_;
};

} // namespace evidence_trace::services
