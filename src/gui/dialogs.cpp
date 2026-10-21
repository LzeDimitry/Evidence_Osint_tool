#include "gui/dialogs.hpp"

#include "gui/gui_helpers.hpp"

#include <QBoxLayout>
#include <QComboBox>
#include <QCompleter>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QClipboard>
#include <QImage>
#include <QImageWriter>
#include <QBuffer>
#include <QMimeData>
#include <QLabel>
#include <QListWidget>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QScrollArea>
#include <QShortcut>
#include <QScreen>
#include <QToolButton>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <memory>

namespace evidence_trace::gui {

namespace {

std::shared_ptr<QStringList> paste_image_paths(QDialog& dialog, QVBoxLayout* parent_layout, QStringList initial = {}) {
    auto paths = std::make_shared<QStringList>(std::move(initial));
    auto* image_row = new QWidget(&dialog);
    auto* row_layout = new QVBoxLayout(image_row);
    row_layout->setContentsMargins(0, 0, 0, 0);
    auto* preview = new QLabel(QStringLiteral("No images attached. Paste an image with Ctrl+V or choose a file."), image_row);
    preview->setObjectName(QStringLiteral("clipboardImagePreview"));
    preview->setAlignment(Qt::AlignCenter);
    preview->setMinimumHeight(96);
    preview->setStyleSheet(QStringLiteral("background:#0a111b;border:1px dashed #34445b;border-radius:7px;padding:8px;color:#94a4bc;"));
    auto* actions = new QHBoxLayout;
    auto* paste = new QPushButton(QStringLiteral("Paste image"), image_row);
    auto* browse = new QPushButton(QStringLiteral("Add image file"), image_row);
    auto* remove = new QPushButton(QStringLiteral("Remove selected"), image_row);
    actions->addWidget(paste); actions->addWidget(browse); actions->addWidget(remove); actions->addStretch();
    row_layout->addWidget(preview); row_layout->addLayout(actions);
    auto* list = new QListWidget(image_row); list->setMaximumHeight(72); row_layout->addWidget(list);
    auto refresh = [preview, list, paths] {
        list->clear();
        for (const auto& path : *paths) list->addItem(QFileInfo(path).fileName());
        if (paths->isEmpty()) { preview->setPixmap({}); preview->setText(QStringLiteral("No images attached. Paste an image with Ctrl+V or choose a file.")); return; }
        QImage image(paths->back());
        if (!image.isNull()) { preview->setText(QString()); preview->setPixmap(QPixmap::fromImage(image).scaled(360, 180, Qt::KeepAspectRatio, Qt::SmoothTransformation)); }
        else preview->setText(QFileInfo(paths->back()).fileName());
    };
    QObject::connect(paste, &QPushButton::clicked, &dialog, [&dialog, paths, refresh] {
        const auto* mime = QGuiApplication::clipboard()->mimeData();
        if (mime == nullptr || !mime->hasImage()) { QMessageBox::information(&dialog, QStringLiteral("No clipboard image"), QStringLiteral("Copy an image first, then paste it here.")); return; }
        const auto image = qvariant_cast<QImage>(mime->imageData());
        if (image.isNull()) return;
        const auto path = (std::filesystem::temp_directory_path() / ("evidence-trace-clipboard-" + domain::new_id() + ".png")).string();
        if (image.save(QString::fromStdString(path), "PNG")) { paths->push_back(QString::fromStdString(path)); refresh(); }
    });
    auto* paste_shortcut = new QShortcut(QKeySequence::Paste, image_row);
    paste_shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    paste_shortcut->setEnabled(QGuiApplication::clipboard()->mimeData()->hasImage());
    QObject::connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, paste_shortcut, [paste_shortcut] {
        paste_shortcut->setEnabled(QGuiApplication::clipboard()->mimeData()->hasImage());
    });
    QObject::connect(paste_shortcut, &QShortcut::activated, paste, &QPushButton::click);
    QObject::connect(browse, &QPushButton::clicked, &dialog, [&dialog, paths, refresh] {
        const auto file = QFileDialog::getOpenFileName(&dialog, QStringLiteral("Choose image"), {}, QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.webp *.gif)"));
        if (!file.isEmpty()) { paths->push_back(file); refresh(); }
    });
    QObject::connect(remove, &QPushButton::clicked, &dialog, [paths, list, refresh] { const auto row = list->currentRow(); if (row >= 0 && row < paths->size()) { paths->removeAt(row); refresh(); } });
    refresh(); parent_layout->addWidget(image_row);
    return paths;
}

std::vector<std::string> split_tag_values(const QString& value) {
    auto normalized = value;
    normalized.replace(',', QChar(' '));
    std::vector<std::string> result;
    for (const auto& item : normalized.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts)) {
        if (!item.isEmpty()) result.push_back(s(item));
    }
    return result;
}

QString array_editor_text(const std::vector<std::string>& values) {
    QStringList lines;
    for (const auto& value : values) {
        auto encoded = q(value);
        encoded.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
        encoded.replace(QChar('\n'), QStringLiteral("\\n"));
        lines.push_back(encoded);
    }
    return lines.join(QChar('\n'));
}

QString string_array_json(const QString& value) {
    QJsonArray values;
    for (const auto& line : value.split(QChar('\n'), Qt::SkipEmptyParts)) {
        QString decoded;
        for (qsizetype i = 0; i < line.size(); ++i) {
            if (line[i] == QChar('\\') && i + 1 < line.size()) {
                const auto next = line[++i];
                if (next == QChar('n')) decoded += QChar('\n');
                else if (next == QChar('\\')) decoded += QChar('\\');
                else { decoded += QChar('\\'); decoded += next; }
            } else {
                decoded += line[i];
            }
        }
        if (!decoded.trimmed().isEmpty()) values.append(decoded);
    }
    return QString::fromUtf8(QJsonDocument(values).toJson(QJsonDocument::Compact));
}

QString tag_array_json(const QString& value) {
    QJsonArray values;
    for (const auto& item : split_tag_values(value)) values.append(QString::fromStdString(item));
    return QString::fromUtf8(QJsonDocument(values).toJson(QJsonDocument::Compact));
}

QWidget* optional_details(const QString& label, QWidget* content, QWidget* parent,
                          bool initially_open = false) {
    auto* section = new QWidget(parent);
    auto* layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    auto* toggle = new QToolButton(section);
    toggle->setObjectName(QStringLiteral("optionalDetailsToggle"));
    toggle->setText(label);
    toggle->setCheckable(true);
    toggle->setChecked(initially_open);
    toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toggle->setArrowType(initially_open ? Qt::DownArrow : Qt::RightArrow);
    content->setVisible(initially_open);
    layout->addWidget(toggle, 0, Qt::AlignLeft);
    layout->addWidget(content);
    QObject::connect(toggle, &QToolButton::toggled, section, [toggle, content](bool open) {
        toggle->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
        content->setVisible(open);
    });
    return section;
}

void select_value(QComboBox* combo, const QString& value) {
    const auto index = combo->findData(value);
    if (index >= 0) combo->setCurrentIndex(index);
}

QString entity_choice_text(const domain::Entity& entity) {
    return q(entity.label + "  ·  " + domain::to_string(entity.type) + "  ·  " + shorten_id(entity.id).toStdString());
}

bool set_form_row_wrap_policy(QLayout* layout, QFormLayout::RowWrapPolicy policy) {
    if (layout == nullptr) return false;
    bool changed = false;
    if (auto* form = qobject_cast<QFormLayout*>(layout)) {
        if (form->rowWrapPolicy() != policy) {
            form->setRowWrapPolicy(policy);
            form->invalidate();
            changed = true;
        }
    }
    for (int i = 0; i < layout->count(); ++i) {
        if (auto* nested = layout->itemAt(i)->layout()) changed = set_form_row_wrap_policy(nested, policy) || changed;
    }
    if (changed) layout->activate();
    return changed;
}

bool set_dialog_form_row_wrap_policy(QDialog& dialog, QFormLayout::RowWrapPolicy policy) {
    bool changed = set_form_row_wrap_policy(dialog.layout(), policy);
    for (auto* form : dialog.findChildren<QFormLayout*>()) {
        if (form->rowWrapPolicy() == policy) continue;
        form->setRowWrapPolicy(policy);
        form->invalidate();
        form->activate();
        changed = true;
    }
    return changed;
}

