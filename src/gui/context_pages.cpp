#include "gui/context_pages.hpp"

#include "gui/dialogs.hpp"
#include "gui/gui_helpers.hpp"

#include <QApplication>
#include <QBoxLayout>
#include <QComboBox>
#include <QClipboard>
#include <QFileDialog>
#include <QFrame>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMenu>
#include <QPushButton>
#include <QSplitter>
#include <QScrollArea>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <unordered_map>

namespace evidence_trace::gui {

namespace {

QTableWidgetItem* cell(const QString& text, const QVariant& data = {}) { auto* result = new QTableWidgetItem(text); result->setToolTip(text); result->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter); if (data.isValid()) result->setData(Qt::UserRole, data); return result; }

QWidget* header_with_actions(const QString& title, const QString& subtitle, const QList<QPushButton*>& actions) {
    auto* row = new QWidget;
    auto* layout = new QBoxLayout(QBoxLayout::LeftToRight, row);
    layout->setContentsMargins(0, 0, 0, 10);
    auto* copy = new QVBoxLayout;
    copy->setContentsMargins(0, 0, 0, 0);
    copy->addWidget(heading(title));
    copy->addWidget(muted(subtitle));
    layout->addLayout(copy, 1);
    if (!actions.isEmpty()) {
        auto* action_strip = new QWidget;
        auto* action_layout = new QHBoxLayout(action_strip);
        action_layout->setContentsMargins(0, 0, 0, 0);
        action_layout->setSpacing(7);
        for (auto* action : actions) action_layout->addWidget(action);
        action_strip->adjustSize();
        action_strip->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        layout->addWidget(action_strip, 0, Qt::AlignBottom);
        observe_resize(row, [row, layout, action_layout](int width) {
            const auto narrow = width < 1050;
            layout->setDirection(narrow ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
            action_layout->setDirection(width < 620 ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
            layout->setAlignment(narrow ? Qt::AlignLeft : Qt::Alignment());
            row->setMinimumHeight(narrow ? 128 : 0);
            row->updateGeometry();
        });
    }
    return row;
}

QString run_target(const std::optional<domain::Id>& entity_id, const std::unordered_map<domain::Id, domain::Entity>& entities) { if (!entity_id) return QStringLiteral("No target entity"); const auto found = entities.find(*entity_id); return found == entities.end() ? shorten_id(*entity_id) : q(found->second.label); }

QString evidence_name(const domain::Evidence& value) {
    if (value.kind == domain::EvidenceKind::File) return q(value.original_filename.value_or("preserved file"));
    if (value.kind == domain::EvidenceKind::Url) return q(value.source_url.value_or("external URL"));
    return value.text_content.empty() ? QStringLiteral("text quotation") : q(value.text_content.substr(0, 70));
}

} // namespace

NotesPage::NotesPage(ApplicationContext& context, const domain::Id& case_id, QWidget* parent) : CasePage(context, case_id, parent) {
    auto* root = new QVBoxLayout(this); root->setContentsMargins(28, 24, 28, 24); root->setSpacing(16); auto* add = button(QStringLiteral("＋ Add manual note"), true); root->addWidget(header_with_actions(QStringLiteral("Notes & Log"), QStringLiteral("Preserve how you found something, including failed leads, unsuccessful checks, and decisions."), {add})); auto* tabs = new QTabWidget; auto* notes_page = new QWidget; auto* notes_layout = new QVBoxLayout(notes_page); notes_layout->setContentsMargins(0, 0, 0, 0); notes_ = table_with_empty_state(); configure_table(notes_, {"Title", "Note", "Updated"}); set_empty_state(notes_, QStringLiteral("No investigation notes yet"), QStringLiteral("Record the searches, observations, and failed leads that make this case reproducible.")); notes_->setColumnWidth(0, 220); notes_->setColumnWidth(1, 400); notes_->setColumnWidth(2, 180); auto* notes_split = responsive_splitter(); notes_split->addWidget(notes_); auto* detail = card(); auto* detail_layout = new QVBoxLayout(detail); detail_layout->setContentsMargins(24, 22, 24, 22); note_title_ = heading(QStringLiteral("Select a note"), 21); note_body_ = muted(QStringLiteral("Manual notes are immutable history entries; corrections are added as new notes.")); detail_layout->addWidget(note_title_); detail_layout->addWidget(note_body_); detail_layout->addStretch(); notes_split->addWidget(detail); notes_layout->addWidget(notes_split); tabs->addTab(notes_page, QStringLiteral("Manual notes")); auto* activity_page = new QWidget; auto* activity_layout = new QVBoxLayout(activity_page); activity_ = table_with_empty_state(activity_page); configure_table(activity_, {"When", "Action", "Object", "Description"}); set_empty_state(activity_, QStringLiteral("No activity yet"), QStringLiteral("The case history will appear here as records are created or changed.")); activity_->setColumnWidth(0, 180); activity_->setColumnWidth(1, 140); activity_->setColumnWidth(2, 140); activity_->setColumnWidth(3, 420); activity_layout->addWidget(activity_); tabs->addTab(activity_page, QStringLiteral("Activity log")); root->addWidget(tabs, 1); connect(add, &QPushButton::clicked, this, [this] { const auto form = note_form(this); if (!form) return; try { context_.investigation.add_note(case_id_, s(form->body), s(form->title)); refresh(); } catch (const std::exception& error) { show_error(this, error); } }); connect(notes_, &QTableWidget::itemSelectionChanged, this, &NotesPage::update_note);
    notes_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(notes_, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& position) {
        const auto index = notes_->indexAt(position);
        if (!index.isValid()) return;
        notes_->selectRow(index.row());
        QMenu menu(this);
        auto* details = menu.addAction(QStringLiteral("View note"));
        auto* copy = menu.addAction(QStringLiteral("Copy note text"));
        auto* correction = menu.addAction(QStringLiteral("Add correction note"));
        const auto action = menu.exec(notes_->viewport()->mapToGlobal(position));
        if (action == details) update_note();
        else if (action == copy) copy_selected_note();
        else if (action == correction) add_correction_note();
    });
    activity_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(activity_, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& position) {
        const auto index = activity_->indexAt(position);
        if (!index.isValid()) return;
        activity_->selectRow(index.row());
        QMenu menu(this);
        auto* copy = menu.addAction(QStringLiteral("Copy event details"));
        if (menu.exec(activity_->viewport()->mapToGlobal(position)) == copy) copy_selected_activity();
    });
    refresh();
}

