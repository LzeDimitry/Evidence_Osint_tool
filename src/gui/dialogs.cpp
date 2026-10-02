#include "gui/dialogs.hpp"

#include "gui/gui_helpers.hpp"

#include <QCheckBox>
#include <QBoxLayout>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QVBoxLayout>

#include <sstream>

namespace evidence_trace::gui {

namespace {

template <typename T>
QString join_values(const std::vector<T>& values) {
    QStringList result;
    for (const auto& value : values) result.push_back(q(value));
    return result.join(QStringLiteral(", "));
}

std::vector<std::string> split_values(const QString& value) {
    std::vector<std::string> result;
    for (const auto& item : value.split(',', Qt::SkipEmptyParts)) {
        const auto trimmed = item.trimmed();
        if (!trimmed.isEmpty()) result.push_back(s(trimmed));
    }
    return result;
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

QDialogButtonBox* dialog_buttons(QDialog& dialog) {
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    if (auto* accept = buttons->button(QDialogButtonBox::Ok)) {
        accept->setText(QStringLiteral("Save"));
        accept->setProperty("accent", true);
        accept->setIcon(ui_icon(UiIcon::Check, QColor("#ffffff")));
        accept->setIconSize(QSize(17, 17));
    }
    if (auto* cancel = buttons->button(QDialogButtonBox::Cancel)) cancel->setText(QStringLiteral("Cancel"));
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    return buttons;
}

void set_primary_action(QDialogButtonBox* buttons, const QString& text) {
    if (buttons == nullptr) return;
    if (auto* accept = buttons->button(QDialogButtonBox::Ok)) accept->setText(text);
}

void select_value(QComboBox* combo, const QString& value) {
    const auto index = combo->findData(value);
    if (index >= 0) combo->setCurrentIndex(index);
}

QString entity_choice_text(const domain::Entity& entity) {
    return q(entity.label + "  ·  " + domain::to_string(entity.type) + "  ·  " + shorten_id(entity.id).toStdString());
}

void set_form_width(QDialog& dialog, int width = 540) {
    dialog.setObjectName(QStringLiteral("modalDialog"));
    dialog.setMinimumWidth(width);
    dialog.setModal(true);
}

} // namespace

std::optional<CaseForm> case_form(QWidget* parent, const std::optional<domain::Case>& existing) {
    QDialog dialog(parent);
    dialog.setWindowTitle(existing ? QStringLiteral("Edit case") : QStringLiteral("Create case"));
    set_form_width(dialog, 670);
    dialog.resize(670, 450);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(20, 18, 20, 14);
    layout->setSpacing(10);

    auto* header = new QWidget;
    auto* header_layout = new QHBoxLayout(header);
    header_layout->setContentsMargins(0, 0, 0, 0);
    header_layout->setSpacing(12);
    auto* icon = new QLabel;
    icon->setFixedSize(40, 40);
    icon->setAlignment(Qt::AlignCenter);
    icon->setPixmap(ui_icon(UiIcon::FolderAdd, QColor("#d0baff")).pixmap(QSize(23, 23)));
    icon->setStyleSheet(QStringLiteral("background:#24164e; border:1px solid #7048d7; border-radius:9px;"));
    auto* header_copy = new QVBoxLayout;
    header_copy->setContentsMargins(0, 0, 0, 0);
    header_copy->setSpacing(1);
    header_copy->addWidget(heading(existing ? QStringLiteral("Edit case") : QStringLiteral("Create case"), 19));
    header_copy->addWidget(muted(existing ? QStringLiteral("Update metadata without changing the investigation history.") : QStringLiteral("Create a new investigation case to organize your analysis, entities and evidence.")));
    header_layout->addWidget(icon);
    header_layout->addLayout(header_copy, 1);
    layout->addWidget(header);

    auto* form = new QVBoxLayout;
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(5);

    const auto add_field = [&](const QString& label_text, QWidget* field) {
        auto* label = new QLabel(label_text);
        label->setObjectName(QStringLiteral("formLabel"));
        form->addWidget(label);
        form->addWidget(field);
    };

    auto* title = new QLineEdit;
    auto* purpose = new QPlainTextEdit;
    auto* description = new QPlainTextEdit;
    description->setParent(&dialog);
    description->hide();
    auto* target = enum_combo(&dialog, {"None", "Person", "Company", "Place", "Domain", "Username", "Event", "Other"}, {"", "person", "company", "place", "domain", "username", "event", "other"});
    target->setItemIcon(1, ui_icon(UiIcon::Entity, QColor("#aebbd2")));
    target->setItemIcon(2, ui_icon(UiIcon::Entity, QColor("#aebbd2")));
    target->setItemIcon(3, ui_icon(UiIcon::Target, QColor("#aebbd2")));
    target->setItemIcon(4, ui_icon(UiIcon::Globe, QColor("#aebbd2")));
    target->setItemIcon(5, ui_icon(UiIcon::Entity, QColor("#aebbd2")));
    target->setItemIcon(6, ui_icon(UiIcon::Activity, QColor("#aebbd2")));
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

    add_field(QStringLiteral("Title *"), title);
    add_field(QStringLiteral("Purpose"), purpose);

    auto* paired_fields = new QHBoxLayout;
    paired_fields->setContentsMargins(0, 0, 0, 0);
    paired_fields->setSpacing(14);
    auto* target_field = new QVBoxLayout;
    target_field->setContentsMargins(0, 0, 0, 0);
    target_field->setSpacing(5);
    target_field->addWidget(new QLabel(QStringLiteral("Primary target type")));
    target_field->addWidget(target);
    auto* scope_field = new QVBoxLayout;
    scope_field->setContentsMargins(0, 0, 0, 0);
    scope_field->setSpacing(5);
    scope_field->addWidget(new QLabel(QStringLiteral("Scope")));
    scope_field->addWidget(scope);
    paired_fields->addLayout(target_field, 1);
    paired_fields->addLayout(scope_field, 1);
    form->addLayout(paired_fields);
    add_field(QStringLiteral("Tags"), tags);
    layout->addLayout(form);
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
    if (existing) {
        title->setText(q(existing->title)); purpose->setPlainText(q(existing->purpose)); description->setPlainText(q(existing->description));
        if (existing->primary_target_type) select_value(target, q(*existing->primary_target_type));
        scope->setPlainText(q(existing->scope));
        QStringList tag_values;
        for (const auto& tag : json_strings(existing->tags_json)) tag_values.push_back(q(tag));
        tags->setText(tag_values.join(QStringLiteral(" ")));
    }
    QObject::disconnect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        if (title->text().trimmed().isEmpty()) { QMessageBox::warning(&dialog, QStringLiteral("Title required"), QStringLiteral("A case title is required.")); return; }
        dialog.accept();
    });
    if (dialog.exec() != QDialog::Accepted) return std::nullopt;
    CaseForm result;
    result.title = title->text().trimmed(); result.purpose = purpose->toPlainText(); result.description = existing ? description->toPlainText() : QString();
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
    icon->setPixmap(ui_icon(existing ? UiIcon::Edit : UiIcon::Entity, QColor("#d7c4ff")).pixmap(QSize(22, 22)));
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
    fields_scroll->setMinimumHeight(220);
    auto* fields_body = new QWidget(fields_scroll);
    auto* fields_layout = new QVBoxLayout(fields_body);
    fields_layout->setContentsMargins(0, 0, 2, 0);
    fields_layout->setSpacing(10);
    fields_scroll->setWidget(fields_body);

