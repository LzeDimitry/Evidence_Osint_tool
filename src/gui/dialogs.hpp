#pragma once

#include "domain/types.hpp"

#include <QString>

#include <optional>
#include <vector>

class QWidget;

namespace evidence_trace::gui {

struct CaseForm {
    QString title;
    QString purpose;
    QString description;
    std::optional<QString> target_type;
    QString scope;
    std::vector<std::string> tags;
};

std::optional<CaseForm> case_form(QWidget* parent, const std::optional<domain::Case>& existing = std::nullopt);

struct EntityForm {
    domain::EntityType type{domain::EntityType::Other};
    QString label;
    QString original_value;
    QString description;
    std::vector<std::string> tags;
    std::vector<std::string> aliases;
};

std::optional<EntityForm> entity_form(QWidget* parent, const std::optional<domain::Entity>& existing = std::nullopt);

struct SourceForm {
    domain::SourceType type{domain::SourceType::Web};
    QString locator;
    QString title;
    QString accessed_at;
    QString author;
    QString reliability_note;
};

std::optional<SourceForm> source_form(QWidget* parent, const std::optional<domain::Source>& existing = std::nullopt);

struct EvidenceForm {
    domain::EvidenceKind kind{domain::EvidenceKind::Text};
    QString file_path;
    QString text;
    QString url;
    QString note;
    QString quotation_location;
    std::optional<domain::Id> source_id;
};

std::optional<EvidenceForm> evidence_form(QWidget* parent, const std::vector<domain::Source>& sources);

struct ClaimForm {
    bool relation{true};
    domain::Id subject_id;
    domain::Id object_id;
    domain::Id context_id;
    QString predicate;
    QString statement;
    domain::ClaimKind kind{domain::ClaimKind::Inference};
    domain::ClaimStatus status{domain::ClaimStatus::Unverified};
    QString reasoning;
};

std::optional<ClaimForm> claim_form(QWidget* parent, const std::vector<domain::Entity>& entities);

struct NoteForm {
    QString title;
    QString body;
};

std::optional<NoteForm> note_form(QWidget* parent);

struct RunForm {
    domain::Id playbook_id;
    std::optional<domain::Id> entity_id;
};

std::optional<RunForm> run_form(QWidget* parent,
                                const std::vector<domain::Playbook>& playbooks,
                                const std::vector<domain::Entity>& entities);

struct RunStepForm {
    domain::RunStepState state{domain::RunStepState::Todo};
    QString note;
};

std::optional<RunStepForm> run_step_form(QWidget* parent, const domain::RunStep& existing);

struct TechniqueForm {
    QString name;
    QString objective;
    QString steps;
    QString example_queries_json;
    QString tools_links_json;
    QString limitations;
    QString tags_json;
};

std::optional<TechniqueForm> technique_form(QWidget* parent, const std::optional<domain::Technique>& existing = std::nullopt);

struct PlaybookForm {
    QString name;
    QString scope;
    QString description;
};

std::optional<PlaybookForm> playbook_form(QWidget* parent, const std::optional<domain::Playbook>& existing = std::nullopt);

struct PlaybookStepForm {
    std::optional<domain::Id> technique_id;
    QString name;
    QString instructions;
};

std::optional<PlaybookStepForm> playbook_step_form(QWidget* parent,
                                                   const std::vector<domain::Technique>& techniques);

struct LinkEvidenceForm {
    domain::Id evidence_id;
    std::optional<QString> import_file_path;
    std::optional<domain::Id> import_source_id;
    domain::EvidenceRole role{domain::EvidenceRole::Supports};
    QString note;
};

std::optional<LinkEvidenceForm> link_evidence_form(QWidget* parent,
                                                   const std::vector<domain::Evidence>& evidence,
                                                   const std::vector<domain::Source>& sources = {},
                                                   bool include_link_note = true,
                                                   bool include_link_role = true);

std::optional<QString> explanation_form(QWidget* parent, const QString& title, const QString& prompt);

} // namespace evidence_trace::gui
