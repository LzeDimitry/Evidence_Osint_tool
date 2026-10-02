#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>

struct sqlite3;
struct sqlite3_stmt;

namespace evidence_trace::database {

class DatabaseError : public std::runtime_error {
public:
    explicit DatabaseError(const std::string& message) : std::runtime_error(message) {}
};

class Statement {
public:
    Statement() = default;
    explicit Statement(sqlite3_stmt* statement) : statement_(statement) {}
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    Statement(Statement&& other) noexcept : statement_(other.statement_) { other.statement_ = nullptr; }
    Statement& operator=(Statement&& other) noexcept;
    ~Statement();

    void bind(int index, const std::string& value);
    void bind(int index, const char* value);
    void bind(int index, std::int64_t value);
    void bind(int index, double value);
    void bind_null(int index);
    void bind(int index, const std::optional<std::string>& value);
    void bind(int index, const std::optional<std::int64_t>& value);

    // Returns true for SQLITE_ROW and false for SQLITE_DONE.
    bool step();
    void reset();
    std::int64_t column_int64(int index) const;
    double column_double(int index) const;
    std::string column_text(int index) const;
    std::optional<std::string> column_optional_text(int index) const;
    bool column_bool(int index) const;
    bool column_is_null(int index) const;

private:
    sqlite3_stmt* statement_{nullptr};
};

class Database {
public:
    explicit Database(const std::filesystem::path& path, bool read_only = false);
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    ~Database();

    void execute(const std::string& sql);
    Statement prepare(const std::string& sql) const;
    std::int64_t last_insert_rowid() const;
    sqlite3* handle() const { return database_; }
    const std::filesystem::path& path() const { return path_; }

private:
    sqlite3* database_{nullptr};
    std::filesystem::path path_;
};

class Transaction {
public:
    explicit Transaction(Database& database);
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    ~Transaction();

    void commit();
    void rollback();

private:
    Database* database_;
    bool active_{true};
};

} // namespace evidence_trace::database