class ResponsiveFormLayoutFilter final : public QObject {
public:
    explicit ResponsiveFormLayoutFilter(QDialog* dialog) : QObject(dialog), dialog_(dialog) {}

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == dialog_ && event->type() == QEvent::Resize && dialog_->layout() != nullptr) {
            const auto policy = dialog_->width() < 520 ? QFormLayout::WrapAllRows : QFormLayout::WrapLongRows;
            if (set_dialog_form_row_wrap_policy(*dialog_, policy)) dialog_->layout()->activate();
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QDialog* dialog_;
};

void enable_responsive_form_rows(QDialog& dialog) {
    dialog.installEventFilter(new ResponsiveFormLayoutFilter(&dialog));
    const auto policy = dialog.width() < 520 ? QFormLayout::WrapAllRows : QFormLayout::WrapLongRows;
    set_dialog_form_row_wrap_policy(dialog, policy);
}

void set_form_width(QDialog& dialog, int width = 540) {
    dialog.setObjectName(QStringLiteral("modalDialog"));
    const auto* screen = dialog.screen() != nullptr ? dialog.screen() : QGuiApplication::primaryScreen();
    const int available_width = screen != nullptr ? screen->availableGeometry().width() : 1200;
    const int available_height = screen != nullptr ? screen->availableGeometry().height() : 900;
    const int bounded_width = std::max(280, std::min(width, available_width - 32));
    // Keep the preferred form width at its natural size, but let users shrink
    // dialogs well below it when working on a small display or tiled window.
    dialog.setMinimumWidth(std::min(420, std::max(280, available_width - 32)));
    dialog.setMaximumSize(std::max(280, available_width - 24), std::max(180, available_height - 32));
    dialog.resize(bounded_width, std::min(560, std::max(180, available_height - 32)));
    dialog.setModal(true);
}

void make_form_scrollable(QDialog& dialog, QDialogButtonBox* buttons) {
    auto* root = qobject_cast<QVBoxLayout*>(dialog.layout());
    if (root == nullptr) return;

    auto* content = new QWidget;
    auto* content_layout = new QVBoxLayout(content);
    content_layout->setContentsMargins(root->contentsMargins());
    content_layout->setSpacing(root->spacing());
    while (root->count() > 0) {
        auto* item = root->takeAt(0);
        if (item->widget() == buttons) {
            delete item;
            continue;
        }
        if (auto* widget = item->widget()) {
            content_layout->addWidget(widget);
            delete item;
        } else if (auto* nested = item->layout()) {
            content_layout->addLayout(nested);
        } else {
            content_layout->addItem(item);
        }
    }
    set_form_row_wrap_policy(content_layout, QFormLayout::WrapLongRows);

    auto* scroll = new QScrollArea(&dialog);
    scroll->setObjectName(QStringLiteral("dialogContentScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setMinimumSize(0, 0);
    scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(content);
    root->addWidget(scroll, 1);
    if (buttons != nullptr) root->addWidget(buttons, 0);

    root->setContentsMargins(12, 10, 12, 10);
    const auto* screen = dialog.screen() != nullptr ? dialog.screen() : QGuiApplication::primaryScreen();
    const QRect available_geometry = screen != nullptr ? screen->availableGeometry() : QRect(0, 0, 1200, 900);
    const int available_height = available_geometry.height();
    dialog.setMinimumHeight(150);
    dialog.setMaximumHeight(std::max(180, available_height - 32));

    const auto refresh_size = [&dialog, root, scroll, content, buttons, available_height, available_geometry] {
        QTimer::singleShot(0, &dialog, [&dialog, root, scroll, content, buttons, available_height, available_geometry] {
            root->activate();
            const int maximum = std::max(180, available_height - 32);
            const auto margins = root->contentsMargins();
            const int buttons_height = buttons != nullptr ? buttons->sizeHint().height() : 0;
            const int chrome_height = margins.top() + margins.bottom() + buttons_height + root->spacing();
            const int desired = std::min(maximum, content->sizeHint().height() + chrome_height);
            dialog.resize(dialog.width(), std::max(150, desired));
            scroll->setMinimumHeight(0);
            const auto frame = dialog.frameGeometry();
            const int x = std::clamp(frame.x(), available_geometry.left(),
                                     std::max(available_geometry.left(), available_geometry.right() - frame.width() + 1));
            const int y = std::clamp(frame.y(), available_geometry.top(),
                                     std::max(available_geometry.top(), available_geometry.bottom() - frame.height() + 1));
            dialog.move(x, y);
        });
    };
    for (auto* toggle : dialog.findChildren<QToolButton*>(QStringLiteral("optionalDetailsToggle"))) {
        QObject::connect(toggle, &QToolButton::toggled, &dialog, [refresh_size](bool) { refresh_size(); });
    }
    enable_responsive_form_rows(dialog);
    dialog.adjustSize();
}

} // namespace

std::optional<CaseForm> case_form(QWidget* parent, const std::optional<domain::Case>& existing) {
    QDialog dialog(parent);
    dialog.setWindowTitle(existing ? QStringLiteral("Edit case") : QStringLiteral("Create case"));
    set_form_width(dialog, 590);
    dialog.resize(590, 350);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(8);
    const int available_height = QGuiApplication::primaryScreen() != nullptr
        ? QGuiApplication::primaryScreen()->availableGeometry().height() : 900;
    dialog.setMaximumHeight(std::max(180, available_height - 32));

    auto* form_scroll = new QScrollArea(&dialog);
    form_scroll->setObjectName(QStringLiteral("caseFormScroll"));
    form_scroll->setWidgetResizable(true);
    form_scroll->setFrameShape(QFrame::NoFrame);
    form_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    form_scroll->setStyleSheet(QStringLiteral("QScrollArea#caseFormScroll { background: #0d1725; border: 0; }"));
    form_scroll->viewport()->setObjectName(QStringLiteral("caseFormViewport"));
    form_scroll->viewport()->setStyleSheet(QStringLiteral("QWidget#caseFormViewport { background: #0d1725; border: 0; }"));
    form_scroll->setMaximumHeight(std::max(80, available_height - 150));
    auto* form_content = new QWidget;
    form_content->setObjectName(QStringLiteral("caseFormContent"));
    form_content->setStyleSheet(QStringLiteral("QWidget#caseFormContent { background: #0d1725; }"));
    auto* form_layout = new QVBoxLayout(form_content);
    form_layout->setContentsMargins(6, 6, 6, 6);
    form_layout->setSpacing(8);

    auto* header = new QWidget;
    auto* header_layout = new QHBoxLayout(header);
    header_layout->setContentsMargins(0, 0, 0, 0);
    header_layout->setSpacing(12);
    auto* icon = new QLabel;
    icon->setFixedSize(40, 40);
    icon->setAlignment(Qt::AlignCenter);
    const auto accent = appearance_accent_color();
    icon->setPixmap(ui_icon(UiIcon::FolderAdd, accent.lighter(170)).pixmap(QSize(23, 23)));
    icon->setStyleSheet(QStringLiteral("background:%1; border:1px solid %2; border-radius:9px;")
        .arg(accent.darker(260).name(), accent.name()));
    auto* header_copy = new QVBoxLayout;
    header_copy->setContentsMargins(0, 0, 0, 0);
    header_copy->setSpacing(1);
    header_copy->addWidget(heading(existing ? QStringLiteral("Edit case") : QStringLiteral("Create case"), 19));
    header_copy->addWidget(muted(existing ? QStringLiteral("Change the case name or optional details.") : QStringLiteral("Give the investigation a name. You can add details later.")));
    header_layout->addWidget(icon);
    header_layout->addLayout(header_copy, 1);
    form_layout->addWidget(header);

    auto* title = new QLineEdit;
    title->setPlaceholderText(QStringLiteral("e.g. Account ownership review"));
    auto* purpose = new QPlainTextEdit;
    purpose->setPlaceholderText(QStringLiteral("What are you trying to find out? (optional)"));
    auto* description = new QPlainTextEdit;
    auto* target = enum_combo(&dialog, {"None", "Person", "Company", "Place", "Domain", "Username", "Event", "Other"}, {"", "person", "company", "place", "domain", "username", "event", "other"});
    target->setItemIcon(1, ui_icon(UiIcon::Person, QColor("#aebbd2")));
    target->setItemIcon(2, ui_icon(UiIcon::Organization, QColor("#aebbd2")));
    target->setItemIcon(3, ui_icon(UiIcon::MapPin, QColor("#aebbd2")));
    target->setItemIcon(4, ui_icon(UiIcon::Globe, QColor("#aebbd2")));
    target->setItemIcon(5, ui_icon(UiIcon::Entity, QColor("#aebbd2")));
    target->setItemIcon(6, ui_icon(UiIcon::Calendar, QColor("#aebbd2")));
    auto* scope = new QPlainTextEdit;
    auto* tags = new QLineEdit;
    tags->setPlaceholderText(QStringLiteral("osint domain person (space-separated)"));
    const auto compact_field = [](QWidget* field, int height) {
        field->setFixedHeight(height);
        field->setStyleSheet(QStringLiteral("QLineEdit, QComboBox, QPlainTextEdit { min-height: 0px; padding: 5px 10px; }") );
    };
    compact_field(title, 36);
    compact_field(purpose, 56);
    description->setFixedHeight(64);
    compact_field(scope, 56);
    compact_field(target, 36);
    compact_field(tags, 36);

    auto* title_label = new QLabel(QStringLiteral("Case name *"));
    title_label->setObjectName(QStringLiteral("formLabel"));
    form_layout->addWidget(title_label);
    form_layout->addWidget(title);
    auto* purpose_label = new QLabel(QStringLiteral("What is this case about? (optional)"));
    purpose_label->setObjectName(QStringLiteral("formLabel"));
    form_layout->addWidget(purpose_label);
    form_layout->addWidget(purpose);

    auto* details_toggle = new QToolButton(&dialog);
    details_toggle->setText(QStringLiteral("More details (optional)"));
    details_toggle->setCheckable(true);
    details_toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    details_toggle->setArrowType(Qt::RightArrow);
    auto* details = new QWidget(&dialog);
    auto* details_form = new QFormLayout(details);
    details_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    details_form->setContentsMargins(0, 4, 0, 0);
    details_form->setSpacing(8);
    details_form->addRow(QStringLiteral("Primary target"), target);
    details_form->addRow(QStringLiteral("Scope"), scope);
    details_form->addRow(QStringLiteral("Tags"), tags);
    details_form->addRow(QStringLiteral("Description"), description);
    details->setVisible(false);
    form_layout->addWidget(details_toggle);
    form_layout->addWidget(details);
    QObject::connect(details_toggle, &QToolButton::toggled, &dialog, [details_toggle, details](bool expanded) {
        details_toggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        details->setVisible(expanded);
        details_toggle->setText(expanded ? QStringLiteral("Hide optional details") : QStringLiteral("More details (optional)"));
    });
    form_layout->addStretch();
    form_scroll->setWidget(form_content);
    layout->addWidget(form_scroll, 1);
    auto* buttons = dialog_buttons(dialog);
    set_primary_action(buttons, existing ? QStringLiteral("Save changes") : QStringLiteral("Create case"));
    if (!existing) {
        if (auto* accept = buttons->button(QDialogButtonBox::Ok)) accept->setIcon(QIcon());
    }
    if (auto* accept = buttons->button(QDialogButtonBox::Ok)) accept->setFixedHeight(40);
    if (auto* cancel = buttons->button(QDialogButtonBox::Cancel)) cancel->setFixedHeight(40);
    if (auto* button_layout = qobject_cast<QBoxLayout*>(buttons->layout())) {
        auto* accept = buttons->button(QDialogButtonBox::Ok);
        auto* cancel = buttons->button(QDialogButtonBox::Cancel);
        if (accept != nullptr && cancel != nullptr) {
            button_layout->removeWidget(accept);
            button_layout->removeWidget(cancel);
            button_layout->addWidget(cancel);
            button_layout->addWidget(accept);
        }
    }
    layout->addWidget(buttons);
    QObject::connect(details_toggle, &QToolButton::toggled, &dialog,
        [&, buttons](bool) {
            QTimer::singleShot(0, &dialog, [&, buttons] {
                form_layout->activate();
                const int body_height = std::min(form_content->sizeHint().height(), available_height - 150);
                form_scroll->setMinimumHeight(std::max(0, body_height));
                const int desired_height = std::min(dialog.maximumHeight(),
                    std::max(150, form_scroll->minimumHeight() + buttons->sizeHint().height() + 50));
                dialog.resize(dialog.width(), desired_height);
            });
        });
    if (existing) {
        title->setText(q(existing->title)); purpose->setPlainText(q(existing->purpose)); description->setPlainText(q(existing->description));
        if (existing->primary_target_type) select_value(target, q(*existing->primary_target_type));
        scope->setPlainText(q(existing->scope));
        QStringList tag_values;
        for (const auto& tag : json_strings(existing->tags_json)) tag_values.push_back(q(tag));
        tags->setText(tag_values.join(QStringLiteral(" ")));
        if (existing->primary_target_type || !existing->scope.empty() || !existing->description.empty() || !tag_values.isEmpty()) details_toggle->setChecked(true);
    }
    QObject::disconnect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        if (title->text().trimmed().isEmpty()) { QMessageBox::warning(&dialog, QStringLiteral("Title required"), QStringLiteral("A case title is required.")); return; }
        dialog.accept();
    });
    enable_responsive_form_rows(dialog);
    if (dialog.exec() != QDialog::Accepted) return std::nullopt;
    CaseForm result;
    result.title = title->text().trimmed(); result.purpose = purpose->toPlainText(); result.description = description->toPlainText();
    const auto target_value = target->currentData().toString(); if (!target_value.isEmpty()) result.target_type = target_value;
    result.scope = scope->toPlainText(); result.tags = split_tag_values(tags->text());
    return result;
}

