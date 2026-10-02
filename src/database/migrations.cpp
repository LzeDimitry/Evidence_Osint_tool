#include "database/migrations.hpp"

#include "domain/types.hpp"

#include <sqlite3.h>

#include <chrono>
#include <filesystem>
#include <sstream>

namespace evidence_trace::database {

namespace {

int schema_version(Database& database) {
    auto statement = database.prepare("PRAGMA user_version;");
    if (!statement.step()) throw DatabaseError("SQLite did not return user_version");
    return static_cast<int>(statement.column_int64(0));
}

void backup_before_migration(const std::filesystem::path& database_path) {
    if (!std::filesystem::exists(database_path) || std::filesystem::file_size(database_path) == 0) return;
    const auto ticks = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const auto backup_path = database_path.string() + ".pre-migration-" + std::to_string(ticks) + ".bak";
    std::filesystem::copy_file(database_path, backup_path, std::filesystem::copy_options::none);
}

const char* migration_sql(int version) {
    if (version == 1) {
        return R"SQL(
CREATE TABLE cases (
    id TEXT PRIMARY KEY NOT NULL,
    title TEXT NOT NULL CHECK(length(trim(title)) > 0),
    purpose TEXT NOT NULL DEFAULT '',
    description TEXT NOT NULL DEFAULT '',
    primary_target_type TEXT,
    scope TEXT NOT NULL DEFAULT '',
    tags_json TEXT NOT NULL DEFAULT '[]',
    status TEXT NOT NULL DEFAULT 'active' CHECK(status IN ('active', 'archived')),
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

CREATE TABLE entities (
    id TEXT PRIMARY KEY NOT NULL,
    case_id TEXT NOT NULL REFERENCES cases(id) ON DELETE RESTRICT,
    type TEXT NOT NULL CHECK(type IN ('person','username','account','email','phone','domain','ip','company','place','url','image','document','event','other')),
    label TEXT NOT NULL CHECK(length(trim(label)) > 0),
    original_value TEXT NOT NULL DEFAULT '',
    canonical_value TEXT NOT NULL DEFAULT '',
    details_json TEXT NOT NULL DEFAULT '{}',
    tags_json TEXT NOT NULL DEFAULT '[]',
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

CREATE TABLE sources (
    id TEXT PRIMARY KEY NOT NULL,
    case_id TEXT NOT NULL REFERENCES cases(id) ON DELETE RESTRICT,
    type TEXT NOT NULL CHECK(type IN ('web','document','registry','person','other')),
    locator TEXT NOT NULL DEFAULT '',
    title TEXT NOT NULL DEFAULT '',
    accessed_at TEXT NOT NULL,
    author TEXT NOT NULL DEFAULT '',
    reliability_note TEXT NOT NULL DEFAULT '',
    created_at TEXT NOT NULL
);

CREATE TABLE evidence (
    id TEXT PRIMARY KEY NOT NULL,
    case_id TEXT NOT NULL REFERENCES cases(id) ON DELETE RESTRICT,
    source_id TEXT REFERENCES sources(id) ON DELETE RESTRICT,
    kind TEXT NOT NULL CHECK(kind IN ('file','text','url')),
    relative_path TEXT,
    original_filename TEXT,
    mime_type TEXT,
    byte_size INTEGER,
    sha256 TEXT,
    text_content TEXT NOT NULL DEFAULT '',
    source_url TEXT,
    captured_at TEXT NOT NULL,
    imported_at TEXT NOT NULL,
    note TEXT NOT NULL DEFAULT '',
    created_at TEXT NOT NULL,
    CHECK(kind != 'file' OR (relative_path IS NOT NULL AND sha256 IS NOT NULL AND byte_size IS NOT NULL AND byte_size >= 0)),
    CHECK(kind != 'text' OR relative_path IS NULL),
    CHECK(kind != 'url' OR (source_url IS NOT NULL AND relative_path IS NULL))
);

CREATE TABLE claims (
    id TEXT PRIMARY KEY NOT NULL,
    case_id TEXT NOT NULL REFERENCES cases(id) ON DELETE RESTRICT,
    statement TEXT NOT NULL CHECK(length(trim(statement)) > 0),
    kind TEXT NOT NULL CHECK(kind IN ('observation','inference')),
    predicate TEXT,
    status TEXT NOT NULL DEFAULT 'unverified' CHECK(status IN ('unverified','possible','probable','confirmed','refuted')),
    reasoning TEXT NOT NULL DEFAULT '',
    recorded_by TEXT NOT NULL DEFAULT '',
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL,
    CHECK(kind != 'inference' OR length(trim(reasoning)) > 0)
);

CREATE TABLE claim_entities (
    claim_id TEXT NOT NULL REFERENCES claims(id) ON DELETE RESTRICT,
    entity_id TEXT NOT NULL REFERENCES entities(id) ON DELETE RESTRICT,
    role TEXT NOT NULL CHECK(role IN ('subject','object','context')),
    PRIMARY KEY(claim_id, entity_id, role)
);

CREATE TABLE claim_evidence (
    claim_id TEXT NOT NULL REFERENCES claims(id) ON DELETE RESTRICT,
    evidence_id TEXT NOT NULL REFERENCES evidence(id) ON DELETE RESTRICT,
    role TEXT NOT NULL CHECK(role IN ('supports','contradicts','context')),
    note TEXT NOT NULL DEFAULT '',
    PRIMARY KEY(claim_id, evidence_id)
);

CREATE TABLE notes (
    id TEXT PRIMARY KEY NOT NULL,
    case_id TEXT NOT NULL REFERENCES cases(id) ON DELETE RESTRICT,
    title TEXT NOT NULL DEFAULT '',
    body TEXT NOT NULL CHECK(length(trim(body)) > 0),
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

CREATE TABLE note_entities (
    note_id TEXT NOT NULL REFERENCES notes(id) ON DELETE RESTRICT,
    entity_id TEXT NOT NULL REFERENCES entities(id) ON DELETE RESTRICT,
    PRIMARY KEY(note_id, entity_id)
);

CREATE TABLE note_evidence (
    note_id TEXT NOT NULL REFERENCES notes(id) ON DELETE RESTRICT,
    evidence_id TEXT NOT NULL REFERENCES evidence(id) ON DELETE RESTRICT,
    PRIMARY KEY(note_id, evidence_id)
);

CREATE TABLE techniques (
    id TEXT PRIMARY KEY NOT NULL,
    name TEXT NOT NULL CHECK(length(trim(name)) > 0),
    objective TEXT NOT NULL DEFAULT '',
    steps TEXT NOT NULL DEFAULT '',
    example_queries_json TEXT NOT NULL DEFAULT '[]',
    tools_links_json TEXT NOT NULL DEFAULT '[]',
    limitations TEXT NOT NULL DEFAULT '',
    tags_json TEXT NOT NULL DEFAULT '[]',
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

CREATE TABLE playbooks (
    id TEXT PRIMARY KEY NOT NULL,
    name TEXT NOT NULL CHECK(length(trim(name)) > 0),
    scope TEXT NOT NULL DEFAULT '',
    description TEXT NOT NULL DEFAULT '',
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

CREATE TABLE playbook_steps (
    id TEXT PRIMARY KEY NOT NULL,
    playbook_id TEXT NOT NULL REFERENCES playbooks(id) ON DELETE RESTRICT,
    technique_id TEXT REFERENCES techniques(id) ON DELETE SET NULL,
    name TEXT NOT NULL CHECK(length(trim(name)) > 0),
    instructions TEXT NOT NULL DEFAULT '',
    position INTEGER NOT NULL CHECK(position >= 0),
    UNIQUE(playbook_id, position)
);

CREATE TABLE playbook_runs (
    id TEXT PRIMARY KEY NOT NULL,
    case_id TEXT NOT NULL REFERENCES cases(id) ON DELETE RESTRICT,
    playbook_id TEXT REFERENCES playbooks(id) ON DELETE SET NULL,
    entity_id TEXT REFERENCES entities(id) ON DELETE RESTRICT,
    name_snapshot TEXT NOT NULL,
    started_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

CREATE TABLE run_steps (
    id TEXT PRIMARY KEY NOT NULL,
    run_id TEXT NOT NULL REFERENCES playbook_runs(id) ON DELETE RESTRICT,
    source_step_id TEXT,
    position INTEGER NOT NULL CHECK(position >= 0),
    name_snapshot TEXT NOT NULL,
    instructions_snapshot TEXT NOT NULL DEFAULT '',
    state TEXT NOT NULL DEFAULT 'todo' CHECK(state IN ('todo','done','skipped')),
    completed_at TEXT,
    note TEXT NOT NULL DEFAULT '',
    UNIQUE(run_id, position)
);

CREATE TABLE run_step_evidence (
    run_step_id TEXT NOT NULL REFERENCES run_steps(id) ON DELETE RESTRICT,
    evidence_id TEXT NOT NULL REFERENCES evidence(id) ON DELETE RESTRICT,
    PRIMARY KEY(run_step_id, evidence_id)
);

CREATE TABLE log_events (
    id TEXT PRIMARY KEY NOT NULL,
    case_id TEXT NOT NULL REFERENCES cases(id) ON DELETE RESTRICT,
    occurred_at TEXT NOT NULL,
    action TEXT NOT NULL,
    object_type TEXT NOT NULL,
    object_id TEXT,
    description TEXT NOT NULL,
    payload_json TEXT NOT NULL DEFAULT '{}'
);

CREATE INDEX idx_entities_case ON entities(case_id);
CREATE INDEX idx_entities_canonical ON entities(case_id, type, canonical_value);
CREATE INDEX idx_sources_case_locator ON sources(case_id, locator);
CREATE INDEX idx_evidence_case ON evidence(case_id);
CREATE INDEX idx_claims_case_status ON claims(case_id, status);
CREATE INDEX idx_claims_updated ON claims(updated_at);
CREATE INDEX idx_log_case_time ON log_events(case_id, occurred_at);
CREATE INDEX idx_runs_case ON playbook_runs(case_id);
CREATE INDEX idx_notes_case ON notes(case_id);

INSERT INTO techniques(id, name, objective, steps, example_queries_json, tools_links_json, limitations, tags_json, created_at, updated_at)
VALUES('00000000-0000-4000-8000-000000000001', 'Check commit history', 'Find reuse of an exact username in public repository history', 'Search commit history for the exact username and record the result URL.', '[]', '[]', 'Results require human review and do not establish ownership.', '["username"]', '2026-09-24T00:00:00.000Z', '2026-09-24T00:00:00.000Z');
INSERT INTO playbooks(id, name, scope, description, created_at, updated_at)
VALUES('00000000-0000-4000-8000-000000000002', 'Username investigation', 'Public sources', 'Starter checklist for investigating a username without automatic matching.', '2026-09-24T00:00:00.000Z', '2026-09-24T00:00:00.000Z');
INSERT INTO playbook_steps(id, playbook_id, technique_id, name, instructions, position)
VALUES('00000000-0000-4000-8000-000000000003', '00000000-0000-4000-8000-000000000002', '00000000-0000-4000-8000-000000000001', 'Check commit history', 'Search commit history for the exact username and record the result URL.', 0);
)SQL";
    }
    if (version == 2) {
        return "ALTER TABLE evidence ADD COLUMN quotation_location TEXT NOT NULL DEFAULT '';";
    }
    if (version == 3) {
        return "ALTER TABLE entities ADD COLUMN aliases_json TEXT NOT NULL DEFAULT '[]';";
    }
    throw DatabaseError("No migration exists for schema version " + std::to_string(version));
}

} // namespace

void apply_migrations(Database& database, const std::filesystem::path& database_path) {
    const int current = schema_version(database);
    if (current > current_schema_version) {
        throw DatabaseError("Database schema version " + std::to_string(current) +
                            " is newer than this application supports (" +
                            std::to_string(current_schema_version) + ")");
    }
    if (current < current_schema_version) backup_before_migration(database_path);
    for (int version = current + 1; version <= current_schema_version; ++version) {
        Transaction transaction(database);
        database.execute(migration_sql(version));
        database.execute("PRAGMA user_version = " + std::to_string(version) + ";");
        transaction.commit();
    }
}

} // namespace evidence_trace::database