    const QStringList type_values = {"person", "username", "account", "email", "phone", "domain", "ip", "company", "place", "url", "image", "document", "event", "other"};
    QStringList type_labels; for (const auto& value : type_values) type_labels << enum_label(value.toStdString());
    auto* type = enum_combo(&dialog, type_labels, type_values);
    auto* label = new QLineEdit; auto* value = new QLineEdit; auto* description = new QPlainTextEdit; auto* tags = new QLineEdit; auto* aliases = new QLineEdit;
    label->setPlaceholderText(QStringLiteral("A readable name for this object"));
    value->setPlaceholderText(QStringLiteral("Username, domain, address, or other observed value"));
    tags->setPlaceholderText(QStringLiteral("Separate tags with spaces, e.g. priority public-records lead"));
    aliases->setPlaceholderText(QStringLiteral("Separate alternate names with commas"));
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
    identity_layout->addWidget(field_block(QStringLiteral("Observed / original value"), value), 2, 0, 1, 2);
    identity_layout->addWidget(field_block(QStringLiteral("Description"), description), 3, 0, 1, 2);
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
    fields_layout->addWidget(metadata);
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
    if (dialog.exec() != QDialog::Accepted) return std::nullopt;
    EntityForm result; result.type = domain::entity_type_from_string(s(type->currentData().toString())); result.label = label->text().trimmed(); result.original_value = value->text().trimmed();
    result.description = description->toPlainText().trimmed(); result.tags = split_tag_values(tags->text()); result.aliases = split_values(aliases->text()); return result;
}

