#include "import_export/json.hpp"

#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace evidence_trace::json {

namespace {

void append_utf8(std::string& output, std::uint32_t codepoint) {
    if (codepoint <= 0x7f) {
        output.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7ff) {
        output.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0xffff) {
        output.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0x10ffff) {
        output.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else {
        throw JsonError("Invalid Unicode code point");
    }
}

class Parser {
public:
    explicit Parser(const std::string& text) : text_(text) {}

    Json parse() {
        skip_space();
        auto value = parse_value();
        skip_space();
        if (position_ != text_.size()) fail("Trailing characters");
        return value;
    }

private:
    [[noreturn]] void fail(const std::string& message) const {
        throw JsonError(message + " at offset " + std::to_string(position_));
    }

    void skip_space() {
        while (position_ < text_.size()) {
            const char character = text_[position_];
            if (character != ' ' && character != '\t' && character != '\r' && character != '\n') break;
            ++position_;
        }
    }

    char take() {
        if (position_ >= text_.size()) fail("Unexpected end of input");
        return text_[position_++];
    }

    void expect(char expected) {
        if (take() != expected) fail(std::string("Expected '") + expected + "'");
    }

    Json parse_value() {
        skip_space();
        if (position_ >= text_.size()) fail("Expected a value");
        switch (text_[position_]) {
        case 'n': return parse_literal("null", Json(nullptr));
        case 't': return parse_literal("true", Json(true));
        case 'f': return parse_literal("false", Json(false));
        case '"': return Json(parse_string());
        case '[': return parse_array();
        case '{': return parse_object();
        default:
            if (text_[position_] == '-' || (text_[position_] >= '0' && text_[position_] <= '9')) return parse_number();
            fail("Unexpected value");
        }
    }

    Json parse_literal(const char* literal, Json value) {
        for (const char* character = literal; *character != '\0'; ++character) {
            if (take() != *character) fail("Invalid literal");
        }
        return value;
    }

    std::uint32_t hex4() {
        std::uint32_t result = 0;
        for (int i = 0; i < 4; ++i) {
            const char character = take();
            result <<= 4;
            if (character >= '0' && character <= '9') result |= static_cast<std::uint32_t>(character - '0');
            else if (character >= 'a' && character <= 'f') result |= static_cast<std::uint32_t>(character - 'a' + 10);
            else if (character >= 'A' && character <= 'F') result |= static_cast<std::uint32_t>(character - 'A' + 10);
            else fail("Invalid Unicode escape");
        }
        return result;
    }

    std::string parse_string() {
        expect('"');
        std::string output;
        while (position_ < text_.size()) {
            const unsigned char character = static_cast<unsigned char>(take());
            if (character == '"') return output;
            if (character < 0x20) fail("Control character in string");
            if (character != '\\') {
                output.push_back(static_cast<char>(character));
                continue;
            }
            const char escaped = take();
            switch (escaped) {
            case '"': output.push_back('"'); break;
            case '\\': output.push_back('\\'); break;
            case '/': output.push_back('/'); break;
            case 'b': output.push_back('\b'); break;
            case 'f': output.push_back('\f'); break;
            case 'n': output.push_back('\n'); break;
            case 'r': output.push_back('\r'); break;
            case 't': output.push_back('\t'); break;
            case 'u': {
                const auto first = hex4();
                std::uint32_t codepoint = first;
                if (first >= 0xd800 && first <= 0xdbff) {
                    if (position_ + 1 >= text_.size() || text_[position_] != '\\' || text_[position_ + 1] != 'u') fail("Unpaired Unicode surrogate");
                    position_ += 2;
                    const auto second = hex4();
                    if (second < 0xdc00 || second > 0xdfff) fail("Invalid Unicode surrogate pair");
                    codepoint = 0x10000 + ((first - 0xd800) << 10) + (second - 0xdc00);
                } else if (first >= 0xdc00 && first <= 0xdfff) {
                    fail("Unpaired Unicode surrogate");
                }
                append_utf8(output, codepoint);
                break;
            }
            default: fail("Invalid string escape");
            }
        }
        fail("Unterminated string");
    }

    Json parse_number() {
        const auto start = position_;
        if (text_[position_] == '-') ++position_;
        if (position_ >= text_.size()) fail("Invalid number");
        if (text_[position_] == '0') {
            ++position_;
        } else {
            if (text_[position_] < '1' || text_[position_] > '9') fail("Invalid number");
            while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9') ++position_;
        }
        if (position_ < text_.size() && text_[position_] == '.') {
            ++position_;
            if (position_ >= text_.size() || text_[position_] < '0' || text_[position_] > '9') fail("Invalid number fraction");
            while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9') ++position_;
        }
        if (position_ < text_.size() && (text_[position_] == 'e' || text_[position_] == 'E')) {
            ++position_;
            if (position_ < text_.size() && (text_[position_] == '+' || text_[position_] == '-')) ++position_;
            if (position_ >= text_.size() || text_[position_] < '0' || text_[position_] > '9') fail("Invalid number exponent");
            while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9') ++position_;
        }
        try { return Json(std::stod(text_.substr(start, position_ - start))); }
        catch (...) { fail("Invalid number"); }
    }

    Json parse_array() {
        expect('[');
        Json::Array values;
        skip_space();
        if (position_ < text_.size() && text_[position_] == ']') { ++position_; return Json(std::move(values)); }
        while (true) {
            values.push_back(parse_value());
            skip_space();
            const char separator = take();
            if (separator == ']') return Json(std::move(values));
            if (separator != ',') fail("Expected ',' or ']'");
            skip_space();
        }
    }

    Json parse_object() {
        expect('{');
        Json::Object values;
        skip_space();
        if (position_ < text_.size() && text_[position_] == '}') { ++position_; return Json(std::move(values)); }
        while (true) {
            skip_space();
            if (position_ >= text_.size() || text_[position_] != '"') fail("Object key must be a string");
            const auto key = parse_string();
            skip_space();
            expect(':');
            values[key] = parse_value();
            skip_space();
            const char separator = take();
            if (separator == '}') return Json(std::move(values));
            if (separator != ',') fail("Expected ',' or '}'");
        }
    }

    const std::string& text_;
    std::size_t position_{0};
};

void dump_string(const std::string& value, std::ostringstream& output) {
    output << '"';
    for (const unsigned char character : value) {
        switch (character) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (character < 0x20) output << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<unsigned int>(character) << std::dec;
            else output << static_cast<char>(character);
        }
    }
    output << '"';
}

void dump_value(const Json& value, std::ostringstream& output) {
    if (value.is_null()) { output << "null"; return; }
    if (value.is_bool()) { output << (value.as_bool() ? "true" : "false"); return; }
    if (value.is_number()) {
        output << std::setprecision(std::numeric_limits<double>::max_digits10) << value.as_number();
        return;
    }
    if (value.is_string()) { dump_string(value.as_string(), output); return; }
    if (value.is_array()) {
        output << '[';
        bool first = true;
        for (const auto& item : value.as_array()) { if (!first) output << ','; first = false; dump_value(item, output); }
        output << ']';
        return;
    }
    output << '{';
    bool first = true;
    for (const auto& [key, item] : value.as_object()) {
        if (!first) output << ',';
        first = false;
        dump_string(key, output); output << ':'; dump_value(item, output);
    }
    output << '}';
}

} // namespace

