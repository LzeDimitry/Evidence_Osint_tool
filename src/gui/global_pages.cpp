#include "gui/global_pages.hpp"

#include "database/migrations.hpp"
#include "gui/dialogs.hpp"
#include "gui/gui_helpers.hpp"

#include <QComboBox>
#include <QBoxLayout>
#include <QCheckBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFrame>
#include <QFormLayout>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMenu>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <array>

namespace evidence_trace::gui {

namespace {

QTableWidgetItem* item(const QString& text, const QVariant& data = {}) {
    auto* result = new QTableWidgetItem(text);
    result->setToolTip(text);
    result->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    if (data.isValid()) result->setData(Qt::UserRole, data);
    return result;
}

QWidget* page_header(const QString& title,
                     const QString& subtitle,
                     QWidget* action = nullptr,
                     int action_breakpoint = 1050,
                     int copy_spacing = -1,
                     int bottom_margin = 12,
                     bool action_align_top = false) {
    auto* row = new QWidget;
    auto* layout = new QBoxLayout(QBoxLayout::LeftToRight, row);
    layout->setContentsMargins(0, 0, 0, bottom_margin);
    auto* copy = new QVBoxLayout;
    copy->setContentsMargins(0, 0, 0, 0);
    if (copy_spacing >= 0) copy->setSpacing(copy_spacing);
    copy->addWidget(heading(title));
    copy->addWidget(muted(subtitle));
    layout->addLayout(copy, 1);
    if (action) {
        layout->addWidget(action, 0, action_align_top ? Qt::AlignTop : Qt::AlignBottom);
        observe_resize(row, [row, layout, action_breakpoint](int width) {
            const auto narrow = width < action_breakpoint;
            layout->setDirection(narrow ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
            layout->setAlignment(narrow ? Qt::AlignLeft : Qt::Alignment());
            row->setMinimumHeight(narrow ? 142 : 0);
            row->updateGeometry();
        });
    }
    return row;
}

QLabel* case_tag_chip(const QString& text, QWidget* parent = nullptr, int maximum_width = 120) {
    return make_tag_chip(text, parent, maximum_width);
}

void compact_case_control(QWidget* control, int height = 38, int vertical_padding = 4) {
    if (control == nullptr) return;
    control->setFixedHeight(height);
    control->setStyleSheet(QStringLiteral("QLineEdit, QComboBox, QPushButton { min-height: 0px; padding: %1px 10px; }").arg(vertical_padding));
}

void clear_case_tag_chips(QWidget* container) {
    if (container == nullptr || container->layout() == nullptr) return;
    while (auto* item = container->layout()->takeAt(0)) {
        delete item->widget();
        delete item;
    }
}

void fill_case_tag_chips(QWidget* container, const QStringList& values, int visible_limit = -1) {
    if (container == nullptr) return;
    auto* layout = qobject_cast<QHBoxLayout*>(container->layout());
    if (layout == nullptr) {
        layout = new QHBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(5);
    }
    clear_case_tag_chips(container);
    if (values.isEmpty()) {
        auto* empty = muted(QStringLiteral("No tags"));
        layout->addWidget(empty);
        layout->addStretch();
        container->setToolTip(QStringLiteral("No tags"));
        container->setAccessibleDescription(container->toolTip());
        return;
    }
    const int count = visible_limit < 0 ? static_cast<int>(values.size()) : std::min(visible_limit, static_cast<int>(values.size()));
    for (int index = 0; index < count; ++index) layout->addWidget(case_tag_chip(values[index], container, visible_limit == 1 ? 92 : 156));
    if (count < values.size()) {
        auto* more = case_tag_chip(QStringLiteral("+%1").arg(values.size() - count), container, 52);
        more->setToolTip(values.mid(count).join(QStringLiteral(" · ")));
        layout->addWidget(more);
    }
    layout->addStretch();
    container->setToolTip(values.join(QStringLiteral(" · ")));
    container->setAccessibleDescription(container->toolTip());
}

QWidget* case_tag_chips(const domain::Case& value) {
    const auto tags = json_strings(value.tags_json);
    QStringList labels;
    for (const auto& tag : tags) labels << q(tag);
    auto* container = new QWidget;
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);
    fill_case_tag_chips(container, labels, 1);
    return container;
}

QString activity_dot_color(const domain::LogEvent& event) {
    const auto action = q(event.action).toLower();
    if (action.contains(QStringLiteral("archive")) || action.contains(QStringLiteral("delete")) || action.contains(QStringLiteral("remove"))) return QStringLiteral("#ed6878");
    if (action.contains(QStringLiteral("create")) || action.contains(QStringLiteral("claim"))) return QStringLiteral("#ef8a38");
    if (action.contains(QStringLiteral("link")) || action.contains(QStringLiteral("update")) || action.contains(QStringLiteral("change"))) return QStringLiteral("#559fff");
    if (action.contains(QStringLiteral("add")) || action.contains(QStringLiteral("evidence")) || action.contains(QStringLiteral("entity"))) return QStringLiteral("#9555ff");
    return QStringLiteral("#9555ff");
}

QString case_description_text(const domain::Case& value) {
    const auto purpose = q(value.purpose);
    const auto description = q(value.description);
    if (purpose.isEmpty()) return description.isEmpty() ? QStringLiteral("No description provided") : description;
    if (description.isEmpty() || description == purpose) return purpose;
    return purpose + QStringLiteral("\n\n") + description;
}

void clear_activity_rows(QWidget* container) {
    if (container == nullptr || container->layout() == nullptr) return;
    while (auto* item = container->layout()->takeAt(0)) {
        if (auto* child = item->widget()) delete child;
        delete item;
    }
}

void render_activity(QWidget* container, const std::vector<domain::LogEvent>& events) {
    if (container == nullptr) return;
    auto* layout = qobject_cast<QVBoxLayout*>(container->layout());
    if (layout == nullptr) return;
    clear_activity_rows(container);
    if (events.empty()) {
        layout->addWidget(muted(QStringLiteral("No activity yet")));
        return;
    }

    const auto first = events.size() > 5 ? events.size() - 5 : 0;
    for (std::size_t index = events.size(); index-- > first;) {
        const auto& event = events[index];
        auto* row = new QWidget(container);
        auto* row_layout = new QHBoxLayout(row);
        row_layout->setContentsMargins(0, 0, 0, 0);
        row_layout->setSpacing(8);

        auto* dot = new QLabel(row);
        dot->setFixedSize(8, 8);
        dot->setStyleSheet(QStringLiteral("QLabel { background: %1; border-radius: 4px; }").arg(activity_dot_color(event)));
        row_layout->addWidget(dot, 0, Qt::AlignTop);

        auto* when = muted(display_time(event.occurred_at));
        when->setMinimumWidth(145);
        when->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        row_layout->addWidget(when, 0, Qt::AlignTop);

        auto* copy = new QVBoxLayout;
        copy->setContentsMargins(0, 0, 0, 0);
        copy->setSpacing(1);
        auto* action = new QLabel(enum_label(event.action));
        action->setStyleSheet(QStringLiteral("QLabel { color: #edf0fa; }") );
        copy->addWidget(action);
        if (!event.description.empty()) copy->addWidget(muted(q(event.description)));
        row_layout->addLayout(copy, 1);
        layout->addWidget(row);
    }
}

} // namespace

CasesPage::CasesPage(ApplicationContext& context, QWidget* parent) : QWidget(parent), context_(context) {
    setMinimumHeight(0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* root = new QHBoxLayout(this); root->setContentsMargins(0, 0, 16, 0); root->setSpacing(12);
    auto* left_panel = new QWidget;
    auto* left_root = new QVBoxLayout(left_panel); left_root->setContentsMargins(16, 16, 0, 16); left_root->setSpacing(12);
    auto* create = button(QStringLiteral("＋  Create case"), true);
    status_filter_ = enum_combo(nullptr, {"Active", "Archived", "All cases"}, {"active", "archived", "all"});
    status_filter_->setVisible(false);
    all_filter_button_ = button(QStringLiteral("All (0)"));
    active_filter_button_ = button(QStringLiteral("Active (0)"));
    archived_filter_button_ = button(QStringLiteral("Archived (0)"));
    all_filter_button_->setProperty("caseFilter", true);
    all_filter_button_->setProperty("filterActive", false);
    active_filter_button_->setProperty("caseFilter", true);
    active_filter_button_->setProperty("filterActive", false);
    archived_filter_button_->setProperty("caseFilter", true);
    archived_filter_button_->setProperty("filterActive", false);
    compact_case_control(create, 39);
    compact_case_control(all_filter_button_, 36, 2);
    compact_case_control(active_filter_button_, 36, 2);
    compact_case_control(archived_filter_button_, 36, 2);
    create->setFixedWidth(136);
    all_filter_button_->setFixedWidth(78);
    active_filter_button_->setFixedWidth(94);
    archived_filter_button_->setFixedWidth(102);
    auto* header_actions = new QWidget;
    auto* header_actions_layout = new QHBoxLayout(header_actions);
    header_actions_layout->setContentsMargins(0, 0, 0, 0);
    header_actions_layout->setSpacing(8);
    header_actions_layout->addWidget(create);
    header_actions_layout->addWidget(all_filter_button_);
    header_actions_layout->addWidget(active_filter_button_);
    header_actions_layout->addWidget(archived_filter_button_);
    auto* header = page_header(QStringLiteral("Cases"), QStringLiteral("Investigations, analysis and evidence organized by case."), header_actions, 900, 0, 12, true);
    left_root->addWidget(header);
    connect(all_filter_button_, &QPushButton::clicked, this, [this] { status_filter_->setCurrentIndex(2); });
    connect(active_filter_button_, &QPushButton::clicked, this, [this] { status_filter_->setCurrentIndex(0); });
    connect(archived_filter_button_, &QPushButton::clicked, this, [this] { status_filter_->setCurrentIndex(1); });

    auto* filters = new QHBoxLayout;
    filters->setSpacing(10);
    search_ = new QLineEdit;
    search_->setPlaceholderText(QStringLiteral("Search cases..."));
    search_->addAction(ui_icon(UiIcon::Search), QLineEdit::LeadingPosition);
    search_->setMinimumWidth(220);
    auto* open = button(QStringLiteral("Open selected"), true);
    compact_case_control(search_);
    archive_button_ = button(QStringLiteral("Archive"));
    restore_button_ = button(QStringLiteral("Restore"));
    filters->addWidget(search_, 1);
    left_root->addLayout(filters);

    auto* list_panel = new QWidget;
    auto* list_layout = new QVBoxLayout(list_panel);
    list_layout->setContentsMargins(0, 0, 0, 0);
    list_layout->setSpacing(0);
    table_ = table_with_empty_state();
    configure_table(table_, {"Case", "Purpose", "Scope", "Last updated", "Entities", "Claims", "Tags"});
    set_empty_state(table_, QStringLiteral("No cases match this view"), QStringLiteral("Create a case to organize entities, evidence, claims, and investigation history."));
    table_->setMinimumWidth(0);
    table_->setMinimumHeight(0);
    table_->setMaximumHeight(QWIDGETSIZE_MAX);
    table_->setWordWrap(true);
    table_->setTextElideMode(Qt::ElideNone);
    table_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table_->verticalHeader()->setDefaultSectionSize(64);
    table_->verticalHeader()->setMinimumSectionSize(64);
    table_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    list_layout->addWidget(table_, 1);
    left_root->addWidget(list_panel, 1);

    auto* split = new QSplitter(Qt::Horizontal);
    split->setMinimumHeight(0);
    split->setChildrenCollapsible(false);
    split->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* left_scroll = new QScrollArea;
    left_scroll->setMinimumHeight(0);
    left_scroll->setObjectName(QStringLiteral("casesMainScroll"));
    left_scroll->setFrameShape(QFrame::NoFrame);
    left_scroll->setWidgetResizable(true);
    left_scroll->setAlignment(Qt::AlignTop);
    left_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    left_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    left_scroll->setWidget(left_panel);
    left_panel->setMinimumHeight(0);
    split->addWidget(left_scroll);
    root->addWidget(split, 1);

    auto* detail = card();
    detail->setMinimumHeight(0);
    auto* detail_layout = new QVBoxLayout(detail);
    detail_layout->setContentsMargins(12, 6, 12, 8);
    detail_layout->setSpacing(8);

    auto* title_row = new QHBoxLayout;
    title_row->setContentsMargins(0, 0, 0, 0);
    title_row->setSpacing(10);
    auto* case_icon = new QLabel;
    case_icon->setFixedSize(45, 45);
    case_icon->setAlignment(Qt::AlignCenter);
    case_icon->setPixmap(ui_icon(UiIcon::Folder, QColor("#d0baff")).pixmap(QSize(25, 25)));
    case_icon->setStyleSheet(QStringLiteral("QLabel { background: #24164e; border: 1px solid #6d44d8; border-radius: 9px; }") );
    auto* title_copy = new QVBoxLayout;
    title_copy->setContentsMargins(0, 0, 0, 0);
    title_copy->setSpacing(2);
    detail_title_ = heading(QStringLiteral("Select a case"), 20);
    detail_title_->setStyleSheet(QStringLiteral("QLabel { color: #f1f3fb; font-size: 20px; font-weight: 700; }") );
    detail_id_ = muted(QStringLiteral(""));
    title_copy->addWidget(detail_title_);
    title_copy->addWidget(detail_id_);
    title_row->addWidget(case_icon);
    title_row->addLayout(title_copy, 1);
    auto* title_widget = new QWidget;
    title_widget->setObjectName(QStringLiteral("caseDetailTitle"));
    title_widget->setLayout(title_row);
    title_widget->setMinimumHeight(45);
    title_widget->setMaximumHeight(45);
    detail_layout->addWidget(title_widget);

    detail_tags_widget_ = new QWidget;
    auto* tag_layout = new QHBoxLayout(detail_tags_widget_);
    tag_layout->setContentsMargins(0, 0, 0, 0);
    tag_layout->setSpacing(5);
    detail_tags_widget_->setFixedHeight(29);
    detail_tags_widget_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    detail_layout->addWidget(detail_tags_widget_);

    auto* empty_case_card = card();
    empty_case_card->setObjectName(QStringLiteral("caseEmptyState"));
    empty_case_card->setMaximumHeight(150);
    auto* empty_case_layout = new QVBoxLayout(empty_case_card);
    empty_case_layout->setContentsMargins(18, 18, 18, 18);
    empty_case_layout->setSpacing(8);
    empty_case_layout->setAlignment(Qt::AlignTop);
    auto* empty_case_title = heading(QStringLiteral("Select a case"), 16);
    empty_case_title->setObjectName(QStringLiteral("caseEmptyTitle"));
    empty_case_layout->addWidget(empty_case_title);
    auto* empty_case_text = muted(QStringLiteral("Choose an investigation from the list, or create a case to start organizing evidence and analysis."));
    empty_case_text->setObjectName(QStringLiteral("caseEmptyDescription"));
    empty_case_text->setWordWrap(true);
    empty_case_layout->addWidget(empty_case_text);
    empty_case_card->hide();
    detail_layout->addWidget(empty_case_card);

    auto make_icon = [](UiIcon kind, const QColor& color) {
        auto* icon = new QLabel;
        icon->setFixedSize(20, 20);
        icon->setAlignment(Qt::AlignCenter);
        icon->setPixmap(ui_icon(kind, color).pixmap(QSize(18, 18)));
        return icon;
    };

    auto* metadata_card = card();
    metadata_card->setObjectName(QStringLiteral("caseMetadataCard"));
    auto* metadata_layout = new QVBoxLayout(metadata_card);
    metadata_layout->setContentsMargins(11, 8, 11, 8);
    metadata_layout->setSpacing(0);
    auto add_section = [&](UiIcon icon_kind, const QString& title, ExpandableText*& target_label, bool divider) {
        auto* section = new QWidget;
        auto* section_layout = new QVBoxLayout(section);
        section_layout->setContentsMargins(0, 0, 0, 8);
        section_layout->setSpacing(5);
        auto* section_header = new QHBoxLayout;
        section_header->setContentsMargins(0, 0, 0, 0);
        section_header->setSpacing(7);
        section_header->addWidget(make_icon(icon_kind, QColor("#bda5ff")));
        section_header->addWidget(new QLabel(title));
        section_header->addStretch();
        target_label = new ExpandableText;
        target_label->setObjectName(QStringLiteral("subtle"));
        target_label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        section_header->addWidget([&] {
            auto* edit_section = button(QStringLiteral("Edit"));
            edit_section->setProperty("inspectorEdit", true);
            edit_section->setFixedWidth(58);
            compact_case_control(edit_section, 35, 2);
            connect(edit_section, &QPushButton::clicked, this, &CasesPage::edit_selected);
            return edit_section;
        }(), 0, Qt::AlignTop);
        section_layout->addLayout(section_header);
        section_layout->addWidget(target_label);
        metadata_layout->addWidget(section);
        if (divider) {
            auto* line = new QFrame;
            line->setFrameShape(QFrame::HLine);
            line->setFrameShadow(QFrame::Plain);
            line->setStyleSheet(QStringLiteral("QFrame { color: #263852; background: #263852; max-height: 1px; }") );
            metadata_layout->addWidget(line);
        }
    };
    add_section(UiIcon::Document, QStringLiteral("Purpose"), detail_body_, true);
    add_section(UiIcon::Target, QStringLiteral("Scope"), detail_scope_, false);
    detail_layout->addWidget(metadata_card);

    auto* overview_card = card();
    overview_card->setObjectName(QStringLiteral("caseOverviewCard"));
    auto* overview_layout = new QVBoxLayout(overview_card);
    overview_layout->setContentsMargins(9, 8, 9, 8);
    overview_layout->setSpacing(6);
    auto* overview_header = new QHBoxLayout;
    overview_header->setSpacing(7);
    overview_header->addWidget(make_icon(UiIcon::Clipboard, QColor("#bda5ff")));
    overview_header->addWidget(new QLabel(QStringLiteral("Case overview")));
    overview_header->addStretch();
    overview_layout->addLayout(overview_header);
    auto* metrics = new QGridLayout;
    metrics->setContentsMargins(22, 0, 0, 0);
    metrics->setHorizontalSpacing(18);
    metrics->setVerticalSpacing(4);
    auto add_metric = [&](UiIcon icon_kind, const QString& label, QLabel*& value, int row, int column) {
        auto* box = new QVBoxLayout;
        box->setContentsMargins(0, 0, 0, 0);
        box->setSpacing(0);
        box->addWidget(muted(label));
        value = new QLabel(QStringLiteral("—"));
        value->setStyleSheet(QStringLiteral("QLabel { color: #edf0fa; font-weight: 700; }") );
        box->addWidget(value);
        auto* content = new QHBoxLayout;
        content->setContentsMargins(0, 0, 0, 0);
        content->setSpacing(7);
        content->addWidget(make_icon(icon_kind, QColor("#bda5ff")), 0, Qt::AlignTop);
        content->addLayout(box, 1);
        auto* holder = new QWidget;
        holder->setLayout(content);
        metrics->addWidget(holder, row, column);
    };
    add_metric(UiIcon::Entity, QStringLiteral("Entities"), detail_entities_count_, 0, 0);
    add_metric(UiIcon::Document, QStringLiteral("Claims"), detail_claims_count_, 0, 1);
    add_metric(UiIcon::Shield, QStringLiteral("Status"), detail_status_value_, 0, 2);
    add_metric(UiIcon::Activity, QStringLiteral("Last updated"), detail_updated_value_, 1, 0);
    overview_layout->addLayout(metrics);
    detail_layout->addWidget(overview_card);

    auto* activity_card = card();
    activity_card->setObjectName(QStringLiteral("caseActivityCard"));
    auto* activity_layout = new QVBoxLayout(activity_card);
    activity_layout->setContentsMargins(9, 8, 9, 8);
    activity_layout->setSpacing(5);
    auto* activity_header = new QHBoxLayout;
    activity_header->setSpacing(7);
    activity_header->addWidget(make_icon(UiIcon::Activity, QColor("#bda5ff")));
    activity_header->addWidget(new QLabel(QStringLiteral("Recent activity")));
    activity_header->addStretch();
    activity_layout->addLayout(activity_header);
    detail_activity_ = new QWidget;
    auto* activity_rows = new QVBoxLayout(detail_activity_);
    activity_rows->setContentsMargins(0, 0, 0, 0);
    activity_rows->setSpacing(5);
    activity_layout->addWidget(detail_activity_);
    detail_layout->addWidget(activity_card);

    auto* unresolved_card = card();
    unresolved_card->setObjectName(QStringLiteral("caseUnresolvedCard"));
    auto* unresolved_layout = new QVBoxLayout(unresolved_card);
    unresolved_layout->setContentsMargins(9, 8, 9, 8);
    unresolved_layout->setSpacing(5);
    auto* unresolved_header = new QHBoxLayout;
    unresolved_header->setSpacing(7);
    unresolved_header->addWidget(make_icon(UiIcon::Warning, QColor("#cbb1ff")));
    unresolved_header->addWidget(new QLabel(QStringLiteral("Unresolved claims")));
    unresolved_header->addStretch();
    unresolved_layout->addLayout(unresolved_header);
    detail_unresolved_ = muted(QString());
    detail_unresolved_->setWordWrap(true);
    detail_unresolved_->setStyleSheet(QStringLiteral("QLabel { background: #16243a; border-radius: 6px; padding: 7px; }") );
    unresolved_layout->addWidget(detail_unresolved_);
    detail_layout->addWidget(unresolved_card);

    auto* bottom = new QHBoxLayout;
    bottom->addStretch();
    open->setText(QStringLiteral("Open case"));
    open->setObjectName(QStringLiteral("openCaseAction"));
    open->setFixedWidth(200);
    archive_button_->setText(QStringLiteral("Archive case"));
    restore_button_->setText(QStringLiteral("Restore case"));
    archive_button_->setFixedWidth(170);
    restore_button_->setFixedWidth(170);
    compact_case_control(archive_button_, 35, 2);
    compact_case_control(restore_button_, 35, 2);
    bottom->addWidget(open);
    bottom->addWidget(archive_button_);
    bottom->addWidget(restore_button_);
    detail->setMinimumWidth(0);
    detail->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* detail_scroll = new QScrollArea;
    detail_scroll->setMinimumHeight(0);
    detail_scroll->setObjectName(QStringLiteral("caseDetailsScroll"));
    detail_scroll->setFrameShape(QFrame::NoFrame);
    detail_scroll->setWidgetResizable(true);
    detail_scroll->setAlignment(Qt::AlignTop);
    detail_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    detail_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    detail_scroll->setMinimumWidth(0);
    detail_scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    detail_scroll->setWidget(detail);
    auto* action_footer = new QWidget;
    action_footer->setObjectName(QStringLiteral("caseActionFooter"));
    action_footer->setStyleSheet(QStringLiteral("QWidget#caseActionFooter { background: #0b111b; border-top: 1px solid #26364c; }") );
    bottom->setContentsMargins(12, 10, 12, 10);
    bottom->setSpacing(10);
    action_footer->setLayout(bottom);
    detail_pane_ = new QWidget;
    auto* detail_pane_layout = new QVBoxLayout(detail_pane_);
    detail_pane_layout->setContentsMargins(0, 0, 0, 0);
    detail_pane_layout->setSpacing(0);
    detail_pane_layout->addWidget(detail_scroll, 1);
    detail_pane_layout->addWidget(action_footer, 0);
    split->addWidget(detail_pane_);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 0);
    split->setSizes({std::max(640, width() * 2 / 3), std::max(320, width() / 3)});
    connect(create, &QPushButton::clicked, this, [&] { const auto form = case_form(this); if (!form) return; try { const auto created = context_.cases.create(s(form->title), s(form->purpose), s(form->description), form->target_type ? std::optional<std::string>(s(*form->target_type)) : std::nullopt, s(form->scope), form->tags); refresh(); if (on_open_case) on_open_case(created.id); } catch (const std::exception& error) { show_error(this, error); } });
    connect(open, &QPushButton::clicked, this, &CasesPage::open_selected); connect(table_, &QTableWidget::cellDoubleClicked, this, [this] { open_selected(); }); connect(table_, &QTableWidget::itemSelectionChanged, this, &CasesPage::update_detail); connect(search_, &QLineEdit::textChanged, this, [this] { refresh(); }); connect(status_filter_, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
    connect(archive_button_, &QPushButton::clicked, this, &CasesPage::archive_selected);
    connect(restore_button_, &QPushButton::clicked, this, &CasesPage::restore_selected);
    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table_, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& position) {
        const auto index = table_->indexAt(position);
        if (!index.isValid()) return;
        table_->selectRow(index.row());
        const auto id = selected_id();
        if (id.empty()) return;
        const auto current = context_.cases.get(id);
        QMenu menu(this);
        auto* details = menu.addAction(QStringLiteral("View details"));
        auto* edit_action = menu.addAction(QStringLiteral("Edit case"));
        menu.addSeparator();
        auto* lifecycle = menu.addAction(current.status == domain::CaseStatus::Archived ? QStringLiteral("Restore case") : QStringLiteral("Archive case"));
        menu.addSeparator();
        auto* remove = menu.addAction(QStringLiteral("Delete case"));
        const auto action = menu.exec(table_->viewport()->mapToGlobal(position));
        if (action == details) open_selected();
        else if (action == edit_action) edit_selected();
        else if (action == lifecycle) current.status == domain::CaseStatus::Archived ? restore_selected() : archive_selected();
        else if (action == remove) delete_selected();
    });
    resize_case_columns();
    refresh();
    QTimer::singleShot(0, this, [this] { resize_case_columns(); });
}