std::optional<SourceForm> source_form(QWidget* parent, const std::optional<domain::Source>& existing) {
    QDialog dialog(parent); dialog.setWindowTitle(existing ? QStringLiteral("Edit source") : QStringLiteral("Add source")); set_form_width(dialog);
    auto* layout = new QVBoxLayout(&dialog); layout->addWidget(heading(existing ? QStringLiteral("Edit source") : QStringLiteral("Add source"), 17));
    auto* form = new QFormLayout; const QStringList values = {"web", "document", "registry", "person", "other"}; QStringList labels; for (const auto& value : values) labels << enum_label(value.toStdString());
    auto* type = enum_combo(&dialog, labels, values); auto* locator = new QLineEdit; auto* title = new QLineEdit; auto* accessed = new QLineEdit; auto* author = new QLineEdit; auto* reliability = new QPlainTextEdit; reliability->setFixedHeight(64);
    form->addRow(QStringLiteral("Type *"), type); form->addRow(QStringLiteral("Locator"), locator); form->addRow(QStringLiteral("Title"), title); form->addRow(QStringLiteral("Accessed UTC"), accessed); form->addRow(QStringLiteral("Author"), author); form->addRow(QStringLiteral("Reliability note"), reliability);
    layout->addLayout(form); auto* buttons = dialog_buttons(dialog); set_primary_action(buttons, existing ? QStringLiteral("Save changes") : QStringLiteral("Add source")); layout->addWidget(buttons);
    if (existing) { select_value(type, q(domain::to_string(existing->type))); locator->setText(q(existing->locator)); title->setText(q(existing->title)); accessed->setText(q(existing->accessed_at)); author->setText(q(existing->author)); reliability->setPlainText(q(existing->reliability_note)); }
    if (dialog.exec() != QDialog::Accepted) return std::nullopt;
    SourceForm result; result.type = domain::source_type_from_string(s(type->currentData().toString())); result.locator = locator->text().trimmed(); result.title = title->text().trimmed(); result.accessed_at = accessed->text().trimmed(); result.author = author->text().trimmed(); result.reliability_note = reliability->toPlainText().trimmed(); return result;
}