std::optional<EntityForm> entity_form(QWidget* parent, const std::optional<domain::Entity>& existing) {
    QDialog dialog(parent);
    dialog.setWindowTitle(existing ? QStringLiteral("Edit entity") : QStringLiteral("Add entity"));
    set_form_width(dialog, 590);
    dialog.resize(630, 550);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(20, 18, 20, 14);
    layout->setSpacing(12);

    auto* hero = new QFrame(&dialog);
    hero->setObjectName(QStringLiteral("entityHero"));
    auto* hero_layout = new QHBoxLayout(hero);
    hero_layout->setContentsMargins(14, 12, 14, 12);
    hero_layout->setSpacing(12);
    auto* icon = new QLabel(hero);
    icon->setObjectName(QStringLiteral("entityHeroIcon"));
    icon->setFixedSize(42, 42);
    icon->setAlignment(Qt::AlignCenter);
    icon->setPixmap(ui_icon(existing ? UiIcon::Edit : UiIcon::Entity, appearance_accent_color().lighter(170)).pixmap(QSize(22, 22)));
    icon->setProperty("accentIconKind", static_cast<int>(existing ? UiIcon::Edit : UiIcon::Entity));
    icon->setProperty("accentIconSize", QSize(22, 22));
    auto* hero_copy = new QVBoxLayout;
    hero_copy->setContentsMargins(0, 0, 0, 0);
    hero_copy->setSpacing(2);
    auto* title = heading(existing ? QStringLiteral("Edit entity") : QStringLiteral("Add entity"), 20);
    title->setObjectName(QStringLiteral("entityDialogTitle"));
    auto* subtitle = muted(QStringLiteral("Describe the object. Record confidence and assessments as claims."));
    subtitle->setWordWrap(true);
    hero_copy->addWidget(title);
    hero_copy->addWidget(subtitle);
    hero_layout->addWidget(icon);
    hero_layout->addLayout(hero_copy, 1);
    layout->addWidget(hero);

    auto* fields_scroll = new QScrollArea(&dialog);
    fields_scroll->setWidgetResizable(true);
    fields_scroll->setFrameShape(QFrame::NoFrame);
    fields_scroll->setMinimumHeight(0);
    auto* fields_body = new QWidget(fields_scroll);
    auto* fields_layout = new QVBoxLayout(fields_body);
    fields_layout->setContentsMargins(0, 0, 2, 0);
    fields_layout->setSpacing(10);
    fields_scroll->setWidget(fields_body);

    const QStringList type_values = {"person", "username", "account", "email", "phone", "domain", "ip", "company", "place", "url", "image", "document", "event", "other"};
    QStringList type_labels; for (const auto& value : type_values) type_labels << enum_label(value.toStdString());
    auto* type = enum_combo(&dialog, type_labels, type_values);
    auto* label = new QLineEdit; auto* value = new QLineEdit; auto* description = new QPlainTextEdit; auto* tags = new QLineEdit; auto* aliases = new QLineEdit;
    auto* phones = new QLineEdit; auto* usernames = new QLineEdit; auto* emails = new QLineEdit; auto* other_ids = new QLineEdit;
    label->setPlaceholderText(QStringLiteral("A readable name for this object"));
    value->setPlaceholderText(QStringLiteral("Username, domain, address, or other observed value"));
    tags->setPlaceholderText(QStringLiteral("Separate tags with spaces, e.g. priority public-records lead"));
    aliases->setPlaceholderText(QStringLiteral("Separate alternate names with commas"));
    phones->setPlaceholderText(QStringLiteral("Separate multiple values with commas"));
    usernames->setPlaceholderText(QStringLiteral("Separate multiple values with commas"));
    emails->setPlaceholderText(QStringLiteral("Separate multiple values with commas"));
    other_ids->setPlaceholderText(QStringLiteral("Separate multiple values with commas"));
    description->setPlaceholderText(QStringLiteral("What is this entity, and what makes it relevant to the case?"));
    description->setFixedHeight(82);
    type->setMinimumHeight(36);
    label->setMinimumHeight(36);
    value->setMinimumHeight(36);
    tags->setMinimumHeight(36);
    aliases->setMinimumHeight(36);

    const auto field_block = [](const QString& caption, QWidget* field, const QString& help = QString()) {
        auto* block = new QWidget;
        auto* block_layout = new QVBoxLayout(block);
        block_layout->setContentsMargins(0, 0, 0, 0);
        block_layout->setSpacing(5);
        auto* caption_label = new QLabel(caption, block);
        caption_label->setObjectName(QStringLiteral("entityFieldLabel"));
        block_layout->addWidget(caption_label);
        block_layout->addWidget(field);
        if (!help.isEmpty()) {
            auto* help_label = muted(help);
            help_label->setWordWrap(true);
            help_label->setObjectName(QStringLiteral("entityFieldHelp"));
            block_layout->addWidget(help_label);
        }
        return block;
    };
    auto* identity = new QFrame(&dialog);
    identity->setObjectName(QStringLiteral("entityIdentityPanel"));
    auto* identity_layout = new QGridLayout(identity);
    identity_layout->setContentsMargins(14, 12, 14, 14);
    identity_layout->setHorizontalSpacing(12);
    identity_layout->setVerticalSpacing(10);
    auto* identity_heading = new QLabel(QStringLiteral("IDENTITY"), identity);
    identity_heading->setObjectName(QStringLiteral("entitySectionLabel"));
    identity_layout->addWidget(identity_heading, 0, 0, 1, 2);
    identity_layout->addWidget(field_block(QStringLiteral("Entity type"), type), 1, 0);
    identity_layout->addWidget(field_block(QStringLiteral("Display label *"), label), 1, 1);
    identity_layout->setColumnStretch(0, 1);
    identity_layout->setColumnStretch(1, 1);
    fields_layout->addWidget(identity);

    auto* metadata = new QFrame(&dialog);
    metadata->setObjectName(QStringLiteral("entityMetadataPanel"));
    auto* metadata_layout = new QGridLayout(metadata);
    metadata_layout->setContentsMargins(14, 11, 14, 13);
    metadata_layout->setHorizontalSpacing(12);
    metadata_layout->setVerticalSpacing(7);
    auto* metadata_heading = new QLabel(QStringLiteral("DISCOVERY DETAILS"), metadata);
    metadata_heading->setObjectName(QStringLiteral("entitySectionLabel"));
    metadata_layout->addWidget(metadata_heading, 0, 0, 1, 2);
    metadata_layout->addWidget(field_block(QStringLiteral("Tags"), tags), 1, 0);
    metadata_layout->addWidget(field_block(QStringLiteral("Aliases"), aliases), 1, 1);
    metadata_layout->setColumnStretch(0, 1);
    metadata_layout->setColumnStretch(1, 1);
    auto* more_details = new QWidget(fields_body);
    auto* more_layout = new QVBoxLayout(more_details);
    more_layout->setContentsMargins(0, 0, 0, 0);
    more_layout->setSpacing(8);
    auto* details_form = new QFormLayout;
    details_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    details_form->addRow(QStringLiteral("Observed / original value"), value);
    details_form->addRow(QStringLiteral("Description"), description);
    more_layout->addLayout(details_form);
    auto* identifier_form = new QFormLayout;
    identifier_form->addRow(QStringLiteral("Phone numbers"), phones);
    identifier_form->addRow(QStringLiteral("Usernames"), usernames);
    identifier_form->addRow(QStringLiteral("Email addresses"), emails);
    identifier_form->addRow(QStringLiteral("Other identifiers"), other_ids);
    more_layout->addLayout(identifier_form);
    auto image_paths = paste_image_paths(dialog, more_layout, [&] { QStringList values; if (existing) for (const auto& path : json_strings(existing->image_paths_json)) values << q(path); return values; }());
    more_layout->addWidget(metadata);
    const bool has_more_details = existing &&
        (!existing->original_value.empty() || !json_description(existing->details_json).empty() ||
         !json_strings(existing->tags_json).empty() || !json_strings(existing->aliases_json).empty());
    fields_layout->addWidget(optional_details(QStringLiteral("More details (optional)"), more_details,
                                              fields_body, has_more_details));
    fields_layout->addStretch(1);
    layout->addWidget(fields_scroll, 1);

    auto* buttons = dialog_buttons(dialog);
    set_primary_action(buttons, existing ? QStringLiteral("Save changes") : QStringLiteral("Create entity"));
    if (auto* accept = buttons->button(QDialogButtonBox::Ok)) accept->setFixedHeight(38);
    if (auto* cancel = buttons->button(QDialogButtonBox::Cancel)) cancel->setFixedHeight(38);
    if (auto* button_layout = qobject_cast<QBoxLayout*>(buttons->layout())) {
        auto* accept = buttons->button(QDialogButtonBox::Ok);
        auto* cancel = buttons->button(QDialogButtonBox::Cancel);
        if (accept != nullptr && cancel != nullptr) {
            button_layout->removeWidget(accept);
            button_layout->removeWidget(cancel);
            button_layout->addWidget(cancel);
            button_layout->addWidget(accept);
        }
    }
    layout->addWidget(buttons);
    if (existing) {
        select_value(type, q(domain::to_string(existing->type))); label->setText(q(existing->label)); value->setText(q(existing->original_value));
        description->setPlainText(q(json_description(existing->details_json)));
        QStringList existing_tags;
        for (const auto& tag : json_strings(existing->tags_json)) existing_tags << q(tag);
        tags->setText(existing_tags.join(QChar(' ')));
        aliases->setText(join_values(json_strings(existing->aliases_json)));
    }
    QObject::disconnect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        if (label->text().trimmed().isEmpty()) { QMessageBox::warning(&dialog, QStringLiteral("Label required"), QStringLiteral("An entity label is required.")); return; }
        dialog.accept();
    });
    enable_responsive_form_rows(dialog);
    if (dialog.exec() != QDialog::Accepted) return std::nullopt;
    EntityForm result; result.type = domain::entity_type_from_string(s(type->currentData().toString())); result.label = label->text().trimmed(); result.original_value = value->text().trimmed();
    result.description = description->toPlainText().trimmed(); result.tags = split_tag_values(tags->text()); result.aliases = split_values(aliases->text());
    for (const auto& item : split_values(phones->text())) result.phone_numbers.push_back(q(item));
    for (const auto& item : split_values(usernames->text())) result.usernames.push_back(q(item));
    for (const auto& item : split_values(emails->text())) result.emails.push_back(q(item));
    for (const auto& item : split_values(other_ids->text())) result.identifiers.push_back(q(item));
    for (const auto& path : *image_paths) result.image_paths.push_back(path);
    return result;
}