void CasesPage::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    const bool narrow_page = width() < 1050;
    if (auto* split = findChild<QSplitter*>()) {
        split->setOrientation(narrow_page ? Qt::Vertical : Qt::Horizontal);
        split->setMinimumHeight(narrow_page ? 700 : 0);
        setMinimumHeight(narrow_page ? 860 : 680);
        if (auto* list_scroll = findChild<QScrollArea*>(QStringLiteral("casesMainScroll")))
            list_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        if (narrow_page) split->setSizes({380, 320});
        else split->setSizes({std::max(640, width() * 68 / 100), std::max(320, width() * 32 / 100)});
    }
    table_->setMinimumHeight(0);
    const auto narrow = width() < 820;
    search_->setMinimumWidth(narrow ? 96 : 160);
    for (auto* action : findChildren<QPushButton*>()) {
        if (!action->property("inspectorEdit").toBool() && (action->text() == QStringLiteral("Edit selected") || action->text() == QStringLiteral("Edit"))) action->setText(narrow ? QStringLiteral("Edit") : QStringLiteral("Edit selected"));
        else if (action->text() == QStringLiteral("Open selected") || action->text() == QStringLiteral("Open")) action->setText(narrow ? QStringLiteral("Open") : QStringLiteral("Open selected"));
    }
    resize_case_columns();
}