std::optional<EvidenceForm> evidence_form(QWidget* parent, const std::vector<domain::Source>& sources) {
    QDialog dialog(parent); dialog.setWindowTitle(QStringLiteral("Add evidence")); set_form_width(dialog, 590);
    auto* layout = new QVBoxLayout(&dialog); layout->addWidget(heading(QStringLiteral("Add evidence"), 17)); layout->addWidget(muted(QStringLiteral("Files are copied into immutable case storage. A URL without a preserved copy remains external only.")));
    auto* form = new QFormLayout; auto* kind = enum_combo(&dialog, {"Text quotation", "External URL", "Preserved file"}, {"text", "url", "file"});
    auto* source = new QComboBox; source->addItem(QStringLiteral("No linked source"), QString()); for (const auto& item : sources) source->addItem(q(item.title.empty() ? item.locator : item.title), q(item.id));
    auto* file_row = new QWidget; auto* file_layout = new QHBoxLayout(file_row); file_layout->setContentsMargins(0, 0, 0, 0); auto* file = new QLineEdit; auto* browse = button(QStringLiteral("Browse")); file_layout->addWidget(file); file_layout->addWidget(browse);
    auto* text = new QPlainTextEdit; text->setFixedHeight(105); auto* url = new QLineEdit; auto* note = new QPlainTextEdit; note->setFixedHeight(64); auto* location = new QLineEdit; auto* source_url = new QLineEdit;
    form->addRow(QStringLiteral("Kind *"), kind); form->addRow(QStringLiteral("Source record"), source); form->addRow(QStringLiteral("File"), file_row); form->addRow(QStringLiteral("Text quotation"), text); form->addRow(QStringLiteral("URL"), url); form->addRow(QStringLiteral("Source URL (optional)"), source_url); form->addRow(QStringLiteral("Quotation location"), location); form->addRow(QStringLiteral("Note"), note);
    layout->addLayout(form); auto* buttons = dialog_buttons(dialog); set_primary_action(buttons, QStringLiteral("Add evidence")); layout->addWidget(buttons);
    auto update_visibility = [&] { const auto selected = kind->currentData().toString(); file_row->setVisible(selected == "file"); text->setVisible(selected == "text"); url->setVisible(selected == "url"); source_url->setVisible(selected != "url"); location->setVisible(selected == "text"); };
    QObject::connect(kind, &QComboBox::currentIndexChanged, &dialog, update_visibility); QObject::connect(browse, &QPushButton::clicked, &dialog, [&] { const auto path = QFileDialog::getOpenFileName(&dialog, QStringLiteral("Select evidence file")); if (!path.isEmpty()) file->setText(path); }); update_visibility();
    QObject::disconnect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        const auto selected = kind->currentData().toString(); const bool valid = (selected == "file" && !file->text().trimmed().isEmpty()) || (selected == "text" && !text->toPlainText().trimmed().isEmpty()) || (selected == "url" && !url->text().trimmed().isEmpty());
        if (!valid) { QMessageBox::warning(&dialog, QStringLiteral("Evidence content required"), QStringLiteral("Provide a file, text quotation, or URL for the selected evidence kind.")); return; } dialog.accept();
    });
    if (dialog.exec() != QDialog::Accepted) return std::nullopt;
    EvidenceForm result; result.kind = domain::evidence_kind_from_string(s(kind->currentData().toString())); result.file_path = file->text().trimmed(); result.text = text->toPlainText(); result.url = result.kind == domain::EvidenceKind::Url ? url->text().trimmed() : source_url->text().trimmed(); result.note = note->toPlainText().trimmed(); result.quotation_location = location->text().trimmed(); const auto source_id = source->currentData().toString(); if (!source_id.isEmpty()) result.source_id = s(source_id); return result;
}