std::optional<SourceForm> source_form(QWidget* parent) {
    QDialog dialog(parent); dialog.setWindowTitle(QStringLiteral("Add source")); set_form_width(dialog);
    auto* layout = new QVBoxLayout(&dialog); layout->addWidget(heading(QStringLiteral("Add source"), 17));
    layout->addWidget(muted(QStringLiteral("Record the page or item you used. A title or locator is enough to start.")));
    auto* type_form = new QFormLayout; const QStringList values = {"web", "document", "registry", "person", "other"}; QStringList labels; for (const auto& value : values) labels << enum_label(value.toStdString());
    auto* type = enum_combo(&dialog, labels, values); auto* locator = new QLineEdit; auto* title = new QLineEdit; auto* accessed = new QLineEdit; auto* author = new QLineEdit; auto* reliability = new QPlainTextEdit; reliability->setFixedHeight(64);
    locator->setPlaceholderText(QStringLiteral("URL, file name, registry, or contact"));
    accessed->setPlaceholderText(QStringLiteral("Leave blank to use the current time (UTC)"));
    type_form->addRow(QStringLiteral("Type"), type); type_form->addRow(QStringLiteral("Locator or URL"), locator);
    auto* extra_form = new QFormLayout; extra_form->addRow(QStringLiteral("Title"), title); extra_form->addRow(QStringLiteral("Accessed time (UTC, optional)"), accessed); extra_form->addRow(QStringLiteral("Author"), author); extra_form->addRow(QStringLiteral("Reliability note"), reliability);
    layout->addLayout(type_form);
    auto* extra = new QWidget(&dialog); extra->setLayout(extra_form);
    layout->addWidget(optional_details(QStringLiteral("More source details (optional)"), extra, &dialog));
    auto* buttons = dialog_buttons(dialog); set_primary_action(buttons, QStringLiteral("Add source")); layout->addWidget(buttons);
    make_form_scrollable(dialog, buttons);
    QObject::disconnect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        if (locator->text().trimmed().isEmpty() && title->text().trimmed().isEmpty()) {
            QMessageBox::warning(&dialog, QStringLiteral("Source details required"), QStringLiteral("Add a title or locator so this source can be identified later."));
            return;
        }
        dialog.accept();
    });
    if (dialog.exec() != QDialog::Accepted) return std::nullopt;
    SourceForm result; result.type = domain::source_type_from_string(s(type->currentData().toString())); result.locator = locator->text().trimmed(); result.title = title->text().trimmed(); result.accessed_at = accessed->text().trimmed(); result.author = author->text().trimmed(); result.reliability_note = reliability->toPlainText().trimmed(); return result;
}

