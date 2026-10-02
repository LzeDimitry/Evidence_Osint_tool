#pragma once

#include "database/sqlite_database.hpp"
#include "domain/types.hpp"
#include "services/event_logger.hpp"
#include "storage/attachment_store.hpp"

#include <filesystem>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

namespace evidence_trace::services {

using EntityLinkInput = std::pair<domain::Id, domain::ClaimEntityRole>;
using EvidenceLinkInput = std::tuple<domain::Id, domain::EvidenceRole, std::string>;

class InvestigationService {
public:
    InvestigationService(database::Database& database,
                         storage::AttachmentStore& attachment_store,
                         EventLogger& logger)
        : database_(database), attachment_store_(attachment_store), logger_(logger) {}

    domain::Entity create_entity(const domain::Id& case_id,
                                 domain::EntityType type,
                                 const std::string& label,
                                 const std::string& original_value = {},
                                 const std::string& details_json = "{}",
                                 const std::vector<std::string>& tags = {},
                                 const std::vector<std::string>& aliases = {});
    void update_entity(const domain::Id& id,
                       const std::string& label,
                       const std::string& original_value,
                       const std::string& details_json,
                       const std::vector<std::string>& tags,
                       const std::vector<std::string>& aliases);
    void delete_entity(const domain::Id& id, bool explicit_confirmation);
    std::vector<domain::Entity> list_entities(const domain::Id& case_id) const;
    domain::Entity get_entity(const domain::Id& id) const;
    std::vector<domain::Entity> possible_duplicates(const domain::Id& case_id,
                                                    domain::EntityType type,
                                                    const std::string& original_value) const;
    std::vector<domain::Claim> claims_for_entity(const domain::Id& entity_id) const;

    domain::Source create_source(const domain::Id& case_id,
                                 domain::SourceType type,
                                 const std::string& locator,
                                 const std::string& title = {},
                                 const std::string& accessed_at = {},
                                 const std::string& author = {},
                                 const std::string& reliability_note = {});
    std::vector<domain::Source> list_sources(const domain::Id& case_id) const;
    domain::Source get_source(const domain::Id& id) const;
    void delete_source(const domain::Id& id, bool explicit_confirmation);

    domain::Evidence add_file_evidence(const domain::Id& case_id,
                                       const std::filesystem::path& original_file,
                                       const std::optional<domain::Id>& source_id = std::nullopt,
                                       const std::optional<std::string>& source_url = std::nullopt,
                                       const std::string& note = {},
                                       const std::optional<std::string>& mime_type = std::nullopt,
                                       const std::string& captured_at = {});
    domain::Evidence add_text_evidence(const domain::Id& case_id,
                                       const std::string& text,
                                       const std::optional<domain::Id>& source_id = std::nullopt,
                                       const std::optional<std::string>& source_url = std::nullopt,
                                       const std::string& note = {},
                                       const std::string& quotation_location = {},
                                       const std::string& captured_at = {});
    domain::Evidence add_url_evidence(const domain::Id& case_id,
                                      const std::string& url,
                                      const std::optional<domain::Id>& source_id = std::nullopt,
                                      const std::string& note = {},
                                      const std::string& captured_at = {});
    domain::Evidence get_evidence(const domain::Id& id) const;
    std::vector<domain::Evidence> list_evidence(const domain::Id& case_id) const;
    domain::IntegrityResult verify_evidence(const domain::Id& id) const;
    std::vector<domain::Claim> claims_for_evidence(const domain::Id& evidence_id) const;
    void delete_evidence(const domain::Id& id, bool explicit_confirmation);

    domain::Claim create_claim(const domain::Id& case_id,
                               const std::string& statement,
                               domain::ClaimKind kind,
                               const std::vector<EntityLinkInput>& entities,
                               const std::optional<std::string>& predicate = std::nullopt,
                               domain::ClaimStatus status = domain::ClaimStatus::Unverified,
                               const std::string& reasoning = {},
                               const std::string& recorded_by = {},
                               const std::vector<EvidenceLinkInput>& evidence_links = {});
    domain::Claim create_relation(const domain::Id& case_id,
                                  const domain::Id& subject_entity_id,
                                  const std::string& predicate,
                                  const domain::Id& object_entity_id,
                                  const std::string& statement = {},
                                  domain::ClaimStatus status = domain::ClaimStatus::Unverified,
                                  const std::string& reasoning = {},
                                  const std::string& recorded_by = {},
                                  const std::vector<EvidenceLinkInput>& evidence_links = {},
                                  domain::ClaimKind kind = domain::ClaimKind::Inference);
    domain::Claim get_claim(const domain::Id& id) const;
    std::vector<domain::Claim> list_claims(const domain::Id& case_id) const;
    std::vector<domain::ClaimEntityLink> claim_entities(const domain::Id& claim_id) const;
    std::vector<domain::ClaimEvidenceLink> claim_evidence(const domain::Id& claim_id) const;
    void link_evidence(const domain::Id& claim_id,
                       const domain::Id& evidence_id,
                       domain::EvidenceRole role,
                       const std::string& note = {});
    void unlink_evidence(const domain::Id& claim_id, const domain::Id& evidence_id);
    void change_claim_status(const domain::Id& claim_id,
                             domain::ClaimStatus status,
                             const std::string& explanation,
                             const std::optional<std::string>& new_reasoning = std::nullopt);
    void change_claim_statement(const domain::Id& claim_id, const std::string& statement);
    void delete_claim(const domain::Id& id, bool explicit_confirmation);

    domain::Note add_note(const domain::Id& case_id,
                          const std::string& body,
                          const std::string& title = {},
                          const std::vector<domain::Id>& entity_ids = {},
                          const std::vector<domain::Id>& evidence_ids = {});
    std::vector<domain::Note> list_notes(const domain::Id& case_id) const;
    std::vector<domain::SearchResult> search(const domain::Id& case_id,
                                             const std::string& query) const;
    std::vector<domain::LogEvent> activity(const domain::Id& case_id) const;

private:
    void ensure_case(const domain::Id& case_id) const;
    domain::Entity get_entity_internal(const domain::Id& id) const;
    domain::Source get_source_internal(const domain::Id& id) const;
    domain::Evidence get_evidence_internal(const domain::Id& id) const;
    domain::Claim get_claim_internal(const domain::Id& id) const;
    void ensure_source_in_case(const std::optional<domain::Id>& source_id, const domain::Id& case_id) const;
    void ensure_entity_links_in_case(const domain::Id& case_id,
                                     const std::vector<EntityLinkInput>& links) const;
    void ensure_evidence_links_in_case(const domain::Id& case_id,
                                       const std::vector<EvidenceLinkInput>& links) const;
    static std::string canonical_value(domain::EntityType type, const std::string& original_value);
    static std::string tags_to_json(const std::vector<std::string>& tags);
    static domain::Entity read_entity(database::Statement& statement);
    static domain::Source read_source(database::Statement& statement);
    static domain::Evidence read_evidence(database::Statement& statement);
    static domain::Claim read_claim(database::Statement& statement);
    static domain::Note read_note(database::Statement& statement);

    database::Database& database_;
    storage::AttachmentStore& attachment_store_;
    EventLogger& logger_;
};

} // namespace evidence_trace::services
