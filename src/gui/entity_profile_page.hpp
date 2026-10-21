#pragma once

#include "gui/application_context.hpp"
#include "gui/investigation_pages.hpp"

#include <QWidget>
#include <QString>
#include <vector>
#include <functional>
#include <optional>

class QLabel;
class QLineEdit;
class QComboBox;
class QTabWidget;
class QScrollArea;
class QPushButton;
class QTableWidget;
class QPlainTextEdit;
class QFrame;
class QLayout;
class QBoxLayout;
class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;

namespace evidence_trace::gui {

class StateTableWidget;
class ExpandableText;

class EntityProfilePage : public CasePage {
public:
    explicit EntityProfilePage(ApplicationContext& context, const domain::Id& case_id, QWidget* parent = nullptr);

    void refresh() override;
    void set_entity(const domain::Id& entity_id, int source_page = 2);
    const domain::Id& current_entity_id() const { return entity_id_; }
    QString current_entity_label() const;

    // Navigation callbacks
    std::function<void()> on_back_to_graph;
    std::function<void()> on_back_to_entities;
    std::function<void()> on_back_to_overview;
    std::function<void(const domain::Id&)> on_open_entity_profile;
    std::function<void(const domain::Id&)> on_view_in_graph;
    std::function<void(const domain::Id&)> on_view_claim_in_graph;
    std::function<void()> on_entity_modified;

private:
    void update_navigation_buttons();
    void edit_profile();
    void edit_description();
    void edit_attributes();
    void add_connection();
    void add_claim();
    void add_note();
    void change_relation_status(const domain::Id& claim_id);
    void change_claim_status(const domain::Id& claim_id);
    void link_claim_evidence(const domain::Id& claim_id);
    void add_image_from_file();
    void paste_image_from_clipboard();
    void remove_selected_image();
    void view_image_fullscreen(const QString& relative_path);

    domain::Id entity_id_;
    int source_page_{2}; // 2 = Graph, 1 = Entities, 0 = Overview
    std::vector<domain::Id> history_stack_;

    // Navigation Bar
    QPushButton* back_button_{nullptr};
    QLabel* breadcrumb_label_{nullptr};
    QPushButton* view_in_graph_button_{nullptr};
    QPushButton* edit_profile_button_{nullptr};
    QPushButton* connect_button_{nullptr};
    QPushButton* add_claim_button_{nullptr};
    QPushButton* add_note_button_{nullptr};

    // Hero Identity Card
    QFrame* hero_card_{nullptr};
    QLabel* photo_label_{nullptr};
    QLabel* type_badge_{nullptr};
    QLabel* title_label_{nullptr};
    QLabel* original_value_label_{nullptr};
    QLabel* duplicate_warning_{nullptr};
    QWidget* tags_widget_{nullptr};
    QLabel* aliases_label_{nullptr};

    QLabel* relation_count_chip_{nullptr};
    QLabel* claim_count_chip_{nullptr};
    QLabel* evidence_count_chip_{nullptr};
    QLabel* note_count_chip_{nullptr};
    QLabel* created_time_label_{nullptr};
    QLabel* updated_time_label_{nullptr};

    // Body Tabs
    QTabWidget* tabs_{nullptr};

    // Tab 1: Overview & Background
    QWidget* overview_tab_{nullptr};
    QFrame* description_card_{nullptr};
    QLabel* description_label_{nullptr};
    QPushButton* edit_desc_button_{nullptr};
    QWidget* desc_empty_widget_{nullptr};

    QFrame* attributes_card_{nullptr};
    QLabel* attributes_card_title_{nullptr};
    QGridLayout* attributes_grid_{nullptr};
    QWidget* attributes_content_{nullptr};
    QPushButton* edit_attrs_button_{nullptr};
    QWidget* attrs_empty_widget_{nullptr};

    QFrame* media_preview_card_{nullptr};
    QHBoxLayout* media_preview_layout_{nullptr};

    // Tab 2: Connections & Relations
    StateTableWidget* relations_table_{nullptr};
    QLineEdit* relations_search_{nullptr};
    QComboBox* relations_direction_filter_{nullptr};

    // Tab 3: Claims & Hypotheses
    StateTableWidget* claims_table_{nullptr};
    QLineEdit* claims_search_{nullptr};
    QComboBox* claims_status_filter_{nullptr};

    // Tab 4: Evidence & Sources
    StateTableWidget* evidence_table_{nullptr};

    // Tab 5: Media Gallery
    QWidget* media_gallery_grid_{nullptr};
    QGridLayout* media_gallery_layout_{nullptr};
    std::vector<std::string> current_images_;

    // Tab 6: Notes
    QWidget* notes_list_{nullptr};
    QVBoxLayout* notes_layout_{nullptr};

    // Tab 7: History
    StateTableWidget* history_table_{nullptr};
};

} // namespace evidence_trace::gui