domain::Id NotesPage::selected_note_id() const { const auto row = notes_->currentRow(); return row < 0 ? domain::Id{} : s(notes_->item(row, 0)->data(Qt::UserRole).toString()); }

void NotesPage::copy_selected_note() {
    const auto id = selected_note_id();
    if (id.empty()) return;
    try {
        for (const auto& note : context_.investigation.list_notes(case_id_)) {
            if (note.id == id) {
                QApplication::clipboard()->setText(QStringLiteral("%1\n\n%2").arg(note.title.empty() ? QStringLiteral("Untitled note") : q(note.title), q(note.body)));
                return;
            }
        }
    } catch (const std::exception& error) { show_error(this, error); }
}

void NotesPage::add_correction_note() {
    const auto form = note_form(this);
    if (!form) return;
    try {
        context_.investigation.add_note(case_id_, s(form->body), s(form->title));
        refresh();
    } catch (const std::exception& error) { show_error(this, error); }
}

void NotesPage::copy_selected_activity() {
    const auto row = activity_->currentRow();
    if (row < 0) return;
    QStringList values;
    for (int column = 0; column < activity_->columnCount(); ++column) {
        if (auto* cell = activity_->item(row, column)) values << cell->text();
    }
    QApplication::clipboard()->setText(values.join(QStringLiteral("\n")));
}

void NotesPage::refresh() {
    notes_->setRowCount(0);
    for (const auto& note : context_.investigation.list_notes(case_id_)) {
        const auto row = notes_->rowCount();
        notes_->insertRow(row);
        notes_->setItem(row, 0, cell(note.title.empty() ? QStringLiteral("Untitled note") : q(note.title), q(note.id)));
        // Keep the complete note available in the list. Qt wraps it to the
        // column width and the selected inspector shows the same full text.
        auto* body = cell(q(note.body));
        notes_->setItem(row, 1, body);
        notes_->setItem(row, 2, cell(display_time(note.updated_at)));
    }
    activity_->setRowCount(0);
    for (const auto& event : context_.investigation.activity(case_id_)) {
        const auto row = activity_->rowCount();
        activity_->insertRow(row);
        activity_->setItem(row, 0, cell(display_time(event.occurred_at)));
        activity_->setItem(row, 1, cell(enum_label(event.action)));
        activity_->setItem(row, 2, cell(enum_label(event.object_type)));
        activity_->setItem(row, 3, cell(q(event.description)));
    }
    bool selected_focus = false;
    if (!focus_note_id_.empty()) {
        for (int row = 0; row < notes_->rowCount(); ++row) {
            if (notes_->item(row, 0) && s(notes_->item(row, 0)->data(Qt::UserRole).toString()) == focus_note_id_) {
                notes_->selectRow(row);
                selected_focus = true;
                break;
            }
        }
    }
    if (!selected_focus && notes_->rowCount() > 0) notes_->selectRow(0);
    focus_note_id_.clear();
    update_note();
}