void CasesPage::resize_case_columns() {
    if (!table_) return;
    auto* header = table_->horizontalHeader();
    header->setStretchLastSection(false);
    header->setMinimumSectionSize(52);
    const auto available = std::max(640, table_->viewport()->width() - 2);
    const std::array<double, 7> proportions = {0.20, 0.24, 0.10, 0.17, 0.08, 0.08, 0.13};
    int used = 0;
    for (int index = 0; index < static_cast<int>(proportions.size()); ++index) {
        header->setSectionResizeMode(index, QHeaderView::Fixed);
        const auto remaining = index == static_cast<int>(proportions.size()) - 1 ? available - used : static_cast<int>(available * proportions[static_cast<std::size_t>(index)]);
        const auto column_width = std::max(52, remaining);
        table_->setColumnWidth(index, column_width);
        used += column_width;
    }
    table_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    resize_case_preview_rows();
}

void CasesPage::resize_case_preview_rows() {
    if (!table_) return;
    for (int row = 0; row < table_->rowCount(); ++row) {
        int height = 74;
        for (const auto column : {1, 2}) {
            auto* preview = dynamic_cast<ExpandableText*>(table_->cellWidget(row, column));
            if (preview) height = std::max(height, preview->contentHeightForWidth(table_->columnWidth(column)));
        }
        table_->setRowHeight(row, height);
    }
    table_->setMinimumHeight(0);
    table_->setMaximumHeight(QWIDGETSIZE_MAX);
}