std::optional<EvidenceForm> evidence_form(QWidget* parent, const std::vector<domain::Source>& sources,
                                          domain::EvidenceKind preferred_kind) {
    QDialog dialog(parent); dialog.setWindowTitle(QStringLiteral("Add evidence")); set_form_width(dialog, 590);
    auto* layout = new QVBoxLayout(&dialog); layout->addWidget(heading(QStringLiteral("Add evidence"), 17)); layout->addWidget(muted(QStringLiteral("Files are copied into immutable case storage. A URL without a preserved copy remains external only.")));
    auto* form = new QFormLayout; auto* kind = enum_combo(&dialog, {"Text quotation", "External URL", "Preserved file"}, {"text", "url", "file"});
    select_value(kind, q(domain::to_string(preferred_kind)));
    auto* source = new QComboBox; source->addItem(QStringLiteral("No linked source"), QString()); for (const auto& item : sources) source->addItem(q(item.title.empty() ? item.locator : item.title), q(item.id));
    auto* file_row = new QWidget; auto* file_layout = new QHBoxLayout(file_row); file_layout->setContentsMargins(0, 0, 0, 0); auto* file = new QLineEdit; auto* browse = button(QStringLiteral("Browse")); file_layout->addWidget(file); file_layout->addWidget(browse);
    auto* text = new QPlainTextEdit; text->setFixedHeight(105); auto* url = new QLineEdit; auto* note = new QPlainTextEdit; note->setFixedHeight(64); auto* location = new QLineEdit; auto* source_url = new QLineEdit;
    form->addRow(QStringLiteral("Kind"), kind); form->addRow(QStringLiteral("File"), file_row); form->addRow(QStringLiteral("Quotation"), text); form->addRow(QStringLiteral("URL"), url);
    auto* details_form = new QFormLayout; details_form->addRow(QStringLiteral("Source record"), source); details_form->addRow(QStringLiteral("Source URL"), source_url); details_form->addRow(QStringLiteral("Quotation location"), location); details_form->addRow(QStringLiteral("Note"), note);
    auto* details = new QWidget(&dialog); details->setLayout(details_form);
    auto image_paths = paste_image_paths(dialog, layout);
    layout->addLayout(form);
    layout->addWidget(optional_details(QStringLiteral("Source and capture details (optional)"), details, &dialog));
    auto* buttons = dialog_buttons(dialog); set_primary_action(buttons, QStringLiteral("Add evidence")); layout->addWidget(buttons);
    make_form_scrollable(dialog, buttons);
    auto update_visibility = [&] { const auto selected = kind->currentData().toString(); file_row->setVisible(selected == "file"); text->setVisible(selected == "text"); url->setVisible(selected == "url"); source_url->setVisible(selected != "url"); location->setVisible(selected == "text"); };
    QObject::connect(kind, &QComboBox::currentIndexChanged, &dialog, update_visibility); QObject::connect(browse, &QPushButton::clicked, &dialog, [&] { const auto path = QFileDialog::getOpenFileName(&dialog, QStringLiteral("Select evidence file")); if (!path.isEmpty()) file->setText(path); }); update_visibility();
    QObject::disconnect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        const auto selected = kind->currentData().toString(); const bool valid = (selected == "file" && !file->text().trimmed().isEmpty()) || (selected == "text" && !text->toPlainText().trimmed().isEmpty()) || (selected == "url" && !url->text().trimmed().isEmpty());
        if (!valid) { QMessageBox::warning(&dialog, QStringLiteral("Evidence content required"), QStringLiteral("Provide a file, text quotation, or URL for the selected evidence kind.")); return; } dialog.accept();
    });
    if (dialog.exec() != QDialog::Accepted) return std::nullopt;
    EvidenceForm result; result.kind = domain::evidence_kind_from_string(s(kind->currentData().toString())); result.file_path = file->text().trimmed(); result.text = text->toPlainText(); result.url = result.kind == domain::EvidenceKind::Url ? url->text().trimmed() : source_url->text().trimmed(); result.note = note->toPlainText().trimmed(); result.quotation_location = location->text().trimmed(); const auto source_id = source->currentData().toString(); if (!source_id.isEmpty()) result.source_id = s(source_id); for (const auto& path : *image_paths) result.image_paths.push_back(path); return result;
}

