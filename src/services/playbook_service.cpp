#include "services/playbook_service.hpp"

#include "domain/types.hpp"

#include <algorithm>
#include <stdexcept>

namespace evidence_trace::services {

namespace {

domain::Technique read_technique_row(database::Statement& statement) {
    domain::Technique value;
    value.id = statement.column_text(0);
    value.name = statement.column_text(1);
    value.objective = statement.column_text(2);
    value.steps = statement.column_text(3);
    value.example_queries_json = statement.column_text(4);
    value.tools_links_json = statement.column_text(5);
    value.limitations = statement.column_text(6);
    value.tags_json = statement.column_text(7);
    value.created_at = statement.column_text(8);
    value.updated_at = statement.column_text(9);
    return value;
}

} // namespace

void PlaybookService::ensure_case(const domain::Id& case_id) const {
    auto statement = database_.prepare("SELECT 1 FROM cases WHERE id = ?;");
    statement.bind(1, case_id);
    if (!statement.step()) throw std::runtime_error("Case not found: " + case_id);
}

domain::Technique PlaybookService::read_technique(database::Statement& statement) {
    return read_technique_row(statement);
}

domain::Playbook PlaybookService::read_playbook(database::Statement& statement) {
    domain::Playbook value;
    value.id = statement.column_text(0);
    value.name = statement.column_text(1);
    value.scope = statement.column_text(2);
    value.description = statement.column_text(3);
    value.created_at = statement.column_text(4);
    value.updated_at = statement.column_text(5);
    return value;
}

domain::PlaybookStep PlaybookService::read_playbook_step(database::Statement& statement) {
    domain::PlaybookStep value;
    value.id = statement.column_text(0);
    value.playbook_id = statement.column_text(1);
    value.technique_id = statement.column_optional_text(2);
    value.name = statement.column_text(3);
    value.instructions = statement.column_text(4);
    value.position = statement.column_int64(5);
    return value;
}

domain::PlaybookRun PlaybookService::read_run(database::Statement& statement) {
    domain::PlaybookRun value;
    value.id = statement.column_text(0);
    value.case_id = statement.column_text(1);
    value.playbook_id = statement.column_optional_text(2);
    value.entity_id = statement.column_optional_text(3);
    value.name_snapshot = statement.column_text(4);
    value.started_at = statement.column_text(5);
    value.updated_at = statement.column_text(6);
    return value;
}

domain::RunStep PlaybookService::read_run_step(database::Statement& statement) {
    domain::RunStep value;
    value.id = statement.column_text(0);
    value.run_id = statement.column_text(1);
    value.source_step_id = statement.column_optional_text(2);
    value.position = statement.column_int64(3);
    value.name_snapshot = statement.column_text(4);
    value.instructions_snapshot = statement.column_text(5);
    value.state = domain::run_step_state_from_string(statement.column_text(6));
    value.completed_at = statement.column_optional_text(7);
    value.note = statement.column_text(8);
    return value;
}

domain::Technique PlaybookService::create_technique(const std::string& name,
                                                    const std::string& objective,
                                                    const std::string& steps,
                                                    const std::string& example_queries_json,
                                                    const std::string& tools_links_json,
                                                    const std::string& limitations,
                                                    const std::string& tags_json) {
    if (domain::trim(name).empty()) throw std::invalid_argument("Technique name is required");
    domain::Technique value;
    value.id = domain::new_id();
    value.name = name;
    value.objective = objective;
    value.steps = steps;
    value.example_queries_json = example_queries_json;
    value.tools_links_json = tools_links_json;
    value.limitations = limitations;
    value.tags_json = tags_json;
    value.created_at = domain::utc_now();
    value.updated_at = value.created_at;
    auto statement = database_.prepare(
        "INSERT INTO techniques(id, name, objective, steps, example_queries_json, tools_links_json, limitations, tags_json, created_at, updated_at) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");
    statement.bind(1, value.id); statement.bind(2, value.name); statement.bind(3, value.objective); statement.bind(4, value.steps);
    statement.bind(5, value.example_queries_json); statement.bind(6, value.tools_links_json); statement.bind(7, value.limitations);
    statement.bind(8, value.tags_json); statement.bind(9, value.created_at); statement.bind(10, value.updated_at); statement.step();
    return value;
}

domain::Technique PlaybookService::get_technique_internal(const domain::Id& id) const {
    auto statement = database_.prepare(
        "SELECT id, name, objective, steps, example_queries_json, tools_links_json, limitations, tags_json, created_at, updated_at "
        "FROM techniques WHERE id = ?;");
    statement.bind(1, id);
    if (!statement.step()) throw std::runtime_error("Technique not found: " + id);
    return read_technique(statement);
}

domain::Technique PlaybookService::get_technique(const domain::Id& id) const { return get_technique_internal(id); }

std::vector<domain::Technique> PlaybookService::list_techniques() const {
    std::vector<domain::Technique> values;
    auto statement = database_.prepare(
        "SELECT id, name, objective, steps, example_queries_json, tools_links_json, limitations, tags_json, created_at, updated_at "
        "FROM techniques ORDER BY name COLLATE NOCASE, id;");
    while (statement.step()) values.push_back(read_technique(statement));
    return values;
}

void PlaybookService::update_technique(const domain::Id& id,
                                       const std::string& name,
                                       const std::string& objective,
                                       const std::string& steps,
                                       const std::string& example_queries_json,
                                       const std::string& tools_links_json,
                                       const std::string& limitations,
                                       const std::string& tags_json) {
    get_technique_internal(id);
    if (domain::trim(name).empty()) throw std::invalid_argument("Technique name is required");
    auto statement = database_.prepare(
        "UPDATE techniques SET name = ?, objective = ?, steps = ?, example_queries_json = ?, tools_links_json = ?, limitations = ?, tags_json = ?, updated_at = ? WHERE id = ?;");
    statement.bind(1, name); statement.bind(2, objective); statement.bind(3, steps); statement.bind(4, example_queries_json);
    statement.bind(5, tools_links_json); statement.bind(6, limitations); statement.bind(7, tags_json); statement.bind(8, domain::utc_now()); statement.bind(9, id);
    statement.step();
}

void PlaybookService::delete_technique(const domain::Id& id, bool explicit_confirmation) {
    get_technique_internal(id);
    if (!explicit_confirmation) throw std::invalid_argument("Technique deletion requires explicit confirmation");
    database::Transaction transaction(database_);
    auto statement = database_.prepare("DELETE FROM techniques WHERE id = ?;");
    statement.bind(1, id);
    statement.step();
    transaction.commit();
}

domain::Playbook PlaybookService::create_playbook(const std::string& name,
                                                  const std::string& scope,
                                                  const std::string& description) {
    if (domain::trim(name).empty()) throw std::invalid_argument("Playbook name is required");
    domain::Playbook value;
    value.id = domain::new_id(); value.name = name; value.scope = scope; value.description = description;
    value.created_at = domain::utc_now(); value.updated_at = value.created_at;
    auto statement = database_.prepare("INSERT INTO playbooks(id, name, scope, description, created_at, updated_at) VALUES(?, ?, ?, ?, ?, ?);");
    statement.bind(1, value.id); statement.bind(2, value.name); statement.bind(3, value.scope); statement.bind(4, value.description);
    statement.bind(5, value.created_at); statement.bind(6, value.updated_at); statement.step();
    return value;
}

domain::Playbook PlaybookService::get_playbook_internal(const domain::Id& id) const {
    auto statement = database_.prepare("SELECT id, name, scope, description, created_at, updated_at FROM playbooks WHERE id = ?;");
    statement.bind(1, id);
    if (!statement.step()) throw std::runtime_error("Playbook not found: " + id);
    return read_playbook(statement);
}

domain::Playbook PlaybookService::get_playbook(const domain::Id& id) const { return get_playbook_internal(id); }

std::vector<domain::Playbook> PlaybookService::list_playbooks() const {
    std::vector<domain::Playbook> values;
    auto statement = database_.prepare("SELECT id, name, scope, description, created_at, updated_at FROM playbooks ORDER BY name COLLATE NOCASE, id;");
    while (statement.step()) values.push_back(read_playbook(statement));
    return values;
}

void PlaybookService::update_playbook(const domain::Id& id,
                                      const std::string& name,
                                      const std::string& scope,
                                      const std::string& description) {
    get_playbook_internal(id);
    if (domain::trim(name).empty()) throw std::invalid_argument("Playbook name is required");
    auto statement = database_.prepare("UPDATE playbooks SET name = ?, scope = ?, description = ?, updated_at = ? WHERE id = ?;");
    statement.bind(1, name);
    statement.bind(2, scope);
    statement.bind(3, description);
    statement.bind(4, domain::utc_now());
    statement.bind(5, id);
    statement.step();
}

void PlaybookService::delete_playbook(const domain::Id& id, bool explicit_confirmation) {
    get_playbook_internal(id);
    if (!explicit_confirmation) throw std::invalid_argument("Playbook deletion requires explicit confirmation");
    database::Transaction transaction(database_);
    auto delete_steps = database_.prepare("DELETE FROM playbook_steps WHERE playbook_id = ?;");
    delete_steps.bind(1, id);
    delete_steps.step();
    auto statement = database_.prepare("DELETE FROM playbooks WHERE id = ?;");
    statement.bind(1, id);
    statement.step();
    transaction.commit();
}

domain::PlaybookStep PlaybookService::add_playbook_step(const domain::Id& playbook_id,
                                                        const std::optional<domain::Id>& technique_id,
                                                        const std::string& name,
                                                        const std::string& instructions) {
    const auto playbook = get_playbook_internal(playbook_id);
    std::string step_name = name;
    std::string step_instructions = instructions;
    if (technique_id.has_value()) {
        const auto technique = get_technique_internal(*technique_id);
        if (step_name.empty()) step_name = technique.name;
        if (step_instructions.empty()) step_instructions = technique.steps;
    }
    if (domain::trim(step_name).empty()) throw std::invalid_argument("Playbook step name is required");
    auto position_statement = database_.prepare("SELECT coalesce(max(position), -1) + 1 FROM playbook_steps WHERE playbook_id = ?;");
    position_statement.bind(1, playbook_id); position_statement.step();
    const auto position = position_statement.column_int64(0);
    domain::PlaybookStep value;
    value.id = domain::new_id(); value.playbook_id = playbook.id; value.technique_id = technique_id;
    value.name = step_name; value.instructions = step_instructions; value.position = position;
    auto statement = database_.prepare("INSERT INTO playbook_steps(id, playbook_id, technique_id, name, instructions, position) VALUES(?, ?, ?, ?, ?, ?);");
    statement.bind(1, value.id); statement.bind(2, value.playbook_id); statement.bind(3, value.technique_id); statement.bind(4, value.name);
    statement.bind(5, value.instructions); statement.bind(6, value.position); statement.step();
    return value;
}

std::vector<domain::PlaybookStep> PlaybookService::list_playbook_steps(const domain::Id& playbook_id) const {
    get_playbook_internal(playbook_id);
    std::vector<domain::PlaybookStep> values;
    auto statement = database_.prepare("SELECT id, playbook_id, technique_id, name, instructions, position FROM playbook_steps WHERE playbook_id = ? ORDER BY position, id;");
    statement.bind(1, playbook_id);
    while (statement.step()) values.push_back(read_playbook_step(statement));
    return values;
}

void PlaybookService::move_playbook_step(const domain::Id& step_id, std::int64_t new_position) {
    auto lookup = database_.prepare("SELECT playbook_id FROM playbook_steps WHERE id = ?;");
    lookup.bind(1, step_id);
    if (!lookup.step()) throw std::runtime_error("Playbook step not found: " + step_id);
    const auto playbook_id = lookup.column_text(0);
    auto steps = list_playbook_steps(playbook_id);
    if (new_position < 0 || new_position >= static_cast<std::int64_t>(steps.size())) throw std::invalid_argument("New playbook step position is outside the current list");
    const auto current = std::find_if(steps.begin(), steps.end(), [&](const domain::PlaybookStep& step) { return step.id == step_id; });
    if (current == steps.end()) throw std::runtime_error("Playbook step not found: " + step_id);
    const auto moved = *current;
    steps.erase(current);
    steps.insert(steps.begin() + new_position, moved);
    database::Transaction transaction(database_);
    auto make_room = database_.prepare("UPDATE playbook_steps SET position = position + 1000000 WHERE playbook_id = ?;");
    make_room.bind(1, playbook_id); make_room.step();
    for (std::size_t position = 0; position < steps.size(); ++position) {
        auto update = database_.prepare("UPDATE playbook_steps SET position = ? WHERE id = ?;");
        update.bind(1, static_cast<std::int64_t>(position)); update.bind(2, steps[position].id); update.step();
    }
    transaction.commit();
}

domain::PlaybookRun PlaybookService::start_run(const domain::Id& case_id,
                                               const domain::Id& playbook_id,
                                               const std::optional<domain::Id>& entity_id) {
    ensure_case(case_id);
    const auto playbook = get_playbook_internal(playbook_id);
    if (entity_id.has_value()) {
        auto statement = database_.prepare("SELECT case_id FROM entities WHERE id = ?;");
        statement.bind(1, *entity_id);
        if (!statement.step()) throw std::runtime_error("Entity not found: " + *entity_id);
        if (statement.column_text(0) != case_id) throw std::invalid_argument("Run entity belongs to a different case");
    }
    domain::PlaybookRun run;
    run.id = domain::new_id(); run.case_id = case_id; run.playbook_id = playbook_id; run.entity_id = entity_id;
    run.name_snapshot = playbook.name; run.started_at = domain::utc_now(); run.updated_at = run.started_at;
    database::Transaction transaction(database_);
    auto insert_run = database_.prepare("INSERT INTO playbook_runs(id, case_id, playbook_id, entity_id, name_snapshot, started_at, updated_at) VALUES(?, ?, ?, ?, ?, ?, ?);");
    insert_run.bind(1, run.id); insert_run.bind(2, run.case_id); insert_run.bind(3, run.playbook_id); insert_run.bind(4, run.entity_id);
    insert_run.bind(5, run.name_snapshot); insert_run.bind(6, run.started_at); insert_run.bind(7, run.updated_at); insert_run.step();
    auto steps = database_.prepare("SELECT id, name, instructions, position FROM playbook_steps WHERE playbook_id = ? ORDER BY position, id;");
    steps.bind(1, playbook_id);
    while (steps.step()) {
        auto insert_step = database_.prepare("INSERT INTO run_steps(id, run_id, source_step_id, position, name_snapshot, instructions_snapshot, state, completed_at, note) VALUES(?, ?, ?, ?, ?, ?, 'todo', NULL, '');");
        insert_step.bind(1, domain::new_id()); insert_step.bind(2, run.id); insert_step.bind(3, steps.column_text(0)); insert_step.bind(4, steps.column_int64(3));
        insert_step.bind(5, steps.column_text(1)); insert_step.bind(6, steps.column_text(2)); insert_step.step();
    }
    logger_.record(case_id, "started", "playbook_run", run.id, "Playbook run started");
    transaction.commit();
    return run;
}

domain::PlaybookRun PlaybookService::get_run_internal(const domain::Id& id) const {
    auto statement = database_.prepare("SELECT id, case_id, playbook_id, entity_id, name_snapshot, started_at, updated_at FROM playbook_runs WHERE id = ?;");
    statement.bind(1, id);
    if (!statement.step()) throw std::runtime_error("Playbook run not found: " + id);
    return read_run(statement);
}

std::vector<domain::PlaybookRun> PlaybookService::list_runs(const domain::Id& case_id) const {
    ensure_case(case_id);
    std::vector<domain::PlaybookRun> values;
    auto statement = database_.prepare("SELECT id, case_id, playbook_id, entity_id, name_snapshot, started_at, updated_at FROM playbook_runs WHERE case_id = ? ORDER BY started_at DESC, id;");
    statement.bind(1, case_id);
    while (statement.step()) values.push_back(read_run(statement));
    return values;
}

std::vector<domain::RunStep> PlaybookService::list_run_steps(const domain::Id& run_id) const {
    get_run_internal(run_id);
    std::vector<domain::RunStep> values;
    auto statement = database_.prepare("SELECT id, run_id, source_step_id, position, name_snapshot, instructions_snapshot, state, completed_at, note FROM run_steps WHERE run_id = ? ORDER BY position, id;");
    statement.bind(1, run_id);
    while (statement.step()) values.push_back(read_run_step(statement));
    return values;
}

domain::RunStep PlaybookService::add_run_step(const domain::Id& run_id,
                                              const std::string& name,
                                              const std::string& instructions) {
    const auto run = get_run_internal(run_id);
    if (domain::trim(name).empty()) throw std::invalid_argument("Run step name is required");
    auto position_statement = database_.prepare("SELECT coalesce(max(position), -1) + 1 FROM run_steps WHERE run_id = ?;");
    position_statement.bind(1, run_id); position_statement.step();
    domain::RunStep value;
    value.id = domain::new_id(); value.run_id = run.id; value.position = position_statement.column_int64(0);
    value.name_snapshot = name; value.instructions_snapshot = instructions; value.state = domain::RunStepState::Todo;
    auto statement = database_.prepare("INSERT INTO run_steps(id, run_id, source_step_id, position, name_snapshot, instructions_snapshot, state, completed_at, note) VALUES(?, ?, NULL, ?, ?, ?, 'todo', NULL, '');");
    statement.bind(1, value.id); statement.bind(2, value.run_id); statement.bind(3, value.position); statement.bind(4, value.name_snapshot); statement.bind(5, value.instructions_snapshot); statement.step();
    logger_.record(run.case_id, "added", "run_step", value.id, "Step added to current playbook run");
    return value;
}

void PlaybookService::update_run_step(const domain::Id& run_step_id,
                                      domain::RunStepState state,
                                      const std::string& note) {
    auto statement = database_.prepare("SELECT r.case_id FROM run_steps s JOIN playbook_runs r ON r.id = s.run_id WHERE s.id = ?;");
    statement.bind(1, run_step_id);
    if (!statement.step()) throw std::runtime_error("Run step not found: " + run_step_id);
    const auto case_id = statement.column_text(0);
    const auto completed_at = state == domain::RunStepState::Todo ? std::optional<std::string>{} : std::optional<std::string>{domain::utc_now()};
    auto update = database_.prepare("UPDATE run_steps SET state = ?, completed_at = ?, note = ? WHERE id = ?;");
    update.bind(1, domain::to_string(state)); update.bind(2, completed_at); update.bind(3, note); update.bind(4, run_step_id); update.step();
    auto run_update = database_.prepare("UPDATE playbook_runs SET updated_at = ? WHERE id = (SELECT run_id FROM run_steps WHERE id = ?);");
    run_update.bind(1, domain::utc_now()); run_update.bind(2, run_step_id); run_update.step();
    logger_.record(case_id, "updated", "run_step", run_step_id, "Playbook run step updated");
}

void PlaybookService::link_step_evidence(const domain::Id& run_step_id, const domain::Id& evidence_id) {
    auto statement = database_.prepare(
        "SELECT r.case_id, e.case_id FROM run_steps s JOIN playbook_runs r ON r.id = s.run_id JOIN evidence e ON e.id = ? WHERE s.id = ?;");
    statement.bind(1, evidence_id); statement.bind(2, run_step_id);
    if (!statement.step()) throw std::runtime_error("Run step or evidence not found");
    if (statement.column_text(0) != statement.column_text(1)) throw std::invalid_argument("Run step and evidence belong to different cases");
    auto link = database_.prepare("INSERT INTO run_step_evidence(run_step_id, evidence_id) VALUES(?, ?);");
    link.bind(1, run_step_id); link.bind(2, evidence_id); link.step();
    logger_.record(statement.column_text(0), "linked", "run_step_evidence", run_step_id, "Evidence linked to playbook step");
}

std::vector<domain::Id> PlaybookService::evidence_for_step(const domain::Id& run_step_id) const {
    auto statement = database_.prepare("SELECT evidence_id FROM run_step_evidence WHERE run_step_id = ? ORDER BY evidence_id;");
    statement.bind(1, run_step_id);
    std::vector<domain::Id> values;
    while (statement.step()) values.push_back(statement.column_text(0));
    return values;
}

} // namespace evidence_trace::services