domain::Id CasesPage::selected_id() const { const auto row = table_->currentRow(); return row < 0 ? domain::Id{} : s(table_->item(row, 0)->data(Qt::UserRole).toString()); }

void CasesPage::set_search(const QString& text) { search_->setText(text); }

void CasesPage::refresh() {
    const auto search = search_->text().trimmed(); const auto values = context_.cases.list(search.isEmpty() ? std::nullopt : std::optional<std::string>(s(search))); const auto all_values = context_.cases.list(); const auto filter = status_filter_->currentData().toString();
    int active_count = 0;
    int archived_count = 0;
    for (const auto& value : all_values) value.status == domain::CaseStatus::Active ? ++active_count : ++archived_count;
    all_filter_button_->setText(QStringLiteral("All (%1)").arg(all_values.size()));
    active_filter_button_->setText(QStringLiteral("Active (%1)").arg(active_count));
    archived_filter_button_->setText(QStringLiteral("Archived (%1)").arg(archived_count));
    all_filter_button_->setProperty("filterActive", filter == QStringLiteral("all"));
    active_filter_button_->setProperty("filterActive", filter == QStringLiteral("active"));
    archived_filter_button_->setProperty("filterActive", filter == QStringLiteral("archived"));
    for (auto* control : {all_filter_button_, active_filter_button_, archived_filter_button_}) { control->style()->unpolish(control); control->style()->polish(control); }
    table_->setRowCount(0);
    for (const auto& value : values) {
        if (filter != "all" && q(domain::to_string(value.status)) != filter) continue;
        const auto row = table_->rowCount();
        table_->insertRow(row);
        auto* case_item = item(q(value.title), q(value.id));
        case_item->setIcon(ui_icon(UiIcon::Folder, QColor("#bda5ff")));
        table_->setItem(row, 0, case_item);
        const auto add_preview = [this, row](int column, const QString& text) {
            // Keep the exact value on the item for tooltips/data access, but
            // leave its painted text empty because the expandable widget is
            // the visible renderer for this cell.
            auto* backing_item = item(QString());
            backing_item->setToolTip(text);
            backing_item->setData(Qt::UserRole, text);
            table_->setItem(row, column, backing_item);
        auto* preview = new ExpandableText(table_);
        preview->setObjectName(QStringLiteral("subtle"));
        preview->setContentsMargins(10, 6, 10, 6);
        preview->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            if (column == 1) preview->setPreviewLineLimit(3);
        preview->setFullText(text);
            preview->on_toggled = [this, row](bool) {
                if (row < table_->rowCount()) table_->selectRow(row);
                resize_case_preview_rows();
            };
            table_->setCellWidget(row, column, preview);
        };
        add_preview(1, q(value.purpose));
        add_preview(2, q(value.scope));
        table_->setItem(row, 3, item(display_time(value.updated_at)));
        table_->setItem(row, 4, item(QString::number(value.entity_count)));
        table_->setItem(row, 5, item(QString::number(value.claim_count)));
        table_->setCellWidget(row, 6, case_tag_chips(value));
        table_->setRowHeight(row, 74);
    }
    if (table_->rowCount() == 0) {
        if (all_values.empty()) set_empty_state(table_, QStringLiteral("No cases yet"), QStringLiteral("Create a case to organize entities, evidence, claims, and investigation history."));
        else set_empty_state(table_, QStringLiteral("No cases match this view"), QStringLiteral("Try another search or choose a different status filter."));
    } else {
        set_empty_state(table_, QString(), QString());
    }
    resize_case_preview_rows();
    if (table_->rowCount() > 0) table_->selectRow(0);
    update_detail();
}