std::optional<ClaimForm> claim_form(QWidget* parent, const std::vector<domain::Entity>& entities) {
    if (entities.empty()) { QMessageBox::information(parent, QStringLiteral("Entities required"), QStringLiteral("Add at least one entity before creating a claim or relation.")); return std::nullopt; }
    QDialog dialog(parent); dialog.setWindowTitle(QStringLiteral("New claim or relation")); set_form_width(dialog, 600); auto* layout = new QVBoxLayout(&dialog); layout->addWidget(heading(QStringLiteral("New claim or relation"), 17));
    auto* form = new QFormLayout; auto* mode = enum_combo(&dialog, {"Directional relation", "Entity claim"}, {"relation", "claim"}); auto* subject = new QComboBox; auto* object = new QComboBox; auto* context = new QComboBox; auto* predicate = new QLineEdit; auto* statement = new QLineEdit; auto* kind = enum_combo(&dialog, {"Observation", "Inference"}, {"observation", "inference"}); const QStringList status_values = {"unverified", "possible", "probable", "confirmed", "refuted"}; QStringList status_labels; for (const auto& value : status_values) status_labels << enum_label(value.toStdString()); auto* status = enum_combo(&dialog, status_labels, status_values); auto* reasoning = new QPlainTextEdit; reasoning->setFixedHeight(88);
    subject->addItem(QStringLiteral("Select subject"), QString()); object->addItem(QStringLiteral("Select object"), QString()); context->addItem(QStringLiteral("Select entity"), QString()); for (const auto& entity : entities) { const auto text = entity_choice_text(entity); subject->addItem(text, q(entity.id)); object->addItem(text, q(entity.id)); context->addItem(text, q(entity.id)); }
    auto* subject_label = new QLabel(QStringLiteral("Subject"));
    auto* predicate_label = new QLabel(QStringLiteral("Predicate"));
    auto* object_label = new QLabel(QStringLiteral("Object"));
    auto* context_label = new QLabel(QStringLiteral("Entity"));
    form->addRow(QStringLiteral("Record as"), mode); form->addRow(subject_label, subject); form->addRow(predicate_label, predicate); form->addRow(object_label, object); form->addRow(context_label, context); form->addRow(QStringLiteral("Statement"), statement); form->addRow(QStringLiteral("Kind"), kind); form->addRow(QStringLiteral("Initial status"), status); form->addRow(QStringLiteral("Reasoning"), reasoning); layout->addLayout(form); layout->addWidget(muted(QStringLiteral("Confirmed claims require written reasoning and at least one supporting evidence item. After you submit this form, choose an existing item or import a new file."))); auto* buttons = dialog_buttons(dialog); layout->addWidget(buttons);
    select_value(kind, QStringLiteral("inference"));
    auto update = [&] { const bool relation = mode->currentData().toString() == "relation"; subject_label->setVisible(relation); subject->setVisible(relation); predicate_label->setVisible(relation); predicate->setVisible(relation); object_label->setVisible(relation); object->setVisible(relation); context_label->setVisible(!relation); context->setVisible(!relation); kind->setEnabled(true); };
    QObject::connect(mode, &QComboBox::currentIndexChanged, &dialog, [&] {
        update();
        set_primary_action(buttons, mode->currentData().toString() == "relation" ? QStringLiteral("Create relation") : QStringLiteral("Create claim"));
    });
    update();
    set_primary_action(buttons, QStringLiteral("Create relation"));
    QObject::disconnect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] { const bool relation = mode->currentData().toString() == "relation"; if (relation && (subject->currentData().toString().isEmpty() || object->currentData().toString().isEmpty() || predicate->text().trimmed().isEmpty())) { QMessageBox::warning(&dialog, QStringLiteral("Relation fields required"), QStringLiteral("Select a subject and object and provide a predicate.")); return; } if (!relation && context->currentData().toString().isEmpty()) { QMessageBox::warning(&dialog, QStringLiteral("Entity required"), QStringLiteral("Select the entity this claim refers to.")); return; } if (statement->text().trimmed().isEmpty()) { QMessageBox::warning(&dialog, QStringLiteral("Statement required"), QStringLiteral("A claim statement is required.")); return; } if (kind->currentData().toString() == "inference" && reasoning->toPlainText().trimmed().isEmpty()) { QMessageBox::warning(&dialog, QStringLiteral("Reasoning required"), QStringLiteral("Inference claims require written reasoning.")); return; } if (status->currentData().toString() == "confirmed" && reasoning->toPlainText().trimmed().isEmpty()) { QMessageBox::warning(&dialog, QStringLiteral("Reasoning required"), QStringLiteral("Confirmed claims require written reasoning.")); return; } dialog.accept(); });
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

