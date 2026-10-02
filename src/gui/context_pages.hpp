#pragma once

#include "gui/investigation_pages.hpp"

#include <QWidget>

class QLabel;
class QComboBox;
class QLineEdit;
class QTableWidget;
class QBoxLayout;
class QResizeEvent;

namespace evidence_trace::gui {

class StateTableWidget;

class NotesPage : public CasePage {
public:
    explicit NotesPage(ApplicationContext& context, const domain::Id& case_id, QWidget* parent = nullptr);
    void refresh() override;
    void show_note(const domain::Id& note_id);

private:
    void update_note();
    void copy_selected_note();
    void add_correction_note();
    void copy_selected_activity();
    domain::Id selected_note_id() const;
    StateTableWidget* notes_{nullptr};
    StateTableWidget* activity_{nullptr};
    QLabel* note_title_{nullptr};
    QLabel* note_body_{nullptr};
    domain::Id focus_note_id_;
};

class RunsPage : public CasePage {
public:
    explicit RunsPage(ApplicationContext& context, const domain::Id& case_id, QWidget* parent = nullptr);
    void refresh() override;
    void show_step(const domain::Id& step_id);

private:
    void update_run();
    void update_selected_step();
    void add_step_to_selected_run();
    void link_selected_step_evidence();
    domain::Id selected_run_id() const;
    domain::Id selected_step_id() const;
    StateTableWidget* runs_{nullptr};
    StateTableWidget* steps_{nullptr};
    StateTableWidget* activity_{nullptr};
    QLabel* run_title_{nullptr};
    QLabel* run_meta_{nullptr};
    domain::Id focus_run_id_;
    domain::Id focus_step_id_;
};

class ExportPage : public CasePage {
public:
    explicit ExportPage(ApplicationContext& context, const domain::Id& case_id, QWidget* parent = nullptr);
    void refresh() override;

private:
    void resizeEvent(QResizeEvent* event) override;
    QLabel* summary_{nullptr};
    QLabel* archive_summary_{nullptr};
    QBoxLayout* columns_{nullptr};
};

} // namespace evidence_trace::gui