void NotesPage::show_note(const domain::Id& note_id) { focus_note_id_ = note_id; refresh(); }

void NotesPage::update_note() { const auto id = selected_note_id(); if (id.empty()) { note_title_->setText(QStringLiteral("Select a note")); note_body_->setText(QStringLiteral("Manual notes are immutable history entries; corrections are added as new notes.")); return; } try { const auto values = context_.investigation.list_notes(case_id_); for (const auto& value : values) if (value.id == id) { note_title_->setText(value.title.empty() ? QStringLiteral("Untitled note") : q(value.title)); note_body_->setText(QStringLiteral("%1\n\nCreated %2\nUpdated %3").arg(q(value.body), display_time(value.created_at), display_time(value.updated_at))); } } catch (const std::exception& error) { show_error(this, error); } }

RunsPage::RunsPage(ApplicationContext& context, const domain::Id& case_id, QWidget* parent) : CasePage(context, case_id, parent) {
    auto* root = new QVBoxLayout(this); root->setContentsMargins(14, 0, 14, 18); root->setSpacing(10); auto* start = button(QStringLiteral("＋ Start run"), true); auto* add_step = button(QStringLiteral("＋ Add step")); auto* update = button(QStringLiteral("Update step")); auto* link = button(QStringLiteral("Link evidence")); root->addWidget(header_with_actions(QStringLiteral("Playbook Runs"), QStringLiteral("Every run is a case-specific snapshot with an auditable checklist and activity log."), {start, add_step, update, link})); auto* split = responsive_splitter(nullptr, 950); split->setChildrenCollapsible(false);
    auto* run_column = card(); auto* run_layout = new QVBoxLayout(run_column); run_column->setMinimumWidth(0); run_layout->setContentsMargins(12, 12, 12, 12); run_layout->addWidget(new QLabel(QStringLiteral("Runs"))); runs_ = table_with_empty_state(); configure_table(runs_, {"Run", "Started", "Target entity"}); set_empty_state(runs_, QStringLiteral("No playbook runs yet"), QStringLiteral("Start a run from a global playbook.")); run_layout->addWidget(runs_, 1); split->addWidget(run_column);
    auto* detail = card(); detail->setMinimumWidth(0); auto* detail_layout = new QVBoxLayout(detail); detail_layout->setContentsMargins(14, 14, 14, 14); run_title_ = heading(QStringLiteral("Select a run"), 18); run_meta_ = muted(QStringLiteral("Start a global playbook to create a historical snapshot.")); detail_layout->addWidget(run_title_); detail_layout->addWidget(run_meta_); steps_ = table_with_empty_state(); configure_table(steps_, {"State", "Step", "Snapshot instruction", "Result note", "Evidence"}); set_empty_state(steps_, QStringLiteral("No steps in this run"), QStringLiteral("The run will show its snapshotted instructions and evidence links here.")); detail_layout->addWidget(steps_, 1); split->addWidget(detail);
    auto* log = card(); log->setMinimumWidth(0); auto* log_layout = new QVBoxLayout(log); log_layout->setContentsMargins(14, 14, 14, 14); auto* log_header = new QHBoxLayout; log_header->addWidget(new QLabel(QStringLiteral("Investigation log"))); log_layout->addLayout(log_header); activity_ = table_with_empty_state(); configure_table(activity_, {"When", "Activity"}); set_empty_state(activity_, QStringLiteral("No activity yet"), QStringLiteral("Changes made during this investigation will appear here.")); log_layout->addWidget(activity_, 1); split->addWidget(log); split->setStretchFactor(0, 1); split->setStretchFactor(1, 2); split->setStretchFactor(2, 1); split->setSizes({300, 600, 300}); root->addWidget(split, 1); QTimer::singleShot(0, split, [split] { if (split->orientation() == Qt::Horizontal) split->setSizes({300, 600, 300}); });
    connect(start, &QPushButton::clicked, this, [this] { const auto form = run_form(this, context_.playbooks.list_playbooks(), context_.investigation.list_entities(case_id_)); if (!form) return; try { context_.playbooks.start_run(case_id_, form->playbook_id, form->entity_id); refresh(); } catch (const std::exception& error) { show_error(this, error); } }); connect(runs_, &QTableWidget::itemSelectionChanged, this, &RunsPage::update_run); connect(update, &QPushButton::clicked, this, [this] { const auto run_id = selected_run_id(); const auto row = steps_->currentRow(); if (run_id.empty() || row < 0) return; try { const auto values = context_.playbooks.list_run_steps(run_id); if (row >= static_cast<int>(values.size())) return; const auto form = run_step_form(this, values[static_cast<std::size_t>(row)]); if (!form) return; context_.playbooks.update_run_step(values[static_cast<std::size_t>(row)].id, form->state, s(form->note)); refresh(); } catch (const std::exception& error) { show_error(this, error); } }); connect(add_step, &QPushButton::clicked, this, [this] { const auto run_id = selected_run_id(); if (run_id.empty()) return; const auto form = playbook_step_form(this, context_.playbooks.list_techniques()); if (!form) return; try { context_.playbooks.add_run_step(run_id, s(form->name), s(form->instructions)); refresh(); } catch (const std::exception& error) { show_error(this, error); } }); connect(link, &QPushButton::clicked, this, [this] {
        const auto row = steps_->currentRow(); const auto run_id = selected_run_id();
        if (run_id.empty() || row < 0) return;
        try {
            const auto values = context_.playbooks.list_run_steps(run_id);
            if (row >= static_cast<int>(values.size())) return;
            const auto form = link_evidence_form(this, context_.investigation.list_evidence(case_id_), context_.investigation.list_sources(case_id_), false, false);
            if (!form) return;
            auto evidence_id = form->evidence_id;
            domain::Id imported_id;
            if (form->import_file_path) {
                const auto imported = context_.investigation.add_file_evidence(case_id_, std::filesystem::path(s(*form->import_file_path)), form->import_source_id);
                imported_id = imported.id;
                evidence_id = imported.id;
            }
            try {
                context_.playbooks.link_step_evidence(values[static_cast<std::size_t>(row)].id, evidence_id);
            } catch (...) {
                if (!imported_id.empty()) {
                    try { context_.investigation.delete_evidence(imported_id, true); } catch (...) { }
                }
                throw;
            }
            refresh();
        } catch (const std::exception& error) { show_error(this, error); }
    });
    runs_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(runs_, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& position) {
        const auto index = runs_->indexAt(position);
        if (!index.isValid()) return;
        runs_->selectRow(index.row());
        QMenu menu(this);
        auto* details = menu.addAction(QStringLiteral("Open run details"));
        auto* add_step = menu.addAction(QStringLiteral("Add step to this run"));
        const auto action = menu.exec(runs_->viewport()->mapToGlobal(position));
        if (action == details) update_run();
        else if (action == add_step) add_step_to_selected_run();
    });
    steps_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(steps_, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& position) {
        const auto index = steps_->indexAt(position);
        if (!index.isValid()) return;
        steps_->selectRow(index.row());
        QMenu menu(this);
        auto* update = menu.addAction(QStringLiteral("Update step"));
        auto* link = menu.addAction(QStringLiteral("Link evidence"));
        if (auto* action = menu.exec(steps_->viewport()->mapToGlobal(position)); action == update) update_selected_step(); else if (action == link) link_selected_step_evidence();
    });
    refresh();
}

