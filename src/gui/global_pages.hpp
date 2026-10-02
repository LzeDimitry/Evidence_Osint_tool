#pragma once

#include "gui/application_context.hpp"

#include <QWidget>

#include <functional>

class QLineEdit;
class QTableWidget;
class QLabel;
class QPushButton;
class QComboBox;
class QResizeEvent;
class QBoxLayout;

namespace evidence_trace::gui {

class StateTableWidget;
class ExpandableText;

class CasesPage : public QWidget {
public:
    explicit CasesPage(ApplicationContext& context, QWidget* parent = nullptr);

    void refresh();
    void set_search(const QString& text);
    std::function<void(const domain::Id&)> on_open_case;

private:
    void resizeEvent(QResizeEvent* event) override;
    void resize_case_columns();
    void open_selected();
    void edit_selected();
    void archive_selected();
    void restore_selected();
    void delete_selected();
    void update_detail();
    void resize_case_preview_rows();
    domain::Id selected_id() const;

    ApplicationContext& context_;
    QLineEdit* search_{nullptr};
    StateTableWidget* table_{nullptr};
    QLabel* detail_title_{nullptr};
    QWidget* detail_pane_{nullptr};
    QLabel* detail_id_{nullptr};
    ExpandableText* detail_body_{nullptr};
    ExpandableText* detail_scope_{nullptr};
    QLabel* detail_tags_{nullptr};
    QWidget* detail_tags_widget_{nullptr};
    QWidget* detail_activity_{nullptr};
    QLabel* detail_unresolved_{nullptr};
    QLabel* detail_entities_count_{nullptr};
    QLabel* detail_claims_count_{nullptr};
    QLabel* detail_status_value_{nullptr};
    QLabel* detail_updated_value_{nullptr};
    QPushButton* archive_button_{nullptr};
    QPushButton* restore_button_{nullptr};
    QPushButton* all_filter_button_{nullptr};
    QPushButton* active_filter_button_{nullptr};
    QPushButton* archived_filter_button_{nullptr};
    QComboBox* status_filter_{nullptr};
};

class PlaybooksPage : public QWidget {
public:
    explicit PlaybooksPage(ApplicationContext& context, QWidget* parent = nullptr);

    void refresh();

private:
    void update_playbook_detail();
    void update_technique_detail();
    void move_selected_step(int delta);
    void edit_selected_playbook();
    void edit_selected_technique();
    void add_step_to_selected_playbook();
    void delete_selected_playbook();
    void delete_selected_technique();
    ApplicationContext& context_;
    StateTableWidget* playbooks_{nullptr};
    StateTableWidget* techniques_{nullptr};
    StateTableWidget* steps_{nullptr};
    QLabel* playbook_title_{nullptr};
    QLabel* playbook_description_{nullptr};
    QLabel* technique_title_{nullptr};
    QLabel* technique_body_{nullptr};
};

class SettingsPage : public QWidget {
public:
    explicit SettingsPage(ApplicationContext& context, QWidget* parent = nullptr);

    std::function<void(const std::filesystem::path&)> on_data_directory_requested;
    std::function<void(bool)> on_navigation_glow_changed;

private:
    void resizeEvent(QResizeEvent* event) override;
    ApplicationContext& context_;
    QLineEdit* data_directory_{nullptr};
    QBoxLayout* columns_{nullptr};
};

} // namespace evidence_trace::gui
