#pragma once

#include "gui/application_context.hpp"

#include <QWidget>

#include <functional>

class QLabel;
class QLineEdit;
class QComboBox;
class QTableWidget;
class QTextEdit;
class QTabWidget;
class QScrollArea;
class QPushButton;
class QBoxLayout;

namespace evidence_trace::gui {

class StateTableWidget;
class ExpandableText;

class CasePage : public QWidget {
public:
    explicit CasePage(ApplicationContext& context, domain::Id case_id, QWidget* parent = nullptr)
        : QWidget(parent), context_(context), case_id_(std::move(case_id)) {}
    virtual void refresh() = 0;
    const domain::Id& case_id() const { return case_id_; }

protected:
    ApplicationContext& context_;
    domain::Id case_id_;
};

class CaseOverviewPage : public CasePage {
public:
    explicit CaseOverviewPage(ApplicationContext& context, const domain::Id& case_id, QWidget* parent = nullptr);
    void refresh() override;
    std::function<void()> on_add_entity;
    std::function<void()> on_add_claim;
    std::function<void()> on_add_evidence;
    std::function<void(const domain::Id&)> on_show_claim;

private:
    ExpandableText* case_purpose_{nullptr};
    QLabel* entity_count_{nullptr};
    QLabel* claim_count_{nullptr};
    QLabel* evidence_count_{nullptr};
    QLabel* source_count_{nullptr};
    StateTableWidget* activity_{nullptr};
    StateTableWidget* unresolved_{nullptr};
};

class EntitiesPage : public CasePage {
public:
    explicit EntitiesPage(ApplicationContext& context, const domain::Id& case_id, QWidget* parent = nullptr);
    void refresh() override;
    void add_entity();
    void show_entity(const domain::Id& entity_id);
    std::function<void(const domain::Id&)> on_show_related_claims;

private:
    void update_detail();
    void edit_selected();
    void show_related_selected();
    void delete_selected();
    domain::Id selected_id() const;
    QLineEdit* search_{nullptr};
    QComboBox* type_filter_{nullptr};
    StateTableWidget* table_{nullptr};
    QScrollArea* detail_scroll_{nullptr};
    QLabel* detail_title_{nullptr};
    QLabel* detail_meta_{nullptr};
    QLabel* detail_description_{nullptr};
    QLabel* detail_aliases_{nullptr};
    QWidget* detail_tags_{nullptr};
    domain::Id focus_entity_id_;
    StateTableWidget* linked_claims_{nullptr};
    StateTableWidget* linked_evidence_{nullptr};
    StateTableWidget* history_{nullptr};
};

class ClaimsPage : public CasePage {
public:
    explicit ClaimsPage(ApplicationContext& context, const domain::Id& case_id, QWidget* parent = nullptr);
    void refresh() override;
    void add_claim();
    void show_related_to_entity(const domain::Id& entity_id);
    void show_claim(const domain::Id& claim_id);
    std::function<void(const domain::Id&)> on_show_entity;
    std::function<void(const domain::Id&)> on_show_claim;
    std::function<void()> on_add_entity;

private:
    void update_detail();
    void edit_statement();
    void change_selected_status();
    void link_selected_evidence();
    void delete_selected();
    domain::Id selected_id() const;
    QComboBox* status_filter_{nullptr};
    QComboBox* type_filter_{nullptr};
    QLineEdit* search_{nullptr};
    QWidget* graph_{nullptr};
    StateTableWidget* table_{nullptr};
    QScrollArea* detail_scroll_{nullptr};
    QLabel* claims_count_{nullptr};
    QLabel* detail_title_{nullptr};
    QLabel* detail_meta_{nullptr};
    QLabel* detail_reasoning_{nullptr};
    QComboBox* detail_kind_{nullptr};
    QComboBox* detail_status_{nullptr};
    QLabel* detail_subject_icon_{nullptr};
    QLabel* detail_subject_{nullptr};
    QLabel* detail_subject_type_{nullptr};
    QLabel* detail_object_icon_{nullptr};
    QLabel* detail_object_{nullptr};
    QLabel* detail_object_type_{nullptr};
    QLabel* detail_added_{nullptr};
    QWidget* supporting_list_{nullptr};
    QWidget* contradicting_list_{nullptr};
    QLabel* supporting_count_{nullptr};
    QLabel* contradicting_count_{nullptr};
    QPushButton* clear_entity_filter_{nullptr};
    domain::Id related_entity_id_;
    domain::Id focus_claim_id_;
    StateTableWidget* entities_{nullptr};
    StateTableWidget* evidence_{nullptr};
    StateTableWidget* history_{nullptr};
};

class EvidencePage : public CasePage {
public:
    explicit EvidencePage(ApplicationContext& context, const domain::Id& case_id, QWidget* parent = nullptr);
    void refresh() override;
    void add_evidence();
    void show_sources();
    void show_evidence(const domain::Id& evidence_id);
    void show_source(const domain::Id& source_id);

private:
    void update_evidence_detail();
    void update_source_detail();
    void verify_selected();
    void preview_selected();
    void delete_selected();
    domain::Id selected_evidence_id() const;
    domain::Id selected_source_id() const;
    StateTableWidget* evidence_table_{nullptr};
    StateTableWidget* sources_table_{nullptr};
    QTabWidget* tabs_{nullptr};
    QScrollArea* detail_scroll_{nullptr};
    QLabel* detail_title_{nullptr};
    QLabel* detail_id_{nullptr};
    QLabel* detail_body_{nullptr};
    QLabel* detail_preview_{nullptr};
    QLabel* detail_links_label_{nullptr};
    QPushButton* verify_button_{nullptr};
    QPushButton* open_button_{nullptr};
    QPushButton* delete_button_{nullptr};
    domain::Id focus_evidence_id_;
    domain::Id focus_source_id_;
    StateTableWidget* linked_claims_{nullptr};
};

} // namespace evidence_trace::gui
