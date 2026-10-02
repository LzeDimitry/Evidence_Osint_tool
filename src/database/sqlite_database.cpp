#include "database/sqlite_database.hpp"

#include <sqlite3.h>

#include <sstream>

namespace evidence_trace::database {

namespace {

[[noreturn]] void throw_sqlite(sqlite3* database, const std::string& context, int code) {
    std::ostringstream message;
    message << context << " (SQLite " << code << ")";
    if (database != nullptr) message << ": " << sqlite3_errmsg(database);
    throw DatabaseError(message.str());
}

void check_bind(sqlite3* database, int result, const std::string& context) {
    if (result != SQLITE_OK) throw_sqlite(database, context, result);
}

} // namespace

Statement& Statement::operator=(Statement&& other) noexcept {
    if (this == &other) return *this;
    if (statement_ != nullptr) sqlite3_finalize(statement_);
    statement_ = other.statement_;
    other.statement_ = nullptr;
    return *this;
}

Statement::~Statement() {
    if (statement_ != nullptr) sqlite3_finalize(statement_);
}

void Statement::bind(int index, const std::string& value) {
    if (statement_ == nullptr) throw DatabaseError("Cannot bind on an empty statement");
    check_bind(sqlite3_db_handle(statement_), sqlite3_bind_text(
        statement_, index, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT),
        "Failed to bind text");
}

void Statement::bind(int index, const char* value) {
    if (value == nullptr) bind_null(index);
    else bind(index, std::string(value));
}

void Statement::bind(int index, std::int64_t value) {
    if (statement_ == nullptr) throw DatabaseError("Cannot bind on an empty statement");
    check_bind(sqlite3_db_handle(statement_), sqlite3_bind_int64(statement_, index, value),
               "Failed to bind integer");
}

void Statement::bind(int index, double value) {
    if (statement_ == nullptr) throw DatabaseError("Cannot bind on an empty statement");
    check_bind(sqlite3_db_handle(statement_), sqlite3_bind_double(statement_, index, value),
               "Failed to bind number");
}

void Statement::bind_null(int index) {
    if (statement_ == nullptr) throw DatabaseError("Cannot bind on an empty statement");
    check_bind(sqlite3_db_handle(statement_), sqlite3_bind_null(statement_, index),
               "Failed to bind NULL");
}

void Statement::bind(int index, const std::optional<std::string>& value) {
    if (value.has_value()) bind(index, *value);
    else bind_null(index);
}

void Statement::bind(int index, const std::optional<std::int64_t>& value) {
    if (value.has_value()) bind(index, *value);
    else bind_null(index);
}

bool Statement::step() {
    if (statement_ == nullptr) throw DatabaseError("Cannot step an empty statement");
    const int result = sqlite3_step(statement_);
    if (result == SQLITE_ROW) return true;
    if (result == SQLITE_DONE) return false;
    throw_sqlite(sqlite3_db_handle(statement_), "Failed to execute statement", result);
}

void Statement::reset() {
    if (statement_ == nullptr) return;
    const int result = sqlite3_reset(statement_);
    if (result != SQLITE_OK) throw_sqlite(sqlite3_db_handle(statement_), "Failed to reset statement", result);
    sqlite3_clear_bindings(statement_);
}

std::int64_t Statement::column_int64(int index) const {
    return sqlite3_column_int64(statement_, index);
}

double Statement::column_double(int index) const {
    return sqlite3_column_double(statement_, index);
}

std::string Statement::column_text(int index) const {
    const auto* text = sqlite3_column_text(statement_, index);
    const int bytes = sqlite3_column_bytes(statement_, index);
    if (text == nullptr || bytes <= 0) return {};
    return std::string(reinterpret_cast<const char*>(text), static_cast<std::size_t>(bytes));
}

std::optional<std::string> Statement::column_optional_text(int index) const {
    if (column_is_null(index)) return std::nullopt;
    return column_text(index);
}

bool Statement::column_bool(int index) const {
    return column_int64(index) != 0;
}

bool Statement::column_is_null(int index) const {
    return sqlite3_column_type(statement_, index) == SQLITE_NULL;
}

Database::Database(const std::filesystem::path& path, bool read_only) : path_(path) {
    const int flags = read_only ? SQLITE_OPEN_READONLY : (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    const int result = sqlite3_open_v2(path.string().c_str(), &database_, flags, nullptr);
    if (result != SQLITE_OK) {
        const std::string message = database_ == nullptr ? "Failed to open database" : sqlite3_errmsg(database_);
        if (database_ != nullptr) sqlite3_close(database_);
        database_ = nullptr;
        throw DatabaseError(message);
    }
    execute("PRAGMA foreign_keys = ON;");
    execute("PRAGMA busy_timeout = 5000;");
}

Database::~Database() {
    if (database_ != nullptr) sqlite3_close(database_);
}

void Database::execute(const std::string& sql) {
    char* error = nullptr;
    const int result = sqlite3_exec(database_, sql.c_str(), nullptr, nullptr, &error);
    if (result != SQLITE_OK) {
        const std::string detail = error == nullptr ? "" : std::string(error);
        sqlite3_free(error);
        throw DatabaseError("Failed to execute SQL: " + detail);
    }
}

Statement Database::prepare(const std::string& sql) const {
    sqlite3_stmt* statement = nullptr;
    const int result = sqlite3_prepare_v2(database_, sql.c_str(), -1, &statement, nullptr);
    if (result != SQLITE_OK) throw_sqlite(database_, "Failed to prepare SQL", result);
    return Statement(statement);
}

std::int64_t Database::last_insert_rowid() const {
    return sqlite3_last_insert_rowid(database_);
}

Transaction::Transaction(Database& database) : database_(&database) {
    database_->execute("BEGIN IMMEDIATE;");
}

Transaction::~Transaction() {
    if (active_) {
        try { database_->execute("ROLLBACK;"); } catch (...) { /* preserve the original exception */ }
    }
}

void Transaction::commit() {
    if (!active_) throw DatabaseError("Transaction is no longer active");
    database_->execute("COMMIT;");
    active_ = false;
}

void Transaction::rollback() {
    if (!active_) return;
    database_->execute("ROLLBACK;");
    active_ = false;
}

} // namespace evidence_trace::database