void CasesPage::open_selected() { const auto id = selected_id(); if (!id.empty() && on_open_case) on_open_case(id); }

void CasesPage::edit_selected() {
    const auto id = selected_id();
    if (id.empty()) return;
    try {
        const auto current = context_.cases.get(id);
        const auto form = case_form(this, current);
        if (!form) return;
        context_.cases.update(id, s(form->title), s(form->purpose), s(form->description), form->target_type ? std::optional<std::string>(s(*form->target_type)) : std::nullopt, s(form->scope), form->tags);
        refresh();
    } catch (const std::exception& error) { show_error(this, error); }
}

void CasesPage::archive_selected() {
    const auto id = selected_id();
    if (id.empty()) return;
    if (QMessageBox::question(this, QStringLiteral("Archive case"), QStringLiteral("Archive this case? It remains readable and can be restored.")) != QMessageBox::Yes) return;
    try { context_.cases.archive(id); refresh(); } catch (const std::exception& error) { show_error(this, error); }
}

void CasesPage::restore_selected() {
    const auto id = selected_id();
    if (id.empty()) return;
    try { context_.cases.restore(id); refresh(); } catch (const std::exception& error) { show_error(this, error); }
}

void CasesPage::delete_selected() {
    const auto id = selected_id();
    if (id.empty()) return;
    try {
        const auto current = context_.cases.get(id);
        const auto confirmation = QMessageBox::warning(this,
                                                       QStringLiteral("Delete case permanently"),
                                                       QStringLiteral("Permanently delete \"%1\" and all of its entities, claims, evidence, notes, runs, logs, and preserved files? This cannot be undone.\n\nUse Archive if you only want to hide the case from the active list.").arg(q(current.title)),
                                                       QMessageBox::Yes | QMessageBox::Cancel,
                                                       QMessageBox::Cancel);
        if (confirmation != QMessageBox::Yes) return;
        context_.cases.delete_case(id, context_.store);
        refresh();
    } catch (const std::exception& error) { show_error(this, error); }
}

void CasesPage::update_detail() {
    const auto id = selected_id(); archive_button_->setEnabled(false); restore_button_->setEnabled(false);
    const auto populated = !id.empty();
    detail_pane_->setVisible(populated);
    if (auto* title_row = findChild<QWidget*>(QStringLiteral("caseDetailTitle"))) title_row->setVisible(populated);
    if (auto* metadata = findChild<QWidget*>(QStringLiteral("caseMetadataCard"))) metadata->setVisible(populated);
    if (auto* empty_state = findChild<QWidget*>(QStringLiteral("caseEmptyState"))) empty_state->setVisible(!populated);
    for (const auto* name : {"caseOverviewCard", "caseActivityCard", "caseUnresolvedCard"})
        if (auto* section = findChild<QWidget*>(QString::fromLatin1(name))) section->setVisible(populated);
    if (detail_tags_widget_) detail_tags_widget_->setVisible(populated);
    if (auto* open = findChild<QPushButton*>(QStringLiteral("openCaseAction"))) { open->setEnabled(populated); open->setVisible(populated); }
    for (auto* edit : findChildren<QPushButton*>(QString(), Qt::FindChildrenRecursively))
        if (edit->property("inspectorEdit").toBool()) edit->setVisible(populated);
    if (id.empty()) {
        detail_title_->setText(table_->rowCount() == 0 ? QStringLiteral("No cases to show") : QStringLiteral("Select a case"));
        if (auto* empty_title = findChild<QLabel*>(QStringLiteral("caseEmptyTitle"))) empty_title->setText(table_->rowCount() == 0 ? QStringLiteral("No cases yet") : QStringLiteral("Select a case"));
        if (auto* empty_description = findChild<QLabel*>(QStringLiteral("caseEmptyDescription"))) empty_description->setText(table_->rowCount() == 0 ? QStringLiteral("Create a case to begin an investigation.") : QStringLiteral("Choose an investigation from the list to inspect its purpose, scope, and current state."));
        detail_id_->clear();
        fill_case_tag_chips(detail_tags_widget_, {});
        detail_body_->setFullText(table_->rowCount() == 0 ? QStringLiteral("Create a case or change the search/status filter to continue.") : QStringLiteral("Choose an investigation to inspect its purpose, scope, and current state."));
        detail_scope_->setFullText(QString());
        detail_entities_count_->setText(QStringLiteral("—"));
        detail_claims_count_->setText(QStringLiteral("—"));
        detail_status_value_->setText(QStringLiteral("—"));
        detail_updated_value_->setText(QStringLiteral("—"));
        clear_activity_rows(detail_activity_);
        if (auto* activity_layout = qobject_cast<QVBoxLayout*>(detail_activity_->layout())) activity_layout->addWidget(muted(QStringLiteral("No activity yet")));
        detail_unresolved_->setText(QStringLiteral("No unresolved claims"));
        archive_button_->setVisible(false);
        restore_button_->setVisible(false);
        return;
    }
    try {
        for (const auto* name : {"caseOverviewCard", "caseActivityCard", "caseUnresolvedCard"})
            if (auto* section = findChild<QWidget*>(QString::fromLatin1(name))) section->setVisible(true);
        if (detail_tags_widget_) detail_tags_widget_->setVisible(true);
        if (auto* open = findChild<QPushButton*>(QStringLiteral("openCaseAction"))) { open->setEnabled(true); open->setVisible(true); }
        for (auto* edit : findChildren<QPushButton*>(QString(), Qt::FindChildrenRecursively))
            if (edit->property("inspectorEdit").toBool()) edit->setVisible(true);
        const auto value = context_.cases.get(id); const auto values = context_.cases.list();
        QStringList tags;
        for (const auto& tag : json_strings(value.tags_json)) tags << q(tag);
        detail_title_->setText(q(value.title));
        detail_id_->setText(QStringLiteral("#%1").arg(shorten_id(id)));
        fill_case_tag_chips(detail_tags_widget_, tags, 3);
        detail_tags_widget_->setToolTip(tags.join(QStringLiteral(" · ")));
        detail_tags_widget_->setAccessibleDescription(detail_tags_widget_->toolTip());
        detail_body_->setFullText(case_description_text(value));
        detail_scope_->setFullText(value.scope.empty() ? QStringLiteral("Not specified") : q(value.scope));
        std::int64_t entity_count = 0;
        std::int64_t claim_count = 0;
        for (const auto& item_value : values) if (item_value.id == id) { entity_count = item_value.entity_count; claim_count = item_value.claim_count; }
        detail_entities_count_->setText(QString::number(entity_count));
        detail_claims_count_->setText(QString::number(claim_count));
        const auto active = value.status == domain::CaseStatus::Active;
        detail_status_value_->setText(QStringLiteral("● %1").arg(enum_label(domain::to_string(value.status))));
        detail_status_value_->setStyleSheet(QStringLiteral("QLabel { color: %1; font-weight: 700; }").arg(active ? QStringLiteral("#65e39b") : QStringLiteral("#ff8994")));
        detail_updated_value_->setText(display_time(value.updated_at));
        render_activity(detail_activity_, context_.investigation.activity(id));
        QString unresolved; for (const auto& claim : context_.investigation.list_claims(id)) if (claim.status != domain::ClaimStatus::Confirmed) unresolved += QStringLiteral("•  %1\n").arg(q(claim.statement)); detail_unresolved_->setText(unresolved.isEmpty() ? QStringLiteral("No unresolved claims") : unresolved.trimmed());
        archive_button_->setEnabled(value.status == domain::CaseStatus::Active); restore_button_->setEnabled(value.status == domain::CaseStatus::Archived);
        archive_button_->setVisible(value.status == domain::CaseStatus::Active);
        restore_button_->setVisible(value.status == domain::CaseStatus::Archived);
    } catch (...) { detail_title_->setText(QStringLiteral("Case unavailable")); detail_body_->setFullText(QStringLiteral("The selected case could not be loaded.")); detail_scope_->setFullText(QString()); fill_case_tag_chips(detail_tags_widget_, {}); clear_activity_rows(detail_activity_); if (auto* activity_layout = qobject_cast<QVBoxLayout*>(detail_activity_->layout())) activity_layout->addWidget(muted(QStringLiteral("No activity yet"))); }
}