std::optional<NoteForm> note_form(QWidget* parent) {
    QDialog dialog(parent); dialog.setWindowTitle(QStringLiteral("Add investigation note")); set_form_width(dialog); auto* layout = new QVBoxLayout(&dialog); layout->addWidget(heading(QStringLiteral("How I found it"), 17)); layout->addWidget(muted(QStringLiteral("Keep failed leads, searches, observations, and decisions reproducible."))); auto* form = new QFormLayout; auto* title = new QLineEdit; auto* body = new QPlainTextEdit; body->setMinimumHeight(150); form->addRow(QStringLiteral("Title"), title); form->addRow(QStringLiteral("Note *"), body); layout->addLayout(form); auto* buttons = dialog_buttons(dialog); set_primary_action(buttons, QStringLiteral("Save note")); layout->addWidget(buttons); QObject::disconnect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept); QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] { if (body->toPlainText().trimmed().isEmpty()) { QMessageBox::warning(&dialog, QStringLiteral("Note required"), QStringLiteral("Write the investigation note before saving.")); return; } dialog.accept(); }); if (dialog.exec() != QDialog::Accepted) return std::nullopt; return NoteForm{title->text().trimmed(), body->toPlainText()};
}

std::optional<RunForm> run_form(QWidget* parent, const std::vector<domain::Playbook>& playbooks, const std::vector<domain::Entity>& entities) {
    if (playbooks.empty()) { QMessageBox::information(parent, QStringLiteral("Playbook required"), QStringLiteral("Create a global playbook template before starting a run.")); return std::nullopt; }
    QDialog dialog(parent); dialog.setWindowTitle(QStringLiteral("Start playbook run")); set_form_width(dialog); auto* layout = new QVBoxLayout(&dialog); layout->addWidget(heading(QStringLiteral("Start playbook run"), 17)); auto* form = new QFormLayout; auto* playbook = new QComboBox; auto* entity = new QComboBox; for (const auto& item : playbooks) playbook->addItem(q(item.name), q(item.id)); entity->addItem(QStringLiteral("No target entity"), QString()); for (const auto& item : entities) entity->addItem(entity_choice_text(item), q(item.id)); form->addRow(QStringLiteral("Playbook *"), playbook); form->addRow(QStringLiteral("Target entity"), entity); layout->addLayout(form); layout->addWidget(muted(QStringLiteral("The run stores an independent snapshot of the selected template steps."))); layout->addWidget(dialog_buttons(dialog)); if (dialog.exec() != QDialog::Accepted) return std::nullopt; RunForm result; result.playbook_id = s(playbook->currentData().toString()); const auto entity_id = entity->currentData().toString(); if (!entity_id.isEmpty()) result.entity_id = s(entity_id); return result;
}

std::optional<RunStepForm> run_step_form(QWidget* parent, const domain::RunStep& existing) {
    QDialog dialog(parent); dialog.setWindowTitle(QStringLiteral("Update playbook step")); set_form_width(dialog); auto* layout = new QVBoxLayout(&dialog); layout->addWidget(heading(q(existing.name_snapshot), 17)); layout->addWidget(muted(q(existing.instructions_snapshot))); auto* form = new QFormLayout; const QStringList values = {"todo", "done", "skipped"}; auto* state = enum_combo(&dialog, {"Todo", "Done", "Skipped"}, values); select_value(state, q(domain::to_string(existing.state))); auto* note = new QPlainTextEdit; note->setFixedHeight(100); note->setPlainText(q(existing.note)); form->addRow(QStringLiteral("State"), state); form->addRow(QStringLiteral("How I found it / result"), note); layout->addLayout(form); layout->addWidget(dialog_buttons(dialog)); if (dialog.exec() != QDialog::Accepted) return std::nullopt; return RunStepForm{domain::run_step_state_from_string(s(state->currentData().toString())), note->toPlainText()};
}

