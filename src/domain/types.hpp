#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace evidence_trace::domain {

using Id = std::string;

enum class CaseStatus { Active, Archived };
enum class EntityType {
    Person,
    Username,
    Account,
    Email,
    Phone,
    Domain,
    Ip,
    Company,
    Place,
    Url,
    Image,
    Document,
    Event,
    Other,
};
enum class SourceType { Web, Document, Registry, Person, Other };
enum class EvidenceKind { File, Text, Url };
enum class ClaimKind { Observation, Inference };
enum class ClaimStatus { Unverified, Possible, Probable, Confirmed, Refuted };
enum class ClaimEntityRole { Subject, Object, Context };
enum class EvidenceRole { Supports, Contradicts, Context };
enum class RunStepState { Todo, Done, Skipped };

std::string to_string(CaseStatus value);
std::string to_string(EntityType value);
std::string to_string(SourceType value);
std::string to_string(EvidenceKind value);
std::string to_string(ClaimKind value);
std::string to_string(ClaimStatus value);
std::string to_string(ClaimEntityRole value);
std::string to_string(EvidenceRole value);
std::string to_string(RunStepState value);

CaseStatus case_status_from_string(const std::string& value);
EntityType entity_type_from_string(const std::string& value);
SourceType source_type_from_string(const std::string& value);
EvidenceKind evidence_kind_from_string(const std::string& value);
ClaimKind claim_kind_from_string(const std::string& value);
ClaimStatus claim_status_from_string(const std::string& value);
ClaimEntityRole claim_entity_role_from_string(const std::string& value);
EvidenceRole evidence_role_from_string(const std::string& value);
RunStepState run_step_state_from_string(const std::string& value);

Id new_id();
std::string utc_now();
std::string lower_ascii(std::string value);
std::string trim(std::string value);

struct Case {
    Id id;
    std::string title;
    std::string purpose;
    std::string description;
    std::optional<std::string> primary_target_type;
    std::string scope;
    std::string tags_json;
    CaseStatus status{CaseStatus::Active};
    std::string created_at;
    std::string updated_at;
};

struct CaseSummary : Case {
    std::int64_t entity_count{0};
    std::int64_t claim_count{0};
};

struct Entity {
    Id id;
    Id case_id;
    EntityType type{EntityType::Other};
    std::string label;
    std::string original_value;
    std::string canonical_value;
    std::string details_json;
    std::string tags_json;
    std::string aliases_json;
    std::string created_at;
    std::string updated_at;
};

struct Source {
    Id id;
    Id case_id;
    SourceType type{SourceType::Other};
    std::string locator;
    std::string title;
    std::string accessed_at;
    std::string author;
    std::string reliability_note;
    std::string created_at;
};

struct Evidence {
    Id id;
    Id case_id;
    std::optional<Id> source_id;
    EvidenceKind kind{EvidenceKind::Text};
    std::optional<std::string> relative_path;
    std::optional<std::string> original_filename;
    std::optional<std::string> mime_type;
    std::optional<std::int64_t> byte_size;
    std::optional<std::string> sha256;
    std::string text_content;
    std::optional<std::string> source_url;
    std::string captured_at;
    std::string imported_at;
    std::string note;
    std::string quotation_location;
    std::string created_at;
};

struct Claim {
    Id id;
    Id case_id;
    std::string statement;
    ClaimKind kind{ClaimKind::Observation};
    std::optional<std::string> predicate;
    ClaimStatus status{ClaimStatus::Unverified};
    std::string reasoning;
    std::string recorded_by;
    std::string created_at;
    std::string updated_at;
};

struct ClaimEntityLink {
    Id claim_id;
    Id entity_id;
    ClaimEntityRole role{ClaimEntityRole::Context};
};

struct ClaimEvidenceLink {
    Id claim_id;
    Id evidence_id;
    EvidenceRole role{EvidenceRole::Context};
    std::string note;
};

struct Note {
    Id id;
    Id case_id;
    std::string title;
    std::string body;
    std::string created_at;
    std::string updated_at;
};

struct Technique {
    Id id;
    std::string name;
    std::string objective;
    std::string steps;
    std::string example_queries_json;
    std::string tools_links_json;
    std::string limitations;
    std::string tags_json;
    std::string created_at;
    std::string updated_at;
};

struct Playbook {
    Id id;
    std::string name;
    std::string scope;
    std::string description;
    std::string created_at;
    std::string updated_at;
};

struct PlaybookStep {
    Id id;
    Id playbook_id;
    std::optional<Id> technique_id;
    std::string name;
    std::string instructions;
    std::int64_t position{0};
};

struct PlaybookRun {
    Id id;
    Id case_id;
    std::optional<Id> playbook_id;
    std::optional<Id> entity_id;
    std::string name_snapshot;
    std::string started_at;
    std::string updated_at;
};

struct RunStep {
    Id id;
    Id run_id;
    std::optional<Id> source_step_id;
    std::int64_t position{0};
    std::string name_snapshot;
    std::string instructions_snapshot;
    RunStepState state{RunStepState::Todo};
    std::optional<std::string> completed_at;
    std::string note;
};

struct LogEvent {
    Id id;
    Id case_id;
    std::string occurred_at;
    std::string action;
    std::string object_type;
    std::optional<Id> object_id;
    std::string description;
    std::string payload_json;
};

struct SearchResult {
    std::string object_type;
    Id object_id;
    std::string title;
    std::string snippet;
};

struct IntegrityResult {
    Id evidence_id;
    bool ok{false};
    std::string message;
    std::optional<std::string> actual_sha256;
};

} // namespace evidence_trace::domain
