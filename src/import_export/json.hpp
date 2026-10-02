#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace evidence_trace::json {

class JsonError : public std::runtime_error {
public:
    explicit JsonError(const std::string& message) : std::runtime_error(message) {}
};

class Json {
public:
    using Object = std::map<std::string, Json>;
    using Array = std::vector<Json>;
    using Value = std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;

    Json() : value_(nullptr) {}
    Json(std::nullptr_t) : value_(nullptr) {}
    Json(bool value) : value_(value) {}
    Json(int value) : value_(static_cast<double>(value)) {}
    Json(std::int64_t value) : value_(static_cast<double>(value)) {}
    Json(double value) : value_(value) {}
    Json(const char* value) : value_(std::string(value == nullptr ? "" : value)) {}
    Json(std::string value) : value_(std::move(value)) {}
    Json(Array value) : value_(std::move(value)) {}
    Json(Object value) : value_(std::move(value)) {}

    static Json parse(const std::string& text);
    std::string dump() const;

    bool is_null() const;
    bool is_bool() const;
    bool is_number() const;
    bool is_string() const;
    bool is_array() const;
    bool is_object() const;

    bool as_bool() const;
    double as_number() const;
    std::int64_t as_int64() const;
    const std::string& as_string() const;
    const Array& as_array() const;
    Array& as_array();
    const Object& as_object() const;
    Object& as_object();

    const Json& at(const std::string& key) const;
    Json& operator[](const std::string& key);
    const Json* find(const std::string& key) const;
    std::string string_or(const std::string& key, const std::string& fallback = {}) const;
    std::optional<std::string> optional_string(const std::string& key) const;

private:
    Value value_;
};

} // namespace evidence_trace::json
