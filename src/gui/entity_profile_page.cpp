#include "gui/entity_profile_page.hpp"
#include "gui/gui_helpers.hpp"
#include "gui/dialogs.hpp"
#include "import_export/json.hpp"

#include <QApplication>
#include <QBoxLayout>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QImage>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QToolTip>

#include <algorithm>
#include <filesystem>
#include <map>
#include <set>
#include <unordered_map>

namespace evidence_trace::gui {

namespace {

QTableWidgetItem* profile_cell(const QString& text, const QVariant& data = {}) {
    auto* item = new QTableWidgetItem(text);
    item->setToolTip(text);
    item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    if (data.isValid()) item->setData(Qt::UserRole, data);
    return item;
}

QString relative_time_str(const std::string& utc_iso) {
    auto parsed = QDateTime::fromString(q(utc_iso), Qt::ISODateWithMs);
    if (!parsed.isValid()) parsed = QDateTime::fromString(q(utc_iso), Qt::ISODate);
    if (!parsed.isValid()) return display_time(utc_iso);
    const auto seconds = std::max<qint64>(0, parsed.toUTC().secsTo(QDateTime::currentDateTimeUtc()));
    if (seconds < 60) return QStringLiteral("just now");
    if (seconds < 3600) return QStringLiteral("%1m ago").arg(seconds / 60);
    if (seconds < 86400) return QStringLiteral("%1h ago").arg(seconds / 3600);
    if (seconds < 604800) return QStringLiteral("%1d ago").arg(seconds / 86400);
    return parsed.toUTC().toString(QStringLiteral("yyyy-MM-dd"));
}

struct ParsedEntityDetails {
    std::string description;
    std::vector<std::string> phone_numbers;
    std::vector<std::string> emails;
    std::vector<std::string> usernames;
    std::vector<std::string> identifiers;
    std::string location;
    std::string website;
    std::string registration_number;
    std::string jurisdiction;
    std::string date_of_birth;
    std::string nationality;
    std::string occupation;
    std::string platform;
    std::string profile_url;
    std::string registrar;
    std::string ip_address;
    std::string hosting_provider;
    std::map<std::string, std::string> custom_attributes;
};

ParsedEntityDetails parse_entity_details(const std::string& json_str) {
    ParsedEntityDetails result;
    if (json_str.empty()) return result;
    try {
        const auto parsed = json::Json::parse(json_str);
        if (!parsed.is_object()) {
            result.description = json_str;
            return result;
        }
        result.description = parsed.string_or("description");
        result.location = parsed.string_or("location");
        result.website = parsed.string_or("website");
        result.registration_number = parsed.string_or("registration_number");
        result.jurisdiction = parsed.string_or("jurisdiction");
        result.date_of_birth = parsed.string_or("date_of_birth");
        result.nationality = parsed.string_or("nationality");
        result.occupation = parsed.string_or("occupation");
        result.platform = parsed.string_or("platform");
        result.profile_url = parsed.string_or("profile_url");
        result.registrar = parsed.string_or("registrar");
        result.ip_address = parsed.string_or("ip_address");
        result.hosting_provider = parsed.string_or("hosting_provider");

        const auto read_string_list = [&](const std::string& key, std::vector<std::string>& dest) {
            const auto* found = parsed.find(key);
            if (!found) return;
            if (found->is_array()) {
                for (const auto& item : found->as_array()) {
                    if (item.is_string() && !item.as_string().empty()) dest.push_back(item.as_string());
                }
            } else if (found->is_string() && !found->as_string().empty()) {
                dest.push_back(found->as_string());
            }
        };

        read_string_list("phone_numbers", result.phone_numbers);
        read_string_list("emails", result.emails);
        read_string_list("usernames", result.usernames);
        read_string_list("identifiers", result.identifiers);

        const auto* custom = parsed.find("custom_attributes");
        if (custom && custom->is_object()) {
            for (const auto& [k, v] : custom->as_object()) {
                if (v.is_string()) result.custom_attributes[k] = v.as_string();
            }
        }
    } catch (...) {
        result.description = json_str;
    }
    return result;
}

std::string dump_entity_details(const ParsedEntityDetails& details) {
    json::Json::Object obj;
    if (!details.description.empty()) obj["description"] = details.description;
    if (!details.location.empty()) obj["location"] = details.location;
    if (!details.website.empty()) obj["website"] = details.website;
    if (!details.registration_number.empty()) obj["registration_number"] = details.registration_number;
    if (!details.jurisdiction.empty()) obj["jurisdiction"] = details.jurisdiction;
    if (!details.date_of_birth.empty()) obj["date_of_birth"] = details.date_of_birth;
    if (!details.nationality.empty()) obj["nationality"] = details.nationality;
    if (!details.occupation.empty()) obj["occupation"] = details.occupation;
    if (!details.platform.empty()) obj["platform"] = details.platform;
    if (!details.profile_url.empty()) obj["profile_url"] = details.profile_url;
    if (!details.registrar.empty()) obj["registrar"] = details.registrar;
    if (!details.ip_address.empty()) obj["ip_address"] = details.ip_address;
    if (!details.hosting_provider.empty()) obj["hosting_provider"] = details.hosting_provider;

    const auto write_list = [&](const std::string& key, const std::vector<std::string>& list) {
        if (list.empty()) return;
        json::Json::Array arr;
        for (const auto& val : list) arr.emplace_back(val);
        obj[key] = std::move(arr);
    };

    write_list("phone_numbers", details.phone_numbers);
    write_list("emails", details.emails);
    write_list("usernames", details.usernames);
    write_list("identifiers", details.identifiers);

    if (!details.custom_attributes.empty()) {
        json::Json::Object custom_obj;
        for (const auto& [k, v] : details.custom_attributes) custom_obj[k] = v;
        obj["custom_attributes"] = std::move(custom_obj);
    }

    return json::Json(std::move(obj)).dump();
}

} // namespace

EntityProfilePage::EntityProfilePage(ApplicationContext& context, const domain::Id& case_id, QWidget* parent)
    : CasePage(context, case_id, parent) {
    setObjectName(QStringLiteral("entityProfilePage"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 14, 20, 20);
    root->setSpacing(14);

    // 1. Navigation and Action Bar
    auto* nav_bar = new QWidget(this);
    auto* nav_layout = new QHBoxLayout(nav_bar);
    nav_layout->setContentsMargins(0, 0, 0, 0);
    nav_layout->setSpacing(8);

    back_button_ = button(QStringLiteral("← Back to Graph"), false);
    back_button_->setFixedHeight(34);
    back_button_->setIcon(ui_icon(UiIcon::ChevronRight, QColor("#cbd5e1"))); // placeholder, text is primary
    nav_layout->addWidget(back_button_);

    breadcrumb_label_ = muted(QStringLiteral(""));
    breadcrumb_label_->setStyleSheet(QStringLiteral("QLabel { color: #8ea1bd; font-size: 10pt; }"));
    nav_layout->addWidget(breadcrumb_label_);
    nav_layout->addStretch();

    view_in_graph_button_ = button(QStringLiteral("View in Graph"), false);
    view_in_graph_button_->setIcon(ui_icon(UiIcon::Graph, appearance_accent_color().lighter(160)));
    view_in_graph_button_->setFixedHeight(34);
    nav_layout->addWidget(view_in_graph_button_);

    connect_button_ = button(QStringLiteral("＋ Add connection"), false);
    connect_button_->setIcon(ui_icon(UiIcon::Link, QColor("#cbd5e1")));
    connect_button_->setFixedHeight(34);
    nav_layout->addWidget(connect_button_);

    add_claim_button_ = button(QStringLiteral("＋ Add claim"), false);
    add_claim_button_->setIcon(ui_icon(UiIcon::Claim, QColor("#cbd5e1")));
    add_claim_button_->setFixedHeight(34);
    nav_layout->addWidget(add_claim_button_);

    add_note_button_ = button(QStringLiteral("＋ Add note"), false);
    add_note_button_->setIcon(ui_icon(UiIcon::Note, QColor("#cbd5e1")));
    add_note_button_->setFixedHeight(34);
    nav_layout->addWidget(add_note_button_);

    edit_profile_button_ = button(QStringLiteral("Edit profile"), true);
    edit_profile_button_->setIcon(ui_icon(UiIcon::Edit, QColor("#ffffff")));
    edit_profile_button_->setFixedHeight(34);
    nav_layout->addWidget(edit_profile_button_);

    root->addWidget(nav_bar);

    // 2. Hero Identity Card
    hero_card_ = card(this);
    auto* hero_layout = new QHBoxLayout(hero_card_);
    hero_layout->setContentsMargins(20, 18, 20, 18);
    hero_layout->setSpacing(20);

    // Avatar/Photo
    photo_label_ = new QLabel(hero_card_);
    photo_label_->setFixedSize(100, 100);
    photo_label_->setAlignment(Qt::AlignCenter);
    photo_label_->setStyleSheet(QStringLiteral("QLabel { background: #121e31; border: 1.5px solid #283e5f; border-radius: 12px; }"));
    photo_label_->setCursor(Qt::PointingHandCursor);
    photo_label_->setToolTip(QStringLiteral("Click to view or manage image"));
    hero_layout->addWidget(photo_label_);

    // Identity Content
    auto* identity_block = new QVBoxLayout;
    identity_block->setContentsMargins(0, 0, 0, 0);
    identity_block->setSpacing(6);

    auto* top_identity_row = new QHBoxLayout;
    top_identity_row->setContentsMargins(0, 0, 0, 0);
    top_identity_row->setSpacing(8);

    type_badge_ = new QLabel(identity_block->widget());
    type_badge_->setStyleSheet(QStringLiteral(
        "QLabel { background: #1e2c45; color: #8ec5fc; border: 1px solid #365078; border-radius: 10px; padding: 3px 9px; font-size: 8.5pt; font-weight: 650; }"));
    top_identity_row->addWidget(type_badge_);

    duplicate_warning_ = new QLabel(identity_block->widget());
    duplicate_warning_->setText(QStringLiteral("⚠ Potential duplicate in case"));
    duplicate_warning_->setStyleSheet(QStringLiteral(
        "QLabel { background: #3b2816; color: #f6ad55; border: 1px solid #7c4a1e; border-radius: 10px; padding: 3px 9px; font-size: 8.5pt; font-weight: 600; }"));
    duplicate_warning_->hide();
    top_identity_row->addWidget(duplicate_warning_);
    top_identity_row->addStretch();
    identity_block->addLayout(top_identity_row);

    title_label_ = heading(QStringLiteral("Entity Name"), 22);
    title_label_->setStyleSheet(QStringLiteral("QLabel { font-weight: 700; color: #f8fafc; }"));
    identity_block->addWidget(title_label_);

    original_value_label_ = new QLabel(identity_block->widget());
    original_value_label_->setStyleSheet(QStringLiteral(
        "QLabel { color: #94a3b8; font-family: monospace; font-size: 9.5pt; background: #162438; border: 1px solid #253952; border-radius: 5px; padding: 2px 7px; }"));
    original_value_label_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    identity_block->addWidget(original_value_label_);

    aliases_label_ = muted(QString());
    aliases_label_->setStyleSheet(QStringLiteral("QLabel { color: #8fa0ba; font-size: 9.5pt; }"));
    identity_block->addWidget(aliases_label_);

    tags_widget_ = new QWidget(identity_block->widget());
    auto* tags_layout = new QHBoxLayout(tags_widget_);
    tags_layout->setContentsMargins(0, 0, 0, 0);
    tags_layout->setSpacing(6);
    tags_layout->addStretch();
    identity_block->addWidget(tags_widget_);

    hero_layout->addLayout(identity_block, 1);

    // Stats & Timestamp Block
    auto* stats_block = new QFrame(hero_card_);
    stats_block->setStyleSheet(QStringLiteral("QFrame { background: #121c2c; border: 1px solid #203148; border-radius: 10px; padding: 8px 14px; }"));
    auto* stats_layout = new QVBoxLayout(stats_block);
    stats_layout->setContentsMargins(8, 8, 8, 8);
    stats_layout->setSpacing(6);

    const auto make_stat_row = [](const QString& label_text, QLabel*& value_label) {
        auto* row = new QHBoxLayout;
        row->setContentsMargins(0, 0, 0, 0);
        auto* lbl = muted(label_text);
        lbl->setStyleSheet(QStringLiteral("QLabel { color: #7f91ab; font-size: 9pt; }"));
        row->addWidget(lbl);
        row->addSpacing(14);
        value_label = new QLabel(QStringLiteral("0"));
        value_label->setStyleSheet(QStringLiteral("QLabel { color: #e2e8f0; font-weight: 650; font-size: 9.5pt; }"));
        value_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(value_label);
        return row;
    };

    stats_layout->addLayout(make_stat_row(QStringLiteral("Connections"), relation_count_chip_));
    stats_layout->addLayout(make_stat_row(QStringLiteral("Claims"), claim_count_chip_));
    stats_layout->addLayout(make_stat_row(QStringLiteral("Evidence"), evidence_count_chip_));
    stats_layout->addLayout(make_stat_row(QStringLiteral("Notes"), note_count_chip_));

    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QStringLiteral("QFrame { color: #203148; }"));
    stats_layout->addWidget(sep);

    created_time_label_ = muted(QStringLiteral("Created —"));
    created_time_label_->setStyleSheet(QStringLiteral("QLabel { color: #64748b; font-size: 8.5pt; }"));
    stats_layout->addWidget(created_time_label_);

    updated_time_label_ = muted(QStringLiteral("Updated —"));
    updated_time_label_->setStyleSheet(QStringLiteral("QLabel { color: #64748b; font-size: 8.5pt; }"));
    stats_layout->addWidget(updated_time_label_);

    hero_layout->addWidget(stats_block, 0, Qt::AlignVCenter);
    root->addWidget(hero_card_);

    // 3. Body Tabs
    tabs_ = new QTabWidget(this);
    tabs_->setObjectName(QStringLiteral("entityProfileTabs"));
    tabs_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // --- Tab 1: Overview & Background ---
    auto* overview_scroll = new QScrollArea(tabs_);
    overview_scroll->setWidgetResizable(true);
    overview_scroll->setFrameShape(QFrame::NoFrame);
    overview_tab_ = new QWidget(overview_scroll);
    auto* overview_layout = new QVBoxLayout(overview_tab_);
    overview_layout->setContentsMargins(6, 12, 6, 16);
    overview_layout->setSpacing(16);

    // Background Card
    description_card_ = card(overview_tab_);
    auto* desc_layout = new QVBoxLayout(description_card_);
    desc_layout->setContentsMargins(20, 16, 20, 18);
    desc_layout->setSpacing(10);

    auto* desc_header = new QHBoxLayout;
    desc_header->setContentsMargins(0, 0, 0, 0);
    auto* desc_title = heading(QStringLiteral("Background & Analysis"), 16);
    desc_title->setStyleSheet(QStringLiteral("QLabel { font-weight: 650; color: #f1f5f9; }"));
    desc_header->addWidget(desc_title);
    desc_header->addStretch();
    edit_desc_button_ = button(QStringLiteral("Edit description"), false);
    edit_desc_button_->setFixedHeight(28);
    edit_desc_button_->setIcon(ui_icon(UiIcon::Edit, QColor("#cbd5e1")));
    desc_header->addWidget(edit_desc_button_);
    desc_layout->addLayout(desc_header);

    description_label_ = new QLabel(description_card_);
    description_label_->setWordWrap(true);
    description_label_->setStyleSheet(QStringLiteral(
        "QLabel { color: #e2e8f0; font-size: 10.5pt; line-height: 1.6; padding: 6px 0; }"));
    description_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    desc_layout->addWidget(description_label_);

    desc_empty_widget_ = new QWidget(description_card_);
    auto* desc_empty_layout = new QVBoxLayout(desc_empty_widget_);
    desc_empty_layout->setContentsMargins(10, 14, 10, 14);
    desc_empty_layout->setSpacing(8);
    auto* empty_msg = muted(QStringLiteral("No background narrative or descriptive summary recorded for this entity yet."));
    empty_msg->setAlignment(Qt::AlignCenter);
    desc_empty_layout->addWidget(empty_msg);
    auto* add_desc_btn = button(QStringLiteral("＋ Add background description"), false);
    add_desc_btn->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    desc_empty_layout->addWidget(add_desc_btn, 0, Qt::AlignCenter);
    connect(add_desc_btn, &QPushButton::clicked, this, &EntityProfilePage::edit_description);
    desc_layout->addWidget(desc_empty_widget_);

    overview_layout->addWidget(description_card_);

    // Structured Attributes Card
    attributes_card_ = card(overview_tab_);
    auto* attrs_card_layout = new QVBoxLayout(attributes_card_);
    attrs_card_layout->setContentsMargins(20, 16, 20, 18);
    attrs_card_layout->setSpacing(12);

    auto* attrs_header = new QHBoxLayout;
    attrs_header->setContentsMargins(0, 0, 0, 0);
    attributes_card_title_ = heading(QStringLiteral("Attributes & Identifiers"), 16);
    attributes_card_title_->setStyleSheet(QStringLiteral("QLabel { font-weight: 650; color: #f1f5f9; }"));
    attrs_header->addWidget(attributes_card_title_);
    attrs_header->addStretch();
    edit_attrs_button_ = button(QStringLiteral("Edit attributes"), false);
    edit_attrs_button_->setFixedHeight(28);
    edit_attrs_button_->setIcon(ui_icon(UiIcon::Edit, QColor("#cbd5e1")));
    attrs_header->addWidget(edit_attrs_button_);
    attrs_card_layout->addLayout(attrs_header);

    attributes_content_ = new QWidget(attributes_card_);
    attributes_grid_ = new QGridLayout(attributes_content_);
    attributes_grid_->setContentsMargins(0, 4, 0, 4);
    attributes_grid_->setHorizontalSpacing(24);
    attributes_grid_->setVerticalSpacing(10);
    attrs_card_layout->addWidget(attributes_content_);

    attrs_empty_widget_ = new QWidget(attributes_card_);
    auto* attrs_empty_layout = new QVBoxLayout(attrs_empty_widget_);
    attrs_empty_layout->setContentsMargins(10, 10, 10, 10);
    attrs_empty_layout->setSpacing(8);
    auto* empty_attr_msg = muted(QStringLiteral("No specialized attributes or contact identifiers recorded."));
    empty_attr_msg->setAlignment(Qt::AlignCenter);
    attrs_empty_layout->addWidget(empty_attr_msg);
    auto* add_attrs_btn = button(QStringLiteral("＋ Add attributes"), false);
    add_attrs_btn->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    attrs_empty_layout->addWidget(add_attrs_btn, 0, Qt::AlignCenter);
    connect(add_attrs_btn, &QPushButton::clicked, this, &EntityProfilePage::edit_attributes);
    attrs_card_layout->addWidget(attrs_empty_widget_);

    overview_layout->addWidget(attributes_card_);

    // Media Preview Card
    media_preview_card_ = card(overview_tab_);
    auto* media_card_layout = new QVBoxLayout(media_preview_card_);
    media_card_layout->setContentsMargins(20, 16, 20, 18);
    media_card_layout->setSpacing(10);
    auto* media_card_header = new QHBoxLayout;
    auto* media_title = heading(QStringLiteral("Attached Photographs & Media"), 16);
    media_card_header->addWidget(media_title);
    media_card_header->addStretch();
    auto* goto_media_tab = button(QStringLiteral("View gallery →"), false);
    goto_media_tab->setFixedHeight(28);
    connect(goto_media_tab, &QPushButton::clicked, this, [this] { tabs_->setCurrentIndex(4); });
    media_card_header->addWidget(goto_media_tab);
    media_card_layout->addLayout(media_card_header);

    auto* media_preview_widget = new QWidget(media_preview_card_);
    media_preview_layout_ = new QHBoxLayout(media_preview_widget);
    media_preview_layout_->setContentsMargins(0, 4, 0, 4);
    media_preview_layout_->setSpacing(12);
    media_card_layout->addWidget(media_preview_widget);
    overview_layout->addWidget(media_preview_card_);

    overview_layout->addStretch();
    overview_scroll->setWidget(overview_tab_);
    tabs_->addTab(overview_scroll, QStringLiteral("Overview"));

    // --- Tab 2: Connections & Relations ---
    auto* relations_tab = new QWidget(tabs_);
    auto* rel_layout = new QVBoxLayout(relations_tab);
    rel_layout->setContentsMargins(6, 12, 6, 12);
    rel_layout->setSpacing(10);

    auto* rel_filter_bar = new QHBoxLayout;
    rel_filter_bar->setContentsMargins(0, 0, 0, 0);
    rel_filter_bar->setSpacing(8);
    relations_search_ = new QLineEdit(relations_tab);
    relations_search_->setPlaceholderText(QStringLiteral("Search connected entities or relationship types…"));
    relations_search_->addAction(ui_icon(UiIcon::Search), QLineEdit::LeadingPosition);
    relations_search_->setFixedHeight(32);
    rel_filter_bar->addWidget(relations_search_, 1);

    relations_direction_filter_ = new QComboBox(relations_tab);
    relations_direction_filter_->addItem(QStringLiteral("All relations"), QStringLiteral("all"));
    relations_direction_filter_->addItem(QStringLiteral("Outgoing (→)"), QStringLiteral("outgoing"));
    relations_direction_filter_->addItem(QStringLiteral("Incoming (←)"), QStringLiteral("incoming"));
    relations_direction_filter_->setFixedHeight(32);
    rel_filter_bar->addWidget(relations_direction_filter_);

    auto* add_rel_btn = button(QStringLiteral("＋ Connect entity"), true);
    add_rel_btn->setFixedHeight(32);
    connect(add_rel_btn, &QPushButton::clicked, this, &EntityProfilePage::add_connection);
    rel_filter_bar->addWidget(add_rel_btn);
    rel_layout->addLayout(rel_filter_bar);

    relations_table_ = table_with_empty_state(relations_tab);
    configure_table(relations_table_, {QStringLiteral("Dir"), QStringLiteral("Relationship"), QStringLiteral("Connected entity"), QStringLiteral("Status"), QStringLiteral("Evidence"), QStringLiteral("Actions")});
    relations_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    relations_table_->setColumnWidth(0, 50);
    relations_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    relations_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    relations_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    relations_table_->setColumnWidth(3, 115);
    relations_table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    relations_table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    relations_table_->setColumnWidth(5, 160);
    relations_table_->set_empty_state(QStringLiteral("No connections recorded"), QStringLiteral("Relationships to other people, organizations, domains, or accounts appear here."));
    rel_layout->addWidget(relations_table_, 1);
    tabs_->addTab(relations_tab, QStringLiteral("Connections (0)"));

    // --- Tab 3: Claims & Hypotheses ---
    auto* claims_tab = new QWidget(tabs_);
    auto* clm_layout = new QVBoxLayout(claims_tab);
    clm_layout->setContentsMargins(6, 12, 6, 12);
    clm_layout->setSpacing(10);

    auto* clm_filter_bar = new QHBoxLayout;
    clm_filter_bar->setContentsMargins(0, 0, 0, 0);
    clm_filter_bar->setSpacing(8);
    claims_search_ = new QLineEdit(claims_tab);
    claims_search_->setPlaceholderText(QStringLiteral("Search claims, statements, reasoning…"));
    claims_search_->addAction(ui_icon(UiIcon::Search), QLineEdit::LeadingPosition);
    claims_search_->setFixedHeight(32);
    clm_filter_bar->addWidget(claims_search_, 1);

    claims_status_filter_ = new QComboBox(claims_tab);
    claims_status_filter_->addItem(QStringLiteral("All statuses"), QStringLiteral("all"));
    claims_status_filter_->addItem(QStringLiteral("Unverified"), QStringLiteral("unverified"));
    claims_status_filter_->addItem(QStringLiteral("Possible"), QStringLiteral("possible"));
    claims_status_filter_->addItem(QStringLiteral("Probable"), QStringLiteral("probable"));
    claims_status_filter_->addItem(QStringLiteral("Confirmed"), QStringLiteral("confirmed"));
    claims_status_filter_->addItem(QStringLiteral("Refuted"), QStringLiteral("refuted"));
    claims_status_filter_->setFixedHeight(32);
    clm_filter_bar->addWidget(claims_status_filter_);

    auto* add_clm_btn = button(QStringLiteral("＋ Add claim"), true);
    add_clm_btn->setFixedHeight(32);
    connect(add_clm_btn, &QPushButton::clicked, this, &EntityProfilePage::add_claim);
    clm_filter_bar->addWidget(add_clm_btn);
    clm_layout->addLayout(clm_filter_bar);

    claims_table_ = table_with_empty_state(claims_tab);
    configure_table(claims_table_, {QStringLiteral("Status"), QStringLiteral("Kind"), QStringLiteral("Statement"), QStringLiteral("Role"), QStringLiteral("Evidence"), QStringLiteral("Actions")});
    claims_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    claims_table_->setColumnWidth(0, 115);
    claims_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    claims_table_->setColumnWidth(1, 100);
    claims_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    claims_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    claims_table_->setColumnWidth(3, 90);
    claims_table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    claims_table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    claims_table_->setColumnWidth(5, 140);
    claims_table_->set_empty_state(QStringLiteral("No claims yet"), QStringLiteral("Assessments, observations, and hypotheses involving this entity appear here."));
    clm_layout->addWidget(claims_table_, 1);
    tabs_->addTab(claims_tab, QStringLiteral("Claims (0)"));

    // --- Tab 4: Evidence & Sources ---
    auto* evidence_tab = new QWidget(tabs_);
    auto* ev_layout = new QVBoxLayout(evidence_tab);
    ev_layout->setContentsMargins(6, 12, 6, 12);
    ev_layout->setSpacing(10);

    evidence_table_ = table_with_empty_state(evidence_tab);
    configure_table(evidence_table_, {QStringLiteral("Kind"), QStringLiteral("Evidence"), QStringLiteral("Source"), QStringLiteral("Integrity"), QStringLiteral("Role")});
    evidence_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    evidence_table_->setColumnWidth(0, 110);
    evidence_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    evidence_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    evidence_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    evidence_table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    evidence_table_->setColumnWidth(4, 110);
    evidence_table_->set_empty_state(QStringLiteral("No evidence linked"), QStringLiteral("Sources and evidence linked to this entity's claims appear here."));
    ev_layout->addWidget(evidence_table_, 1);
    tabs_->addTab(evidence_tab, QStringLiteral("Evidence (0)"));

    // --- Tab 5: Media Gallery ---
    auto* media_tab = new QWidget(tabs_);
    auto* media_tab_layout = new QVBoxLayout(media_tab);
    media_tab_layout->setContentsMargins(6, 12, 6, 12);
    media_tab_layout->setSpacing(10);

    auto* media_action_bar = new QHBoxLayout;
    auto* add_img_btn = button(QStringLiteral("＋ Add image from file"), false);
    add_img_btn->setFixedHeight(32);
    connect(add_img_btn, &QPushButton::clicked, this, &EntityProfilePage::add_image_from_file);
    media_action_bar->addWidget(add_img_btn);

    auto* paste_img_btn = button(QStringLiteral("Paste image (Ctrl+V)"), false);
    paste_img_btn->setFixedHeight(32);
    connect(paste_img_btn, &QPushButton::clicked, this, &EntityProfilePage::paste_image_from_clipboard);
    media_action_bar->addWidget(paste_img_btn);

    auto* remove_img_btn = button(QStringLiteral("Remove selected image"), false);
    remove_img_btn->setFixedHeight(32);
    connect(remove_img_btn, &QPushButton::clicked, this, &EntityProfilePage::remove_selected_image);
    media_action_bar->addWidget(remove_img_btn);
    media_action_bar->addStretch();
    media_tab_layout->addLayout(media_action_bar);

    auto* media_scroll = new QScrollArea(media_tab);
    media_scroll->setWidgetResizable(true);
    media_scroll->setFrameShape(QFrame::NoFrame);
    media_gallery_grid_ = new QWidget(media_scroll);
    media_gallery_layout_ = new QGridLayout(media_gallery_grid_);
    media_gallery_layout_->setContentsMargins(8, 8, 8, 8);
    media_gallery_layout_->setSpacing(14);
    media_scroll->setWidget(media_gallery_grid_);
    media_tab_layout->addWidget(media_scroll, 1);
    tabs_->addTab(media_tab, QStringLiteral("Media (0)"));

    // --- Tab 6: Notes ---
    auto* notes_tab = new QWidget(tabs_);
    auto* notes_tab_layout = new QVBoxLayout(notes_tab);
    notes_tab_layout->setContentsMargins(6, 12, 6, 12);
    notes_tab_layout->setSpacing(10);

    auto* notes_action_bar = new QHBoxLayout;
    auto* add_note_btn = button(QStringLiteral("＋ Add note"), true);
    add_note_btn->setFixedHeight(32);
    connect(add_note_btn, &QPushButton::clicked, this, &EntityProfilePage::add_note);
    notes_action_bar->addWidget(add_note_btn);
    notes_action_bar->addStretch();
    notes_tab_layout->addLayout(notes_action_bar);

    auto* notes_scroll = new QScrollArea(notes_tab);
    notes_scroll->setWidgetResizable(true);
    notes_scroll->setFrameShape(QFrame::NoFrame);
    notes_list_ = new QWidget(notes_scroll);
    notes_layout_ = new QVBoxLayout(notes_list_);
    notes_layout_->setContentsMargins(8, 8, 8, 8);
    notes_layout_->setSpacing(10);
    notes_scroll->setWidget(notes_list_);
    notes_tab_layout->addWidget(notes_scroll, 1);
    tabs_->addTab(notes_tab, QStringLiteral("Notes (0)"));

    // --- Tab 7: History ---
    auto* history_tab = new QWidget(tabs_);
    auto* hist_layout = new QVBoxLayout(history_tab);
    hist_layout->setContentsMargins(6, 12, 6, 12);
    hist_layout->setSpacing(10);

    history_table_ = table_with_empty_state(history_tab);
    configure_table(history_table_, {QStringLiteral("When"), QStringLiteral("Action"), QStringLiteral("Description")});
    history_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    history_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    history_table_->setColumnWidth(1, 120);
    history_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    history_table_->set_empty_state(QStringLiteral("No activity yet"), QStringLiteral("Changes and updates to this entity are logged here."));
    hist_layout->addWidget(history_table_, 1);
    tabs_->addTab(history_tab, QStringLiteral("History (0)"));

    root->addWidget(tabs_, 1);

    // Setup action connections
    connect(back_button_, &QPushButton::clicked, this, [this] {
        if (!history_stack_.empty()) {
            const auto prev = history_stack_.back();
            history_stack_.pop_back();
            set_entity(prev, source_page_);
            return;
        }
        if (source_page_ == 2 && on_back_to_graph) on_back_to_graph();
        else if (source_page_ == 1 && on_back_to_entities) on_back_to_entities();
        else if (source_page_ == 0 && on_back_to_overview) on_back_to_overview();
        else if (on_back_to_graph) on_back_to_graph();
    });

    connect(view_in_graph_button_, &QPushButton::clicked, this, [this] {
        if (on_view_in_graph) on_view_in_graph(entity_id_);
    });

    connect(edit_profile_button_, &QPushButton::clicked, this, &EntityProfilePage::edit_profile);
    connect(edit_desc_button_, &QPushButton::clicked, this, &EntityProfilePage::edit_description);
    connect(edit_attrs_button_, &QPushButton::clicked, this, &EntityProfilePage::edit_attributes);
    connect(connect_button_, &QPushButton::clicked, this, &EntityProfilePage::add_connection);
    connect(add_claim_button_, &QPushButton::clicked, this, &EntityProfilePage::add_claim);
    connect(add_note_button_, &QPushButton::clicked, this, &EntityProfilePage::add_note);

    connect(relations_search_, &QLineEdit::textChanged, this, [this] { refresh(); });
    connect(relations_direction_filter_, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
    connect(claims_search_, &QLineEdit::textChanged, this, [this] { refresh(); });
    connect(claims_status_filter_, &QComboBox::currentIndexChanged, this, [this] { refresh(); });

    // Table context menus
    relations_table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(relations_table_, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        const auto index = relations_table_->indexAt(pos);
        if (!index.isValid()) return;
        relations_table_->selectRow(index.row());
        const auto claim_id = s(relations_table_->item(index.row(), 1)->data(Qt::UserRole).toString());
        const auto target_id = s(relations_table_->item(index.row(), 2)->data(Qt::UserRole).toString());
        QMenu menu(this);
        auto* open_target = menu.addAction(QStringLiteral("Open connected entity profile"));
        auto* view_graph = menu.addAction(QStringLiteral("View relation in graph"));
        auto* change_st = menu.addAction(QStringLiteral("Change verification status…"));
        menu.addSeparator();
        auto* del_rel = menu.addAction(QStringLiteral("Remove connection"));
        const auto chosen = menu.exec(relations_table_->viewport()->mapToGlobal(pos));
        if (chosen == open_target && !target_id.empty()) {
            history_stack_.push_back(entity_id_);
            set_entity(target_id, source_page_);
        } else if (chosen == view_graph && on_view_claim_in_graph) {
            on_view_claim_in_graph(claim_id);
        } else if (chosen == change_st) {
            change_relation_status(claim_id);
        } else if (chosen == del_rel) {
            if (QMessageBox::question(this, QStringLiteral("Remove connection"), QStringLiteral("Remove this relationship between entities?"), QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
                try {
                    context_.investigation.delete_claim(claim_id, true);
                    refresh();
                    if (on_entity_modified) on_entity_modified();
                } catch (const std::exception& err) {
                    show_error(this, err);
                }
            }
        }
    });

    connect(relations_table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        const auto target_id = s(relations_table_->item(row, 2)->data(Qt::UserRole).toString());
        if (!target_id.empty()) {
            history_stack_.push_back(entity_id_);
            set_entity(target_id, source_page_);
        }
    });

    claims_table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(claims_table_, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        const auto index = claims_table_->indexAt(pos);
        if (!index.isValid()) return;
        claims_table_->selectRow(index.row());
        const auto claim_id = s(claims_table_->item(index.row(), 2)->data(Qt::UserRole).toString());
        QMenu menu(this);
        auto* view_graph = menu.addAction(QStringLiteral("View in graph"));
        auto* change_st = menu.addAction(QStringLiteral("Change status…"));
        auto* link_ev = menu.addAction(QStringLiteral("Link evidence…"));
        const auto chosen = menu.exec(claims_table_->viewport()->mapToGlobal(pos));
        if (chosen == view_graph && on_view_claim_in_graph) {
            on_view_claim_in_graph(claim_id);
        } else if (chosen == change_st) {
            change_claim_status(claim_id);
        } else if (chosen == link_ev) {
            link_claim_evidence(claim_id);
        }
    });