domain::Id RunsPage::selected_run_id() const { const auto row = runs_->currentRow(); return row < 0 ? domain::Id{} : s(runs_->item(row, 0)->data(Qt::UserRole).toString()); }
domain::Id RunsPage::selected_step_id() const { const auto row = steps_->currentRow(); return row < 0 ? domain::Id{} : s(steps_->item(row, 1)->data(Qt::UserRole).toString()); }

void RunsPage::update_selected_step() {
    const auto run_id = selected_run_id();
    const auto row = steps_->currentRow();
    if (run_id.empty() || row < 0) return;
    try {
        const auto values = context_.playbooks.list_run_steps(run_id);
        if (row >= static_cast<int>(values.size())) return;
        const auto form = run_step_form(this, values[static_cast<std::size_t>(row)]);
        if (!form) return;
        context_.playbooks.update_run_step(values[static_cast<std::size_t>(row)].id, form->state, s(form->note));
        refresh();
    } catch (const std::exception& error) { show_error(this, error); }
}

void RunsPage::add_step_to_selected_run() {
    const auto run_id = selected_run_id();
    if (run_id.empty()) return;
    const auto form = playbook_step_form(this, context_.playbooks.list_techniques());
    if (!form) return;
    try {
        context_.playbooks.add_run_step(run_id, s(form->name), s(form->instructions));
        refresh();
    } catch (const std::exception& error) { show_error(this, error); }
}

