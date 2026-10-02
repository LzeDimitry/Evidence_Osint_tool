#include "gui/main_window.hpp"

#include "gui/context_pages.hpp"
#include "gui/global_pages.hpp"
#include "gui/gui_helpers.hpp"
#include "gui/investigation_pages.hpp"

#include <QDialog>
#include <QDialogButtonBox>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPixmap>
#include <QPushButton>
#include <QRadialGradient>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QStringList>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace evidence_trace::gui {

namespace {

enum class NavIcon { Brand, Cases, Playbooks, Settings, Overview, Entities, Claims, Evidence, Notes, Runs, Export, Bell };

QIcon painted_icon(NavIcon kind, const QColor& color = QColor("#bdc8df")) {
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    switch (kind) {
    case NavIcon::Brand: {
        const QPolygonF top{QPointF(12, 2.2), QPointF(20, 6.8), QPointF(12, 11.4), QPointF(4, 6.8)};
        const QPolygonF left{QPointF(4, 6.8), QPointF(12, 11.4), QPointF(12, 21.5), QPointF(4, 16.9)};
        const QPolygonF right{QPointF(20, 6.8), QPointF(12, 11.4), QPointF(12, 21.5), QPointF(20, 16.9)};
        painter.setPen(Qt::NoPen); painter.setBrush(QColor("#7042dc")); painter.drawPolygon(top); painter.setBrush(QColor("#4d2aa5")); painter.drawPolygon(left); painter.setBrush(QColor("#8c62f0")); painter.drawPolygon(right);
        break;
    }
    case NavIcon::Cases:
        painter.drawRoundedRect(QRectF(3.5, 6.5, 17, 13), 2, 2);
        painter.drawLine(QPointF(5, 6.5), QPointF(8, 3.8));
        painter.drawLine(QPointF(8, 3.8), QPointF(13, 3.8));
        painter.drawLine(QPointF(13, 3.8), QPointF(15, 6.5));
        break;
    case NavIcon::Playbooks:
        painter.drawRoundedRect(QRectF(4, 3.5, 16, 17), 1.5, 1.5);
        painter.drawLine(QPointF(12, 3.5), QPointF(12, 20.5));
        painter.drawLine(QPointF(6.5, 8), QPointF(10, 8));
        painter.drawLine(QPointF(14, 8), QPointF(17.5, 8));
        break;
    case NavIcon::Settings:
        painter.drawEllipse(QRectF(8, 8, 8, 8));
        for (int i = 0; i < 8; ++i) {
            const auto angle = i * 45.0 * 3.1415926535 / 180.0;
            const QPointF a(12 + std::cos(angle) * 6.2, 12 + std::sin(angle) * 6.2);
            const QPointF b(12 + std::cos(angle) * 8.5, 12 + std::sin(angle) * 8.5);
            painter.drawLine(a, b);
        }
        break;
    case NavIcon::Overview:
        painter.drawLine(QPointF(4, 6), QPointF(20, 6)); painter.drawLine(QPointF(4, 12), QPointF(20, 12)); painter.drawLine(QPointF(4, 18), QPointF(20, 18));
        painter.drawEllipse(QRectF(6, 4, 4, 4)); painter.drawEllipse(QRectF(14, 10, 4, 4)); painter.drawEllipse(QRectF(8, 16, 4, 4));
        break;
    case NavIcon::Entities:
        painter.drawEllipse(QRectF(8, 3.5, 8, 8)); painter.drawArc(QRectF(4, 12, 16, 9), 0, 180 * 16);
        break;
    case NavIcon::Claims:
        painter.drawEllipse(QRectF(3.5, 8, 5, 5)); painter.drawEllipse(QRectF(15.5, 3.5, 5, 5)); painter.drawEllipse(QRectF(15.5, 15.5, 5, 5));
        painter.drawLine(QPointF(8.2, 10), QPointF(15.2, 6)); painter.drawLine(QPointF(8.2, 12), QPointF(15.2, 18));
        break;
    case NavIcon::Evidence:
        painter.drawRoundedRect(QRectF(5, 3.5, 14, 17), 1.5, 1.5); painter.drawLine(QPointF(9, 8), QPointF(16, 8)); painter.drawLine(QPointF(9, 12), QPointF(16, 12)); painter.drawLine(QPointF(9, 16), QPointF(14, 16));
        break;
    case NavIcon::Notes:
        painter.drawRoundedRect(QRectF(4, 4, 16, 16), 2, 2); painter.drawLine(QPointF(8, 9), QPointF(16, 9)); painter.drawLine(QPointF(8, 13), QPointF(16, 13)); painter.drawLine(QPointF(8, 17), QPointF(13, 17));
        break;
    case NavIcon::Runs:
        painter.drawRoundedRect(QRectF(4, 4, 16, 16), 2, 2); painter.drawLine(QPointF(8, 8), QPointF(16, 8)); painter.drawLine(QPointF(8, 12), QPointF(16, 12)); painter.drawLine(QPointF(8, 16), QPointF(13, 16));
        break;
    case NavIcon::Export:
        painter.drawRect(QRectF(5, 5, 14, 15)); painter.drawLine(QPointF(12, 3), QPointF(12, 13)); painter.drawLine(QPointF(8.5, 9), QPointF(12, 13)); painter.drawLine(QPointF(15.5, 9), QPointF(12, 13));
        break;
    case NavIcon::Bell:
        painter.drawArc(QRectF(6, 5, 12, 13), 0, 180 * 16); painter.drawLine(QPointF(6, 12), QPointF(6, 17)); painter.drawLine(QPointF(18, 12), QPointF(18, 17)); painter.drawLine(QPointF(4, 18), QPointF(20, 18)); painter.drawEllipse(QRectF(10.5, 20, 3, 3));
        break;
    }
    return QIcon(pixmap);
}

QPushButton* navigation_button(const QString& text, std::optional<NavIcon> icon = std::nullopt) {
    auto* result = new QPushButton(text);
    if (icon) { result->setIcon(painted_icon(*icon)); result->setIconSize(QSize(20, 20)); result->setProperty("navIcon", static_cast<int>(*icon)); }
    result->setProperty("nav", true); result->setCursor(Qt::PointingHandCursor); return result;
}

QTableWidgetItem* result_cell(const QString& text) {
    auto* result = new QTableWidgetItem(text);
    result->setToolTip(text);
    result->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    return result;
}

QString section_for_result(const std::string& object_type) {
    if (object_type == "entity") return QStringLiteral("entities");
    if (object_type == "claim") return QStringLiteral("claims");
    if (object_type == "source") return QStringLiteral("sources");
    if (object_type == "evidence") return QStringLiteral("evidence");
    if (object_type == "note") return QStringLiteral("notes");
    if (object_type == "run_step") return QStringLiteral("runs");
    return QStringLiteral("overview");
}

void configure_shell_for_section(QWidget* shell, QFrame* sidebar, QLineEdit* search, bool cases, int window_width) {
    if (sidebar == nullptr) return;
    sidebar->setProperty("casesShell", cases);
    sidebar->setFixedWidth(window_width < 1100 ? 190 : 214);
    if (auto* side_layout = qobject_cast<QVBoxLayout*>(sidebar->layout())) {
        side_layout->setContentsMargins(12, 16, 12, 16);
        side_layout->setSpacing(6);
    }
    for (auto* nav : sidebar->findChildren<QPushButton*>()) {
        if (!nav->property("nav").toBool()) continue;
        if (nav->text() == QStringLiteral("Help")) nav->setVisible(true);
        nav->setMinimumHeight(38);
        nav->setMaximumHeight(42);
        nav->setIconSize(QSize(19, 19));
    }
    auto* topbar = shell ? shell->findChild<QFrame*>(QStringLiteral("topbar")) : nullptr;
    if (topbar) {
        topbar->setFixedHeight(58);
    }
    if (search) {
        if (cases) {
            const auto search_width = std::clamp(window_width - (window_width < 1100 ? 390 : 900), 380, 680);
            search->setMinimumWidth(search_width);
            search->setMaximumWidth(search_width);
            search->setFixedWidth(search_width);
            search->setMinimumHeight(36);
            search->setFixedHeight(36);
        } else {
            search->setMinimumWidth(300);
            search->setMaximumWidth(720);
            search->setMinimumHeight(38);
            search->setMaximumHeight(QWIDGETSIZE_MAX);
        }
    }
    if (topbar) { topbar->updateGeometry(); if (search) search->updateGeometry(); }
}

QScrollArea* scroll_page(QWidget* page, int minimum_height = 680) {
    // Keep the page's designed vertical rhythm on small windows so the
    // scroll area, rather than child layouts, owns the overflow.  Without a
    // minimum the page is compressed to the viewport and headers/actions are
    // visibly clipped at the 900x600 minimum window size.
    page->setMinimumHeight(minimum_height);
    page->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* scroll = new QScrollArea;
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setAlignment(Qt::AlignTop);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(page);
    return scroll;
}

QFrame* case_header_chip(const QString& text, UiIcon icon, const QColor& color, QWidget* parent = nullptr) {
    auto* chip = new QFrame(parent);
    chip->setObjectName(QStringLiteral("caseHeaderChip"));
    auto* layout = new QHBoxLayout(chip);
    layout->setContentsMargins(6, 0, 6, 0);
    layout->setSpacing(5);
    auto* icon_label = new QLabel(chip);
    icon_label->setPixmap(ui_icon(icon, color).pixmap(QSize(14, 14)));
    icon_label->setFixedSize(14, 14);
    layout->addWidget(icon_label);
    auto* value = new QLabel(text, chip);
    value->setObjectName(QStringLiteral("caseChipText"));
    value->setProperty("chipColor", color.name());
    layout->addWidget(value);
    chip->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    chip->setFixedHeight(25);
    return chip;
}

QString compact_relative_time(const std::string& utc_iso) {
    auto parsed = QDateTime::fromString(q(utc_iso), Qt::ISODateWithMs);
    if (!parsed.isValid()) parsed = QDateTime::fromString(q(utc_iso), Qt::ISODate);
    if (!parsed.isValid()) return display_time(utc_iso);
    const auto seconds = std::max<qint64>(0, parsed.toUTC().secsTo(QDateTime::currentDateTimeUtc()));
    if (seconds < 60) return QStringLiteral("just now");
    if (seconds < 3600) return QStringLiteral("%1 minutes ago").arg(seconds / 60);
    if (seconds < 86400) return QStringLiteral("%1 hours ago").arg(seconds / 3600);
    if (seconds < 604800) return QStringLiteral("%1 days ago").arg(seconds / 86400);
    return display_time(utc_iso).left(10);
}

class NavigationGlow final : public QWidget {
public:
    explicit NavigationGlow(QWidget* parent = nullptr) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_NoSystemBackground);
    }

    void setSourceX(int x) {
        if (source_x_ == x) return;
        source_x_ = x;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.save();
        painter.translate(source_x_, 0);
        painter.scale(0.72, 1.0);
        QRadialGradient beam(QPointF(0, 0), 138);
        beam.setColorAt(0.0, QColor(146, 92, 255, 62));
        beam.setColorAt(0.34, QColor(120, 72, 226, 34));
        beam.setColorAt(0.72, QColor(92, 55, 182, 10));
        beam.setColorAt(1.0, QColor(80, 48, 160, 0));
        painter.fillRect(QRectF(-220, 0, 440, height()), beam);
        painter.restore();
    }