PlaybooksPage::PlaybooksPage(ApplicationContext& context, QWidget* parent) : QWidget(parent), context_(context) {
    auto* root = new QVBoxLayout(this); root->setContentsMargins(28, 24, 28, 24); root->setSpacing(16); auto* new_technique = button(QStringLiteral("＋  New technique"), true); auto* new_playbook = button(QStringLiteral("＋  New playbook")); root->addWidget(page_header(QStringLiteral("Playbooks"), QStringLiteral("Reusable methods are global templates. Case runs keep their own snapshots.")));
    auto* action_strip = new QWidget; auto* actions = new QBoxLayout(QBoxLayout::LeftToRight, action_strip); actions->setContentsMargins(0, 0, 0, 0); actions->setSpacing(12); auto* create_group = new QGroupBox(QStringLiteral("Create")); auto* create_layout = new QHBoxLayout(create_group); create_layout->setContentsMargins(12, 10, 12, 10); new_technique->setMinimumWidth(155); new_playbook->setMinimumWidth(155); create_layout->addWidget(new_technique); create_layout->addWidget(new_playbook); auto* manage_group = new QGroupBox(QStringLiteral("Manage selected")); auto* manage_layout = new QGridLayout(manage_group); manage_layout->setContentsMargins(12, 10, 12, 10); manage_layout->setHorizontalSpacing(8); manage_layout->setVerticalSpacing(8); auto* edit_technique = button(QStringLiteral("Edit technique")); auto* edit_playbook = button(QStringLiteral("Edit playbook")); auto* add_step = button(QStringLiteral("Add step")); auto* move_up = button(QStringLiteral("Move step ↑")); auto* move_down = button(QStringLiteral("Move step ↓")); edit_technique->setMinimumWidth(130); edit_playbook->setMinimumWidth(130); add_step->setMinimumWidth(130); move_up->setMinimumWidth(130); move_down->setMinimumWidth(130); manage_layout->addWidget(edit_technique, 0, 0); manage_layout->addWidget(edit_playbook, 0, 1); manage_layout->addWidget(add_step, 1, 0); manage_layout->addWidget(move_up, 1, 1); manage_layout->addWidget(move_down, 2, 0); actions->addWidget(create_group); actions->addWidget(manage_group); actions->addStretch(); observe_resize(action_strip, [action_strip, actions](int width) { const auto narrow = width < 1050; actions->setDirection(narrow ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight); action_strip->setMinimumHeight(narrow ? 315 : 0); action_strip->updateGeometry(); }); root->addWidget(action_strip);
    auto* tabs = new QTabWidget; auto* template_page = new QWidget; auto* template_layout = new QVBoxLayout(template_page); template_layout->setContentsMargins(0, 0, 0, 0); auto* template_split = responsive_splitter(); auto* left = new QSplitter(Qt::Vertical); playbooks_ = table_with_empty_state(); configure_table(playbooks_, {"Playbook", "Scope", "Updated"}); stretch_table_columns(playbooks_); set_empty_state(playbooks_, QStringLiteral("No playbooks yet"), QStringLiteral("Create a reusable method for future case runs.")); techniques_ = table_with_empty_state(); configure_table(techniques_, {"Technique", "Objective", "Updated"}); stretch_table_columns(techniques_); set_empty_state(techniques_, QStringLiteral("No techniques yet"), QStringLiteral("Techniques become reusable steps inside your playbooks.")); left->addWidget(playbooks_); left->addWidget(techniques_); left->setStretchFactor(0, 1); left->setStretchFactor(1, 1); template_split->addWidget(left);
    auto* details = card(); auto* details_layout = new QVBoxLayout(details); details_layout->setContentsMargins(24, 22, 24, 22); playbook_title_ = heading(QStringLiteral("Select a template"), 21); playbook_description_ = muted(QStringLiteral("Global playbooks and techniques are stored independently of cases.")); technique_title_ = heading(QStringLiteral(""), 18); technique_body_ = muted(QStringLiteral("")); steps_ = table_with_empty_state(); configure_table(steps_, {"#", "Step", "Instructions"}); stretch_table_columns(steps_); set_empty_state(steps_, QStringLiteral("No steps in this playbook"), QStringLiteral("Add an ordered step or select a technique to build the method.")); details_layout->addWidget(playbook_title_); details_layout->addWidget(playbook_description_); details_layout->addSpacing(18); details_layout->addWidget(technique_title_); details_layout->addWidget(technique_body_); details_layout->addSpacing(18); details_layout->addWidget(steps_, 1); template_split->addWidget(details); template_layout->addWidget(template_split); tabs->addTab(template_page, QStringLiteral("Templates")); root->addWidget(tabs, 1);
    connect(new_technique, &QPushButton::clicked, this, [this] { const auto form = technique_form(this); if (!form) return; try { context_.playbooks.create_technique(s(form->name), s(form->objective), s(form->steps), s(form->example_queries_json), s(form->tools_links_json), s(form->limitations), s(form->tags_json)); refresh(); } catch (const std::exception& error) { show_error(this, error); } });
    connect(new_playbook, &QPushButton::clicked, this, [this] { const auto form = playbook_form(this); if (!form) return; try { context_.playbooks.create_playbook(s(form->name), s(form->scope), s(form->description)); refresh(); } catch (const std::exception& error) { show_error(this, error); } });
    connect(playbooks_, &QTableWidget::itemSelectionChanged, this, &PlaybooksPage::update_playbook_detail); connect(techniques_, &QTableWidget::itemSelectionChanged, this, &PlaybooksPage::update_technique_detail);
    connect(edit_technique, &QPushButton::clicked, this, &PlaybooksPage::edit_selected_technique);
    connect(edit_playbook, &QPushButton::clicked, this, &PlaybooksPage::edit_selected_playbook);
    connect(add_step, &QPushButton::clicked, this, &PlaybooksPage::add_step_to_selected_playbook);
    connect(move_up, &QPushButton::clicked, this, [this] { move_selected_step(-1); }); connect(move_down, &QPushButton::clicked, this, [this] { move_selected_step(1); });
    playbooks_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(playbooks_, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& position) {
        const auto index = playbooks_->indexAt(position);
        if (!index.isValid()) return;
        playbooks_->selectRow(index.row());
        QMenu menu(this);
        auto* details = menu.addAction(QStringLiteral("View playbook details"));
        auto* edit = menu.addAction(QStringLiteral("Edit playbook"));
        auto* add = menu.addAction(QStringLiteral("Add step"));
        menu.addSeparator();
        auto* remove = menu.addAction(QStringLiteral("Delete playbook"));
        const auto action = menu.exec(playbooks_->viewport()->mapToGlobal(position));
        if (action == details) update_playbook_detail();
        else if (action == edit) edit_selected_playbook();
        else if (action == add) add_step_to_selected_playbook();
        else if (action == remove) delete_selected_playbook();
    });
    techniques_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(techniques_, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& position) {
        const auto index = techniques_->indexAt(position);
        if (!index.isValid()) return;
        techniques_->selectRow(index.row());
        QMenu menu(this);
        auto* details = menu.addAction(QStringLiteral("View technique details"));
        auto* edit = menu.addAction(QStringLiteral("Edit technique"));
        menu.addSeparator();
        auto* remove = menu.addAction(QStringLiteral("Delete technique"));
        const auto action = menu.exec(techniques_->viewport()->mapToGlobal(position));
        if (action == details) update_technique_detail();
        else if (action == edit) edit_selected_technique();
        else if (action == remove) delete_selected_technique();
    });
    steps_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(steps_, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& position) {
        const auto index = steps_->indexAt(position);
        if (!index.isValid()) return;
        steps_->selectRow(index.row());
        QMenu menu(this);
        auto* up = menu.addAction(QStringLiteral("Move step up"));
        auto* down = menu.addAction(QStringLiteral("Move step down"));
        const auto action = menu.exec(steps_->viewport()->mapToGlobal(position));
        if (action == up) move_selected_step(-1);
        else if (action == down) move_selected_step(1);
    });
    refresh();
}