void RunsPage::link_selected_step_evidence() {
    const auto row = steps_->currentRow();
    const auto run_id = selected_run_id();
    if (run_id.empty() || row < 0) return;
    try {
        const auto values = context_.playbooks.list_run_steps(run_id);
        if (row >= static_cast<int>(values.size())) return;
        const auto form = link_evidence_form(this, context_.investigation.list_evidence(case_id_), context_.investigation.list_sources(case_id_), false, false);
        if (!form) return;
        auto evidence_id = form->evidence_id;
        domain::Id imported_id;
        if (form->import_file_path) {
            const auto imported = context_.investigation.add_file_evidence(case_id_, std::filesystem::path(s(*form->import_file_path)), form->import_source_id);
            imported_id = imported.id;
            evidence_id = imported.id;
        }
        try {
            context_.playbooks.link_step_evidence(values[static_cast<std::size_t>(row)].id, evidence_id);
        } catch (...) {
            if (!imported_id.empty()) {
                try { context_.investigation.delete_evidence(imported_id, true); } catch (...) { }
            }
            throw;
        }
        refresh();
    } catch (const std::exception& error) { show_error(this, error); }
}

void RunsPage::refresh() { const auto entities = context_.investigation.list_entities(case_id_); std::unordered_map<domain::Id, domain::Entity> map; for (const auto& value : entities) map.emplace(value.id, value); runs_->setRowCount(0); for (const auto& run : context_.playbooks.list_runs(case_id_)) { const auto row = runs_->rowCount(); runs_->insertRow(row); runs_->setItem(row, 0, cell(q(run.name_snapshot), q(run.id))); runs_->setItem(row, 1, cell(display_time(run.started_at))); runs_->setItem(row, 2, cell(run_target(run.entity_id, map))); } activity_->setRowCount(0); for (const auto& event : context_.investigation.activity(case_id_)) { const auto row = activity_->rowCount(); activity_->insertRow(row); activity_->setItem(row, 0, cell(display_time(event.occurred_at))); activity_->setItem(row, 1, cell(QStringLiteral("%1\n%2").arg(enum_label(event.action), q(event.description)))); } bool selected_focus = false; if (!focus_run_id_.empty()) for (int row = 0; row < runs_->rowCount(); ++row) if (runs_->item(row, 0) && s(runs_->item(row, 0)->data(Qt::UserRole).toString()) == focus_run_id_) { runs_->selectRow(row); selected_focus = true; break; } if (!selected_focus && runs_->rowCount() > 0) runs_->selectRow(0); update_run(); if (!focus_step_id_.empty()) for (int row = 0; row < steps_->rowCount(); ++row) if (steps_->item(row, 1) && s(steps_->item(row, 1)->data(Qt::UserRole).toString()) == focus_step_id_) { steps_->selectRow(row); break; } focus_run_id_.clear(); focus_step_id_.clear(); }

void RunsPage::show_step(const domain::Id& step_id) { focus_step_id_ = step_id; for (const auto& run : context_.playbooks.list_runs(case_id_)) for (const auto& step : context_.playbooks.list_run_steps(run.id)) if (step.id == step_id) { focus_run_id_ = run.id; refresh(); return; } }