std::optional<TechniqueForm> technique_form(QWidget* parent, const std::optional<domain::Technique>& existing) {
    QDialog dialog(parent); dialog.setWindowTitle(existing ? QStringLiteral("Edit technique") : QStringLiteral("New technique")); set_form_width(dialog, 620); auto* layout = new QVBoxLayout(&dialog); layout->addWidget(heading(existing ? QStringLiteral("Edit technique") : QStringLiteral("New technique"), 17)); auto* form = new QFormLayout; auto* name = new QLineEdit; auto* objective = new QLineEdit; auto* steps = new QPlainTextEdit; auto* queries = new QLineEdit; auto* tools = new QLineEdit; auto* limitations = new QPlainTextEdit; auto* tags = new QLineEdit; steps->setFixedHeight(90); limitations->setFixedHeight(65); form->addRow(QStringLiteral("Name *"), name); form->addRow(QStringLiteral("Objective"), objective); form->addRow(QStringLiteral("Instructions"), steps); form->addRow(QStringLiteral("Example queries JSON"), queries); form->addRow(QStringLiteral("Tools / links JSON"), tools); form->addRow(QStringLiteral("Limitations"), limitations); form->addRow(QStringLiteral("Tags JSON"), tags); layout->addLayout(form); auto* buttons = dialog_buttons(dialog); layout->addWidget(buttons); if (existing) { name->setText(q(existing->name)); objective->setText(q(existing->objective)); steps->setPlainText(q(existing->steps)); queries->setText(q(existing->example_queries_json)); tools->setText(q(existing->tools_links_json)); limitations->setPlainText(q(existing->limitations)); tags->setText(q(existing->tags_json)); } QObject::disconnect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept); QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] { if (name->text().trimmed().isEmpty()) { QMessageBox::warning(&dialog, QStringLiteral("Name required"), QStringLiteral("A technique name is required.")); return; } dialog.accept(); }); if (dialog.exec() != QDialog::Accepted) return std::nullopt; return TechniqueForm{name->text().trimmed(), objective->text().trimmed(), steps->toPlainText(), queries->text().trimmed().isEmpty() ? QStringLiteral("[]") : queries->text().trimmed(), tools->text().trimmed().isEmpty() ? QStringLiteral("[]") : tools->text().trimmed(), limitations->toPlainText(), tags->text().trimmed().isEmpty() ? QStringLiteral("[]") : tags->text().trimmed()};
}

std::optional<PlaybookForm> playbook_form(QWidget* parent, const std::optional<domain::Playbook>& existing) {
    QDialog dialog(parent); dialog.setWindowTitle(existing ? QStringLiteral("Edit playbook") : QStringLiteral("New playbook")); set_form_width(dialog); auto* layout = new QVBoxLayout(&dialog); layout->addWidget(heading(existing ? QStringLiteral("Edit playbook") : QStringLiteral("New playbook"), 17)); auto* form = new QFormLayout; auto* name = new QLineEdit; auto* scope = new QLineEdit; auto* description = new QPlainTextEdit; description->setFixedHeight(100); form->addRow(QStringLiteral("Name *"), name); form->addRow(QStringLiteral("Scope"), scope); form->addRow(QStringLiteral("Description"), description); layout->addLayout(form); auto* buttons = dialog_buttons(dialog); layout->addWidget(buttons); if (existing) { name->setText(q(existing->name)); scope->setText(q(existing->scope)); description->setPlainText(q(existing->description)); } QObject::disconnect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept); QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] { if (name->text().trimmed().isEmpty()) { QMessageBox::warning(&dialog, QStringLiteral("Name required"), QStringLiteral("A playbook name is required.")); return; } dialog.accept(); }); if (dialog.exec() != QDialog::Accepted) return std::nullopt; return PlaybookForm{name->text().trimmed(), scope->text().trimmed(), description->toPlainText()};
}