Json Json::parse(const std::string& text) { return Parser(text).parse(); }

std::string Json::dump() const {
    std::ostringstream output;
    dump_value(*this, output);
    return output.str();
}

bool Json::is_null() const { return std::holds_alternative<std::nullptr_t>(value_); }
bool Json::is_bool() const { return std::holds_alternative<bool>(value_); }
bool Json::is_number() const { return std::holds_alternative<double>(value_); }
bool Json::is_string() const { return std::holds_alternative<std::string>(value_); }
bool Json::is_array() const { return std::holds_alternative<Array>(value_); }
bool Json::is_object() const { return std::holds_alternative<Object>(value_); }

bool Json::as_bool() const {
    if (!is_bool()) throw JsonError("Expected a JSON boolean");
    return std::get<bool>(value_);
}

double Json::as_number() const {
    if (!is_number()) throw JsonError("Expected a JSON number");
    return std::get<double>(value_);
}

std::int64_t Json::as_int64() const {
    const auto number = as_number();
    if (!std::isfinite(number) || std::floor(number) != number || number < static_cast<double>(std::numeric_limits<std::int64_t>::min()) || number > static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
        throw JsonError("Expected a JSON integer");
    }
    return static_cast<std::int64_t>(number);
}

const std::string& Json::as_string() const {
    if (!is_string()) throw JsonError("Expected a JSON string");
    return std::get<std::string>(value_);
}

const Json::Array& Json::as_array() const {
    if (!is_array()) throw JsonError("Expected a JSON array");
    return std::get<Array>(value_);
}

Json::Array& Json::as_array() {
    if (!is_array()) throw JsonError("Expected a JSON array");
    return std::get<Array>(value_);
}

const Json::Object& Json::as_object() const {
    if (!is_object()) throw JsonError("Expected a JSON object");
    return std::get<Object>(value_);
}

Json::Object& Json::as_object() {
    if (!is_object()) throw JsonError("Expected a JSON object");
    return std::get<Object>(value_);
}

const Json& Json::at(const std::string& key) const {
    const auto* value = find(key);
    if (value == nullptr) throw JsonError("Missing JSON field: " + key);
    return *value;
}

Json& Json::operator[](const std::string& key) {
    if (!is_object()) value_ = Object{};
    return std::get<Object>(value_)[key];
}

const Json* Json::find(const std::string& key) const {
    if (!is_object()) throw JsonError("Expected a JSON object");
    const auto iterator = std::get<Object>(value_).find(key);
    return iterator == std::get<Object>(value_).end() ? nullptr : &iterator->second;
}

std::string Json::string_or(const std::string& key, const std::string& fallback) const {
    const auto* value = find(key);
    return value == nullptr || value->is_null() ? fallback : value->as_string();
}

std::optional<std::string> Json::optional_string(const std::string& key) const {
    const auto* value = find(key);
    if (value == nullptr || value->is_null()) return std::nullopt;
    return value->as_string();
}

} // namespace evidence_trace::json