void RunsPage::update_run() { const auto id = selected_run_id(); steps_->setRowCount(0); if (id.empty()) { run_title_->setText(QStringLiteral("Select a run")); run_meta_->setText(QStringLiteral("Start a global playbook to create a historical snapshot.")); return; } try { const auto runs = context_.playbooks.list_runs(case_id_); for (const auto& run : runs) if (run.id == id) { run_title_->setText(q(run.name_snapshot)); run_meta_->setText(QStringLiteral("Started %1\nSnapshot remains independent of the global template.").arg(display_time(run.started_at))); } const auto evidence_values = context_.investigation.list_evidence(case_id_); std::unordered_map<domain::Id, domain::Evidence> evidence_by_id; for (const auto& value : evidence_values) evidence_by_id.emplace(value.id, value); for (const auto& step : context_.playbooks.list_run_steps(id)) { const auto row = steps_->rowCount(); steps_->insertRow(row); steps_->setCellWidget(row, 0, status_badge(domain::to_string(step.state))); steps_->setItem(row, 1, cell(q(step.name_snapshot), q(step.id))); steps_->setItem(row, 2, cell(q(step.instructions_snapshot))); steps_->setItem(row, 3, cell(q(step.note))); QStringList linked; for (const auto& evidence_id : context_.playbooks.evidence_for_step(step.id)) { const auto found = evidence_by_id.find(evidence_id); linked << (found == evidence_by_id.end() ? shorten_id(evidence_id) : evidence_name(found->second)); } steps_->setItem(row, 4, cell(linked.isEmpty() ? QStringLiteral("None") : linked.join(QStringLiteral(", ")))); } } catch (const std::exception& error) { show_error(this, error); } }

