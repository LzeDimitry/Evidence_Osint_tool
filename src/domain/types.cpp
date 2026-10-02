#include "domain/types.hpp"

#include <array>
#include <cctype>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>

namespace evidence_trace::domain {

namespace {

template <typename T>
[[noreturn]] T invalid_enum(const std::string& value, const char* name) {
    throw std::invalid_argument("Unknown " + std::string(name) + ": " + value);
}

} // namespace

std::string to_string(CaseStatus value) {
    return value == CaseStatus::Active ? "active" : "archived";
}

std::string to_string(EntityType value) {
    switch (value) {
    case EntityType::Person: return "person";
    case EntityType::Username: return "username";
    case EntityType::Account: return "account";
    case EntityType::Email: return "email";
    case EntityType::Phone: return "phone";
    case EntityType::Domain: return "domain";
    case EntityType::Ip: return "ip";
    case EntityType::Company: return "company";
    case EntityType::Place: return "place";
    case EntityType::Url: return "url";
    case EntityType::Image: return "image";
    case EntityType::Document: return "document";
    case EntityType::Event: return "event";
    case EntityType::Other: return "other";
    }
    return "other";
}

std::string to_string(SourceType value) {
    switch (value) {
    case SourceType::Web: return "web";
    case SourceType::Document: return "document";
    case SourceType::Registry: return "registry";
    case SourceType::Person: return "person";
    case SourceType::Other: return "other";
    }
    return "other";
}

std::string to_string(EvidenceKind value) {
    switch (value) {
    case EvidenceKind::File: return "file";
    case EvidenceKind::Text: return "text";
    case EvidenceKind::Url: return "url";
    }
    return "text";
}

std::string to_string(ClaimKind value) {
    return value == ClaimKind::Observation ? "observation" : "inference";
}

std::string to_string(ClaimStatus value) {
    switch (value) {
    case ClaimStatus::Unverified: return "unverified";
    case ClaimStatus::Possible: return "possible";
    case ClaimStatus::Probable: return "probable";
    case ClaimStatus::Confirmed: return "confirmed";
    case ClaimStatus::Refuted: return "refuted";
    }
    return "unverified";
}

std::string to_string(ClaimEntityRole value) {
    switch (value) {
    case ClaimEntityRole::Subject: return "subject";
    case ClaimEntityRole::Object: return "object";
    case ClaimEntityRole::Context: return "context";
    }
    return "context";
}

std::string to_string(EvidenceRole value) {
    switch (value) {
    case EvidenceRole::Supports: return "supports";
    case EvidenceRole::Contradicts: return "contradicts";
    case EvidenceRole::Context: return "context";
    }
    return "context";
}

std::string to_string(RunStepState value) {
    switch (value) {
    case RunStepState::Todo: return "todo";
    case RunStepState::Done: return "done";
    case RunStepState::Skipped: return "skipped";
    }
    return "todo";
}

CaseStatus case_status_from_string(const std::string& value) {
    if (value == "active") return CaseStatus::Active;
    if (value == "archived") return CaseStatus::Archived;
    return invalid_enum<CaseStatus>(value, "case status");
}

EntityType entity_type_from_string(const std::string& value) {
    const std::array<std::pair<const char*, EntityType>, 14> values{{
        {"person", EntityType::Person}, {"username", EntityType::Username},
        {"account", EntityType::Account}, {"email", EntityType::Email},
        {"phone", EntityType::Phone}, {"domain", EntityType::Domain},
        {"ip", EntityType::Ip}, {"company", EntityType::Company},
        {"place", EntityType::Place}, {"url", EntityType::Url},
        {"image", EntityType::Image}, {"document", EntityType::Document},
        {"event", EntityType::Event}, {"other", EntityType::Other},
    }};
    for (const auto& [name, type] : values) if (value == name) return type;
    return invalid_enum<EntityType>(value, "entity type");
}

SourceType source_type_from_string(const std::string& value) {
    if (value == "web") return SourceType::Web;
    if (value == "document") return SourceType::Document;
    if (value == "registry") return SourceType::Registry;
    if (value == "person") return SourceType::Person;
    if (value == "other") return SourceType::Other;
    return invalid_enum<SourceType>(value, "source type");
}

EvidenceKind evidence_kind_from_string(const std::string& value) {
    if (value == "file") return EvidenceKind::File;
    if (value == "text") return EvidenceKind::Text;
    if (value == "url") return EvidenceKind::Url;
    return invalid_enum<EvidenceKind>(value, "evidence kind");
}

ClaimKind claim_kind_from_string(const std::string& value) {
    if (value == "observation") return ClaimKind::Observation;
    if (value == "inference") return ClaimKind::Inference;
    return invalid_enum<ClaimKind>(value, "claim kind");
}

ClaimStatus claim_status_from_string(const std::string& value) {
    if (value == "unverified") return ClaimStatus::Unverified;
    if (value == "possible") return ClaimStatus::Possible;
    if (value == "probable") return ClaimStatus::Probable;
    if (value == "confirmed") return ClaimStatus::Confirmed;
    if (value == "refuted") return ClaimStatus::Refuted;
    return invalid_enum<ClaimStatus>(value, "claim status");
}

ClaimEntityRole claim_entity_role_from_string(const std::string& value) {
    if (value == "subject") return ClaimEntityRole::Subject;
    if (value == "object") return ClaimEntityRole::Object;
    if (value == "context") return ClaimEntityRole::Context;
    return invalid_enum<ClaimEntityRole>(value, "claim entity role");
}

EvidenceRole evidence_role_from_string(const std::string& value) {
    if (value == "supports") return EvidenceRole::Supports;
    if (value == "contradicts") return EvidenceRole::Contradicts;
    if (value == "context") return EvidenceRole::Context;
    return invalid_enum<EvidenceRole>(value, "evidence role");
}

RunStepState run_step_state_from_string(const std::string& value) {
    if (value == "todo") return RunStepState::Todo;
    if (value == "done") return RunStepState::Done;
    if (value == "skipped") return RunStepState::Skipped;
    return invalid_enum<RunStepState>(value, "run step state");
}

Id new_id() {
    std::random_device device;
    std::mt19937_64 generator(device());
    std::array<std::uint8_t, 16> bytes{};
    for (auto& byte : bytes) byte = static_cast<std::uint8_t>(generator() & 0xffU);
    bytes[6] = static_cast<std::uint8_t>((bytes[6] & 0x0fU) | 0x40U);
    bytes[8] = static_cast<std::uint8_t>((bytes[8] & 0x3fU) | 0x80U);
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) output << '-';
        output << std::setw(2) << static_cast<unsigned int>(bytes[i]);
    }
    return output.str();
}

std::string utc_now() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &time);
#else
    gmtime_r(&time, &utc);
#endif
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    std::ostringstream output;
    output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S")
           << '.' << std::setfill('0') << std::setw(3) << milliseconds.count() << 'Z';
    return output.str();
}

std::string lower_ascii(std::string value) {
    for (char& character : value) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return value;
}

std::string trim(std::string value) {
    auto first = value.begin();
    while (first != value.end() && std::isspace(static_cast<unsigned char>(*first))) ++first;
    auto last = value.end();
    while (last != first && std::isspace(static_cast<unsigned char>(*(last - 1)))) --last;
    return std::string(first, last);
}

} // namespace evidence_trace::domain
