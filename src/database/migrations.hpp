#pragma once

#include "database/sqlite_database.hpp"

#include <filesystem>

namespace evidence_trace::database {

constexpr int current_schema_version = 3;

void apply_migrations(Database& database, const std::filesystem::path& database_path);

} // namespace evidence_trace::database