ExportPage::ExportPage(ApplicationContext& context, const domain::Id& case_id, QWidget* parent) : CasePage(context, case_id, parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 24);
    root->setSpacing(16);
    root->addWidget(header_with_actions(QStringLiteral("Export & Recovery"), QStringLiteral("Archives restore a case; Markdown reports are separate reading copies."), {}));

    auto* content = new QWidget;
    content->setMaximumWidth(1500);
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    columns_ = new QBoxLayout(QBoxLayout::LeftToRight, content);
    auto* columns = columns_;
    columns->setContentsMargins(0, 0, 0, 0);
    columns->setSpacing(16);

    auto* summary_card = card();
    summary_card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto* summary_layout = new QVBoxLayout(summary_card);
    summary_layout->setContentsMargins(20, 18, 20, 18);
    summary_layout->addWidget(heading(QStringLiteral("Export this case"), 18));
    summary_ = muted(QString());
    summary_layout->addWidget(summary_);
    auto* archive_preview = card(); auto* archive_layout = new QVBoxLayout(archive_preview); archive_layout->setContentsMargins(12, 10, 12, 10); archive_layout->addWidget(new QLabel(QStringLiteral("Archive contents (preview)"))); archive_layout->addWidget(muted(QStringLiteral("[folder] case/\n   - case.json        Case records and relationships\n   - manifest.json    Entry sizes and SHA-256 hashes\n   - attachments/     Preserved evidence bytes"))); summary_layout->addWidget(archive_preview);
    summary_layout->addWidget(new QLabel(QStringLiteral("Summary")));
    archive_summary_ = muted(QString()); summary_layout->addWidget(archive_summary_);
    summary_layout->addSpacing(8);
    auto* archive = button(QStringLiteral("Export case ZIP"), true);
    auto* report = button(QStringLiteral("Generate Markdown report"));
    summary_layout->addWidget(archive);
    summary_layout->addWidget(report);
    summary_layout->addStretch();

    auto* import_card = card();
    import_card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto* import_layout = new QVBoxLayout(import_card);
    import_layout->setContentsMargins(20, 18, 20, 18);
    import_layout->addWidget(heading(QStringLiteral("Import and validate"), 18));
    import_layout->addWidget(muted(QStringLiteral("The archive is validated before the database changes. Unsafe paths, SHA-256 mismatches, unsupported schema versions, and malformed references reject the whole import.")));
    auto* validation_note = new QLabel(QStringLiteral("Validation is all-or-nothing: existing case data is left unchanged when an archive fails."));
    validation_note->setWordWrap(true);
    validation_note->setStyleSheet(QStringLiteral("QLabel { color: #c5b6ff; background: #17112f; border: 1px solid #4e3a8b; border-radius: 8px; padding: 12px; }"));
    import_layout->addWidget(validation_note);
    auto* drop = new QLabel(QStringLiteral("[upload]\n\nDrop a case archive here\nor click to select a ZIP file")); drop->setAlignment(Qt::AlignCenter); drop->setMinimumHeight(110); drop->setStyleSheet(QStringLiteral("QLabel { color: #c8b9ff; border: 1px dashed #6b52b7; border-radius: 8px; padding: 12px; }")); import_layout->addWidget(drop);
    import_layout->addWidget(new QLabel(QStringLiteral("Validation checks\n- Supported schema version\n- Safe file paths\n- SHA-256 verification\n- Duplicate ID handling")));
    auto* mode = enum_combo(nullptr, {"Skip existing case", "Import as copy"}, {"skip", "copy"});
    import_layout->addWidget(mode);
    auto* import_button = button(QStringLiteral("Choose ZIP and import"), true);
    import_layout->addWidget(import_button);
    import_layout->addStretch();

    columns->addWidget(summary_card, 1);
    columns->addWidget(import_card, 1);
    columns->setAlignment(summary_card, Qt::AlignTop);
    columns->setAlignment(import_card, Qt::AlignTop);
    auto* content_scroll = new QScrollArea;
    content_scroll->setFrameShape(QFrame::NoFrame);
    content_scroll->setWidgetResizable(true);
    content_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    content_scroll->setWidget(content);
    root->addWidget(content_scroll, 1);
    QTimer::singleShot(0, this, [this] { if (columns_) columns_->setDirection(width() < 1100 ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight); });
    connect(archive, &QPushButton::clicked, this, [this] { const auto path = QFileDialog::getSaveFileName(this, QStringLiteral("Export case archive"), QString(), QStringLiteral("ZIP archive (*.zip)")); if (path.isEmpty()) return; try { context_.portability.export_case(case_id_, std::filesystem::path(s(path))); QMessageBox::information(this, QStringLiteral("Archive exported"), QStringLiteral("Case archive written to %1").arg(path)); refresh(); } catch (const std::exception& error) { show_error(this, error); } }); connect(report, &QPushButton::clicked, this, [this] { const auto path = QFileDialog::getSaveFileName(this, QStringLiteral("Write Markdown report"), QString(), QStringLiteral("Markdown report (*.md)")); if (path.isEmpty()) return; try { context_.portability.write_report(case_id_, std::filesystem::path(s(path))); QMessageBox::information(this, QStringLiteral("Report generated"), QStringLiteral("Markdown report written to %1").arg(path)); refresh(); } catch (const std::exception& error) { show_error(this, error); } }); connect(import_button, &QPushButton::clicked, this, [this, mode] { const auto path = QFileDialog::getOpenFileName(this, QStringLiteral("Select case archive"), QString(), QStringLiteral("ZIP archive (*.zip)")); if (path.isEmpty()) return; try { const auto result = context_.portability.import_case(std::filesystem::path(s(path)), mode->currentData().toString() == "copy" ? import_export::ImportMode::ImportAsCopy : import_export::ImportMode::SkipExisting); QMessageBox::information(this, QStringLiteral("Import complete"), q(result.message)); refresh(); } catch (const std::exception& error) { show_error(this, error); } }); refresh();
}

void ExportPage::refresh() { const auto value = context_.cases.get(case_id_); const auto entities = context_.investigation.list_entities(case_id_); const auto evidence = context_.investigation.list_evidence(case_id_); const auto claims = context_.investigation.list_claims(case_id_); const auto notes = context_.investigation.list_notes(case_id_); const auto runs = context_.playbooks.list_runs(case_id_); summary_->setText(QStringLiteral("Case: %1\n\nThe ZIP includes case.json, manifest.json, preserved attachment bytes, claims, evidence links, notes, activity log, and playbook-run snapshots. The global template database is not exported.\n\nStatus: %2\nData directory: %3").arg(q(value.title), enum_label(domain::to_string(value.status)), q(context_.data_directory.string()))); archive_summary_->setText(QStringLiteral("Entities     %1\nEvidence     %2\nRelations    %3\nNotes        %4\nPlaybook runs %5").arg(entities.size()).arg(evidence.size()).arg(claims.size()).arg(notes.size()).arg(runs.size())); }

void ExportPage::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (columns_) columns_->setDirection(width() < 1100 ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
}

} // namespace evidence_trace::gui