void PlaybooksPage::edit_selected_technique() {
    const auto row = techniques_->currentRow();
    if (row < 0) return;
    try {
        const auto current = context_.playbooks.get_technique(s(techniques_->item(row, 0)->data(Qt::UserRole).toString()));
        const auto form = technique_form(this, current);
        if (!form) return;
        context_.playbooks.update_technique(current.id, s(form->name), s(form->objective), s(form->steps), s(form->example_queries_json), s(form->tools_links_json), s(form->limitations), s(form->tags_json));
        refresh();
    } catch (const std::exception& error) { show_error(this, error); }
}

void PlaybooksPage::edit_selected_playbook() {
    const auto row = playbooks_->currentRow();
    if (row < 0) return;
    try {
        const auto current = context_.playbooks.get_playbook(s(playbooks_->item(row, 0)->data(Qt::UserRole).toString()));
        const auto form = playbook_form(this, current);
        if (!form) return;
        context_.playbooks.update_playbook(current.id, s(form->name), s(form->scope), s(form->description));
        refresh();
    } catch (const std::exception& error) { show_error(this, error); }
}

void PlaybooksPage::add_step_to_selected_playbook() {
    const auto row = playbooks_->currentRow();
    if (row < 0) return;
    try {
        const auto form = playbook_step_form(this, context_.playbooks.list_techniques());
        if (!form) return;
        context_.playbooks.add_playbook_step(s(playbooks_->item(row, 0)->data(Qt::UserRole).toString()), form->technique_id, s(form->name), s(form->instructions));
        refresh();
    } catch (const std::exception& error) { show_error(this, error); }
}