private:
    int source_x_{0};
};

} // namespace

CaseWorkspace::CaseWorkspace(ApplicationContext& context, const domain::Id& case_id, QWidget* parent)
    : QWidget(parent), context_(context), case_id_(case_id) {
    setObjectName(QStringLiteral("caseWorkspace"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 8, 1, 0);
    root->setSpacing(0);

    auto* breadcrumb_bar = new QWidget;
    auto* breadcrumb_layout = new QHBoxLayout(breadcrumb_bar);
    breadcrumb_layout->setContentsMargins(20, 0, 0, 6);
    breadcrumb_layout->setSpacing(7);
    auto* back = button(QStringLiteral("Cases"));
    back->setProperty("breadcrumb", true);
    back->setFixedHeight(16);
    breadcrumb_layout->addWidget(back, 0, Qt::AlignLeft);
    auto* breadcrumb_chevron = new QLabel;
    breadcrumb_chevron->setPixmap(ui_icon(UiIcon::ChevronRight, QColor("#66758e")).pixmap(QSize(12, 12)));
    breadcrumb_chevron->setFixedSize(12, 16);
    breadcrumb_layout->addWidget(breadcrumb_chevron, 0, Qt::AlignVCenter);
    case_breadcrumb_ = muted(QStringLiteral("Investigation"));
    case_breadcrumb_->setObjectName(QStringLiteral("caseBreadcrumb"));
    case_breadcrumb_->setWordWrap(false);
    breadcrumb_layout->addWidget(case_breadcrumb_);
    breadcrumb_layout->addStretch();
    root->addWidget(breadcrumb_bar);

    auto* header = new QWidget;
    case_header_layout_ = new QBoxLayout(QBoxLayout::LeftToRight, header);
    auto* header_layout = case_header_layout_;
    header_layout->setContentsMargins(20, 2, 10, 10);
    header_layout->setSpacing(10);
    case_title_block_ = new QWidget(header);
    auto* title_layout = new QVBoxLayout(case_title_block_);
    title_layout->setContentsMargins(0, 0, 0, 0);
    title_layout->setSpacing(2);
    case_title_ = heading(QStringLiteral("Case"), 20);
    case_title_->setObjectName(QStringLiteral("caseTitle"));
    case_title_->setWordWrap(false);
    case_title_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    case_subtitle_ = new ExpandableText;
    case_subtitle_->setObjectName(QStringLiteral("caseDescription"));
    case_subtitle_->setAccessibleName(QStringLiteral("Case scope"));
    case_subtitle_->setAccessibleDescription(QStringLiteral("A two-line preview. Click to expand or collapse the full scope."));
    case_subtitle_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    case_subtitle_->setPreviewLineLimit(2);
    case_subtitle_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* title_row = new QHBoxLayout;
    title_row->setContentsMargins(0, 0, 0, 0);
    title_row->setSpacing(7);
    title_row->addWidget(case_title_);
    title_layout->addLayout(title_row);
    title_layout->addWidget(case_subtitle_);
    header_layout->addWidget(case_title_block_, 1);

    case_meta_block_ = new QWidget(header);
    case_meta_block_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    auto* meta = new QHBoxLayout(case_meta_block_);
    meta->setContentsMargins(0, 4, 0, 0);
    meta->setSpacing(6);
    auto* status_chip = new QFrame;
    status_chip->setObjectName(QStringLiteral("caseStatusChip"));
    status_chip->setFixedHeight(30);
    status_chip->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    auto* status_layout = new QHBoxLayout(status_chip);
    status_layout->setContentsMargins(6, 0, 5, 0);
    status_layout->setSpacing(5);
    auto* status_icon = new QLabel;
    status_icon->setPixmap(ui_icon(UiIcon::FolderOpen, QColor("#b899ff")).pixmap(QSize(13, 13)));
    status_icon->setFixedSize(13, 13);
    status_layout->addWidget(status_icon);
    case_status_ = new QLabel(QStringLiteral("Open"));
    case_status_->setObjectName(QStringLiteral("caseStatusText"));
    status_layout->addWidget(case_status_);
    meta->addWidget(status_chip);
    auto* entities_chip = case_header_chip(QStringLiteral("0 Entities"), UiIcon::Entity, QColor("#c5cfdf"));
    entities_chip->setFixedHeight(30);
    case_entities_ = entities_chip->findChild<QLabel*>(QStringLiteral("caseChipText"));
    meta->addWidget(entities_chip);
    auto* evidence_chip = case_header_chip(QStringLiteral("0 Evidence"), UiIcon::Evidence, QColor("#c5cfdf"));
    evidence_chip->setFixedHeight(30);
    case_evidence_ = evidence_chip->findChild<QLabel*>(QStringLiteral("caseChipText"));
    meta->addWidget(evidence_chip);
    meta->addSpacing(8);
    case_updated_ = new QLabel(QStringLiteral("Updated just now"));
    case_updated_->setObjectName(QStringLiteral("caseUpdated"));
    case_updated_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    case_updated_->setMinimumWidth(116);
    meta->addWidget(case_updated_);
    header_layout->addWidget(case_meta_block_, 0, Qt::AlignTop);
    root->addWidget(header);

    navigation_bar_ = new QWidget;
    auto* nav_bar = navigation_bar_;
    auto* nav_layout = new QHBoxLayout(nav_bar);
    nav_layout->setContentsMargins(20, 0, 0, 0);
    nav_layout->setSpacing(2);
    const QStringList labels = {"Overview", "Entities", "Claims && Relations", "Sources && Evidence", "Notes && Log", "Playbook Runs", "Export"};
    const std::array<NavIcon, 7> icons = {NavIcon::Overview, NavIcon::Entities, NavIcon::Claims, NavIcon::Evidence, NavIcon::Notes, NavIcon::Runs, NavIcon::Export};
    for (int index = 0; index < labels.size(); ++index) {
        auto* tab = new QWidget;
        auto* tab_layout = new QVBoxLayout(tab);
        tab_layout->setContentsMargins(0, 0, 0, 0);
        tab_layout->setSpacing(0);
        auto* nav = navigation_button(labels[index], icons[static_cast<std::size_t>(index)]);
        nav->setProperty("caseTab", true);
        nav->setIconSize(QSize(14, 14));
        tab_layout->addWidget(nav);
        navigation_.push_back(nav);
        nav_layout->addWidget(tab);
        connect(nav, &QPushButton::clicked, this, [this, index] { show_page(index); });
    }
    nav_bar->adjustSize();
    auto* nav_scroll = new QScrollArea;
    navigation_scroll_ = nav_scroll;
    nav_scroll->setObjectName(QStringLiteral("caseNavigationScroll"));
    nav_scroll->setFrameShape(QFrame::NoFrame);
    nav_scroll->setWidgetResizable(false);
    nav_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    nav_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    nav_scroll->setWidget(nav_bar);
    nav_scroll->setFixedHeight(58);
    root->addWidget(nav_scroll);

    pages_ = new QStackedWidget; root->addWidget(pages_, 1);
    auto* overview = new CaseOverviewPage(context_, case_id_, pages_); entities_page_ = new EntitiesPage(context_, case_id_, pages_); claims_page_ = new ClaimsPage(context_, case_id_, pages_); evidence_page_ = new EvidencePage(context_, case_id_, pages_); auto* notes = new NotesPage(context_, case_id_, pages_); auto* runs = new RunsPage(context_, case_id_, pages_); auto* export_page = new ExportPage(context_, case_id_, pages_);
    case_pages_ = {overview, entities_page_, claims_page_, evidence_page_, notes, runs, export_page};
    pages_->addWidget(scroll_page(overview));
    pages_->addWidget(scroll_page(entities_page_));
    claims_page_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    pages_->addWidget(scroll_page(claims_page_, 0));
    pages_->addWidget(scroll_page(evidence_page_));
    pages_->addWidget(scroll_page(notes));
    pages_->addWidget(scroll_page(runs));
    pages_->addWidget(scroll_page(export_page));
    navigation_glow_ = new NavigationGlow(this);
    navigation_glow_enabled_ = navigation_glow_enabled();
    navigation_glow_->setVisible(navigation_glow_enabled_);
    overview->on_add_entity = [this] { add_entity(); }; overview->on_add_claim = [this] { add_claim(); }; overview->on_add_evidence = [this] { add_evidence(); };
    overview->on_show_claim = [this](const domain::Id& claim_id) { show_page(2); claims_page_->show_claim(claim_id); };
    entities_page_->on_show_related_claims = [this](const domain::Id& entity_id) { show_page(2); claims_page_->show_related_to_entity(entity_id); };
    claims_page_->on_show_entity = [this](const domain::Id& entity_id) { show_page(1); entities_page_->show_entity(entity_id); };
    claims_page_->on_show_claim = [this](const domain::Id& claim_id) { show_page(2); claims_page_->show_claim(claim_id); };
    claims_page_->on_add_entity = [this] { add_entity(); };
    connect(back, &QPushButton::clicked, this, [this] { if (on_back) on_back(); }); QTimer::singleShot(0, this, [this] { update_case_subtitle(); }); show_page(0); refresh();
}

void CaseWorkspace::show_page(int index) {
    if (index < 0 || index >= static_cast<int>(case_pages_.size())) return;
    pages_->setCurrentIndex(index);
    for (int position = 0; position < static_cast<int>(navigation_.size()); ++position) {
        auto* nav = navigation_[static_cast<std::size_t>(position)];
        const bool active = position == index;
        nav->setProperty("navActive", active);
        if (nav->property("caseTab").toBool() && nav->property("navIcon").isValid()) {
            const bool show_icon = width() >= 1100 || width() < 560;
            nav->setIcon(show_icon ? painted_icon(static_cast<NavIcon>(nav->property("navIcon").toInt()), active ? QColor("#cdbaff") : QColor("#bdc8df")) : QIcon());
        }
        nav->style()->unpolish(nav);
        nav->style()->polish(nav);
        nav->update();
    }
    update_navigation_glow();
}

void CaseWorkspace::set_navigation_glow_enabled(bool enabled) {
    navigation_glow_enabled_ = enabled;
    if (navigation_glow_ == nullptr) return;
    navigation_glow_->setVisible(enabled);
    if (enabled) update_navigation_glow();
}

void CaseWorkspace::update_navigation_glow() {
    if (!navigation_glow_enabled_ || navigation_glow_ == nullptr || navigation_scroll_ == nullptr) return;
    const int top = navigation_scroll_->mapTo(this, QPoint(0, navigation_scroll_->height())).y();
    navigation_glow_->setGeometry(0, top, width(), std::min(138, std::max(0, height() - top)));
    for (auto* tab : navigation_) {
        if (!tab->property("navActive").toBool()) continue;
        const int source_x = tab->mapTo(this, QPoint(tab->width() / 2, tab->height())).x();
        if (auto* glow = dynamic_cast<NavigationGlow*>(navigation_glow_)) glow->setSourceX(source_x);
        break;
    }
    navigation_glow_->raise();
}

void CaseWorkspace::update_case_subtitle() {
    if (case_subtitle_ == nullptr) return;
    if (case_subtitle_->fullText() != case_subtitle_full_) case_subtitle_->setFullText(case_subtitle_full_);
    case_subtitle_->setToolTip(QStringLiteral("Click to expand or collapse the full case scope.\n\n%1").arg(case_subtitle_full_));
    case_subtitle_->updateGeometry();
}

void CaseWorkspace::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    const bool compact = width() < 1100;
    if (case_header_layout_) {
        case_header_layout_->setDirection(compact ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
        case_header_layout_->setContentsMargins(compact ? 14 : 20, compact ? 0 : 2, compact ? 14 : 10, compact ? 7 : 10);
        case_header_layout_->setSpacing(compact ? 5 : 10);
    }
    if (case_title_block_) case_title_block_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    if (case_meta_block_) case_meta_block_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    if (case_updated_) case_updated_->setVisible(!compact);
    if (case_title_) {
        auto font = case_title_->font();
        font.setPointSize(compact ? 18 : 20);
        case_title_->setFont(font);
        case_title_->setToolTip(case_title_->text());
    }
    const bool icons_only = width() < 560;
    const QStringList compact_tabs = {QStringLiteral("Overview"), QStringLiteral("Entities"), QStringLiteral("Claims"), QStringLiteral("Evidence"), QStringLiteral("Notes"), QStringLiteral("Runs"), QStringLiteral("Export")};
    const QStringList full_tabs = {QStringLiteral("Overview"), QStringLiteral("Entities"), QStringLiteral("Claims & Relations"), QStringLiteral("Sources & Evidence"), QStringLiteral("Notes & Log"), QStringLiteral("Playbook Runs"), QStringLiteral("Export")};
    for (int index = 0; index < static_cast<int>(navigation_.size()); ++index) {
        auto* tab = navigation_[static_cast<std::size_t>(index)];
        const QString full_tab = full_tabs[index];
        QString escaped_full_tab = full_tab;
        escaped_full_tab.replace(QStringLiteral("&"), QStringLiteral("&&"));
        tab->setText(icons_only ? QString() : compact ? compact_tabs[index] : escaped_full_tab);
        tab->setToolTip(full_tab);
        tab->setAccessibleName(full_tab);
        tab->setIconSize(icons_only ? QSize(17, 17) : compact ? QSize(14, 14) : QSize(16, 16));
        const bool active = tab->property("navActive").toBool();
        const auto icon_color = active ? QColor("#cdbaff") : QColor("#bdc8df");
        tab->setIcon((!compact || icons_only) ? painted_icon(static_cast<NavIcon>(tab->property("navIcon").toInt()), icon_color) : QIcon());
        if (icons_only) {
            tab->setFixedWidth(42);
            tab->setStyleSheet(QStringLiteral("padding: 0;"));
        } else {
            tab->setMinimumWidth(0);
            tab->setMaximumWidth(QWIDGETSIZE_MAX);
            tab->setStyleSheet(QString());
        }
    }
    if (navigation_bar_) navigation_bar_->adjustSize();
    update_navigation_glow();
    update_case_subtitle();
}

void CaseWorkspace::add_entity() { show_page(1); entities_page_->add_entity(); }
void CaseWorkspace::add_claim() { show_page(2); claims_page_->add_claim(); }
void CaseWorkspace::add_evidence() { show_page(3); evidence_page_->add_evidence(); }

void CaseWorkspace::refresh() { try {
    const auto value = context_.cases.get(case_id_);
    case_title_->setText(q(value.title));
    case_breadcrumb_->setText(q(value.title));
    const auto summary = !value.scope.empty() ? value.scope : !value.description.empty() ? value.description : value.purpose;
    case_subtitle_full_ = summary.empty() ? QStringLiteral("Case scope not provided") : q(summary).replace(QStringLiteral("\r\n"), QStringLiteral("\n")).replace(QChar('\r'), QChar('\n')).trimmed();
    case_breadcrumb_->setToolTip(q(value.title));
    update_case_subtitle();
    case_status_->setText(value.status == domain::CaseStatus::Active ? QStringLiteral("Open") : QStringLiteral("Archived"));
    const auto cases = context_.cases.list();
    const auto evidence_count = context_.investigation.list_evidence(case_id_).size();
    std::int64_t entity_count = 0;
    for (const auto& item : cases) if (item.id == case_id_) entity_count = item.entity_count;
    case_entities_->setText(QStringLiteral("%1 Entities").arg(entity_count));
    case_evidence_->setText(QStringLiteral("%1 Evidence").arg(evidence_count));
    case_updated_->setText(QStringLiteral("Updated %1").arg(compact_relative_time(value.updated_at)));
    case_updated_->setToolTip(QStringLiteral("Last updated: %1").arg(q(value.updated_at)));
    for (auto* page : case_pages_) page->refresh();
} catch (const std::exception& error) { show_error(this, error); } }

void CaseWorkspace::search(const QString& query) {
    const auto text = query.trimmed(); if (text.isEmpty()) return; try { const auto results = context_.investigation.search(case_id_, s(text)); QDialog dialog(this); dialog.setWindowTitle(QStringLiteral("Search in %1").arg(case_title_->text())); dialog.resize(820, 520); auto* layout = new QVBoxLayout(&dialog); layout->setContentsMargins(24, 22, 24, 22); layout->addWidget(heading(QStringLiteral("Search results"), 21)); layout->addWidget(muted(QStringLiteral("Results are limited to the current case and include entities, sources, evidence, claims, notes, and run steps."))); auto* table = table_with_empty_state(); configure_table(table, {"Type", "Title", "Details"}); table->set_empty_state(QStringLiteral("No matching records"), QStringLiteral("Try another phrase or search a broader record type.")); for (const auto& result : results) { const auto row = table->rowCount(); table->insertRow(row); table->setItem(row, 0, result_cell(enum_label(result.object_type))); table->setItem(row, 1, result_cell(q(result.title))); table->setItem(row, 2, result_cell(q(result.snippet))); } layout->addWidget(table, 1); auto* close = new QDialogButtonBox(QDialogButtonBox::Close); connect(close, &QDialogButtonBox::rejected, &dialog, &QDialog::reject); layout->addWidget(close); dialog.exec(); } catch (const std::exception& error) { show_error(this, error); } }

MainWindow::MainWindow(std::filesystem::path data_directory, QWidget* parent) : QMainWindow(parent), context_(std::make_unique<ApplicationContext>(std::move(data_directory))) { setWindowTitle(QStringLiteral("Evidence Trace")); setMinimumSize(900, 600); resize(1440, 900); build_shell(); rebuild_pages(); }

void MainWindow::build_shell() {
    shell_ = new QWidget;
    shell_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setCentralWidget(shell_);
    auto* root = new QVBoxLayout(shell_);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    topbar_ = new QFrame;
    topbar_->setObjectName(QStringLiteral("topbar"));
    topbar_->setFixedHeight(60);
    root->addWidget(topbar_);

    auto* brand = new QWidget(topbar_);
    brand->setObjectName(QStringLiteral("topbarBrand"));
    brand->setGeometry(24, 13, 170, 32);
    auto* brand_layout = new QHBoxLayout(brand);
    brand_layout->setContentsMargins(0, 0, 0, 0);
    brand_layout->setSpacing(8);
    auto* mark = new QLabel(brand);
    mark->setPixmap(painted_icon(NavIcon::Brand).pixmap(QSize(22, 22)));
    mark->setFixedSize(25, 25);
    auto* name = new QLabel(QStringLiteral("Evidence Trace"), brand);
    name->setObjectName(QStringLiteral("appTitle"));
    name->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    brand_layout->addWidget(mark);
    brand_layout->addWidget(name, 1);

    global_search_ = new QLineEdit(topbar_);
    global_search_->setObjectName(QStringLiteral("globalSearch"));
    global_search_->setPlaceholderText(QStringLiteral("Search across cases, entities, evidence..."));
    global_search_->addAction(ui_icon(UiIcon::Search, QColor("#b9c7df")), QLineEdit::LeadingPosition);
    global_search_->setFixedWidth(680);
    global_search_->setFixedHeight(38);
    global_search_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    global_shortcut_ = new QLabel(QStringLiteral("Ctrl K"), global_search_);
    global_shortcut_->setObjectName(QStringLiteral("searchShortcut"));
    global_shortcut_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    global_shortcut_->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto* body = new QWidget;
    auto* body_layout = new QHBoxLayout(body);
    body_layout->setContentsMargins(0, 0, 0, 0);
    body_layout->setSpacing(0);
    sidebar_ = new QFrame;
    auto* sidebar = sidebar_;
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(214);
    auto* sidebar_layout = new QVBoxLayout(sidebar);
    sidebar_layout->setContentsMargins(12, 16, 12, 16);
    sidebar_layout->setSpacing(6);
    cases_nav_ = navigation_button(QStringLiteral("Cases"), NavIcon::Cases);
    playbooks_nav_ = navigation_button(QStringLiteral("Playbooks"), NavIcon::Playbooks);
    settings_nav_ = navigation_button(QStringLiteral("Settings"), NavIcon::Settings);
    sidebar_layout->addWidget(cases_nav_);
    sidebar_layout->addWidget(playbooks_nav_);
    sidebar_layout->addWidget(settings_nav_);
    sidebar_layout->addStretch();
    body_layout->addWidget(sidebar);
    auto* right = new QWidget;
    right->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* right_layout = new QVBoxLayout(right);
    right_layout->setContentsMargins(0, 0, 0, 0);
    right_layout->setSpacing(0);
    content_ = new QStackedWidget;
    content_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    right_layout->addWidget(content_, 1);
    body_layout->addWidget(right, 1);
    root->addWidget(body, 1);
    connect(cases_nav_, &QPushButton::clicked, this, &MainWindow::show_cases); connect(playbooks_nav_, &QPushButton::clicked, this, &MainWindow::show_playbooks); connect(settings_nav_, &QPushButton::clicked, this, &MainWindow::show_settings); connect(global_search_, &QLineEdit::returnPressed, this, [this] { run_global_search(global_search_->text()); });
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    if (topbar_ && global_search_) {
        const auto position_topbar = [this] {
            if (!topbar_ || !global_search_) return;
            const auto search_x = std::max(0, (topbar_->width() - global_search_->width()) / 2 + (topbar_->height() <= 40 ? 5 : 0));
            global_search_->move(search_x, topbar_->height() <= 40 ? 2 : 10);
            if (global_shortcut_) global_shortcut_->setGeometry(global_search_->width() - 44, 0, 36, global_search_->height());
        };
        position_topbar();
        QTimer::singleShot(0, this, position_topbar);
    }
    if (sidebar_) {
        sidebar_->setFixedWidth(width() < 1100 ? 190 : 214);
        // The first resize happens after the window receives its real screen
        // geometry. Reapply the shell sizing here so the initial paint uses
        // the same search layout as a later section switch.
        configure_shell_for_section(shell_, sidebar_, global_search_, sidebar_->property("casesShell").toBool(), width());
        if (topbar_ && global_search_) {
            const auto search_x = std::max(0, (topbar_->width() - global_search_->width()) / 2 + (topbar_->height() <= 40 ? 5 : 0));
            global_search_->move(search_x, topbar_->height() <= 40 ? 2 : 10);
            if (global_shortcut_) global_shortcut_->setGeometry(global_search_->width() - 44, 0, 36, global_search_->height());
        }
    }
}

void CaseWorkspace::show_section(const QString& section) {
    const auto value = section.trimmed().toLower();
    if (value == "overview") show_page(0);
    else if (value == "entities") show_page(1);
    else if (value == "claims" || value == "relations") show_page(2);
    else if (value == "evidence") show_page(3);
    else if (value == "sources") { show_page(3); evidence_page_->show_sources(); }
    else if (value == "notes" || value == "log") show_page(4);
    else if (value == "runs" || value == "playbooks") show_page(5);
    else if (value == "export") show_page(6);
}

void CaseWorkspace::show_record(const QString& section, const QString& object_type, const domain::Id& object_id) {
    show_section(section);
    if (object_id.empty()) return;
    const auto type = object_type.toLower().replace(QLatin1Char(' '), QLatin1Char('_'));
    if (type == QStringLiteral("entity")) entities_page_->show_entity(object_id);
    else if (type == QStringLiteral("claim")) claims_page_->show_claim(object_id);
    else if (type == QStringLiteral("evidence")) evidence_page_->show_evidence(object_id);
    else if (type == QStringLiteral("source")) evidence_page_->show_source(object_id);
    else if (type == QStringLiteral("note")) {
        if (auto* notes = dynamic_cast<NotesPage*>(case_pages_[4])) notes->show_note(object_id);
    } else if (type == QStringLiteral("run_step")) {
        if (auto* runs = dynamic_cast<RunsPage*>(case_pages_[5])) runs->show_step(object_id);
    }
}

void MainWindow::rebuild_pages() {
    while (content_->count() > 0) { auto* widget = content_->widget(0); content_->removeWidget(widget); delete widget; }
    workspace_ = nullptr; cases_page_ = nullptr; playbooks_page_ = nullptr; settings_page_ = nullptr;
    cases_page_ = new CasesPage(*context_, this); playbooks_page_ = new PlaybooksPage(*context_, this); settings_page_ = new SettingsPage(*context_, this); content_->addWidget(cases_page_); content_->addWidget(scroll_page(playbooks_page_)); content_->addWidget(scroll_page(settings_page_));
    cases_page_->on_open_case = [this](const domain::Id& id) { open_case(id); };
    settings_page_->on_data_directory_requested = [this](const std::filesystem::path& path) { reload_data_directory(path); };
    settings_page_->on_navigation_glow_changed = [this](bool enabled) {
        if (workspace_ != nullptr) workspace_->set_navigation_glow_enabled(enabled);
    };
    show_cases();
}

void MainWindow::set_navigation(QPushButton* active) { for (auto* nav : {cases_nav_, playbooks_nav_, settings_nav_}) { nav->setProperty("navActive", nav == active); nav->style()->unpolish(nav); nav->style()->polish(nav); } }
void MainWindow::show_cases() {
    statusBar()->setVisible(true);
    configure_shell_for_section(shell_, sidebar_, global_search_, true, width());
    content_->setCurrentIndex(0); set_navigation(cases_nav_); if (cases_page_) cases_page_->refresh();
}
void MainWindow::show_playbooks() {
    statusBar()->setVisible(true);
    configure_shell_for_section(shell_, sidebar_, global_search_, false, width());
    content_->setCurrentIndex(1); set_navigation(playbooks_nav_); if (playbooks_page_) playbooks_page_->refresh();
}
void MainWindow::show_settings() {
    statusBar()->setVisible(true);
    configure_shell_for_section(shell_, sidebar_, global_search_, false, width());
    content_->setCurrentIndex(2); set_navigation(settings_nav_);
}

void MainWindow::open_case(const domain::Id& case_id) { if (workspace_) { content_->removeWidget(workspace_); delete workspace_; workspace_ = nullptr; } try { workspace_ = new CaseWorkspace(*context_, case_id, this); workspace_->on_back = [this] { show_cases(); }; content_->addWidget(workspace_); content_->setCurrentWidget(workspace_); set_navigation(cases_nav_); statusBar()->clearMessage(); statusBar()->setVisible(false); } catch (const std::exception& error) { show_error(this, error); } }

void MainWindow::show_case_section(const QString& section) { if (workspace_) workspace_->show_section(section); }

void MainWindow::reload_data_directory(const std::filesystem::path& directory) {
    try {
        if (std::filesystem::absolute(directory) == context_->data_directory) return;

        // Build the replacement first so a failure leaves the current pages and
        // their ApplicationContext references valid.
        auto replacement = std::make_unique<ApplicationContext>(directory);
        context_.swap(replacement);
        rebuild_pages();
        statusBar()->showMessage(QStringLiteral("Data directory reloaded"), 2500);
    } catch (const std::exception& error) {
        show_error(this, error);
    }
}

void MainWindow::run_global_search(const QString& query) {
    const auto text = query.trimmed();
    if (text.isEmpty()) return;
    try {
        QDialog dialog(this);
        dialog.setWindowTitle(QStringLiteral("Global search: %1").arg(text));
        dialog.resize(980, 560);
        auto* layout = new QVBoxLayout(&dialog);
        layout->addWidget(heading(QStringLiteral("Search across cases, entities, and evidence"), 17));
        layout->addWidget(muted(QStringLiteral("Double-click a result or use Open selected to navigate to its case record. Case filters remain available on the Cases screen.")));
        auto* table = table_with_empty_state();
        configure_table(table, {"Case", "Record type", "Record", "Details"});
        table->set_empty_state(QStringLiteral("No matching records"), QStringLiteral("Search checks case metadata and real investigation records across the local workspace."));
        table->setColumnWidth(0, 190); table->setColumnWidth(1, 120); table->setColumnWidth(2, 260); table->setColumnWidth(3, 360);
        layout->addWidget(table, 1);

        const auto add_result = [&](const domain::Id& case_id, const QString& case_title, const QString& object_type, const QString& title, const QString& details, const QString& section, const domain::Id& object_id) {
            const auto row = table->rowCount();
            table->insertRow(row);
            table->setItem(row, 0, result_cell(case_title));
            table->setItem(row, 1, result_cell(object_type));
            table->setItem(row, 2, result_cell(title));
            table->setItem(row, 3, result_cell(details));
            table->item(row, 0)->setData(Qt::UserRole, q(case_id));
            table->item(row, 0)->setData(Qt::UserRole + 1, section);
            table->item(row, 0)->setData(Qt::UserRole + 2, q(object_id));
            table->item(row, 0)->setData(Qt::UserRole + 3, object_type);
        };
        for (const auto& summary : context_->cases.list(std::optional<std::string>(s(text)))) {
            add_result(summary.id, q(summary.title), QStringLiteral("Case"), q(summary.title), QStringLiteral("Title or tag match · %1 entities · %2 claims").arg(summary.entity_count).arg(summary.claim_count), QStringLiteral("overview"), summary.id);
        }
        for (const auto& summary : context_->cases.list()) {
            for (const auto& result : context_->investigation.search(summary.id, s(text))) {
                add_result(summary.id, q(summary.title), enum_label(result.object_type), q(result.title), q(result.snippet), section_for_result(result.object_type), result.object_id);
            }
        }
        if (table->rowCount() == 0) {
            auto* empty = muted(QStringLiteral("No matching records were found. Search checks case title/tags plus entity labels, aliases, tags, source URLs, evidence filenames/content/notes, claims, notes, and playbook step text."));
            layout->addWidget(empty);
        }
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
        auto* open = button(QStringLiteral("Open selected"), true);
        buttons->addButton(open, QDialogButtonBox::AcceptRole);
        buttons->button(QDialogButtonBox::Close)->setDefault(true);
        open->setEnabled(false);
        QObject::connect(table, &QTableWidget::itemSelectionChanged, &dialog, [table, open] { open->setEnabled(table->currentRow() >= 0); });
        const auto navigate = [&] {
            const auto row = table->currentRow();
            if (row < 0) return;
            const auto* first = table->item(row, 0);
            const auto case_id = s(first->data(Qt::UserRole).toString());
            const auto section = first->data(Qt::UserRole + 1).toString();
            const auto object_id = s(first->data(Qt::UserRole + 2).toString());
            const auto object_type = first->data(Qt::UserRole + 3).toString();
            dialog.accept();
            open_case(case_id);
            if (workspace_) workspace_->show_record(section, object_type, object_id);
        };
        QObject::connect(open, &QPushButton::clicked, &dialog, navigate);
        QObject::connect(table, &QTableWidget::cellDoubleClicked, &dialog, [navigate](int, int) { navigate(); });
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addWidget(buttons);
        dialog.exec();
    } catch (const std::exception& error) {
        show_error(this, error);
    }
}

} // namespace evidence_trace::gui