std::optional<ClaimForm> claim_form(QWidget* parent, const std::vector<domain::Entity>& entities,
                                    const domain::Id& subject_id, const domain::Id& object_id) {
    if (entities.empty()) { QMessageBox::information(parent, QStringLiteral("Entities required"), QStringLiteral("Add at least one entity before creating a claim or relation.")); return std::nullopt; }
    QDialog dialog(parent); dialog.setWindowTitle(QStringLiteral("New claim or relation")); set_form_width(dialog, 600); auto* layout = new QVBoxLayout(&dialog); layout->addWidget(heading(QStringLiteral("New claim or relation"), 17));
    auto* form = new QFormLayout; auto* mode = enum_combo(&dialog, {"Directional relation", "Entity claim"}, {"relation", "claim"}); auto* subject = new QComboBox; auto* object = new QComboBox; auto* context = new QComboBox; auto* predicate = new QLineEdit; auto* statement = new QLineEdit; auto* kind = enum_combo(&dialog, {"Observation", "Inference"}, {"observation", "inference"}); const QStringList status_values = {"unverified", "possible", "probable", "confirmed", "refuted"}; QStringList status_labels; for (const auto& value : status_values) status_labels << enum_label(value.toStdString()); auto* status = enum_combo(&dialog, status_labels, status_values); auto* reasoning = new QPlainTextEdit; reasoning->setFixedHeight(88);
    subject->addItem(QStringLiteral("Select subject"), QString()); object->addItem(QStringLiteral("Select object"), QString()); context->addItem(QStringLiteral("Select entity"), QString()); for (const auto& entity : entities) { const auto text = entity_choice_text(entity); subject->addItem(text, q(entity.id)); object->addItem(text, q(entity.id)); context->addItem(text, q(entity.id)); }
    const QStringList selector_prompts = {QStringLiteral("Choose or search a subject"), QStringLiteral("Choose or search an object"), QStringLiteral("Choose or search an entity")};
    int selector_index = 0;
    for (auto* selector : {subject, object, context}) {
        selector->setEditable(true);
        selector->setInsertPolicy(QComboBox::NoInsert);
        selector->completer()->setCompletionMode(QCompleter::PopupCompletion);
        selector->completer()->setFilterMode(Qt::MatchContains);
        selector->completer()->setCaseSensitivity(Qt::CaseInsensitive);
        selector->setCurrentIndex(-1);
        selector->lineEdit()->setPlaceholderText(selector_prompts[selector_index++]);
    }
    select_value(subject, q(subject_id));
    select_value(object, q(object_id));
    predicate->setPlaceholderText(QStringLiteral("e.g. owns, created, located in"));
    statement->setPlaceholderText(QStringLiteral("Auto-filled for relations; edit if needed"));
    auto* subject_label = new QLabel(QStringLiteral("Subject"));
    auto* predicate_label = new QLabel(QStringLiteral("Predicate"));
    auto* object_label = new QLabel(QStringLiteral("Object"));
    auto* context_label = new QLabel(QStringLiteral("Entity"));
    auto* kind_label = new QLabel(QStringLiteral("Claim kind"));
    form->addRow(QStringLiteral("Record as"), mode); form->addRow(subject_label, subject); form->addRow(predicate_label, predicate); form->addRow(object_label, object); form->addRow(context_label, context); form->addRow(QStringLiteral("Statement *"), statement); form->addRow(kind_label, kind); layout->addLayout(form);
    auto* assessment_form = new QFormLayout; assessment_form->addRow(QStringLiteral("Initial status"), status); assessment_form->addRow(QStringLiteral("Reasoning"), reasoning);
    auto* assessment = new QWidget(&dialog); assessment->setLayout(assessment_form);
    auto* assessment_section = optional_details(QStringLiteral("Reasoning and status (required for inference or confirmation)"), assessment, &dialog);
    layout->addWidget(assessment_section);
    layout->addWidget(muted(QStringLiteral("Inferences need written reasoning. Confirmed claims also need reasoning and supporting evidence.")));
    auto* buttons = dialog_buttons(dialog); layout->addWidget(buttons);
    make_form_scrollable(dialog, buttons);
    select_value(kind, QStringLiteral("observation"));
    QString suggested_statement;
    const auto selected_label = [&entities](const QString& id) {
        for (const auto& entity : entities) if (q(entity.id) == id) return q(entity.label);
        return QString();
    };
    const auto update_suggested_statement = [&] {
        if (mode->currentData().toString() != "relation") return;
        const auto subject_text = selected_label(subject->currentData().toString());
        const auto predicate_text = predicate->text().trimmed();
        const auto object_text = selected_label(object->currentData().toString());
        const auto current = statement->text().trimmed();
        if (current.isEmpty() || current == suggested_statement) {
            const auto suggestion = subject_text.isEmpty() || predicate_text.isEmpty() || object_text.isEmpty()
                ? QString() : QStringLiteral("%1 %2 %3").arg(subject_text, predicate_text, object_text);
            statement->setText(suggestion);
            suggested_statement = suggestion;
        }
    };
    auto update = [&] { const bool relation = mode->currentData().toString() == "relation"; if (!relation && statement->text().trimmed() == suggested_statement) { statement->clear(); suggested_statement.clear(); } subject_label->setVisible(relation); subject->setVisible(relation); predicate_label->setVisible(relation); predicate->setVisible(relation); object_label->setVisible(relation); object->setVisible(relation); context_label->setVisible(!relation); context->setVisible(!relation); kind->setEnabled(true); };
    QObject::connect(subject, qOverload<int>(&QComboBox::currentIndexChanged), &dialog, update_suggested_statement);
    QObject::connect(object, qOverload<int>(&QComboBox::currentIndexChanged), &dialog, update_suggested_statement);
    QObject::connect(predicate, &QLineEdit::textChanged, &dialog, update_suggested_statement);
    QObject::connect(mode, &QComboBox::currentIndexChanged, &dialog, [&] {
        update();
        statement->setPlaceholderText(mode->currentData().toString() == "relation"
            ? QStringLiteral("Auto-filled from the selected relation; edit if needed")
            : QStringLiteral("Enter a concise claim statement"));
        set_primary_action(buttons, mode->currentData().toString() == "relation" ? QStringLiteral("Create relation") : QStringLiteral("Create claim"));
        update_suggested_statement();
    });
    QObject::connect(kind, qOverload<int>(&QComboBox::currentIndexChanged), &dialog, [assessment_section, kind] {
        if (kind->currentData().toString() == "inference") {
            if (auto* toggle = assessment_section->findChild<QToolButton*>(QStringLiteral("optionalDetailsToggle"))) toggle->setChecked(true);
        }
    });
    update();
    set_primary_action(buttons, QStringLiteral("Create relation"));
    QObject::disconnect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] { const bool relation = mode->currentData().toString() == "relation"; if (relation && (subject->currentData().toString().isEmpty() || object->currentData().toString().isEmpty() || predicate->text().trimmed().isEmpty())) { QMessageBox::warning(&dialog, QStringLiteral("Relation fields required"), QStringLiteral("Select a subject and object and provide a predicate.")); return; } if (!relation && context->currentData().toString().isEmpty()) { QMessageBox::warning(&dialog, QStringLiteral("Entity required"), QStringLiteral("Select the entity this claim refers to.")); return; } if (statement->text().trimmed().isEmpty()) { QMessageBox::warning(&dialog, QStringLiteral("Statement required"), QStringLiteral("A claim statement is required.")); return; } if ((kind->currentData().toString() == "inference" || status->currentData().toString() == "confirmed") && reasoning->toPlainText().trimmed().isEmpty()) { if (auto* toggle = assessment_section->findChild<QToolButton*>(QStringLiteral("optionalDetailsToggle"))) toggle->setChecked(true); QMessageBox::warning(&dialog, QStringLiteral("Reasoning required"), QStringLiteral("Add reasoning for an inference or confirmed claim.")); return; } dialog.accept(); });
    if (dialog.exec() != QDialog::Accepted) return std::nullopt;
    ClaimForm result;
    result.relation = mode->currentData().toString() == "relation";
    result.subject_id = s(subject->currentData().toString());
    result.object_id = s(object->currentData().toString());
    result.context_id = s(context->currentData().toString());
    result.predicate = predicate->text().trimmed();
    result.statement = statement->text().trimmed();
    result.kind = domain::claim_kind_from_string(s(kind->currentData().toString()));
    result.status = domain::claim_status_from_string(s(status->currentData().toString()));
    result.reasoning = reasoning->toPlainText().trimmed();
    return result;
}

std::optional<NoteForm> note_form(QWidget* parent,
                                 const std::vector<domain::Entity>& entities,
                                 const std::vector<domain::Evidence>& evidence) {
    QDialog dialog(parent); dialog.setWindowTitle(QStringLiteral("Add investigation note")); set_form_width(dialog);
    auto* layout = new QVBoxLayout(&dialog); layout->addWidget(heading(QStringLiteral("How I found it"), 17));
    layout->addWidget(muted(QStringLiteral("Record a search, observation, failed lead, or decision.")));
    auto* title = new QLineEdit; auto* body = new QPlainTextEdit;
    body->setPlaceholderText(QStringLiteral("What did you do or find?")); body->setMinimumHeight(150); layout->addWidget(body, 1);
    auto* title_form = new QFormLayout; title_form->addRow(QStringLiteral("Title"), title);
    auto* entity = new QComboBox; entity->addItem(QStringLiteral("No linked entity"), QString());
    for (const auto& item : entities) entity->addItem(entity_choice_text(item), q(item.id));
    auto* evidence_item = new QComboBox; evidence_item->addItem(QStringLiteral("No linked evidence"), QString());
    for (const auto& item : evidence) {
        const auto label = item.kind == domain::EvidenceKind::File ? item.original_filename.value_or("Preserved file")
            : item.kind == domain::EvidenceKind::Url ? item.source_url.value_or("External URL") : "Text quotation";
        evidence_item->addItem(q(label), q(item.id));
    }
    auto* related_form = new QFormLayout; related_form->addRow(QStringLiteral("Related entity"), entity); related_form->addRow(QStringLiteral("Related evidence"), evidence_item);
    auto* related = new QWidget(&dialog); related->setLayout(related_form);
    auto* details = new QWidget(&dialog); auto* details_layout = new QVBoxLayout(details);
    details_layout->setContentsMargins(0, 0, 0, 0); details_layout->setSpacing(8);
    auto* title_details = new QWidget(details); title_details->setLayout(title_form);
    details_layout->addWidget(title_details); details_layout->addWidget(related);
    layout->addWidget(optional_details(QStringLiteral("Title or link to a record (optional)"), details, &dialog));
    auto* buttons = dialog_buttons(dialog); set_primary_action(buttons, QStringLiteral("Save note")); layout->addWidget(buttons);
    make_form_scrollable(dialog, buttons);
    QObject::disconnect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] { if (body->toPlainText().trimmed().isEmpty()) { QMessageBox::warning(&dialog, QStringLiteral("Note required"), QStringLiteral("Write the investigation note before saving.")); return; } dialog.accept(); });
    if (dialog.exec() != QDialog::Accepted) return std::nullopt;
    NoteForm result; result.title = title->text().trimmed(); result.body = body->toPlainText();
    const auto entity_id = entity->currentData().toString(); if (!entity_id.isEmpty()) result.entity_ids.push_back(s(entity_id));
    const auto evidence_id = evidence_item->currentData().toString(); if (!evidence_id.isEmpty()) result.evidence_ids.push_back(s(evidence_id));
    return result;
}