void PlaybooksPage::delete_selected_playbook() {
    const auto row = playbooks_->currentRow();
    if (row < 0) return;
    try {
        const auto id = s(playbooks_->item(row, 0)->data(Qt::UserRole).toString());
        const auto current = context_.playbooks.get_playbook(id);
        if (QMessageBox::warning(this, QStringLiteral("Delete playbook permanently"), QStringLiteral("Permanently delete \"%1\" and its global template steps? Existing case runs keep their snapshots. This cannot be undone.").arg(q(current.name)), QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
        context_.playbooks.delete_playbook(id, true);
        refresh();
    } catch (const std::exception& error) { show_error(this, error); }
}

void PlaybooksPage::delete_selected_technique() {
    const auto row = techniques_->currentRow();
    if (row < 0) return;
    try {
        const auto id = s(techniques_->item(row, 0)->data(Qt::UserRole).toString());
        const auto current = context_.playbooks.get_technique(id);
        if (QMessageBox::warning(this, QStringLiteral("Delete technique permanently"), QStringLiteral("Permanently delete \"%1\"? Existing playbook steps retain their saved names and instructions. This cannot be undone.").arg(q(current.name)), QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
        context_.playbooks.delete_technique(id, true);
        refresh();
    } catch (const std::exception& error) { show_error(this, error); }
}

void PlaybooksPage::refresh() {
    playbooks_->setRowCount(0); for (const auto& value : context_.playbooks.list_playbooks()) { const auto row = playbooks_->rowCount(); playbooks_->insertRow(row); playbooks_->setItem(row, 0, item(q(value.name), q(value.id))); playbooks_->setItem(row, 1, item(q(value.scope))); playbooks_->setItem(row, 2, item(display_time(value.updated_at))); }
    techniques_->setRowCount(0); for (const auto& value : context_.playbooks.list_techniques()) { const auto row = techniques_->rowCount(); techniques_->insertRow(row); techniques_->setItem(row, 0, item(q(value.name), q(value.id))); techniques_->setItem(row, 1, item(q(value.objective))); techniques_->setItem(row, 2, item(display_time(value.updated_at))); }
    if (playbooks_->rowCount() > 0) playbooks_->selectRow(0);
    if (techniques_->rowCount() > 0) techniques_->selectRow(0);
    update_playbook_detail(); update_technique_detail();
}

void PlaybooksPage::move_selected_step(int delta) { const auto playbook_row = playbooks_->currentRow(); const auto step_row = steps_->currentRow(); if (playbook_row < 0 || step_row < 0) return; try { const auto playbook_id = s(playbooks_->item(playbook_row, 0)->data(Qt::UserRole).toString()); const auto steps = context_.playbooks.list_playbook_steps(playbook_id); if (step_row >= static_cast<int>(steps.size())) return; const auto new_position = steps[static_cast<std::size_t>(step_row)].position + delta; if (new_position < 0 || new_position >= static_cast<std::int64_t>(steps.size())) return; context_.playbooks.move_playbook_step(steps[static_cast<std::size_t>(step_row)].id, new_position); refresh(); } catch (const std::exception& error) { show_error(this, error); } }

void PlaybooksPage::update_playbook_detail() {
    const auto row = playbooks_->currentRow(); steps_->setRowCount(0); if (row < 0) { playbook_title_->setText(QStringLiteral("Select a template")); playbook_description_->setText(QStringLiteral("Global playbooks and techniques are stored independently of cases.")); return; } try { const auto value = context_.playbooks.get_playbook(s(playbooks_->item(row, 0)->data(Qt::UserRole).toString())); playbook_title_->setText(q(value.name)); playbook_description_->setText(QStringLiteral("%1\n\nScope: %2").arg(q(value.description), q(value.scope.empty() ? "Not specified" : value.scope))); for (const auto& step : context_.playbooks.list_playbook_steps(value.id)) { const auto line = steps_->rowCount(); steps_->insertRow(line); steps_->setItem(line, 0, item(QString::number(step.position + 1))); steps_->setItem(line, 1, item(q(step.name), q(step.id))); steps_->setItem(line, 2, item(q(step.instructions))); } } catch (...) { }
}

void PlaybooksPage::update_technique_detail() { const auto row = techniques_->currentRow(); if (row < 0) { technique_title_->clear(); technique_body_->clear(); return; } try { const auto value = context_.playbooks.get_technique(s(techniques_->item(row, 0)->data(Qt::UserRole).toString())); technique_title_->setText(q(value.name)); technique_body_->setText(QStringLiteral("%1\n\nInstructions\n%2\n\nLimitations\n%3").arg(q(value.objective), q(value.steps), q(value.limitations.empty() ? "Not specified" : value.limitations))); } catch (...) { } }

SettingsPage::SettingsPage(ApplicationContext& context, QWidget* parent) : QWidget(parent), context_(context) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 24);
    root->setSpacing(16);
    root->addWidget(page_header(QStringLiteral("Settings"), QStringLiteral("Configure the local environment and application boundaries.")));

    auto* content = new QWidget;
    content->setMaximumWidth(1500);
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    columns_ = new QBoxLayout(QBoxLayout::LeftToRight, content);
    auto* columns = columns_;
    columns->setContentsMargins(0, 0, 0, 0);
    columns->setSpacing(16);

    auto* left = new QVBoxLayout;
    left->setSpacing(16);
    auto* storage = card();
    storage->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto* storage_layout = new QVBoxLayout(storage);
    storage_layout->setContentsMargins(20, 18, 20, 18);
    storage_layout->addWidget(heading(QStringLiteral("Local data directory"), 18));
    storage_layout->addWidget(muted(QStringLiteral("Cases, SQLite metadata, preserved evidence, and temporary import files stay on this device.")));
    auto* row = new QHBoxLayout;
    data_directory_ = new QLineEdit(q(context_.data_directory.string()));
    data_directory_->setReadOnly(true);
    data_directory_->setCursorPosition(0);
    data_directory_->setToolTip(q(context_.data_directory.string()));
    auto* change = button(QStringLiteral("Change…"), true);
    auto* open = button(QStringLiteral("Open folder"));
    row->addWidget(data_directory_, 1);
    row->addWidget(change);
    row->addWidget(open);
    storage_layout->addLayout(row);
    left->addWidget(storage);

    auto* policy = card();
    policy->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto* policy_layout = new QFormLayout(policy);
    policy_layout->setContentsMargins(20, 18, 20, 18);
    policy_layout->setVerticalSpacing(12);
    policy_layout->addRow(QStringLiteral("Database schema"), new QLabel(QStringLiteral("Version %1").arg(database::current_schema_version)));
    policy_layout->addRow(QStringLiteral("Attachment limit"), new QLabel(display_bytes(context_.store.max_file_size()) + QStringLiteral(" per file")));
    policy_layout->addRow(QStringLiteral("Network / telemetry"), new QLabel(QStringLiteral("Disabled in the MVP")));
    policy_layout->addRow(QStringLiteral("Ukrainian localization"), new QLabel(QStringLiteral("Planned — English strings are externalizable")));
    left->addWidget(policy);

    auto* right = new QVBoxLayout;
    right->setSpacing(16);
    auto* recovery = card();
    recovery->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto* recovery_layout = new QVBoxLayout(recovery);
    recovery_layout->setContentsMargins(20, 18, 20, 18);
    recovery_layout->addWidget(heading(QStringLiteral("Recovery and error states"), 18));
    recovery_layout->addWidget(muted(QStringLiteral("Integrity mismatches, unsafe archive paths, and malformed imports are rejected with specific messages. Existing data is left unchanged when validation fails.")));
    auto* recovery_note = new QLabel(QStringLiteral("Validation happens before import. A failed archive never partially changes the case database."));
    recovery_note->setWordWrap(true);
    recovery_note->setStyleSheet(QStringLiteral("QLabel { color: #c5b6ff; background: #17112f; border: 1px solid #4e3a8b; border-radius: 8px; padding: 12px; }"));
    recovery_layout->addWidget(recovery_note);
    right->addWidget(recovery);

    auto* boundaries = card();
    boundaries->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto* boundaries_layout = new QVBoxLayout(boundaries);
    boundaries_layout->setContentsMargins(20, 18, 20, 18);
    boundaries_layout->addWidget(heading(QStringLiteral("Workspace boundaries"), 18));
    boundaries_layout->addWidget(muted(QStringLiteral("Evidence stays local-first. Files are preserved under the selected data directory and are never executed by the application preview.")));
    right->addWidget(boundaries);
    right->addStretch();

    columns->addLayout(left, 1);
    columns->addLayout(right, 1);
    columns->setAlignment(left, Qt::AlignTop);
    columns->setAlignment(right, Qt::AlignTop);
    auto* content_scroll = new QScrollArea;
    content_scroll->setFrameShape(QFrame::NoFrame);
    content_scroll->setWidgetResizable(true);
    content_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    content_scroll->setWidget(content);
    auto* tabs = new QTabWidget;
    tabs->addTab(content_scroll, QStringLiteral("General"));
    tabs->setTabIcon(0, ui_icon(UiIcon::Settings, QColor("#bd9fff")));
    const auto add_placeholder_tab = [tabs](const QString& name) {
        auto* page = new QWidget;
        auto* page_layout = new QVBoxLayout(page);
        page_layout->setContentsMargins(18, 18, 18, 18);
        page_layout->addWidget(muted(QStringLiteral("%1 settings are available through the local-first workspace controls.").arg(name)));
        page_layout->addStretch();
        tabs->addTab(page, name);
    };
    add_placeholder_tab(QStringLiteral("Data"));

    auto* appearance_page = new QWidget;
    auto* appearance_layout = new QVBoxLayout(appearance_page);
    appearance_layout->setContentsMargins(18, 18, 18, 18);
    auto* navigation_appearance = card();
    auto* navigation_appearance_layout = new QVBoxLayout(navigation_appearance);
    navigation_appearance_layout->setContentsMargins(20, 18, 20, 18);
    navigation_appearance_layout->setSpacing(10);
    navigation_appearance_layout->addWidget(heading(QStringLiteral("Navigation appearance"), 18));
    navigation_appearance_layout->addWidget(muted(QStringLiteral("Choose whether the selected case tab casts a soft purple glow into the page below.")));
    auto* glow_row = new QWidget(navigation_appearance);
    auto* glow_layout = new QHBoxLayout(glow_row);
    glow_layout->setContentsMargins(0, 4, 0, 0);
    auto* glow_toggle = new QCheckBox(QStringLiteral("Enable projector-style tab glow"), glow_row);
    glow_toggle->setChecked(navigation_glow_enabled());
    glow_toggle->setToolTip(QStringLiteral("Show or hide the soft light below the selected case tab."));
    auto* glow_state = new QLabel(glow_toggle->isChecked() ? QStringLiteral("ON") : QStringLiteral("OFF"), glow_row);
    glow_state->setAlignment(Qt::AlignCenter);
    glow_state->setMinimumWidth(46);
    glow_state->setStyleSheet(glow_toggle->isChecked()
        ? QStringLiteral("QLabel { color:#d5c4ff; background:#24164e; border:1px solid #7544df; border-radius:5px; padding:3px 8px; font-weight:700; }")
        : QStringLiteral("QLabel { color:#9baac2; background:#111a27; border:1px solid #2a3a50; border-radius:5px; padding:3px 8px; font-weight:700; }"));
    glow_layout->addWidget(glow_toggle);
    glow_layout->addStretch();
    glow_layout->addWidget(glow_state);
    navigation_appearance_layout->addWidget(glow_row);
    appearance_layout->addWidget(navigation_appearance);
    appearance_layout->addStretch();
    tabs->addTab(appearance_page, QStringLiteral("Appearance"));
    connect(glow_toggle, &QCheckBox::toggled, this, [this, glow_state](bool enabled) {
        set_navigation_glow_enabled(enabled);
        glow_state->setText(enabled ? QStringLiteral("ON") : QStringLiteral("OFF"));
        glow_state->setStyleSheet(enabled
            ? QStringLiteral("QLabel { color:#d5c4ff; background:#24164e; border:1px solid #7544df; border-radius:5px; padding:3px 8px; font-weight:700; }")
            : QStringLiteral("QLabel { color:#9baac2; background:#111a27; border:1px solid #2a3a50; border-radius:5px; padding:3px 8px; font-weight:700; }"));
        if (on_navigation_glow_changed) on_navigation_glow_changed(enabled);
    });

    add_placeholder_tab(QStringLiteral("Shortcuts"));
    add_placeholder_tab(QStringLiteral("About"));
    tabs->setTabIcon(1, ui_icon(UiIcon::Folder, QColor("#aebbd2")));
    tabs->setTabIcon(2, ui_icon(UiIcon::Eye, QColor("#aebbd2")));
    tabs->setTabIcon(3, ui_icon(UiIcon::Clipboard, QColor("#aebbd2")));
    tabs->setTabIcon(4, ui_icon(UiIcon::Info, QColor("#aebbd2")));
    root->addWidget(tabs, 1);
    QTimer::singleShot(0, this, [this] { if (columns_) columns_->setDirection(width() < 1100 ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight); });
    connect(change, &QPushButton::clicked, this, [this] { const auto selected = QFileDialog::getExistingDirectory(this, QStringLiteral("Choose local data directory"), data_directory_->text()); if (selected.isEmpty()) return; data_directory_->setText(selected); if (on_data_directory_requested) on_data_directory_requested(std::filesystem::path(s(selected))); }); connect(open, &QPushButton::clicked, this, [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(data_directory_->text())); });
}

void SettingsPage::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (columns_) columns_->setDirection(width() < 1100 ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
}

} // namespace evidence_trace::gui