    connect(claims_table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        const auto claim_id = s(claims_table_->item(row, 2)->data(Qt::UserRole).toString());
        if (on_view_claim_in_graph) on_view_claim_in_graph(claim_id);
    });
}

void EntityProfilePage::set_entity(const domain::Id& entity_id, int source_page) {
    entity_id_ = entity_id;
    source_page_ = source_page;
    update_navigation_buttons();
    refresh();
}

QString EntityProfilePage::current_entity_label() const {
    if (entity_id_.empty()) return QStringLiteral("Entity Profile");
    try {
        const auto entity = context_.investigation.get_entity(entity_id_);
        return q(entity.label);
    } catch (...) {
        return QStringLiteral("Entity Profile");
    }
}

void EntityProfilePage::update_navigation_buttons() {
    if (!history_stack_.empty()) {
        try {
            const auto prev = context_.investigation.get_entity(history_stack_.back());
            back_button_->setText(QStringLiteral("← Back to %1").arg(q(prev.label)));
        } catch (...) {
            back_button_->setText(QStringLiteral("← Back"));
        }
    } else {
        if (source_page_ == 2) back_button_->setText(QStringLiteral("← Back to Graph"));
        else if (source_page_ == 1) back_button_->setText(QStringLiteral("← Back to Entities"));
        else if (source_page_ == 0) back_button_->setText(QStringLiteral("← Back to Overview"));
        else back_button_->setText(QStringLiteral("← Back"));
    }
}