std::optional<RunForm> run_form(QWidget* parent, const std::vector<domain::Playbook>& playbooks, const std::vector<domain::Entity>& entities) {
    if (playbooks.empty()) { QMessageBox::information(parent, QStringLiteral("Playbook required"), QStringLiteral("Create a global playbook template before starting a run.")); return std::nullopt; }
    QDialog dialog(parent); dialog.setWindowTitle(QStringLiteral("Start playbook run")); set_form_width(dialog); auto* layout = new QVBoxLayout(&dialog); layout->addWidget(heading(QStringLiteral("Start playbook run"), 17)); auto* form = new QFormLayout; auto* playbook = new QComboBox; auto* entity = new QComboBox; for (const auto& item : playbooks) playbook->addItem(q(item.name), q(item.id)); entity->addItem(QStringLiteral("No target entity"), QString()); for (const auto& item : entities) entity->addItem(entity_choice_text(item), q(item.id)); form->addRow(QStringLiteral("Playbook *"), playbook); form->addRow(QStringLiteral("Target entity"), entity); layout->addLayout(form); layout->addWidget(muted(QStringLiteral("The run stores an independent snapshot of the selected template steps."))); auto* buttons = dialog_buttons(dialog); layout->addWidget(buttons); make_form_scrollable(dialog, buttons); if (dialog.exec() != QDialog::Accepted) return std::nullopt; RunForm result; result.playbook_id = s(playbook->currentData().toString()); const auto entity_id = entity->currentData().toString(); if (!entity_id.isEmpty()) result.entity_id = s(entity_id); return result;
}

std::optional<RunStepForm> run_step_form(QWidget* parent, const domain::RunStep& existing) {
    QDialog dialog(parent); dialog.setWindowTitle(QStringLiteral("Update playbook step")); set_form_width(dialog); auto* layout = new QVBoxLayout(&dialog); layout->addWidget(heading(q(existing.name_snapshot), 17)); layout->addWidget(muted(q(existing.instructions_snapshot))); auto* form = new QFormLayout; const QStringList values = {"todo", "done", "skipped"}; auto* state = enum_combo(&dialog, {"Todo", "Done", "Skipped"}, values); select_value(state, q(domain::to_string(existing.state))); auto* note = new QPlainTextEdit; note->setFixedHeight(100); note->setPlainText(q(existing.note)); form->addRow(QStringLiteral("State"), state); form->addRow(QStringLiteral("How I found it / result"), note); layout->addLayout(form); auto* buttons = dialog_buttons(dialog); layout->addWidget(buttons); make_form_scrollable(dialog, buttons); if (dialog.exec() != QDialog::Accepted) return std::nullopt; return RunStepForm{domain::run_step_state_from_string(s(state->currentData().toString())), note->toPlainText()};
}

std::optional<TechniqueForm> technique_form(QWidget* parent, const std::optional<domain::Technique>& existing) {
    QDialog dialog(parent);
    dialog.setWindowTitle(existing ? QStringLiteral("Edit technique") : QStringLiteral("New technique"));
    set_form_width(dialog, 620);
    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(heading(existing ? QStringLiteral("Edit technique") : QStringLiteral("New technique"), 17));
    auto* name = new QLineEdit;
    auto* objective = new QLineEdit;
    auto* steps = new QPlainTextEdit;
    auto* queries = new QPlainTextEdit;
    auto* tools = new QPlainTextEdit;
    auto* limitations = new QPlainTextEdit;
    auto* tags = new QLineEdit;
    name->setPlaceholderText(QStringLiteral("e.g. Check public commit history"));
    steps->setPlaceholderText(QStringLiteral("Describe what to check"));
    steps->setFixedHeight(90);
    limitations->setFixedHeight(65);
    auto* basic = new QFormLayout;
    basic->addRow(QStringLiteral("Name *"), name);
    basic->addRow(QStringLiteral("Instructions"), steps);
    layout->addLayout(basic);
    auto* advanced_form = new QFormLayout;
    queries->setPlaceholderText(QStringLiteral("One example per line"));
    tools->setPlaceholderText(QStringLiteral("One tool or link per line"));
    queries->setFixedHeight(72);
    tools->setFixedHeight(72);
    tags->setPlaceholderText(QStringLiteral("Separate tags with spaces"));
    advanced_form->addRow(QStringLiteral("Objective"), objective);
    advanced_form->addRow(QStringLiteral("Example queries (one per line)"), queries);
    advanced_form->addRow(QStringLiteral("Tools / links (one per line)"), tools);
    advanced_form->addRow(QStringLiteral("Limitations"), limitations);
    advanced_form->addRow(QStringLiteral("Tags"), tags);
    auto* advanced = new QWidget(&dialog);
    advanced->setLayout(advanced_form);
    const bool has_advanced = existing && (!existing->objective.empty() || !json_strings(existing->example_queries_json).empty() ||
        !json_strings(existing->tools_links_json).empty() || !existing->limitations.empty() || !json_strings(existing->tags_json).empty());
    layout->addWidget(optional_details(QStringLiteral("More details (optional)"), advanced, &dialog, has_advanced));
    auto* buttons = dialog_buttons(dialog);
    set_primary_action(buttons, existing ? QStringLiteral("Save changes") : QStringLiteral("Create technique"));
    layout->addWidget(buttons);
    make_form_scrollable(dialog, buttons);
    if (existing) {
        name->setText(q(existing->name)); objective->setText(q(existing->objective)); steps->setPlainText(q(existing->steps));
        queries->setPlainText(array_editor_text(json_strings(existing->example_queries_json)));
        tools->setPlainText(array_editor_text(json_strings(existing->tools_links_json))); limitations->setPlainText(q(existing->limitations));
        QStringList tag_values; for (const auto& item : json_strings(existing->tags_json)) tag_values << q(item);
        tags->setText(tag_values.join(QChar(' ')));
    }
    QObject::disconnect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        if (name->text().trimmed().isEmpty()) { QMessageBox::warning(&dialog, QStringLiteral("Name required"), QStringLiteral("A technique name is required.")); return; }
        dialog.accept();
    });
    if (dialog.exec() != QDialog::Accepted) return std::nullopt;
    return TechniqueForm{name->text().trimmed(), objective->text().trimmed(), steps->toPlainText(),
        string_array_json(queries->toPlainText()), string_array_json(tools->toPlainText()), limitations->toPlainText(),
        tag_array_json(tags->text())};
}

std::optional<PlaybookForm> playbook_form(QWidget* parent, const std::optional<domain::Playbook>& existing) {
    QDialog dialog(parent); dialog.setWindowTitle(existing ? QStringLiteral("Edit playbook") : QStringLiteral("New playbook")); set_form_width(dialog);
    auto* layout = new QVBoxLayout(&dialog); layout->addWidget(heading(existing ? QStringLiteral("Edit playbook") : QStringLiteral("New playbook"), 17));
    auto* name = new QLineEdit; name->setPlaceholderText(QStringLiteral("e.g. Username investigation")); layout->addWidget(name);
    auto* scope = new QLineEdit; auto* description = new QPlainTextEdit; description->setFixedHeight(100);
    auto* details_form = new QFormLayout; details_form->addRow(QStringLiteral("Scope"), scope); details_form->addRow(QStringLiteral("Description"), description);
    auto* details = new QWidget(&dialog); details->setLayout(details_form);
    const bool has_details = existing && (!existing->scope.empty() || !existing->description.empty());
    layout->addWidget(optional_details(QStringLiteral("More details (optional)"), details, &dialog, has_details));
    auto* buttons = dialog_buttons(dialog); set_primary_action(buttons, existing ? QStringLiteral("Save changes") : QStringLiteral("Create playbook")); layout->addWidget(buttons);
    make_form_scrollable(dialog, buttons);
    if (existing) { name->setText(q(existing->name)); scope->setText(q(existing->scope)); description->setPlainText(q(existing->description)); }
    QObject::disconnect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] { if (name->text().trimmed().isEmpty()) { QMessageBox::warning(&dialog, QStringLiteral("Name required"), QStringLiteral("A playbook name is required.")); return; } dialog.accept(); });
    if (dialog.exec() != QDialog::Accepted) return std::nullopt;
    return PlaybookForm{name->text().trimmed(), scope->text().trimmed(), description->toPlainText()};
}