std::optional<PlaybookStepForm> playbook_step_form(QWidget* parent, const std::vector<domain::Technique>& techniques) {
    QDialog dialog(parent); dialog.setWindowTitle(QStringLiteral("Add playbook step")); set_form_width(dialog); auto* layout = new QVBoxLayout(&dialog); layout->addWidget(heading(QStringLiteral("Add playbook step"), 17)); auto* form = new QFormLayout; auto* technique = new QComboBox; technique->addItem(QStringLiteral("Custom step"), QString()); for (const auto& item : techniques) technique->addItem(q(item.name), q(item.id)); auto* name = new QLineEdit; auto* instructions = new QPlainTextEdit; instructions->setFixedHeight(90); form->addRow(QStringLiteral("Technique"), technique); form->addRow(QStringLiteral("Step name *"), name); form->addRow(QStringLiteral("Instructions"), instructions); layout->addLayout(form); layout->addWidget(dialog_buttons(dialog)); QObject::connect(technique, &QComboBox::currentIndexChanged, &dialog, [&](int index) { if (index <= 0) return; const auto& item = techniques[static_cast<std::size_t>(index - 1)]; if (name->text().isEmpty()) name->setText(q(item.name)); if (instructions->toPlainText().isEmpty()) instructions->setPlainText(q(item.steps)); }); if (dialog.exec() != QDialog::Accepted) return std::nullopt; if (name->text().trimmed().isEmpty()) { QMessageBox::warning(parent, QStringLiteral("Name required"), QStringLiteral("A step name is required.")); return std::nullopt; } PlaybookStepForm result; const auto technique_id = technique->currentData().toString(); if (!technique_id.isEmpty()) result.technique_id = s(technique_id); result.name = name->text().trimmed(); result.instructions = instructions->toPlainText(); return result;
}

std::optional<LinkEvidenceForm> link_evidence_form(QWidget* parent,
                                                   const std::vector<domain::Evidence>& evidence,
                                                   const std::vector<domain::Source>& sources,
                                                   bool include_link_note,
                                                   bool include_link_role) {
    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Link evidence"));
    set_form_width(dialog, 600);
    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(heading(QStringLiteral("Link evidence"), 17));
    layout->addWidget(muted(QStringLiteral("Choose an evidence record already preserved in this case, or import a new local file and link the new record after it is copied and hashed.")));

    auto* form = new QFormLayout;
    auto* item = new QComboBox;
    for (const auto& value : evidence) {
        const auto kind = domain::to_string(value.kind);
        const auto name = value.kind == domain::EvidenceKind::File ? value.original_filename.value_or("preserved file") : value.kind == domain::EvidenceKind::Url ? value.source_url.value_or("external URL") : QStringLiteral("text quotation").toStdString();
        const auto label = QStringLiteral("%1  ·  %2  ·  %3").arg(q(name), enum_label(kind), shorten_id(value.id));
        item->addItem(label, q(value.id));
        item->setItemData(item->count() - 1, QStringLiteral("Existing evidence: %1\nID: %2").arg(q(name), q(value.id)), Qt::ToolTipRole);
    }
    item->addItem(QStringLiteral("Import new preserved file…"), QStringLiteral("__import_file__"));
    item->setItemData(item->count() - 1, QStringLiteral("Copy a local file into immutable case storage, calculate SHA-256, and link that new evidence record."), Qt::ToolTipRole);
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
    auto* role = enum_combo(&dialog, {"Supports", "Contradicts", "Context"}, {"supports", "contradicts", "context"});
    auto* role_label = new QLabel(QStringLiteral("Role"));
    auto* note_label = new QLabel(QStringLiteral("Link note"));
    auto* note = new QLineEdit;
    note->setPlaceholderText(QStringLiteral("Optional explanation for this association"));
    form->addRow(QStringLiteral("Evidence"), item);
    form->addRow(source_label, source);
    form->addRow(file_label, file_row);
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
        const bool importing = item->currentData().toString() == "__import_file__";
        source_label->setVisible(importing);
        source->setVisible(importing);
        file_label->setVisible(importing);
        file_row->setVisible(importing);
    };
    QObject::connect(item, &QComboBox::currentIndexChanged, &dialog, update_visibility);
    QObject::connect(browse, &QPushButton::clicked, &dialog, [&] {
        const auto path = QFileDialog::getOpenFileName(&dialog, QStringLiteral("Select file to preserve"));
        if (!path.isEmpty()) file->setText(path);
    });
    update_visibility();

    QObject::disconnect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        const bool importing = item->currentData().toString() == "__import_file__";
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
    if (item->currentData().toString() == "__import_file__") {
        result.import_file_path = file->text().trimmed();
        const auto source_id = source->currentData().toString();
        if (!source_id.isEmpty()) result.import_source_id = s(source_id);
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