void EntityProfilePage::refresh() {
    if (entity_id_.empty()) return;
    try {
        const auto entity = context_.investigation.get_entity(entity_id_);
        update_navigation_buttons();

        // Breadcrumb
        breadcrumb_label_->setText(QStringLiteral("Case: %1  /  Entity: %2")
            .arg(q(context_.cases.get(case_id_).title), q(entity.label)));

        // Hero Card
        title_label_->setText(q(entity.label));
        type_badge_->setText(enum_label(domain::to_string(entity.type)));

        // Original observed value
        if (!entity.original_value.empty() && entity.original_value != entity.label) {
            original_value_label_->setText(QStringLiteral("Observed: %1").arg(q(entity.original_value)));
            original_value_label_->show();
        } else {
            original_value_label_->hide();
        }

        // Possible duplicate warning
        const auto duplicates = context_.investigation.possible_duplicates(case_id_, entity.type, entity.original_value.empty() ? entity.label : entity.original_value);
        duplicate_warning_->setVisible(duplicates.size() > 1);

        // Tags
        clear_layout(qobject_cast<QBoxLayout*>(tags_widget_->layout()));
        auto* tags_layout = qobject_cast<QBoxLayout*>(tags_widget_->layout());
        const auto tags = json_strings(entity.tags_json);
        tags_widget_->setVisible(!tags.empty());
        for (const auto& tag : tags) {
            auto* chip = make_tag_chip(q(tag), tags_widget_);
            tags_layout->addWidget(chip);
        }
        tags_layout->addStretch();

        // Aliases
        const auto aliases = json_strings(entity.aliases_json);
        if (!aliases.empty()) {
            aliases_label_->setText(QStringLiteral("Aliases: %1").arg(join_values(aliases)));
            aliases_label_->show();
        } else {
            aliases_label_->hide();
        }

        // Photo / Avatar
        current_images_ = json_strings(entity.image_paths_json);
        if (!current_images_.empty()) {
            const auto img_path = context_.store.absolute_path(current_images_.front());
            const auto image = QImage(QString::fromStdString(img_path.string()));
            if (!image.isNull()) {
                photo_label_->setPixmap(QPixmap::fromImage(image).scaled(100, 100, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
            } else {
                photo_label_->setPixmap(ui_icon(entity_icon(entity.type), appearance_accent_color().lighter(160)).pixmap(QSize(44, 44)));
            }
        } else {
            photo_label_->setPixmap(ui_icon(entity_icon(entity.type), appearance_accent_color().lighter(160)).pixmap(QSize(44, 44)));
        }

        // Details JSON
        const auto details = parse_entity_details(entity.details_json);

        // Description
        const auto desc_text = q(details.description).trimmed();
        if (!desc_text.isEmpty()) {
            description_label_->setText(desc_text);
            description_label_->show();
            desc_empty_widget_->hide();
            edit_desc_button_->show();
        } else {
            description_label_->hide();
            desc_empty_widget_->show();
            edit_desc_button_->hide();
        }

        // Connected entities and claims
        const auto all_entities = context_.investigation.list_entities(case_id_);
        std::unordered_map<domain::Id, domain::Entity> entity_map;
        for (const auto& item : all_entities) entity_map.emplace(item.id, item);

        const auto claims = context_.investigation.claims_for_entity(entity_id_);
        const auto all_evidence = context_.investigation.list_evidence(case_id_);
        std::unordered_map<domain::Id, domain::Evidence> evidence_map;
        for (const auto& ev : all_evidence) evidence_map.emplace(ev.id, ev);

        const auto all_sources = context_.investigation.list_sources(case_id_);
        std::unordered_map<domain::Id, domain::Source> source_map;
        for (const auto& sc : all_sources) source_map.emplace(sc.id, sc);

        // Build Structured Attributes
        clear_layout(attributes_grid_);
        int attr_row = 0;

        const auto add_attr_field = [&](const QString& label, const QString& value, bool copyable = false) {
            if (value.trimmed().isEmpty()) return;
            auto* lbl = muted(label);
            lbl->setStyleSheet(QStringLiteral("QLabel { color: #8fa0ba; font-size: 9pt; font-weight: 600; }"));
            attributes_grid_->addWidget(lbl, attr_row, 0, Qt::AlignTop);

            auto* val = new QLabel(value);
            val->setWordWrap(true);
            val->setTextInteractionFlags(Qt::TextSelectableByMouse);
            val->setStyleSheet(copyable
                ? QStringLiteral("QLabel { color: #38bdf8; font-family: monospace; font-size: 9.5pt; }")
                : QStringLiteral("QLabel { color: #f1f5f9; font-size: 9.5pt; }"));
            attributes_grid_->addWidget(val, attr_row, 1);
            ++attr_row;
        };

        const auto add_list_field = [&](const QString& label, const std::vector<std::string>& list, bool copyable = false) {
            if (list.empty()) return;
            add_attr_field(label, join_values(list), copyable);
        };

        // Populate connected contact entities if present
        std::vector<std::string> connected_phones = details.phone_numbers;
        std::vector<std::string> connected_emails = details.emails;
        std::vector<std::string> connected_usernames = details.usernames;
        std::vector<std::string> connected_domains;

        for (const auto& claim : claims) {
            if (!claim.predicate) continue;
            for (const auto& link : context_.investigation.claim_entities(claim.id)) {
                if (link.entity_id == entity_id_) continue;
                const auto it = entity_map.find(link.entity_id);
                if (it == entity_map.end()) continue;
                const auto& other = it->second;
                if (other.type == domain::EntityType::Phone) {
                    if (std::find(connected_phones.begin(), connected_phones.end(), other.original_value) == connected_phones.end())
                        connected_phones.push_back(other.original_value.empty() ? other.label : other.original_value);
                } else if (other.type == domain::EntityType::Email) {
                    if (std::find(connected_emails.begin(), connected_emails.end(), other.original_value) == connected_emails.end())
                        connected_emails.push_back(other.original_value.empty() ? other.label : other.original_value);
                } else if (other.type == domain::EntityType::Username) {
                    if (std::find(connected_usernames.begin(), connected_usernames.end(), other.original_value) == connected_usernames.end())
                        connected_usernames.push_back(other.original_value.empty() ? other.label : other.original_value);
                } else if (other.type == domain::EntityType::Domain) {
                    if (std::find(connected_domains.begin(), connected_domains.end(), other.original_value) == connected_domains.end())
                        connected_domains.push_back(other.original_value.empty() ? other.label : other.original_value);
                }
            }
        }

        // Adaptive presentation by type
        if (entity.type == domain::EntityType::Person) {
            attributes_card_title_->setText(QStringLiteral("Person Details & Contact Identifiers"));
            add_list_field(QStringLiteral("Phone numbers"), connected_phones, true);
            add_list_field(QStringLiteral("Email addresses"), connected_emails, true);
            add_list_field(QStringLiteral("Usernames & handles"), connected_usernames, true);
            add_list_field(QStringLiteral("Other identifiers"), details.identifiers, true);
            add_attr_field(QStringLiteral("Residence / Location"), q(details.location));
            add_attr_field(QStringLiteral("Date of birth / Age"), q(details.date_of_birth));
            add_attr_field(QStringLiteral("Nationality / Citizenship"), q(details.nationality));
            add_attr_field(QStringLiteral("Occupation / Role"), q(details.occupation));
        } else if (entity.type == domain::EntityType::Company) {
            attributes_card_title_->setText(QStringLiteral("Corporate Profile & Organizational Details"));
            add_attr_field(QStringLiteral("Registration Number / Tax ID"), q(details.registration_number), true);
            add_attr_field(QStringLiteral("Jurisdiction / Country"), q(details.jurisdiction));
            add_attr_field(QStringLiteral("Headquarters / Address"), q(details.location));
            add_attr_field(QStringLiteral("Official Website"), q(details.website), true);
            add_list_field(QStringLiteral("Associated domains"), connected_domains, true);
            add_list_field(QStringLiteral("Contact phones"), connected_phones, true);
            add_list_field(QStringLiteral("Contact emails"), connected_emails, true);
        } else if (entity.type == domain::EntityType::Domain || entity.type == domain::EntityType::Url || entity.type == domain::EntityType::Ip) {
            attributes_card_title_->setText(QStringLiteral("Network & Technical Intelligence"));
            add_attr_field(QStringLiteral("Target URL / Host"), q(details.website.empty() ? entity.original_value : details.website), true);
            add_attr_field(QStringLiteral("Resolved IP Address"), q(details.ip_address), true);
            add_attr_field(QStringLiteral("Registrar / Nameservers"), q(details.registrar));
            add_attr_field(QStringLiteral("Hosting Provider / ASN"), q(details.hosting_provider));
        } else if (entity.type == domain::EntityType::Username || entity.type == domain::EntityType::Account) {
            attributes_card_title_->setText(QStringLiteral("Account & Profile Attributes"));
            add_attr_field(QStringLiteral("Platform / Network"), q(details.platform));
            add_attr_field(QStringLiteral("Profile URL"), q(details.profile_url), true);
            add_list_field(QStringLiteral("Linked emails"), connected_emails, true);
            add_list_field(QStringLiteral("Linked phones"), connected_phones, true);
        } else {
            attributes_card_title_->setText(QStringLiteral("Entity Attributes & Properties"));
            add_attr_field(QStringLiteral("Location / Place"), q(details.location));
            add_attr_field(QStringLiteral("Reference link"), q(details.website), true);
            add_list_field(QStringLiteral("Identifiers"), details.identifiers, true);
        }

        // Custom attributes
        for (const auto& [k, v] : details.custom_attributes) {
            add_attr_field(title_case(q(k)), q(v));
        }

        const bool has_attrs = attr_row > 0;
        attributes_content_->setVisible(has_attrs);
        attrs_empty_widget_->setVisible(!has_attrs);

        // Media Preview Card
        clear_layout(media_preview_layout_);
        if (!current_images_.empty()) {
            media_preview_card_->show();
            for (const auto& rel_path : current_images_) {
                const auto abs_path = context_.store.absolute_path(rel_path);
                const auto img = QImage(QString::fromStdString(abs_path.string()));
                if (img.isNull()) continue;
                auto* thumb = new QLabel;
                thumb->setFixedSize(90, 90);
                thumb->setStyleSheet(QStringLiteral("QLabel { background: #111a27; border: 1px solid #24354c; border-radius: 8px; }"));
                thumb->setAlignment(Qt::AlignCenter);
                thumb->setPixmap(QPixmap::fromImage(img).scaled(86, 86, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                thumb->setCursor(Qt::PointingHandCursor);
                thumb->setToolTip(QStringLiteral("Click to view full image"));
                media_preview_layout_->addWidget(thumb);
            }
            media_preview_layout_->addStretch();
        } else {
            media_preview_card_->hide();
        }

        // Populate Tab 2: Connections & Relations
        relations_table_->setRowCount(0);
        const auto rel_search = relations_search_->text().trimmed().toLower();
        const auto rel_dir = relations_direction_filter_->currentData().toString();
        int total_relations = 0;

        for (const auto& claim : claims) {
            if (!claim.predicate) continue;
            const auto entity_links = context_.investigation.claim_entities(claim.id);
            domain::Id other_id;
            QString direction = QStringLiteral("—");
            bool is_outgoing = false;
            bool is_incoming = false;

            for (const auto& link : entity_links) {
                if (link.entity_id == entity_id_) {
                    if (link.role == domain::ClaimEntityRole::Subject) is_outgoing = true;
                    else if (link.role == domain::ClaimEntityRole::Object) is_incoming = true;
                } else {
                    other_id = link.entity_id;
                }
            }

            if (other_id.empty()) continue;
            const auto other_it = entity_map.find(other_id);
            if (other_it == entity_map.end()) continue;
            const auto& other_entity = other_it->second;

            if (is_outgoing) direction = QStringLiteral("→");
            else if (is_incoming) direction = QStringLiteral("←");

            if (rel_dir == "outgoing" && !is_outgoing) continue;
            if (rel_dir == "incoming" && !is_incoming) continue;

            const auto pred_label = human_readable_predicate(q(*claim.predicate));
            const auto haystack = QStringLiteral("%1 %2 %3").arg(q(other_entity.label), pred_label, enum_label(domain::to_string(other_entity.type))).toLower();
            if (!rel_search.isEmpty() && !haystack.contains(rel_search)) continue;

            ++total_relations;
            const auto row = relations_table_->rowCount();
            relations_table_->insertRow(row);

            // Dir
            auto* dir_item = profile_cell(direction);
            dir_item->setTextAlignment(Qt::AlignCenter);
            dir_item->setForeground(is_outgoing ? QColor("#38bdf8") : is_incoming ? QColor("#a78bfa") : QColor("#94a3b8"));
            relations_table_->setItem(row, 0, dir_item);

            // Predicate
            auto* pred_item = profile_cell(pred_label, q(claim.id));
            pred_item->setToolTip(QStringLiteral("%1 (%2)").arg(pred_label, q(*claim.predicate)));
            relations_table_->setItem(row, 1, pred_item);

            // Connected entity
            const auto other_type_str = enum_label(domain::to_string(other_entity.type));
            auto* other_item = profile_cell(QStringLiteral("%1  (%2)").arg(q(other_entity.label), other_type_str), q(other_entity.id));
            set_accent_icon(other_item, entity_icon(other_entity.type));
            relations_table_->setItem(row, 2, other_item);

            // Status
            relations_table_->setCellWidget(row, 3, status_badge(domain::to_string(claim.status)));

            // Evidence count
            const auto ev_links = context_.investigation.claim_evidence(claim.id);
            auto* ev_cell = profile_cell(ev_links.empty() ? QStringLiteral("No evidence") : QStringLiteral("%1 item%2").arg(ev_links.size()).arg(ev_links.size() == 1 ? "" : "s"));
            relations_table_->setItem(row, 4, ev_cell);

            // Action widget
            auto* actions_widget = new QWidget;
            auto* act_layout = new QHBoxLayout(actions_widget);
            act_layout->setContentsMargins(4, 2, 4, 2);
            act_layout->setSpacing(6);

            auto* open_btn = button(QStringLiteral("Profile"), false);
            open_btn->setFixedHeight(24);
            connect(open_btn, &QPushButton::clicked, this, [this, other_id] {
                history_stack_.push_back(entity_id_);
                set_entity(other_id, source_page_);
            });
            act_layout->addWidget(open_btn);

            auto* graph_btn = button(QStringLiteral("Graph"), false);
            graph_btn->setFixedHeight(24);
            connect(graph_btn, &QPushButton::clicked, this, [this, claim_id = claim.id] {
                if (on_view_claim_in_graph) on_view_claim_in_graph(claim_id);
            });
            act_layout->addWidget(graph_btn);

            relations_table_->setCellWidget(row, 5, actions_widget);
        }

        tabs_->setTabText(1, QStringLiteral("Connections (%1)").arg(total_relations));
        relation_count_chip_->setText(QString::number(total_relations));

        // Populate Tab 3: Claims & Hypotheses
        claims_table_->setRowCount(0);
        const auto clm_search = claims_search_->text().trimmed().toLower();
        const auto clm_status = claims_status_filter_->currentData().toString();
        int total_claims = 0;
        std::set<domain::Id> linked_evidence_ids;

        for (const auto& claim : claims) {
            if (clm_status != "all" && q(domain::to_string(claim.status)) != clm_status) continue;
            const auto haystack = QStringLiteral("%1 %2").arg(q(claim.statement), q(claim.reasoning)).toLower();
            if (!clm_search.isEmpty() && !haystack.contains(clm_search)) continue;

            ++total_claims;
            const auto row = claims_table_->rowCount();
            claims_table_->insertRow(row);

            // Status
            claims_table_->setCellWidget(row, 0, status_badge(domain::to_string(claim.status)));

            // Kind
            auto* kind_item = profile_cell(claim.kind == domain::ClaimKind::Observation ? QStringLiteral("Observation") : QStringLiteral("Inference"));
            kind_item->setForeground(claim.kind == domain::ClaimKind::Observation ? QColor("#86efac") : QColor("#c084fc"));
            claims_table_->setItem(row, 1, kind_item);

            // Statement
            auto* stmt_item = profile_cell(q(claim.statement), q(claim.id));
            if (!claim.reasoning.empty()) stmt_item->setToolTip(QStringLiteral("%1\n\nReasoning: %2").arg(q(claim.statement), q(claim.reasoning)));
            claims_table_->setItem(row, 2, stmt_item);

            // Role
            QString role_str = QStringLiteral("Context");
            for (const auto& link : context_.investigation.claim_entities(claim.id)) {
                if (link.entity_id == entity_id_) {
                    if (link.role == domain::ClaimEntityRole::Subject) role_str = QStringLiteral("Subject");
                    else if (link.role == domain::ClaimEntityRole::Object) role_str = QStringLiteral("Object");
                    break;
                }
            }
            claims_table_->setItem(row, 3, profile_cell(role_str));

            // Evidence
            const auto ev_links = context_.investigation.claim_evidence(claim.id);
            for (const auto& l : ev_links) linked_evidence_ids.insert(l.evidence_id);
            int sup = 0, con = 0;
            for (const auto& l : ev_links) {
                if (l.role == domain::EvidenceRole::Supports) ++sup;
                else if (l.role == domain::EvidenceRole::Contradicts) ++con;
            }
            QString ev_summary = ev_links.empty() ? QStringLiteral("No evidence") : QStringLiteral("%1 items").arg(ev_links.size());
            if (sup > 0 && con > 0) ev_summary = QStringLiteral("✓%1 · ✗%2").arg(sup).arg(con);
            else if (sup > 0) ev_summary = QStringLiteral("✓ %1 support").arg(sup);
            else if (con > 0) ev_summary = QStringLiteral("✗ %1 contradict").arg(con);

            claims_table_->setItem(row, 4, profile_cell(ev_summary));

            // Actions
            auto* act_widget = new QWidget;
            auto* act_layout = new QHBoxLayout(act_widget);
            act_layout->setContentsMargins(4, 2, 4, 2);
            act_layout->setSpacing(6);

            auto* st_btn = button(QStringLiteral("Status"), false);
            st_btn->setFixedHeight(24);
            connect(st_btn, &QPushButton::clicked, this, [this, claim_id = claim.id] { change_claim_status(claim_id); });
            act_layout->addWidget(st_btn);

            auto* gr_btn = button(QStringLiteral("Graph"), false);
            gr_btn->setFixedHeight(24);
            connect(gr_btn, &QPushButton::clicked, this, [this, claim_id = claim.id] {
                if (on_view_claim_in_graph) on_view_claim_in_graph(claim_id);
            });
            act_layout->addWidget(gr_btn);

            claims_table_->setCellWidget(row, 5, act_widget);
        }

        tabs_->setTabText(2, QStringLiteral("Claims (%1)").arg(total_claims));
        claim_count_chip_->setText(QString::number(total_claims));

        // Populate Tab 4: Evidence & Sources
        evidence_table_->setRowCount(0);
        for (const auto& ev_id : linked_evidence_ids) {
            const auto found = evidence_map.find(ev_id);
            if (found == evidence_map.end()) continue;
            const auto& ev = found->second;
            const auto row = evidence_table_->rowCount();
            evidence_table_->insertRow(row);

            // Kind
            auto* kind_item = profile_cell(enum_label(domain::to_string(ev.kind)));
            set_accent_icon(kind_item, evidence_icon(ev.kind));
            evidence_table_->setItem(row, 0, kind_item);

            // Evidence label / quotation
            QString label_str = ev.text_content.empty() ? (ev.original_filename ? q(*ev.original_filename) : (ev.source_url ? q(*ev.source_url) : shorten_id(ev.id))) : q(ev.text_content);
            if (label_str.length() > 90) label_str = label_str.left(87) + QStringLiteral("…");
            auto* ev_item = profile_cell(label_str, q(ev.id));
            if (!ev.note.empty()) ev_item->setToolTip(QStringLiteral("%1\n\nNote: %2").arg(label_str, q(ev.note)));
            evidence_table_->setItem(row, 1, ev_item);

            // Source
            QString source_title = QStringLiteral("—");
            if (ev.source_id) {
                const auto sc_it = source_map.find(*ev.source_id);
                if (sc_it != source_map.end()) source_title = q(sc_it->second.title.empty() ? sc_it->second.locator : sc_it->second.title);
            }
            evidence_table_->setItem(row, 2, profile_cell(source_title));

            // Integrity
            QString integ = ev.kind == domain::EvidenceKind::Url ? QStringLiteral("External URL") : (ev.sha256 ? QStringLiteral("SHA-256 stored") : QStringLiteral("Not preserved"));
            evidence_table_->setItem(row, 3, profile_cell(integ));

            // Role
            evidence_table_->setItem(row, 4, profile_cell(QStringLiteral("Referenced")));
        }
        tabs_->setTabText(3, QStringLiteral("Evidence (%1)").arg(linked_evidence_ids.size()));
        evidence_count_chip_->setText(QString::number(linked_evidence_ids.size()));

        // Populate Tab 5: Media Gallery
        clear_layout(media_gallery_layout_);
        int img_col = 0, img_row = 0;
        for (const auto& rel_path : current_images_) {
            const auto abs_path = context_.store.absolute_path(rel_path);
            const auto img = QImage(QString::fromStdString(abs_path.string()));
            if (img.isNull()) continue;

            auto* img_card = card(media_gallery_grid_);
            auto* card_layout = new QVBoxLayout(img_card);
            card_layout->setContentsMargins(10, 10, 10, 10);
            card_layout->setSpacing(6);

            auto* img_lbl = new QLabel(img_card);
            img_lbl->setFixedSize(180, 140);
            img_lbl->setAlignment(Qt::AlignCenter);
            img_lbl->setStyleSheet(QStringLiteral("QLabel { background: #0c1420; border-radius: 6px; }"));
            img_lbl->setPixmap(QPixmap::fromImage(img).scaled(176, 136, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            card_layout->addWidget(img_lbl);

            auto* path_lbl = muted(QString::fromStdString(std::filesystem::path(rel_path).filename().string()));
            path_lbl->setAlignment(Qt::AlignCenter);
            card_layout->addWidget(path_lbl);

            auto* view_btn = button(QStringLiteral("Enlarge"), false);
            view_btn->setFixedHeight(26);
            connect(view_btn, &QPushButton::clicked, this, [this, q_rel = q(rel_path)] { view_image_fullscreen(q_rel); });
            card_layout->addWidget(view_btn);

            media_gallery_layout_->addWidget(img_card, img_row, img_col);
            ++img_col;
            if (img_col >= 4) {
                img_col = 0;
                ++img_row;
            }
        }
        tabs_->setTabText(4, QStringLiteral("Media (%1)").arg(current_images_.size()));

        // Populate Tab 6: Notes
        clear_layout(notes_layout_);
        const auto notes = context_.investigation.list_notes(case_id_);
        int entity_note_count = 0;
        for (const auto& note : notes) {
            const auto links = context_.investigation.note_entity_links(note.id);
            if (std::find(links.begin(), links.end(), entity_id_) == links.end()) continue;
            ++entity_note_count;

            auto* note_card = card(notes_list_);
            auto* card_layout = new QVBoxLayout(note_card);
            card_layout->setContentsMargins(16, 12, 16, 14);
            card_layout->setSpacing(6);

            auto* note_head = new QHBoxLayout;
            auto* note_title = heading(note.title.empty() ? QStringLiteral("Untitled note") : q(note.title), 14);
            note_title->setStyleSheet(QStringLiteral("QLabel { font-weight: 650; color: #f1f5f9; }"));
            note_head->addWidget(note_title);
            note_head->addStretch();
            auto* note_time = muted(relative_time_str(note.updated_at));
            note_time->setToolTip(display_time(note.updated_at));
            note_head->addWidget(note_time);
            card_layout->addLayout(note_head);

            auto* note_body = new QLabel(q(note.body));
            note_body->setWordWrap(true);
            note_body->setTextInteractionFlags(Qt::TextSelectableByMouse);
            note_body->setStyleSheet(QStringLiteral("QLabel { color: #cbd5e1; font-size: 10pt; line-height: 1.5; }"));
            card_layout->addWidget(note_body);

            notes_layout_->addWidget(note_card);
        }

        if (entity_note_count == 0) {
            auto* empty_note_card = card(notes_list_);
            auto* enc_layout = new QVBoxLayout(empty_note_card);
            enc_layout->setContentsMargins(20, 20, 20, 20);
            enc_layout->setSpacing(8);
            auto* empty_msg = muted(QStringLiteral("No dedicated investigation notes linked to this entity yet."));
            empty_msg->setAlignment(Qt::AlignCenter);
            enc_layout->addWidget(empty_msg);
            auto* quick_add_note = button(QStringLiteral("＋ Add note now"), false);
            quick_add_note->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
            connect(quick_add_note, &QPushButton::clicked, this, &EntityProfilePage::add_note);
            enc_layout->addWidget(quick_add_note, 0, Qt::AlignCenter);
            notes_layout_->addWidget(empty_note_card);
        }

        notes_layout_->addStretch();
        tabs_->setTabText(5, QStringLiteral("Notes (%1)").arg(entity_note_count));
        note_count_chip_->setText(QString::number(entity_note_count));

        // Populate Tab 7: History
        history_table_->setRowCount(0);
        int hist_count = 0;
        for (const auto& ev : context_.investigation.activity(case_id_)) {
            if (ev.object_id && *ev.object_id == entity_id_) {
                ++hist_count;
                const auto row = history_table_->rowCount();
                history_table_->insertRow(row);
                history_table_->setItem(row, 0, profile_cell(display_time(ev.occurred_at)));
                history_table_->setItem(row, 1, profile_cell(enum_label(ev.action)));
                history_table_->setItem(row, 2, profile_cell(q(ev.description)));
            }
        }
        tabs_->setTabText(6, QStringLiteral("History (%1)").arg(hist_count));

        // Timestamps
        created_time_label_->setText(QStringLiteral("Created %1").arg(relative_time_str(entity.created_at)));
        created_time_label_->setToolTip(display_time(entity.created_at));
        updated_time_label_->setText(QStringLiteral("Updated %1").arg(relative_time_str(entity.updated_at)));
        updated_time_label_->setToolTip(display_time(entity.updated_at));

    } catch (const std::exception& err) {
        show_error(this, err);
    }
}

void EntityProfilePage::edit_profile() {
    if (entity_id_.empty()) return;
    try {
        const auto current = context_.investigation.get_entity(entity_id_);
        const auto form = entity_form(this, current);
        if (!form) return;

        context_.investigation.update_entity(entity_id_,
                                             s(form->label),
                                             s(form->original_value),
                                             details_with_description(s(form->description)),
                                             form->tags,
                                             form->aliases,
                                             form->type);

        const auto old_images = json_strings(current.image_paths_json);
        for (const auto& path : old_images) {
            if (std::find_if(form->image_paths.begin(), form->image_paths.end(), [&path](const QString& cand) { return cand.toStdString() == path; }) == form->image_paths.end()) {
                context_.investigation.remove_entity_image(entity_id_, path);
            }
        }
        for (const auto& path : form->image_paths) {
            if (std::find(old_images.begin(), old_images.end(), path.toStdString()) == old_images.end()) {
                context_.investigation.add_entity_image(entity_id_, std::filesystem::path(s(path)));
            }
        }

        refresh();
        if (on_case_data_changed) on_case_data_changed();
        if (on_entity_modified) on_entity_modified();
    } catch (const std::exception& err) {
        show_error(this, err);
    }
}

void EntityProfilePage::edit_description() {
    if (entity_id_.empty()) return;
    try {
        const auto current = context_.investigation.get_entity(entity_id_);
        auto details = parse_entity_details(current.details_json);

        QDialog dialog(this);
        dialog.setWindowTitle(QStringLiteral("Edit background description"));
        dialog.resize(540, 360);
        auto* layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(18, 16, 18, 14);
        layout->setSpacing(10);

        layout->addWidget(heading(QStringLiteral("Background & Analysis"), 16));
        auto* editor = new QPlainTextEdit(&dialog);
        editor->setPlainText(q(details.description));
        editor->setPlaceholderText(QStringLiteral("Add substantial background, narrative notes, or investigation context for this entity…"));
        layout->addWidget(editor, 1);

        auto* buttons = dialog_buttons(dialog);
        set_primary_action(buttons, QStringLiteral("Save description"));
        layout->addWidget(buttons);

        if (dialog.exec() != QDialog::Accepted) return;

        details.description = s(editor->toPlainText().trimmed());
        const auto new_json = dump_entity_details(details);
        context_.investigation.update_entity(entity_id_,
                                             current.label,
                                             current.original_value,
                                             new_json,
                                             json_strings(current.tags_json),
                                             json_strings(current.aliases_json));
        refresh();
        if (on_case_data_changed) on_case_data_changed();
        if (on_entity_modified) on_entity_modified();
    } catch (const std::exception& err) {
        show_error(this, err);
    }
}

void EntityProfilePage::edit_attributes() {
    if (entity_id_.empty()) return;
    try {
        const auto current = context_.investigation.get_entity(entity_id_);
        auto details = parse_entity_details(current.details_json);

        QDialog dialog(this);
        dialog.setWindowTitle(QStringLiteral("Edit attributes & identifiers"));
        dialog.resize(560, 480);
        auto* layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(18, 16, 18, 14);
        layout->setSpacing(10);

        layout->addWidget(heading(QStringLiteral("Attributes & Identifiers"), 16));
        auto* form = new QFormLayout;
        form->setRowWrapPolicy(QFormLayout::WrapLongRows);

        auto* phones_edit = new QLineEdit(&dialog);
        phones_edit->setText(join_values(details.phone_numbers));
        phones_edit->setPlaceholderText(QStringLiteral("Comma-separated phone numbers"));

        auto* emails_edit = new QLineEdit(&dialog);
        emails_edit->setText(join_values(details.emails));
        emails_edit->setPlaceholderText(QStringLiteral("Comma-separated email addresses"));

        auto* usernames_edit = new QLineEdit(&dialog);
        usernames_edit->setText(join_values(details.usernames));
        usernames_edit->setPlaceholderText(QStringLiteral("Comma-separated usernames / handles"));

        auto* location_edit = new QLineEdit(&dialog);
        location_edit->setText(q(details.location));
        location_edit->setPlaceholderText(QStringLiteral("City, jurisdiction, or address"));

        auto* website_edit = new QLineEdit(&dialog);
        website_edit->setText(q(details.website));
        website_edit->setPlaceholderText(QStringLiteral("https://..."));

        auto* reg_num_edit = new QLineEdit(&dialog);
        reg_num_edit->setText(q(details.registration_number));
        reg_num_edit->setPlaceholderText(QStringLiteral("Registration number, tax ID, or LEI"));

        auto* occupation_edit = new QLineEdit(&dialog);
        occupation_edit->setText(q(details.occupation));
        occupation_edit->setPlaceholderText(QStringLiteral("Role, title, or trade"));

        auto* platform_edit = new QLineEdit(&dialog);
        platform_edit->setText(q(details.platform));
        platform_edit->setPlaceholderText(QStringLiteral("e.g. GitHub, LinkedIn, Telegram"));

        if (current.type == domain::EntityType::Person) {
            form->addRow(QStringLiteral("Phone numbers"), phones_edit);
            form->addRow(QStringLiteral("Email addresses"), emails_edit);
            form->addRow(QStringLiteral("Usernames"), usernames_edit);
            form->addRow(QStringLiteral("Location / Residence"), location_edit);
            form->addRow(QStringLiteral("Occupation / Title"), occupation_edit);
        } else if (current.type == domain::EntityType::Company) {
            form->addRow(QStringLiteral("Registration / Tax ID"), reg_num_edit);
            form->addRow(QStringLiteral("Jurisdiction / Headquarter"), location_edit);
            form->addRow(QStringLiteral("Official Website"), website_edit);
            form->addRow(QStringLiteral("Contact phones"), phones_edit);
            form->addRow(QStringLiteral("Contact emails"), emails_edit);
        } else if (current.type == domain::EntityType::Username || current.type == domain::EntityType::Account) {
            form->addRow(QStringLiteral("Platform / Network"), platform_edit);
            form->addRow(QStringLiteral("Profile URL"), website_edit);
            form->addRow(QStringLiteral("Linked emails"), emails_edit);
            form->addRow(QStringLiteral("Linked phones"), phones_edit);
        } else {
            form->addRow(QStringLiteral("Location / Address"), location_edit);
            form->addRow(QStringLiteral("Website / Reference URL"), website_edit);
            form->addRow(QStringLiteral("Contact phones"), phones_edit);
            form->addRow(QStringLiteral("Contact emails"), emails_edit);
        }

        layout->addLayout(form);
        auto* buttons = dialog_buttons(dialog);
        set_primary_action(buttons, QStringLiteral("Save attributes"));
        layout->addWidget(buttons);

        if (dialog.exec() != QDialog::Accepted) return;

        details.phone_numbers = split_values(phones_edit->text());
        details.emails = split_values(emails_edit->text());
        details.usernames = split_values(usernames_edit->text());
        details.location = s(location_edit->text().trimmed());
        details.website = s(website_edit->text().trimmed());
        details.registration_number = s(reg_num_edit->text().trimmed());
        details.occupation = s(occupation_edit->text().trimmed());
        details.platform = s(platform_edit->text().trimmed());

        const auto new_json = dump_entity_details(details);
        context_.investigation.update_entity(entity_id_,
                                             current.label,
                                             current.original_value,
                                             new_json,
                                             json_strings(current.tags_json),
                                             json_strings(current.aliases_json));
        refresh();
        if (on_case_data_changed) on_case_data_changed();
        if (on_entity_modified) on_entity_modified();
    } catch (const std::exception& err) {
        show_error(this, err);
    }
}

void EntityProfilePage::add_connection() {
    if (entity_id_.empty()) return;
    try {
        const auto entities = context_.investigation.list_entities(case_id_);
        if (entities.size() < 2) {
            QMessageBox::information(this, QStringLiteral("More entities needed"), QStringLiteral("At least two entities are required to create a connection."));
            return;
        }

        QDialog dialog(this);
        dialog.setWindowTitle(QStringLiteral("Add connection"));
        dialog.resize(520, 360);
        auto* layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(18, 16, 18, 14);
        layout->setSpacing(10);

        layout->addWidget(heading(QStringLiteral("Connect Entity"), 16));
        auto* form = new QFormLayout;

        auto* current_lbl = new QLabel(current_entity_label(), &dialog);
        current_lbl->setStyleSheet(QStringLiteral("QLabel { font-weight: 600; color: #38bdf8; }"));
        form->addRow(QStringLiteral("Source entity"), current_lbl);

        auto* target_combo = new QComboBox(&dialog);
        for (const auto& item : entities) {
            if (item.id == entity_id_) continue;
            target_combo->addItem(QStringLiteral("%1 (%2)").arg(q(item.label), enum_label(domain::to_string(item.type))), q(item.id));
        }
        form->addRow(QStringLiteral("Target entity"), target_combo);

        auto* predicate_combo = new QComboBox(&dialog);
        predicate_combo->setEditable(true);
        predicate_combo->addItem(QStringLiteral("Related to"), QStringLiteral("related_to"));
        predicate_combo->addItem(QStringLiteral("Employed by / Works at"), QStringLiteral("employed_by"));
        predicate_combo->addItem(QStringLiteral("Owns / Controls"), QStringLiteral("owns"));
        predicate_combo->addItem(QStringLiteral("Member of"), QStringLiteral("member_of"));
        predicate_combo->addItem(QStringLiteral("Located in"), QStringLiteral("located_in"));
        predicate_combo->addItem(QStringLiteral("Subsidiary of"), QStringLiteral("subsidiary_of"));
        predicate_combo->addItem(QStringLiteral("Identifier for"), QStringLiteral("identifier_for"));
        predicate_combo->addItem(QStringLiteral("Associated with"), QStringLiteral("associated_with"));
        predicate_combo->addItem(QStringLiteral("Communicated with"), QStringLiteral("communicated_with"));
        form->addRow(QStringLiteral("Relationship type"), predicate_combo);

        auto* direction_combo = new QComboBox(&dialog);
        direction_combo->addItem(QStringLiteral("Outgoing (This entity → Target)"), QStringLiteral("outgoing"));
        direction_combo->addItem(QStringLiteral("Incoming (Target → This entity)"), QStringLiteral("incoming"));
        form->addRow(QStringLiteral("Direction"), direction_combo);

        auto* reasoning_edit = new QPlainTextEdit(&dialog);
        reasoning_edit->setFixedHeight(60);
        reasoning_edit->setPlaceholderText(QStringLiteral("Optional reasoning or context…"));
        form->addRow(QStringLiteral("Reasoning (optional)"), reasoning_edit);

        layout->addLayout(form);
        auto* buttons = dialog_buttons(dialog);
        set_primary_action(buttons, QStringLiteral("Create connection"));
        layout->addWidget(buttons);

        if (dialog.exec() != QDialog::Accepted) return;

        const auto target_id = s(target_combo->currentData().toString());
        if (target_id.empty()) return;

        auto raw_pred = predicate_combo->currentData().toString();
        if (raw_pred.isEmpty()) raw_pred = predicate_combo->currentText().trimmed().toLower().replace(QLatin1Char(' '), QLatin1Char('_'));

        const bool is_outgoing = direction_combo->currentData().toString() == QStringLiteral("outgoing");
        const auto subject_id = is_outgoing ? entity_id_ : target_id;
        const auto object_id = is_outgoing ? target_id : entity_id_;

        context_.investigation.create_relation(case_id_,
                                               subject_id,
                                               s(raw_pred),
                                               object_id,
                                               {},
                                               domain::ClaimStatus::Unverified,
                                               s(reasoning_edit->toPlainText().trimmed()),
                                               {},
                                               {},
                                               domain::ClaimKind::Observation);
        refresh();
        if (on_case_data_changed) on_case_data_changed();
        if (on_entity_modified) on_entity_modified();
    } catch (const std::exception& err) {
        show_error(this, err);
    }
}

void EntityProfilePage::add_claim() {
    if (entity_id_.empty()) return;
    try {
        const auto entities = context_.investigation.list_entities(case_id_);
        const auto form = claim_form(this, entities, entity_id_, {});
        if (!form) return;

        if (form->relation) {
            context_.investigation.create_relation(case_id_,
                                                   form->subject_id,
                                                   s(form->predicate),
                                                   form->object_id,
                                                   s(form->statement),
                                                   form->status,
                                                   s(form->reasoning),
                                                   {},
                                                   {},
                                                   form->kind);
        } else {
            std::vector<services::EntityLinkInput> entity_links;
            entity_links.emplace_back(form->context_id.empty() ? entity_id_ : form->context_id, domain::ClaimEntityRole::Context);
            context_.investigation.create_claim(case_id_,
                                                s(form->statement),
                                                form->kind,
                                                entity_links,
                                                std::nullopt,
                                                form->status,
                                                s(form->reasoning));
        }

        refresh();
        if (on_case_data_changed) on_case_data_changed();
        if (on_entity_modified) on_entity_modified();
    } catch (const std::exception& err) {
        show_error(this, err);
    }
}

void EntityProfilePage::add_note() {
    if (entity_id_.empty()) return;
    try {
        QDialog dialog(this);
        dialog.setWindowTitle(QStringLiteral("Add note for entity"));
        dialog.resize(520, 320);
        auto* layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(18, 16, 18, 14);
        layout->setSpacing(10);

        layout->addWidget(heading(QStringLiteral("Add Note"), 16));
        auto* form = new QFormLayout;
        auto* title_edit = new QLineEdit(&dialog);
        title_edit->setPlaceholderText(QStringLiteral("Note title (optional)"));
        form->addRow(QStringLiteral("Title"), title_edit);

        auto* body_edit = new QPlainTextEdit(&dialog);
        body_edit->setPlaceholderText(QStringLiteral("Write observation, working theory, or lead note…"));
        form->addRow(QStringLiteral("Content *"), body_edit);
        layout->addLayout(form);

        auto* buttons = dialog_buttons(dialog);
        set_primary_action(buttons, QStringLiteral("Save note"));
        layout->addWidget(buttons);

        if (dialog.exec() != QDialog::Accepted) return;
        const auto body_text = body_edit->toPlainText().trimmed();
        if (body_text.isEmpty()) return;

        context_.investigation.add_note(case_id_,
                                        s(body_text),
                                        s(title_edit->text().trimmed()),
                                        {entity_id_},
                                        {});
        refresh();
        if (on_case_data_changed) on_case_data_changed();
    } catch (const std::exception& err) {
        show_error(this, err);
    }
}

void EntityProfilePage::change_relation_status(const domain::Id& claim_id) {
    change_claim_status(claim_id);
}

void EntityProfilePage::change_claim_status(const domain::Id& claim_id) {
    if (claim_id.empty()) return;
    try {
        const auto current = context_.investigation.get_claim(claim_id);
        QStringList labels;
        for (const auto& value : {"unverified", "possible", "probable", "confirmed", "refuted"}) labels << enum_label(value);
        bool ok = false;
        const auto choice = QInputDialog::getItem(this, QStringLiteral("Change claim status"), QStringLiteral("New status"), labels, labels.indexOf(enum_label(domain::to_string(current.status))), false, &ok);
        if (!ok) return;
        const auto explanation = explanation_form(this, QStringLiteral("Explain status change"), QStringLiteral("Why is the assessment changing? This explanation remains in the activity log."));
        if (!explanation) return;
        context_.investigation.change_claim_status(claim_id, domain::claim_status_from_string(s(choice.toLower().replace(' ', '_'))), s(*explanation));
        refresh();
        if (on_case_data_changed) on_case_data_changed();
        if (on_entity_modified) on_entity_modified();
    } catch (const std::exception& err) {
        show_error(this, err);
    }
}

void EntityProfilePage::link_claim_evidence(const domain::Id& claim_id) {
    if (claim_id.empty()) return;
    try {
        const auto available_evidence = context_.investigation.list_evidence(case_id_);
        const auto available_sources = context_.investigation.list_sources(case_id_);
        const auto form = link_evidence_form(this, available_evidence, available_sources);
        if (!form) return;

        const auto evidence_id = persist_link_evidence(context_.investigation, case_id_, *form);
        context_.investigation.link_evidence(claim_id, evidence_id, form->role, s(form->note));
        refresh();
        if (on_case_data_changed) on_case_data_changed();
        if (on_entity_modified) on_entity_modified();
    } catch (const std::exception& err) {
        show_error(this, err);
    }
}

void EntityProfilePage::add_image_from_file() {
    if (entity_id_.empty()) return;
    const auto file_path = QFileDialog::getOpenFileName(this, QStringLiteral("Select image"), QString(), QStringLiteral("Image files (*.png *.jpg *.jpeg *.bmp *.webp *.svg)"));
    if (file_path.isEmpty()) return;
    try {
        context_.investigation.add_entity_image(entity_id_, std::filesystem::path(s(file_path)));
        refresh();
        if (on_case_data_changed) on_case_data_changed();
        if (on_entity_modified) on_entity_modified();
    } catch (const std::exception& err) {
        show_error(this, err);
    }
}

void EntityProfilePage::paste_image_from_clipboard() {
    if (entity_id_.empty()) return;
    const auto* clipboard = QApplication::clipboard();
    const auto image = clipboard->image();
    if (image.isNull()) {
        QMessageBox::information(this, QStringLiteral("Clipboard empty"), QStringLiteral("No image data found on the clipboard."));
        return;
    }
    try {
        const auto temp_path = std::filesystem::temp_directory_path() / (domain::new_id() + ".png");
        image.save(QString::fromStdString(temp_path.string()));
        context_.investigation.add_entity_image(entity_id_, temp_path);
        std::filesystem::remove(temp_path);
        refresh();
        if (on_case_data_changed) on_case_data_changed();
        if (on_entity_modified) on_entity_modified();
    } catch (const std::exception& err) {
        show_error(this, err);
    }
}

void EntityProfilePage::remove_selected_image() {
    if (entity_id_.empty() || current_images_.empty()) return;
    QStringList items;
    for (const auto& path : current_images_) items << q(std::filesystem::path(path).filename().string());

    bool ok = false;
    const auto chosen = QInputDialog::getItem(this, QStringLiteral("Remove image"), QStringLiteral("Select image to remove:"), items, 0, false, &ok);
    if (!ok || chosen.isEmpty()) return;

    for (const auto& path : current_images_) {
        if (QString::fromStdString(std::filesystem::path(path).filename().string()) == chosen) {
            try {
                context_.investigation.remove_entity_image(entity_id_, path);
                refresh();
                if (on_case_data_changed) on_case_data_changed();
                if (on_entity_modified) on_entity_modified();
            } catch (const std::exception& err) {
                show_error(this, err);
            }
            break;
        }
    }
}

void EntityProfilePage::view_image_fullscreen(const QString& relative_path) {
    const auto abs_path = context_.store.absolute_path(s(relative_path));
    const auto img = QImage(QString::fromStdString(abs_path.string()));
    if (img.isNull()) return;

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Image Preview — %1").arg(q(std::filesystem::path(s(relative_path)).filename().string())));
    dialog.resize(760, 580);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(16, 16, 16, 14);

    auto* lbl = new QLabel(&dialog);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setPixmap(QPixmap::fromImage(img).scaled(720, 520, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    layout->addWidget(lbl, 1);

    auto* close_btn = button(QStringLiteral("Close"), false);
    connect(close_btn, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addWidget(close_btn, 0, Qt::AlignRight);

    dialog.exec();
}

} // namespace evidence_trace::gui