std::optional<PlaybookStepForm> playbook_step_form(QWidget* parent, const std::vector<domain::Technique>& techniques) {
    QDialog dialog(parent); dialog.setWindowTitle(QStringLiteral("Add playbook step")); set_form_width(dialog); auto* layout = new QVBoxLayout(&dialog); layout->addWidget(heading(QStringLiteral("Add playbook step"), 17)); auto* form = new QFormLayout; auto* technique = new QComboBox; technique->addItem(QStringLiteral("Custom step"), QString()); for (const auto& item : techniques) technique->addItem(q(item.name), q(item.id)); auto* name = new QLineEdit; auto* instructions = new QPlainTextEdit; instructions->setFixedHeight(90); form->addRow(QStringLiteral("Start from"), technique); form->addRow(QStringLiteral("Step name *"), name); form->addRow(QStringLiteral("Instructions"), instructions); layout->addLayout(form); auto* buttons = dialog_buttons(dialog); layout->addWidget(buttons); make_form_scrollable(dialog, buttons); QObject::connect(technique, &QComboBox::currentIndexChanged, &dialog, [&](int index) { if (index <= 0) return; const auto& item = techniques[static_cast<std::size_t>(index - 1)]; if (name->text().isEmpty()) name->setText(q(item.name)); if (instructions->toPlainText().isEmpty()) instructions->setPlainText(q(item.steps)); }); QObject::disconnect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept); QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] { if (name->text().trimmed().isEmpty()) { QMessageBox::warning(&dialog, QStringLiteral("Step name required"), QStringLiteral("Add a name or choose a technique to fill it in.")); return; } dialog.accept(); }); if (dialog.exec() != QDialog::Accepted) return std::nullopt; PlaybookStepForm result; const auto technique_id = technique->currentData().toString(); if (!technique_id.isEmpty()) result.technique_id = s(technique_id); result.name = name->text().trimmed(); result.instructions = instructions->toPlainText(); return result;
}

std::optional<LinkEvidenceForm> link_evidence_form(QWidget* parent,
                                                   const std::vector<domain::Evidence>& evidence,
                                                   const std::vector<domain::Source>& sources,
                                                   bool include_link_note,
                                                   bool include_link_role,
                                                   domain::EvidenceRole preferred_role,
                                                   bool allow_no_evidence) {
    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Link evidence"));
    set_form_width(dialog, 600);
    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(heading(QStringLiteral("Link evidence"), 17));
    layout->addWidget(muted(QStringLiteral("Choose an existing item, add a URL or text observation, or preserve a local file. The relationship status remains your assessment.")));

    auto* form = new QFormLayout;
    auto* item = new QComboBox;
    if (allow_no_evidence) item->addItem(QStringLiteral("Create without an evidence link"), QStringLiteral("__none__"));
    for (const auto& value : evidence) {
        const auto kind = domain::to_string(value.kind);
        const auto name = value.kind == domain::EvidenceKind::File ? value.original_filename.value_or("preserved file") : value.kind == domain::EvidenceKind::Url ? value.source_url.value_or("external URL") : QStringLiteral("text quotation").toStdString();
        const auto label = QStringLiteral("%1  ·  %2  ·  %3").arg(q(name), enum_label(kind), shorten_id(value.id));
        item->addItem(label, q(value.id));
        item->setItemData(item->count() - 1, QStringLiteral("Existing evidence: %1\nID: %2").arg(q(name), q(value.id)), Qt::ToolTipRole);
    }
    item->addItem(QStringLiteral("Import new preserved file…"), QStringLiteral("__import_file__"));
    item->setItemData(item->count() - 1, QStringLiteral("Copy a local file into immutable case storage, calculate SHA-256, and link that new evidence record."), Qt::ToolTipRole);
    item->addItem(QStringLiteral("Add external URL…"), QStringLiteral("__new_url__"));
    item->addItem(QStringLiteral("Add text observation…"), QStringLiteral("__new_text__"));
    auto* source_label = new QLabel(QStringLiteral("Source record"));
    auto* source = new QComboBox;
    source->addItem(QStringLiteral("No linked source"), QString());
    for (const auto& value : sources) source->addItem(q(value.title.empty() ? value.locator : value.title), q(value.id));
    auto* file_label = new QLabel(QStringLiteral("New file"));
    auto* file_row = new QWidget;
    auto* file_layout = new QHBoxLayout(file_row);
    file_layout->setContentsMargins(0, 0, 0, 0);
    auto* file = new QLineEdit;
    file->setPlaceholderText(QStringLiteral("Select a local file to preserve"));
    auto* browse = button(QStringLiteral("Browse…"));
    file_layout->addWidget(file, 1);
    file_layout->addWidget(browse);
    auto* url = new QLineEdit; url->setPlaceholderText(QStringLiteral("https://…"));
    auto* text = new QPlainTextEdit; text->setPlaceholderText(QStringLiteral("What did the source show?")); text->setFixedHeight(76);
    auto* role = enum_combo(&dialog, {"Supports", "Contradicts", "Context"}, {"supports", "contradicts", "context"});
    select_value(role, q(domain::to_string(preferred_role)));
    auto* role_label = new QLabel(QStringLiteral("Role"));
    auto* note_label = new QLabel(QStringLiteral("Link note"));
    auto* note = new QLineEdit;
    note->setPlaceholderText(QStringLiteral("Optional explanation for this association"));
    form->addRow(QStringLiteral("Evidence"), item);
    form->addRow(source_label, source);
    form->addRow(file_label, file_row);
    form->addRow(QStringLiteral("URL"), url);
    form->addRow(QStringLiteral("Text"), text);
    form->addRow(role_label, role);
    form->addRow(note_label, note);
    role_label->setVisible(include_link_role);
    role->setVisible(include_link_role);
    note_label->setVisible(include_link_note);
    note->setVisible(include_link_note);
    layout->addLayout(form);
    auto* buttons = dialog_buttons(dialog);
    layout->addWidget(buttons);

    const auto update_visibility = [&] {
        const auto selected = item->currentData().toString();
        const bool importing = selected == "__import_file__";
        const bool adding_reference = selected == "__new_url__" || selected == "__new_text__";
        source_label->setVisible(importing || adding_reference);
        source->setVisible(importing || adding_reference);
        file_label->setVisible(importing);
        file_row->setVisible(importing);
        url->setVisible(selected == "__new_url__");
        text->setVisible(selected == "__new_text__");
    };
    QObject::connect(item, &QComboBox::currentIndexChanged, &dialog, update_visibility);
    QObject::connect(browse, &QPushButton::clicked, &dialog, [&] {
        const auto path = QFileDialog::getOpenFileName(&dialog, QStringLiteral("Select file to preserve"));
        if (!path.isEmpty()) file->setText(path);
    });
    update_visibility();

    make_form_scrollable(dialog, buttons);
    QObject::disconnect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        const bool importing = item->currentData().toString() == "__import_file__";
        const auto selected = item->currentData().toString();
        if (selected == "__none__") { dialog.accept(); return; }
        if (selected == "__new_url__" && url->text().trimmed().isEmpty()) { QMessageBox::warning(&dialog, QStringLiteral("URL required"), QStringLiteral("Enter the online location for this evidence.")); return; }
        if (selected == "__new_text__" && text->toPlainText().trimmed().isEmpty()) { QMessageBox::warning(&dialog, QStringLiteral("Text required"), QStringLiteral("Enter the observation to preserve.")); return; }
        if (!importing && item->currentData().toString().isEmpty()) {
            QMessageBox::warning(&dialog, QStringLiteral("Evidence required"), QStringLiteral("Select an existing evidence record or choose Import new preserved file."));
            return;
        }
        if (importing && !QFileInfo(file->text().trimmed()).isFile()) {
            QMessageBox::warning(&dialog, QStringLiteral("File required"), QStringLiteral("Select an existing regular file. It will be copied into this case and will not be executed."));
            return;
        }
        dialog.accept();
    });
    if (dialog.exec() != QDialog::Accepted) return std::nullopt;

    LinkEvidenceForm result;
    const auto selected = item->currentData().toString();
    if (selected == "__none__") return result;
    if (selected == "__import_file__") {
        result.import_file_path = file->text().trimmed();
        const auto source_id = source->currentData().toString();
        if (!source_id.isEmpty()) result.import_source_id = s(source_id);
    } else if (selected == "__new_url__") {
        result.new_url = url->text().trimmed();
        const auto source_id = source->currentData().toString(); if (!source_id.isEmpty()) result.import_source_id = s(source_id);
    } else if (selected == "__new_text__") {
        result.new_text = text->toPlainText().trimmed();
        const auto source_id = source->currentData().toString(); if (!source_id.isEmpty()) result.import_source_id = s(source_id);
    } else {
        result.evidence_id = s(item->currentData().toString());
    }
    result.role = domain::evidence_role_from_string(s(role->currentData().toString()));
    result.note = note->text().trimmed();
    return result;
}

std::optional<QString> explanation_form(QWidget* parent, const QString& title, const QString& prompt) {
    bool ok = false; const auto result = QInputDialog::getMultiLineText(parent, title, prompt, QString(), &ok); if (!ok || result.trimmed().isEmpty()) return std::nullopt; return result.trimmed();
}

} // namespace evidence_trace::gui
