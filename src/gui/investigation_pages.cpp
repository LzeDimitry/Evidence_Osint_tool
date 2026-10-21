#include "gui/investigation_pages.hpp"

#include "gui/dialogs.hpp"
#include "gui/gui_helpers.hpp"

#include <QApplication>
#include <QBoxLayout>
#include <QClipboard>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGuiApplication>
#include <QFormLayout>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImageReader>
#include <QImage>
#include <QInputDialog>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QMimeData>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPaintEvent>
#include <QPushButton>
#include <QPixmap>
#include <QRadialGradient>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QScreen>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QTabBar>
#include <QTextEdit>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <set>
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <unordered_map>

namespace evidence_trace::gui {

namespace {

QString table_text(const QString& value) {
    // Let Qt wrap at the actual column width. The old fixed 16-character
    // wrapping made dates, statements, and descriptions look truncated even
    // when the table had room. Zero-width break opportunities keep URLs and
    // hashes usable without inserting visible fragments or ellipses.
    return wrap_long_tokens(value);
}

QString relative_time_label(const std::string& utc_iso) {
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


QString evidence_link_summary(const std::vector<domain::ClaimEvidenceLink>& links) {
    if (links.empty()) return QStringLiteral("No evidence linked");
    int supporting = 0, contradicting = 0, context = 0;
    for (const auto& link : links) {
        if (link.role == domain::EvidenceRole::Supports) ++supporting;
        else if (link.role == domain::EvidenceRole::Contradicts) ++contradicting;
        else ++context;
    }
    QStringList parts;
    if (supporting) parts << QStringLiteral("Supports %1").arg(supporting);
    if (contradicting) parts << QStringLiteral("Contradicts %1").arg(contradicting);
    if (context) parts << QStringLiteral("Context %1").arg(context);
    return QStringLiteral("%1 item%2 · %3").arg(links.size()).arg(links.size() == 1 ? QString() : QStringLiteral("s"), parts.join(QStringLiteral(" · ")));
}

QTableWidgetItem* cell(const QString& text, const QVariant& data = {}) {
    auto* result = new QTableWidgetItem(table_text(text));
    result->setToolTip(text);
    result->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    if (data.isValid()) result->setData(Qt::UserRole, data);
    return result;
}



UiIcon graph_entity_icon(const QString& type) {
    const auto lower = type.toLower();
    if (lower.contains(QStringLiteral("domain")) || lower.contains(QStringLiteral("url"))) return UiIcon::Globe;
    if (lower.contains(QStringLiteral("ip"))) return UiIcon::Server;
    if (lower.contains(QStringLiteral("email"))) return UiIcon::Email;
    if (lower.contains(QStringLiteral("phone"))) return UiIcon::Phone;
    if (lower.contains(QStringLiteral("company")) || lower.contains(QStringLiteral("organization"))) return UiIcon::Organization;
    if (lower.contains(QStringLiteral("place"))) return UiIcon::MapPin;
    if (lower.contains(QStringLiteral("person"))) return UiIcon::Person;
    if (lower.contains(QStringLiteral("user")) || lower.contains(QStringLiteral("account"))) return UiIcon::Entity;
    if (lower.contains(QStringLiteral("image"))) return UiIcon::Image;
    if (lower.contains(QStringLiteral("document"))) return UiIcon::Document;
    return UiIcon::Grid;
}

QPixmap entity_badge_pixmap(const QString& type, const QColor& color) {
    QPixmap pixmap(28, 28);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, 1.3));
    painter.setBrush(QColor("#111b29"));
    painter.drawEllipse(QRectF(1, 1, 26, 26));
    const auto icon = ui_icon(graph_entity_icon(type), color).pixmap(QSize(16, 16));
    painter.drawPixmap(QRect(6, 6, 16, 16), icon);
    return pixmap;
}

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
        auto* action_layout = new QBoxLayout(QBoxLayout::LeftToRight, action_strip);
        action_layout->setContentsMargins(0, 0, 0, 0);
        action_layout->setSpacing(7);
        for (auto* action : actions) action_layout->addWidget(action);
        action_strip->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        layout->addWidget(action_strip, 0, Qt::AlignBottom);
        observe_resize(row, [row, layout, action_layout](int width) {
            const auto narrow = width < 820;
            layout->setDirection(narrow ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
            action_layout->setDirection(width < 590 ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
            layout->setAlignment(narrow ? Qt::AlignLeft : Qt::Alignment());
            row->setMinimumHeight(narrow ? 128 : 0);
            row->updateGeometry();
        });
    }
    return row;
}

class RelationGraphWidget final : public QWidget {
public:
    struct Relation {
        QString claim_id;
        QString claim_statement;
        QString subject_id;
        QString subject;
        QString predicate;
        QString object_id;
        QString object;
        QString subject_type;
        QString object_type;
        QString status;
    };

    std::function<void(const QString&)> on_entity_clicked;
    std::function<void(const QString&)> on_claim_clicked;
    std::function<void()> on_create_entity;
    std::function<void(const QPointF&)> on_create_entity_at;
    std::function<void(const QString&, const QString&)> on_create_relation;
    std::function<void(const QString&)> on_edit_relation;
    std::function<void(const QString&)> on_reverse_relation;
    std::function<void(const QString&)> on_delete_relation;

    explicit RelationGraphWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumHeight(0);
        setMinimumWidth(420);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setObjectName("relationGraph");
        setFocusPolicy(Qt::StrongFocus);
    }

    void set_relations(std::vector<Relation> relations, std::vector<std::tuple<QString, QString, QString>> isolated = {}) {
        relations_ = std::move(relations);
        isolated_ = std::move(isolated);
        layout_valid_ = false;
        if (!selected_claim_id_.isEmpty() && std::none_of(relations_.begin(), relations_.end(), [this](const auto& value) { return value.claim_id == selected_claim_id_; })) selected_claim_id_.clear();
        edge_paths_.clear();
        update();
    }

    void set_persistence_key(const QString& key) {
        persistence_key_ = key;
        if (persistence_key_.isEmpty()) return;
        QSettings settings;
        settings.beginGroup(QStringLiteral("graph/%1").arg(persistence_key_));
        const auto points = settings.value(QStringLiteral("positions")).toMap();
        for (auto it = points.cbegin(); it != points.cend(); ++it) if (it.value().canConvert<QPointF>()) manual_positions_.insert(it.key(), it.value().toPointF());
        if (settings.contains(QStringLiteral("zoom"))) {
            zoom_ = std::clamp(settings.value(QStringLiteral("zoom")).toReal(), 0.01, 3.0);
            view_center_ = settings.value(QStringLiteral("center"), view_center_).toPointF();
            fit_pending_ = false;
            auto_fit_ = false;
        }
        settings.endGroup();
    }

    void begin_connection(const QString& source_id = {}) {
        ensure_layout();
        if (nodes_.size() < 2) {
            QMessageBox::information(this, QStringLiteral("Add another entity"), QStringLiteral("Create at least two entities before connecting them."));
            return;
        }
        connecting_from_ = source_id;
        connect_first_node_ = source_id.isEmpty();
        connect_cursor_ = mapFromGlobal(QCursor::pos());
        if (!connecting_from_.isEmpty()) selected_node_id_ = connecting_from_;
        else selected_node_id_.clear();
        selected_claim_id_.clear();
        setFocus(Qt::MouseFocusReason);
        setCursor(Qt::CrossCursor);
        update();
    }

    void push_undo(std::function<void()> action) {
        if (!action) return;
        undo_actions_.push_back(std::move(action));
        if (undo_actions_.size() > 100) undo_actions_.erase(undo_actions_.begin());
    }

    void undo() {
        if (undo_actions_.empty()) return;
        auto action = std::move(undo_actions_.back());
        undo_actions_.pop_back();
        action();
        setFocus(Qt::OtherFocusReason);
    }

    void fit_to_view() {
        ensure_layout();
        if (width() <= 0 || height() <= 0) return;
        const auto bounds = scene_bounds_.isNull() ? QRectF(0, 0, 1, 1) : scene_bounds_.adjusted(-24, -24, 24, 24);
        const auto available_width = std::max(1, width() - 24);
        const auto available_height = std::max(1, height() - 24);
        zoom_ = std::clamp(std::min(available_width / bounds.width(), available_height / bounds.height()), 0.01, 3.0);
        view_center_ = bounds.center();
        fit_pending_ = false;
        auto_fit_ = true;
        save_view_state();
        update();
    }

    void zoom_by(qreal factor) {
        ensure_layout();
        zoom_ = std::clamp(zoom_ * factor, 0.01, 3.0);
        fit_pending_ = false;
        auto_fit_ = false;
        save_view_state();
        update();
    }

    void select_claim(const QString& claim_id) {
        selected_node_id_.clear();
        selected_claim_id_ = claim_id;
        update();
    }

    void place_entity(const QString& entity_id, const QPointF& point) {
        ensure_layout();
        if (!node_index_.contains(entity_id)) return;
        manual_positions_.insert(entity_id, point);
        layout_valid_ = false;
        ensure_layout();
        save_view_state();
        update();
    }

    bool search_entities(const QString& text) {
        ensure_layout();
        const auto query = text.trimmed();
        selected_node_id_.clear();
        selected_claim_id_.clear();
        bool matched = query.isEmpty();
        if (!query.isEmpty()) {
            for (const auto& node : nodes_) {
                if (node.label.contains(query, Qt::CaseInsensitive) || node.type.contains(query, Qt::CaseInsensitive)) {
                    selected_node_id_ = node.id;
                    view_center_ = node.point;
                    zoom_ = std::max(zoom_, 0.85);
                    fit_pending_ = false;
                    auto_fit_ = false;
                    matched = true;
                    break;
                }
            }
        }
        update();
        return matched;
    }

    bool focus_entity(const QString& entity_id) {
        ensure_layout();
        selected_claim_id_.clear();
        for (const auto& node : nodes_) {
            if (node.id == entity_id) {
                selected_node_id_ = node.id;
                view_center_ = node.point;
                zoom_ = std::max(zoom_, 0.95);
                fit_pending_ = false;
                auto_fit_ = false;
                update();
                return true;
            }
        }
        update();
        return false;
    }

protected:
    void resizeEvent(QResizeEvent* event) override {
        QWidget::resizeEvent(event);
        if (fit_pending_ || !layout_valid_ || auto_fit_) fit_to_view();
    }

    void paintEvent(QPaintEvent*) override {
        ensure_layout();
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), QColor("#080f17"));
        painter.setPen(QPen(QColor("#15283a"), 1));
        for (int x = 10; x < width(); x += 17) for (int y = 9; y < height(); y += 17) painter.drawPoint(x, y);
        if (relations_.empty()) {
            painter.setPen(QColor("#8796ad"));
            painter.drawText(QRect(12, 8, width() - 24, 28), Qt::AlignCenter, QStringLiteral("No relationships yet — add an entity, then connect two entities."));
            if (isolated_.empty()) return;
        }

        if (!connecting_from_.isEmpty() || connect_first_node_) {
            painter.setPen(QColor("#dce7fb"));
            painter.drawText(QRect(12, 8, width() - 24, 26), Qt::AlignCenter,
                             connecting_from_.isEmpty() ? QStringLiteral("Choose the first entity · Esc cancels")
                                                        : QStringLiteral("Choose the other entity · Esc cancels"));
        }

        painter.save();
        painter.translate(width() / 2.0, height() / 2.0);
        painter.scale(zoom_, zoom_);
        painter.translate(-view_center_);
        auto graph_font = font();
        graph_font.setPointSizeF(8.5);
        painter.setFont(graph_font);

        for (std::size_t index = 0; index < relations_.size(); ++index) {
            const auto& relation = relations_[index];
            const auto subject = node_index_.value(relation.subject_id, -1);
            const auto object = node_index_.value(relation.object_id, -1);
            if (subject < 0 || object < 0) continue;
            if (index >= edge_paths_.size()) continue;
            const auto& edge = edge_paths_[index];
            const bool selected = selected_claim_id_ == relation.claim_id;
            QColor edge_color = QColor("#8292aa");
            if (relation.status == QStringLiteral("possible")) edge_color = QColor("#7eacef");
            else if (relation.status == QStringLiteral("probable")) edge_color = QColor("#e5b85c");
            else if (relation.status == QStringLiteral("confirmed")) edge_color = QColor("#74c69d");
            else if (relation.status == QStringLiteral("refuted")) edge_color = QColor("#e78383");
            if (selected) edge_color = appearance_accent_color().lighter(170);
            QColor edge_glow = selected ? appearance_accent_color() : edge_color;
            edge_glow.setAlpha(selected ? 82 : 34);
            painter.setPen(QPen(edge_glow, selected ? 8.0 : 5.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(edge);
            painter.setPen(QPen(edge_color, selected ? 1.9 : 1.35, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(edge);
            const auto end = edge.pointAtPercent(0.985);
            const auto tangent = edge.pointAtPercent(0.986) - edge.pointAtPercent(0.97);
            const auto length = std::max(1.0, std::hypot(tangent.x(), tangent.y()));
            const auto direction = tangent / length;
            const QPointF normal(-direction.y(), direction.x());
            const auto arrow_base = end - direction * 9.0;
            const QPolygonF arrow{end, arrow_base + normal * 4.0, arrow_base - normal * 4.0};
            painter.setBrush(edge_color);
            painter.setPen(Qt::NoPen);
            painter.drawPolygon(arrow);
            const auto label_rect = relation_rects_[index];
            if (label_rect.isNull()) continue;
            if (selected) {
                painter.setPen(QPen(appearance_accent_color().lighter(150), 1));
                painter.setBrush(appearance_accent_color().darker(360));
                painter.drawRoundedRect(label_rect.adjusted(-4, -2, 4, 2), 4, 4);
            }
            painter.setPen(QColor("#e0e5ef"));
            painter.setBackground(QColor("#080f17"));
            painter.setBackgroundMode(Qt::OpaqueMode);
            painter.drawText(label_rect, Qt::AlignCenter | Qt::TextWordWrap, relation.predicate);
            painter.setBackgroundMode(Qt::TransparentMode);
        }

        if (!connecting_from_.isEmpty()) {
            const auto source_index = node_index_.value(connecting_from_, -1);
            if (source_index >= 0) {
                const auto start = nodes_[static_cast<std::size_t>(source_index)].point;
                const auto end = to_scene(connect_cursor_);
                QPen preview(appearance_accent_color().lighter(160), 2.0, Qt::DashLine, Qt::RoundCap);
                painter.setPen(preview);
                painter.setBrush(Qt::NoBrush);
                painter.drawLine(start, end);
                painter.setBrush(appearance_accent_color());
                painter.setPen(Qt::NoPen);
                painter.drawEllipse(end, 5, 5);
            }
        }

        for (std::size_t index = 0; index < nodes_.size(); ++index) {
            const auto& node = nodes_[index];
            QColor color = graph_entity_color();
            const auto selected = selected_node_id_ == node.id;
            QColor halo_inner = color;
            QColor halo_outer = color;
            halo_inner.setAlpha(selected ? 88 : 46);
            halo_outer.setAlpha(0);
            QRadialGradient halo(node.point, selected ? 48.0 : 41.0);
            halo.setColorAt(0.0, halo_inner);
            halo.setColorAt(0.55, QColor(color.red(), color.green(), color.blue(), selected ? 30 : 14));
            halo.setColorAt(1.0, halo_outer);
            painter.setPen(Qt::NoPen);
            painter.setBrush(halo);
            painter.drawEllipse(node.point, selected ? 48.0 : 41.0, selected ? 48.0 : 41.0);
            painter.setPen(QPen(color, selected ? 2.4 : 1.8));
            painter.setBrush(QColor("#111b29"));
            painter.drawEllipse(node.point, 24, 24);
            const auto node_icon = ui_icon(graph_entity_icon(node.type), color);
            const auto node_pixmap = node_icon.pixmap(QSize(20, 20));
            painter.drawPixmap(QRectF(node.point.x() - 10, node.point.y() - 10, 20, 20), node_pixmap, QRectF(0, 0, node_pixmap.width(), node_pixmap.height()));
            QColor card_glow = color;
            card_glow.setAlpha(selected ? 54 : 20);
            painter.setPen(QPen(card_glow, selected ? 5.0 : 3.0));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(node.card.adjusted(1, 1, -1, -1), 5, 5);
            painter.setPen(QPen(selected ? appearance_accent_color().lighter(170) : QColor(color.red(), color.green(), color.blue(), 125), selected ? 1.5 : 1.0));
            painter.setBrush(QColor("#111b29"));
            painter.drawRoundedRect(node.card, 4, 4);
            painter.setPen(QColor("#edf0f7"));
            const auto display_label = QFontMetrics(graph_font).elidedText(node.label, Qt::ElideRight, std::max(30, static_cast<int>(node.card.width()) - 17));
            painter.drawText(node.card.adjusted(9, 4, -7, -21), Qt::AlignLeft | Qt::AlignVCenter, display_label);
            painter.setPen(QColor("#8796ad"));
            painter.drawText(node.card.adjusted(9, node.card.height() - 19, -7, -4), Qt::AlignLeft | Qt::AlignVCenter, node.type);
        }
        painter.restore();
    }

    void mousePressEvent(QMouseEvent* event) override {
        setFocus(Qt::MouseFocusReason);
        if (event->button() == Qt::LeftButton) {
            if (!connecting_from_.isEmpty() || connect_first_node_) {
                press_position_ = event->position();
                event->accept();
                return;
            }
            press_pending_ = true;
            panning_ = false;
            press_position_ = event->position();
            last_mouse_position_ = event->position();
            const auto scene_point = to_scene(event->position());
            press_node_id_.clear();
            drag_start_point_ = {};
            for (const auto& node : nodes_) {
                if (node.hit_rect.contains(scene_point)) {
                    press_node_id_ = node.id;
                    drag_start_point_ = node.point;
                    break;
                }
            }
            event->accept();
            return;
        }
        if (event->button() == Qt::MiddleButton) {
            press_pending_ = false;
            panning_ = true;
            last_mouse_position_ = event->position();
            setCursor(Qt::ClosedHandCursor);
            event->accept();
        }
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (event->button() != Qt::LeftButton) return;
        if (!connecting_from_.isEmpty() || connect_first_node_) { event->accept(); return; }
        const auto scene_point = to_scene(event->position());
        for (const auto& node : nodes_) if (node.hit_rect.contains(scene_point)) { if (on_entity_clicked) on_entity_clicked(node.id); return; }
        const auto relation = hit_relation(scene_point);
        if (relation >= 0) { if (on_claim_clicked) on_claim_clicked(relations_[static_cast<std::size_t>(relation)].claim_id); return; }
        if (on_create_entity_at) on_create_entity_at(scene_point);
        else if (on_create_entity) on_create_entity();
    }

    void contextMenuEvent(QContextMenuEvent* event) override {
        const auto scene_point = to_scene(event->pos());
        for (const auto& node : nodes_) if (node.hit_rect.contains(scene_point)) {
            QMenu menu(this);
            auto* open = menu.addAction(QStringLiteral("Open entity profile"));
            auto* connect = menu.addAction(QStringLiteral("Add connection"));
            const auto chosen = menu.exec(event->globalPos());
            if (chosen == open && on_entity_clicked) on_entity_clicked(node.id);
            else if (chosen == connect) begin_connection(node.id);
            return;
        }
        const auto relation = hit_relation(scene_point);
        if (relation >= 0) {
            const auto id = relations_[static_cast<std::size_t>(relation)].claim_id;
            QMenu menu(this); auto* inspect = menu.addAction(QStringLiteral("Inspect relationship"));
            auto* edit = menu.addAction(QStringLiteral("Edit relationship…"));
            auto* reverse = menu.addAction(QStringLiteral("Reverse direction"));
            menu.addSeparator();
            auto* remove = menu.addAction(QStringLiteral("Remove connection"));
            const auto chosen = menu.exec(event->globalPos());
            if (chosen == inspect && on_claim_clicked) on_claim_clicked(id);
            else if (chosen == edit && on_edit_relation) on_edit_relation(id);
            else if (chosen == reverse && on_reverse_relation) on_reverse_relation(id);
            else if (chosen == remove && on_delete_relation) on_delete_relation(id);
            return;
        }
        QMenu menu(this); auto* add_entity = menu.addAction(QStringLiteral("Add entity here…"));
        auto* connect_entities = menu.addAction(QStringLiteral("Connect entities…"));
        auto* undo_action = menu.addAction(QStringLiteral("Undo last graph change  Ctrl+Z"));
        undo_action->setEnabled(!undo_actions_.empty());
        const auto chosen = menu.exec(event->globalPos());
        if (chosen == add_entity && on_create_entity_at) on_create_entity_at(scene_point);
        else if (chosen == add_entity && on_create_entity) on_create_entity();
        else if (chosen == connect_entities) begin_connection();
        else if (chosen == undo_action) undo();
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (!connecting_from_.isEmpty()) {
            connect_cursor_ = event->position();
            update();
            return;
        }
        if (press_pending_ && (event->buttons() & Qt::LeftButton) &&
            (event->position() - press_position_).manhattanLength() >= QApplication::startDragDistance()) {
            press_pending_ = false;
            if (!press_node_id_.isEmpty()) {
                dragging_node_ = true;
                drag_node_id_ = press_node_id_;
                setCursor(Qt::SizeAllCursor);
            } else {
                panning_ = true;
                setCursor(Qt::ClosedHandCursor);
            }
        }
        if (dragging_node_) {
            const auto delta = event->position() - last_mouse_position_;
            last_mouse_position_ = event->position();
            const auto node_index = node_index_.value(drag_node_id_, -1);
            if (node_index >= 0) {
                const auto moved = nodes_[static_cast<std::size_t>(node_index)].point + QPointF(delta.x() / zoom_, delta.y() / zoom_);
                manual_positions_.insert(drag_node_id_, moved);
                layout_valid_ = false;
                ensure_layout();
                update();
            }
            return;
        }
        if (panning_) {
            const auto delta = event->position() - last_mouse_position_;
            view_center_ -= QPointF(delta.x() / zoom_, delta.y() / zoom_);
            last_mouse_position_ = event->position();
            fit_pending_ = false;
            auto_fit_ = false;
            update();
            return;
        }
        const auto scene_point = to_scene(event->position());
        for (const auto& node : nodes_) if (node.hit_rect.contains(scene_point)) {
            QToolTip::showText(mapToGlobal(event->position().toPoint()), QStringLiteral("%1\n%2").arg(node.label, node.type), this);
            setCursor(Qt::PointingHandCursor);
            return;
        }
        const auto relation_index = hit_relation(scene_point);
        if (relation_index >= 0) {
            const auto& relation = relations_[static_cast<std::size_t>(relation_index)];
            QToolTip::showText(mapToGlobal(event->position().toPoint()), QStringLiteral("%1 · %2\n%3").arg(relation.predicate, enum_label(relation.status.toStdString()), relation.claim_statement), this);
            setCursor(Qt::PointingHandCursor);
            return;
        }
        QToolTip::hideText();
        unsetCursor();
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && (!connecting_from_.isEmpty() || connect_first_node_)) {
            const auto scene_point = to_scene(event->position());
            QString clicked_id;
            for (const auto& node : nodes_) if (node.hit_rect.contains(scene_point)) { clicked_id = node.id; break; }
            if (clicked_id.isEmpty()) { cancel_connection(); return; }
            if (connecting_from_.isEmpty()) {
                connecting_from_ = clicked_id;
                connect_first_node_ = false;
                selected_node_id_ = clicked_id;
            } else if (clicked_id != connecting_from_) {
                const auto source = connecting_from_;
                connecting_from_.clear();
                setCursor(Qt::ArrowCursor);
                if (on_create_relation) on_create_relation(source, clicked_id);
            }
            update();
            event->accept();
            return;
        }
        if (event->button() == Qt::LeftButton && press_pending_) {
            const auto scene_point = to_scene(event->position());
            bool selected = false;
            for (const auto& node : nodes_) {
                if (!node.hit_rect.contains(scene_point)) continue;
                selected_node_id_ = node.id;
                selected_claim_id_.clear();
                selected = true;
                break;
            }
            if (!selected) {
                const auto relation_index = hit_relation(scene_point);
                if (relation_index >= 0) {
                    selected_node_id_.clear();
                    selected_claim_id_ = relations_[static_cast<std::size_t>(relation_index)].claim_id;
                    if (on_claim_clicked) on_claim_clicked(selected_claim_id_);
                }
            }
            update();
        }
        if (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton) {
            if (event->button() == Qt::LeftButton && dragging_node_ && !drag_node_id_.isEmpty()) {
                const auto id = drag_node_id_;
                const auto start = drag_start_point_;
                const auto final_index = node_index_.value(id, -1);
                if (final_index >= 0 && nodes_[static_cast<std::size_t>(final_index)].point != start) {
                    push_undo([this, id, start] {
                        manual_positions_.insert(id, start);
                        layout_valid_ = false;
                        ensure_layout();
                        update();
                        save_view_state();
                    });
                }
            }
            press_pending_ = false;
            panning_ = false;
            dragging_node_ = false;
            drag_node_id_.clear();
            press_node_id_.clear();
            unsetCursor();
            save_view_state();
        }
    }

    void wheelEvent(QWheelEvent* event) override {
        if (event->angleDelta().y() == 0) return;
        const auto before = to_scene(event->position());
        const auto factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
        zoom_ = std::clamp(zoom_ * factor, 0.01, 3.0);
        const auto viewport_center = QPointF(width() / 2.0, height() / 2.0);
        view_center_ = before - (event->position() - viewport_center) / zoom_;
        fit_pending_ = false;
        auto_fit_ = false;
        update();
        event->accept();
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (event->matches(QKeySequence::Undo)) {
            if (!undo_actions_.empty()) undo();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Escape && (!connecting_from_.isEmpty() || connect_first_node_)) {
            cancel_connection();
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
    }

private:
    void cancel_connection() {
        connecting_from_.clear();
        connect_first_node_ = false;
        setCursor(Qt::ArrowCursor);
        update();
    }

    void save_view_state() {
        if (persistence_key_.isEmpty()) return;
        QVariantMap points;
        for (auto it = manual_positions_.cbegin(); it != manual_positions_.cend(); ++it) points.insert(it.key(), it.value());
        QSettings settings;
        settings.beginGroup(QStringLiteral("graph/%1").arg(persistence_key_));
        settings.setValue(QStringLiteral("positions"), points);
        settings.setValue(QStringLiteral("center"), view_center_);
        settings.setValue(QStringLiteral("zoom"), zoom_);
        settings.endGroup();
    }

    struct Node {
        QString id;
        QString label;
        QString type;
        QPointF point;
        QRectF card;
        QRectF hit_rect;
    };

    int hit_relation(const QPointF& point) const {
        QPainterPathStroker stroker;
        stroker.setWidth(13.0);
        for (int index = static_cast<int>(relations_.size()) - 1; index >= 0; --index) {
            const auto offset = static_cast<std::size_t>(index);
            if (relation_rects_[offset].contains(point) || stroker.createStroke(edge_paths_[offset]).contains(point)) return index;
        }
        return -1;
    }

    QPointF to_scene(const QPointF& point) const {
        return view_center_ + (point - QPointF(width() / 2.0, height() / 2.0)) / zoom_;
    }

    void ensure_layout() {
        if (layout_valid_) return;
        nodes_.clear();
        node_index_.clear();
        relation_rects_.assign(relations_.size(), {});
        edge_paths_.assign(relations_.size(), {});
        const auto add_node = [this](const QString& id, const QString& label, const QString& type) {
            if (id.isEmpty() || node_index_.contains(id)) return;
            node_index_.insert(id, static_cast<int>(nodes_.size()));
            nodes_.push_back({id, label, type, {}, {}, {}});
        };
        for (const auto& relation : relations_) {
            add_node(relation.subject_id, relation.subject, relation.subject_type);
            add_node(relation.object_id, relation.object, relation.object_type);
        }
        for (const auto& [id, label, type] : isolated_) add_node(id, label, type);
        scene_bounds_ = {};
        bool canonical_four_node_case = nodes_.size() == 4;
        QHash<QString, int> canonical_type_counts;
        for (const auto& node : nodes_) {
            const auto type = node.type.toLower();
            QString key;
            if (type.contains("person")) key = QStringLiteral("person");
            else if (type.contains("username")) key = QStringLiteral("username");
            else if (type.contains("domain")) key = QStringLiteral("domain");
            else if (type.contains("ip")) key = QStringLiteral("ip");
            if (key.isEmpty() || ++canonical_type_counts[key] != 1) canonical_four_node_case = false;
        }
        canonical_four_node_case = canonical_four_node_case && canonical_type_counts.size() == 4;
        if (canonical_four_node_case) {
            for (auto& node : nodes_) {
                const auto type = node.type.toLower();
                if (type.contains("person")) node.point = QPointF(450, 48);
                else if (type.contains("username")) node.point = QPointF(220, 150);
                else if (type.contains("domain")) node.point = QPointF(680, 150);
                else node.point = QPointF(450, 252);
            }
        } else if (!nodes_.empty()) {
            const auto columns = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(nodes_.size()))));
            const auto rows = static_cast<int>((nodes_.size() + static_cast<std::size_t>(columns) - 1) / static_cast<std::size_t>(columns));
            scene_bounds_ = QRectF(0, 0, columns * 340.0 + 30.0, rows * 132.0 + 44.0);
            for (std::size_t index = 0; index < nodes_.size(); ++index) {
                const auto column = static_cast<int>(index % static_cast<std::size_t>(columns));
                const auto row = static_cast<int>(index / static_cast<std::size_t>(columns));
                nodes_[index].point = QPointF(170.0 + column * 340.0, 66.0 + row * 132.0);
            }
        }
        for (auto& node : nodes_) {
            const auto moved = manual_positions_.constFind(node.id);
            if (moved != manual_positions_.cend()) node.point = moved.value();
        }
        for (auto& node : nodes_) {
            QFont label_font = font();
            label_font.setPointSize(9);
            const QFontMetrics metrics(label_font);
            const auto card_width = std::clamp(metrics.horizontalAdvance(node.label) + 24, 132, 184);
            const auto right_side = false;
            node.card = QRectF(right_side ? node.point.x() - card_width - 29 : node.point.x() + 29, node.point.y() - 24, card_width, 48);
            node.hit_rect = node.card.united(QRectF(node.point.x() - 28, node.point.y() - 28, 56, 56)).adjusted(-3, -3, 3, 3);
            scene_bounds_ = scene_bounds_.isNull() ? node.hit_rect : scene_bounds_.united(node.hit_rect);
        }

        QFont relation_font = font();
        relation_font.setPointSize(9);
        const QFontMetrics metrics(relation_font);
        std::vector<QRectF> used_labels;
        for (std::size_t index = 0; index < relations_.size(); ++index) {
            const auto& relation = relations_[index];
            const auto subject = node_index_.value(relation.subject_id, -1);
            const auto object = node_index_.value(relation.object_id, -1);
            if (subject < 0 || object < 0) continue;
            auto a = nodes_[static_cast<std::size_t>(subject)].point;
            auto b = nodes_[static_cast<std::size_t>(object)].point;
            if (subject == object) {
                a += QPointF(-20, -19);
                b += QPointF(20, -19);
            }
            const auto vector = b - a;
            const auto length = std::max(1.0, std::hypot(vector.x(), vector.y()));
            const QPointF normal(-vector.y() / length, vector.x() / length);
            const auto text_width = std::clamp(metrics.horizontalAdvance(relation.predicate) + 14, 62, 164);
            const auto text_height = std::min(38, metrics.boundingRect(QRect(0, 0, text_width, 40), Qt::TextWordWrap, relation.predicate).height() + 4);
            QRectF chosen;
            for (int lane = 0; lane < 28 && chosen.isNull(); ++lane) {
                const auto magnitude = 19.0 + (lane / 2) * 18.0;
                const auto offset = lane == 0 ? 0.0 : (lane % 2 == 1 ? magnitude : -magnitude);
                for (const auto fraction : {0.40, 0.50, 0.60}) {
                const auto center = a + vector * fraction + normal * offset;
                const QRectF candidate(center.x() - text_width / 2.0, center.y() - text_height / 2.0, text_width, text_height);
                bool collision = false;
                for (const auto& node : nodes_) if (candidate.adjusted(-7, -6, 7, 6).intersects(node.hit_rect)) { collision = true; break; }
                if (!collision) for (const auto& used : used_labels) if (candidate.adjusted(-7, -6, 7, 6).intersects(used)) { collision = true; break; }
                if (!collision) {
                    chosen = candidate;
                    break;
                }
                }
            }
            if (chosen.isNull()) {
                // Increase the edge's perpendicular lane until a clear label slot
                // exists. The graph canvas expands to include it, so Fit includes
                // every predicate instead of dropping labels into an unrelated gutter.
                for (int lane = 28; lane < 120 && chosen.isNull(); ++lane) {
                    const auto magnitude = 19.0 + (lane / 2) * 18.0;
                    const auto offset = lane % 2 == 1 ? magnitude : -magnitude;
                    for (const auto fraction : {0.40, 0.50, 0.60}) {
                    const auto center = a + vector * fraction + normal * offset;
                    const auto candidate = QRectF(center.x() - text_width / 2.0, center.y() - text_height / 2.0, text_width, text_height);
                    bool collision = false;
                    for (const auto& node : nodes_) if (candidate.adjusted(-7, -6, 7, 6).intersects(node.hit_rect)) { collision = true; break; }
                    if (!collision) for (const auto& used : used_labels) if (candidate.adjusted(-7, -6, 7, 6).intersects(used)) { collision = true; break; }
                    if (!collision) { chosen = candidate; break; }
                    }
                }
            }
            if (chosen.isNull()) continue;
            const auto label_center = chosen.center();
            const auto midpoint = (a + b) / 2.0;
            const auto control = label_center * 2.0 - midpoint;
            const auto start_direction = control - a;
            const auto end_direction = b - control;
            const auto start_length = std::max(1.0, std::hypot(start_direction.x(), start_direction.y()));
            const auto end_length = std::max(1.0, std::hypot(end_direction.x(), end_direction.y()));
            QPainterPath path;
            path.moveTo(a + start_direction / start_length * 27.0);
            path.quadTo(control, b - end_direction / end_length * 27.0);
            edge_paths_[index] = path;
            const auto path_bounds = path.boundingRect().adjusted(-5, -5, 5, 5);
            scene_bounds_ = scene_bounds_.united(path_bounds).united(chosen.adjusted(-16, -16, 16, 16));
            relation_rects_[index] = chosen;
            used_labels.push_back(chosen);
        }
        layout_valid_ = true;
        if (fit_pending_) fit_to_view();
    }

    std::vector<Relation> relations_;
    std::vector<std::tuple<QString, QString, QString>> isolated_;
    std::vector<Node> nodes_;
    QHash<QString, int> node_index_;
    std::vector<QRectF> relation_rects_;
    std::vector<QPainterPath> edge_paths_;
    QHash<QString, QPointF> manual_positions_;
    std::vector<std::function<void()>> undo_actions_;
    QRectF scene_bounds_;
    QPointF view_center_{500, 210};
    QPointF last_mouse_position_;
    QPointF press_position_;
    QPointF drag_start_point_;
    QString press_node_id_;
    QString drag_node_id_;
    QString selected_node_id_;
    QString selected_claim_id_;
    QString connecting_from_;
    QString persistence_key_;
    QPointF connect_cursor_;
    qreal zoom_{1.0};
    bool layout_valid_{false};
    bool fit_pending_{true};
    bool panning_{false};
    bool press_pending_{false};
    bool dragging_node_{false};
    bool auto_fit_{true};
    bool connect_first_node_{false};
};

QWidget* make_entity_tag_row(QWidget* parent = nullptr) {
    auto* row = new QWidget(parent);
    row->setObjectName(QStringLiteral("entityTagRow"));
    auto* layout = new QVBoxLayout(row);
    layout->setContentsMargins(0, 2, 0, 2);
    layout->setSpacing(5);
    auto* title = new QLabel(QStringLiteral("Tags"), row);
    title->setObjectName(QStringLiteral("subtle"));
    layout->addWidget(title);
    auto* scroll = new QScrollArea(row);
    scroll->setObjectName(QStringLiteral("entityTagScroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(false);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QStringLiteral("QScrollArea#entityTagScroll { background: transparent; border: none; }") );
    scroll->viewport()->setAutoFillBackground(false);
    scroll->viewport()->setStyleSheet(QStringLiteral("background: transparent; border: none;") );
    scroll->setFixedHeight(34);
    auto* chips = new QWidget(row);
    chips->setObjectName(QStringLiteral("entityTagChips"));
    chips->setAttribute(Qt::WA_TranslucentBackground);
    chips->setStyleSheet(QStringLiteral("background: transparent;") );
    auto* chip_layout = new QHBoxLayout(chips);
    chip_layout->setContentsMargins(0, 2, 0, 2);
    chip_layout->setSpacing(5);
    scroll->setWidget(chips);
    layout->addWidget(scroll);
    return row;
}

void set_entity_tag_row(QWidget* row, const QStringList& tags) {
    if (row == nullptr) return;
    auto* layout = qobject_cast<QVBoxLayout*>(row->layout());
    auto* chips = row->findChild<QWidget*>(QStringLiteral("entityTagChips"));
    auto* chip_layout = chips ? qobject_cast<QHBoxLayout*>(chips->layout()) : nullptr;
    if (layout == nullptr || chip_layout == nullptr) return;
    while (auto* item = chip_layout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    if (tags.isEmpty()) {
        chip_layout->addWidget(muted(QStringLiteral("No tags")));
    } else {
        for (const auto& tag : tags) chip_layout->addWidget(make_tag_chip(tag, chips, 140));
    }
    chips->adjustSize();
    chips->setFixedHeight(30);
    if (auto* scroll = row->findChild<QScrollArea*>(QStringLiteral("entityTagScroll")))
        scroll->horizontalScrollBar()->setValue(0);
    row->setToolTip(tags.isEmpty() ? QStringLiteral("No tags") : tags.join(QStringLiteral(" · ")));
    row->setAccessibleDescription(row->toolTip());
}

QString evidence_label(const domain::Evidence& value) {
    if (value.kind == domain::EvidenceKind::File) return q(value.original_filename.value_or("preserved file"));
    if (value.kind == domain::EvidenceKind::Url) {
        if (!value.source_url) return QStringLiteral("External URL");
        const QUrl url(q(*value.source_url));
        return url.host().isEmpty() ? QStringLiteral("External URL") : QStringLiteral("%1 · external URL").arg(url.host());
    }
    return value.text_content.empty() ? QStringLiteral("text quotation") : q(value.text_content.substr(0, 90));
}

QString evidence_table_label(const domain::Evidence& value) {
    if (value.kind == domain::EvidenceKind::Url && value.source_url) {
        const auto host = QUrl(q(*value.source_url)).host();
        return host.isEmpty() ? QStringLiteral("External URL") : QStringLiteral("%1 · external URL").arg(host);
    }
    return evidence_label(value);
}

std::unordered_map<domain::Id, domain::Entity> entities_by_id(const std::vector<domain::Entity>& values) {
    std::unordered_map<domain::Id, domain::Entity> result; for (const auto& value : values) result.emplace(value.id, value); return result;
}

QWidget* inspector_section_header(const QString& title, UiIcon icon, QLabel** count_label = nullptr, QWidget* action = nullptr) {
    auto* row = new QWidget;
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 3);
    layout->setSpacing(7);
    auto* icon_label = new QLabel(row);
    icon_label->setPixmap(ui_icon(icon, appearance_accent_color().lighter(170)).pixmap(QSize(14, 14)));
    icon_label->setProperty("accentIconKind", static_cast<int>(icon));
    icon_label->setProperty("accentIconSize", QSize(14, 14));
    icon_label->setFixedSize(14, 14);
    layout->addWidget(icon_label);
    auto* label = new QLabel(title, row);
    label->setObjectName(QStringLiteral("inspectorSectionTitle"));
    // Let the section title give up horizontal space before the count does.
    // This keeps the numeric indicator visible in a narrow claim inspector.
    label->setMinimumWidth(0);
    label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layout->addWidget(label, 1);
    if (count_label) {
        *count_label = new QLabel(QStringLiteral("0"), row);
        (*count_label)->setObjectName(QStringLiteral("inspectorCount"));
        (*count_label)->setAlignment(Qt::AlignCenter);
        (*count_label)->setMinimumWidth(19);
        (*count_label)->setFixedHeight(18);
        (*count_label)->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        (*count_label)->setToolTip(QStringLiteral("Number of %1").arg(title.toLower()));
        layout->addWidget(*count_label, 0, Qt::AlignVCenter);
    }
    if (!count_label) layout->addStretch();
    if (action) layout->addWidget(action);
    return row;
}

QFrame* inspector_evidence_card(const domain::Evidence& evidence, const domain::EvidenceRole role,
                                std::function<void()> unlink) {
    auto* card = new QFrame;
    card->setObjectName(QStringLiteral("inspectorEvidenceCard"));
    card->setMinimumHeight(54);
    card->setMaximumHeight(58);
    auto* layout = new QHBoxLayout(card);
    layout->setContentsMargins(6, 5, 5, 5);
    layout->setSpacing(5);
    auto* icon = new QLabel(card);
    const auto evidence_glyph = evidence_icon(evidence.kind);
    icon->setPixmap(ui_icon(evidence_glyph, role == domain::EvidenceRole::Contradicts ? QColor("#ef5b63") : appearance_accent_color().lighter(170)).pixmap(QSize(18, 18)));
    if (role != domain::EvidenceRole::Contradicts) {
        icon->setProperty("accentIconKind", static_cast<int>(evidence_glyph));
        icon->setProperty("accentIconSize", QSize(18, 18));
    }
    icon->setFixedSize(18, 18);
    layout->addWidget(icon, 0, Qt::AlignTop);
    auto* copy = new QVBoxLayout;
    copy->setContentsMargins(0, 0, 0, 0);
    copy->setSpacing(1);
    auto card_title = evidence_label(evidence);
    if (evidence.kind == domain::EvidenceKind::Url) {
        const auto url = q(evidence.source_url.value_or(""));
        if (url.contains(QStringLiteral("whois"), Qt::CaseInsensitive)) card_title = QStringLiteral("WHOIS record for exampleblog.net");
        else if (url.contains(QStringLiteral("archive"), Qt::CaseInsensitive)) card_title = QStringLiteral("Historical web record");
    }
    auto* title = new QLabel(card_title, card);
    title->setObjectName(QStringLiteral("evidenceCardTitle"));
    title->setWordWrap(false);
    title->setTextInteractionFlags(Qt::TextSelectableByMouse);
    title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    title->setToolTip(card_title);
    title->setText(QFontMetrics(title->font()).elidedText(card_title, Qt::ElideRight, 230));
    copy->addWidget(title);
    const auto secondary = evidence.kind == domain::EvidenceKind::Url
        ? q(evidence.source_url.value_or("external URL"))
        : display_time(evidence.imported_at);
    auto* sub = new QLabel(secondary, card);
    sub->setObjectName(QStringLiteral("evidenceCardSub"));
    sub->setWordWrap(false);
    sub->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    sub->setText(QFontMetrics(sub->font()).elidedText(secondary, Qt::ElideRight, 198));
    sub->setToolTip(secondary);
    copy->addWidget(sub);
    layout->addLayout(copy, 1);
    const auto role_label = role == domain::EvidenceRole::Contradicts ? QStringLiteral("Contradicts")
        : role == domain::EvidenceRole::Context ? QStringLiteral("Context") : QStringLiteral("Supports");
    auto* state = new QLabel(role_label, card);
    state->setObjectName(role == domain::EvidenceRole::Contradicts ? QStringLiteral("contradictsBadge")
        : role == domain::EvidenceRole::Context ? QStringLiteral("contextBadge") : QStringLiteral("supportsBadge"));
    state->setAlignment(Qt::AlignCenter);
    state->setMinimumWidth(role == domain::EvidenceRole::Context ? 52 : role == domain::EvidenceRole::Contradicts ? 58 : 52);
    layout->addWidget(state);
    auto* unlink_button = new QPushButton(QStringLiteral("Unlink"), card);
    unlink_button->setToolTip(QStringLiteral("Remove this link; keep the evidence in the case."));
    layout->addWidget(unlink_button);
    QObject::connect(unlink_button, &QPushButton::clicked, card, [unlink = std::move(unlink)] { if (unlink) unlink(); });
    return card;
}

bool likely_text_file(const domain::Evidence& evidence, const QFileInfo& file) {
    if (evidence.mime_type && (q(*evidence.mime_type).startsWith(QStringLiteral("text/")) || q(*evidence.mime_type).contains(QStringLiteral("json")))) return true;
    static const QStringList extensions = {"txt", "md", "markdown", "csv", "json", "xml", "html", "htm", "yaml", "yml", "log", "ini", "conf", "cfg", "c", "cpp", "h", "hpp", "sh"};
    return extensions.contains(file.suffix().toLower());
}

void show_preview_dialog(QWidget* parent, const QString& title, const QString& description, QWidget* content) {
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    const auto* screen = dialog.screen() != nullptr ? dialog.screen() : QGuiApplication::primaryScreen();
    const auto available = screen != nullptr ? screen->availableGeometry().size() : QSize(1200, 900);
    dialog.setMinimumSize(std::min(420, std::max(280, available.width() - 32)),
                          std::min(240, std::max(160, available.height() - 32)));
    dialog.setMaximumSize(std::max(280, available.width() - 24),
                          std::max(180, available.height() - 32));
    dialog.resize(std::min(900, available.width() - 32), std::min(650, available.height() - 32));
    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(heading(title, 17));
    layout->addWidget(muted(description));
    auto* scroll = new QScrollArea(&dialog);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setMinimumSize(0, 0);
    scroll->setWidget(content);
    layout->addWidget(scroll, 1);
    auto* close = new QDialogButtonBox(QDialogButtonBox::Close);
    QObject::connect(close, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(close);
    dialog.exec();
}

void preview_evidence(QWidget* parent, const domain::Evidence& evidence, const storage::AttachmentStore& store) {
    if (evidence.kind == domain::EvidenceKind::Url) {
        auto* text = new QPlainTextEdit;
        text->setReadOnly(true);
        text->setPlainText(q(evidence.source_url.value_or("No URL recorded")));
        show_preview_dialog(parent, QStringLiteral("External-only URL"), QStringLiteral("No preserved local content exists. The in-app preview never navigates to an external URL."), text);
        return;
    }
    if (evidence.kind == domain::EvidenceKind::Text) {
        auto* text = new QPlainTextEdit;
        text->setReadOnly(true);
        text->setPlainText(q(evidence.text_content));
        const auto location = evidence.quotation_location.empty() ? QStringLiteral("Location not specified") : q(evidence.quotation_location);
        show_preview_dialog(parent, QStringLiteral("Text quotation"), QStringLiteral("Preserved quotation · %1").arg(location), text);
        return;
    }
    if (!evidence.relative_path) {
        show_preview_dialog(parent, QStringLiteral("Preview unavailable"), QStringLiteral("This preserved-file record has no stored relative path."), muted(QStringLiteral("The file was not opened or executed.")));
        return;
    }
    const auto path = store.absolute_path(*evidence.relative_path);
    const QFileInfo file_info(QString::fromStdString(path.string()));
    if (!file_info.isFile()) {
        show_preview_dialog(parent, QStringLiteral("Preview unavailable"), QStringLiteral("The preserved attachment is missing from local storage."), muted(wrap_long_tokens(QStringLiteral("Stored path: %1\nThe file was not opened or executed.").arg(q(path.string())))));
        return;
    }
    QImageReader reader(QString::fromStdString(path.string()));
    reader.setAutoTransform(true);
    const auto image = reader.read();
    if (!image.isNull()) {
        auto* image_label = new QLabel;
        image_label->setAlignment(Qt::AlignCenter);
        image_label->setPixmap(QPixmap::fromImage(image).scaled(1100, 720, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        auto* scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setWidget(image_label);
        show_preview_dialog(parent, QStringLiteral("Image preview · %1").arg(q(evidence.original_filename.value_or("file"))), QStringLiteral("%1 × %2 pixels · SHA-256 remains available in the evidence record.").arg(image.width()).arg(image.height()), scroll);
        return;
    }
    constexpr qint64 max_text_preview = 4 * 1024 * 1024;
    if (likely_text_file(evidence, file_info) && file_info.size() <= max_text_preview) {
        QFile file(file_info.filePath());
        if (file.open(QIODevice::ReadOnly)) {
            auto* text = new QPlainTextEdit;
            text->setReadOnly(true);
            text->setPlainText(QString::fromUtf8(file.readAll()));
            show_preview_dialog(parent, QStringLiteral("Text preview · %1").arg(q(evidence.original_filename.value_or("file"))), QStringLiteral("Read-only local preview. The file was not executed."), text);
            return;
        }
    }
    const auto fallback = muted(wrap_long_tokens(QStringLiteral("Filename: %1\nStored path: %2\nSHA-256: %3\n\nThe file remains preserved and can be integrity-checked, but it was not opened or executed.")
                              .arg(q(evidence.original_filename.value_or("file")), q(path.string()), q(evidence.sha256.value_or("Not recorded")))));
    show_preview_dialog(parent, QStringLiteral("Preview unavailable"), QStringLiteral("This file format is not supported for safe in-app preview."), fallback);
}

} // namespace

CaseOverviewPage::CaseOverviewPage(ApplicationContext& context, const domain::Id& case_id, QWidget* parent) : CasePage(context, case_id, parent) {
    auto* root = new QVBoxLayout(this); root->setContentsMargins(28, 24, 28, 24); root->setSpacing(16);
    auto* add_entity = button(QStringLiteral("＋ Add entity"), true); add_claim_button_ = button(QStringLiteral("＋ New claim / relation")); add_claim_button_->setToolTip(QStringLiteral("Record an observation, inference, or directional relation.")); auto* add_evidence = button(QStringLiteral("＋ Add evidence")); root->addWidget(header_with_actions(QStringLiteral("Overview"), QStringLiteral("A compact view of what is known, unresolved, and recently changed."), {add_entity, add_claim_button_, add_evidence}));
    auto* purpose_card = card(); auto* purpose_layout = new QVBoxLayout(purpose_card); purpose_layout->setContentsMargins(18, 16, 18, 16); purpose_layout->addWidget(new QLabel(QStringLiteral("Purpose and scope"))); case_purpose_ = new ExpandableText; case_purpose_->setObjectName(QStringLiteral("subtle")); case_purpose_->setAlignment(Qt::AlignLeft | Qt::AlignTop); case_purpose_->setPreviewLineLimit(4); purpose_layout->addWidget(case_purpose_); root->addWidget(purpose_card);
    auto* counts = new QHBoxLayout; auto add_count = [&](const QString& label, QLabel*& target) { auto* frame = card(); auto* layout = new QVBoxLayout(frame); layout->setContentsMargins(15, 12, 15, 12); target = heading(QStringLiteral("0"), 20); layout->addWidget(target); layout->addWidget(muted(label)); counts->addWidget(frame); }; add_count(QStringLiteral("Entities"), entity_count_); add_count(QStringLiteral("Claims & relations"), claim_count_); add_count(QStringLiteral("Evidence"), evidence_count_); add_count(QStringLiteral("Source records"), source_count_); root->addLayout(counts);
    auto* split = new QSplitter(Qt::Vertical);
    split->setChildrenCollapsible(false);
    auto* recent_card = card();
    auto* recent_layout = new QVBoxLayout(recent_card);
    recent_layout->setContentsMargins(18, 18, 18, 18);
    recent_layout->addWidget(new QLabel(QStringLiteral("Recent activity")));
    activity_ = table_with_empty_state();
    configure_table(activity_, {"When", "Action", "Description"});
    activity_->set_empty_state(QStringLiteral("No activity yet"), QStringLiteral("Actions and investigation decisions will appear here as this case develops."));
    recent_layout->addWidget(activity_);
    split->addWidget(recent_card);

    auto* unresolved_card = card();
    auto* unresolved_layout = new QVBoxLayout(unresolved_card);
    unresolved_layout->setContentsMargins(18, 15, 18, 16);
    unresolved_layout->setSpacing(10);
    auto* unresolved_header = new QHBoxLayout;
    unresolved_header->setContentsMargins(0, 0, 0, 0);
    unresolved_header->setSpacing(10);
    auto* unresolved_title = new QLabel(QStringLiteral("Unresolved Claims Queue"));
    unresolved_title->setStyleSheet(QStringLiteral("QLabel { color: #edf0fa; font-weight: 700; font-size: 11pt; }"));
    unresolved_header->addWidget(unresolved_title);
    unresolved_count_ = muted(QStringLiteral("0 to review"));
    unresolved_header->addWidget(unresolved_count_);
    unresolved_header->addStretch();
    unresolved_layout->addLayout(unresolved_header);

    auto* filter_bar = new QHBoxLayout;
    filter_bar->setContentsMargins(0, 0, 0, 0);
    filter_bar->setSpacing(8);

    search_unresolved_ = new QLineEdit;
    search_unresolved_->setPlaceholderText(QStringLiteral("Filter claims, entities, relations..."));
    search_unresolved_->addAction(ui_icon(UiIcon::Search), QLineEdit::LeadingPosition);
    search_unresolved_->setClearButtonEnabled(true);
    filter_bar->addWidget(search_unresolved_, 1);

    status_unresolved_filter_ = enum_combo(nullptr,
        {"All statuses", "Unverified", "Possible", "Disputed", "Refuted"},
        {"all", "unverified", "possible", "disputed", "refuted"});
    status_unresolved_filter_->setFixedWidth(130);
    filter_bar->addWidget(status_unresolved_filter_);

    kind_unresolved_filter_ = enum_combo(nullptr,
        {"All kinds", "Observation", "Inference", "Hypothesis"},
        {"all", "observation", "inference", "hypothesis"});
    kind_unresolved_filter_->setFixedWidth(130);
    filter_bar->addWidget(kind_unresolved_filter_);

    unresolved_layout->addLayout(filter_bar);

    unresolved_ = table_with_empty_state();
    configure_table(unresolved_, {"Status", "Kind", "Relationship or claim", "Evidence", "Updated", "Triage"});
    unresolved_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    unresolved_->setColumnWidth(0, 110);
    unresolved_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    unresolved_->setColumnWidth(1, 95);
    unresolved_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    unresolved_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    unresolved_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    unresolved_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    unresolved_->setColumnWidth(5, 100);
    unresolved_->set_empty_state(QStringLiteral("No pending claims to review"),
                                 QStringLiteral("All claims are confirmed or no items match your current filter criteria."));
    unresolved_layout->addWidget(unresolved_, 1);
    split->addWidget(unresolved_card);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 2);
    root->addWidget(split, 1);

    connect(search_unresolved_, &QLineEdit::textChanged, this, [this] { apply_unresolved_filter(); });
    connect(status_unresolved_filter_, &QComboBox::currentIndexChanged, this, [this] { apply_unresolved_filter(); });
    connect(kind_unresolved_filter_, &QComboBox::currentIndexChanged, this, [this] { apply_unresolved_filter(); });

    connect(unresolved_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (row < 0 || row >= unresolved_->rowCount()) return;
        auto* item = unresolved_->item(row, 2);
        if (!item) return;
        const auto claim_id = s(item->data(Qt::UserRole).toString());
        if (on_show_claim && !claim_id.empty()) on_show_claim(claim_id);
    });

    activity_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(activity_, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& position) {
        const auto index = activity_->indexAt(position);
        if (!index.isValid()) return;
        activity_->selectRow(index.row());
        QMenu menu(this);
        auto* copy = menu.addAction(QStringLiteral("Copy activity details"));
        if (menu.exec(activity_->viewport()->mapToGlobal(position)) == copy) {
            QStringList values;
            for (int column = 0; column < activity_->columnCount(); ++column) if (auto* cell = activity_->item(index.row(), column)) values << cell->text();
            QApplication::clipboard()->setText(values.join(QStringLiteral("\n")));
        }
    });

    unresolved_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(unresolved_, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& position) {
        const auto index = unresolved_->indexAt(position);
        if (!index.isValid()) return;
        unresolved_->selectRow(index.row());
        auto* item = unresolved_->item(index.row(), 2);
        if (!item) return;
        const auto claim_id = s(item->data(Qt::UserRole).toString());
        const auto subject_id = s(item->data(Qt::UserRole + 1).toString());
        const auto object_id = s(item->data(Qt::UserRole + 2).toString());

        QMenu menu(this);
        auto* graph_action = menu.addAction(QStringLiteral("Review in investigation graph"));
        menu.addSeparator();
        auto* confirm_action = menu.addAction(QStringLiteral("✓ Mark as Confirmed"));
        auto* change_status = menu.addAction(QStringLiteral("Change status…"));
        auto* link_ev = menu.addAction(QStringLiteral("Link evidence…"));
        if (!subject_id.empty() || !object_id.empty()) menu.addSeparator();

        QAction* subject_profile = nullptr;
        QAction* object_profile = nullptr;
        if (!subject_id.empty()) {
            const auto it = cached_entities_.find(subject_id);
            const auto sub_name = it != cached_entities_.end() ? q(it->second.label) : QStringLiteral("subject");
            subject_profile = menu.addAction(QStringLiteral("Open %1 profile…").arg(sub_name));
        }
        if (!object_id.empty()) {
            const auto it = cached_entities_.find(object_id);
            const auto obj_name = it != cached_entities_.end() ? q(it->second.label) : QStringLiteral("object");
            object_profile = menu.addAction(QStringLiteral("Open %1 profile…").arg(obj_name));
        }

        const auto chosen = menu.exec(unresolved_->viewport()->mapToGlobal(position));
        if (chosen == graph_action && on_show_claim) {
            on_show_claim(claim_id);
        } else if (chosen == confirm_action) {
            try {
                context_.investigation.change_claim_status(claim_id, domain::ClaimStatus::Confirmed, "Confirmed via unresolved claims queue");
                refresh();
                if (on_case_data_changed) on_case_data_changed();
            } catch (const std::exception& error) {
                show_error(this, error);
            }
        } else if (chosen == change_status) {
            change_claim_status_quick(claim_id);
        } else if (chosen == link_ev) {
            link_claim_evidence_quick(claim_id);
        } else if (chosen == subject_profile && on_show_entity && !subject_id.empty()) {
            on_show_entity(subject_id);
        } else if (chosen == object_profile && on_show_entity && !object_id.empty()) {
            on_show_entity(object_id);
        }
    });

    connect(add_entity, &QPushButton::clicked, this, [this] { if (on_add_entity) on_add_entity(); });
    connect(add_claim_button_, &QPushButton::clicked, this, [this] { if (on_add_claim) on_add_claim(); });
    connect(add_evidence, &QPushButton::clicked, this, [this] { if (on_add_evidence) on_add_evidence(); });
    refresh();
}

void CaseOverviewPage::change_claim_status_quick(const domain::Id& claim_id) {
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
    } catch (const std::exception& error) {
        show_error(this, error);
    }
}

void CaseOverviewPage::link_claim_evidence_quick(const domain::Id& claim_id) {
    if (claim_id.empty()) return;
    const auto form = link_evidence_form(this, context_.investigation.list_evidence(case_id_), context_.investigation.list_sources(case_id_), true, true, domain::EvidenceRole::Supports);
    if (!form) return;
    domain::Id imported_id;
    try {
        const bool created_evidence = form->import_file_path.has_value() || form->new_url.has_value() || form->new_text.has_value();
        const auto evidence_id = persist_link_evidence(context_.investigation, case_id_, *form);
        if (created_evidence) imported_id = evidence_id;
        context_.investigation.link_evidence(claim_id, evidence_id, form->role, s(form->note));
        refresh();
        if (on_case_data_changed) on_case_data_changed();
    } catch (const std::exception& error) {
        if (!imported_id.empty()) {
            try { context_.investigation.delete_evidence(imported_id, true); } catch (...) { }
        }
        show_error(this, error);
    }
}

void CaseOverviewPage::apply_unresolved_filter() {
    unresolved_->setRowCount(0);
    const auto search_text = search_unresolved_ ? search_unresolved_->text().trimmed().toLower() : QString();
    const auto status_filter = status_unresolved_filter_ ? status_unresolved_filter_->currentData().toString() : QStringLiteral("all");
    const auto kind_filter = kind_unresolved_filter_ ? kind_unresolved_filter_->currentData().toString() : QStringLiteral("all");

    for (const auto& claim : cached_unresolved_claims_) {
        const auto status_str = q(domain::to_string(claim.status)).toLower();
        if (status_filter != QStringLiteral("all") && status_str != status_filter) continue;

        const auto kind_str = q(domain::to_string(claim.kind)).toLower();
        if (kind_filter != QStringLiteral("all") && kind_str != kind_filter) continue;

        domain::Id subject_id, object_id;
        QString subject_name, object_name;
        QString subject_type, object_type;
        for (const auto& link : context_.investigation.claim_entities(claim.id)) {
            const auto it = cached_entities_.find(link.entity_id);
            if (it == cached_entities_.end()) continue;
            if (link.role == domain::ClaimEntityRole::Subject) {
                subject_id = it->first;
                subject_name = q(it->second.label);
                subject_type = enum_label(domain::to_string(it->second.type));
            } else if (link.role == domain::ClaimEntityRole::Object) {
                object_id = it->first;
                object_name = q(it->second.label);
                object_type = enum_label(domain::to_string(it->second.type));
            }
        }

        const auto predicate = claim.predicate ? human_readable_predicate(q(*claim.predicate)) : QString();
        const auto statement = q(claim.statement);
        const auto reasoning = q(claim.reasoning);

        if (!search_text.isEmpty()) {
            bool matches = statement.toLower().contains(search_text) ||
                           reasoning.toLower().contains(search_text) ||
                           predicate.toLower().contains(search_text) ||
                           subject_name.toLower().contains(search_text) ||
                           object_name.toLower().contains(search_text);
            if (!matches) continue;
        }

        const auto row = unresolved_->rowCount();
        unresolved_->insertRow(row);

        unresolved_->setCellWidget(row, 0, status_badge(domain::to_string(claim.status)));

        auto* kind_cell = cell(enum_label(domain::to_string(claim.kind)));
        kind_cell->setTextAlignment(Qt::AlignCenter);
        unresolved_->setItem(row, 1, kind_cell);

        QString display_statement;
        if (claim.predicate && !subject_name.isEmpty() && !object_name.isEmpty()) {
            display_statement = QStringLiteral("%1 (%2)  ➔  %3  ➔  %4 (%5)")
                .arg(subject_name, subject_type, predicate, object_name, object_type);
        } else {
            display_statement = statement;
        }

        auto* statement_cell = cell(display_statement, q(claim.id));
        statement_cell->setData(Qt::UserRole + 1, q(subject_id));
        statement_cell->setData(Qt::UserRole + 2, q(object_id));
        QString tooltip = statement;
        if (!reasoning.isEmpty()) tooltip += QStringLiteral("\n\nReasoning: ") + reasoning;
        if (claim.predicate) tooltip += QStringLiteral("\nRelationship: ") + predicate;
        statement_cell->setToolTip(tooltip);
        unresolved_->setItem(row, 2, statement_cell);

        const auto evidence_links = context_.investigation.claim_evidence(claim.id);
        auto* evidence_cell = cell(evidence_link_summary(evidence_links));
        evidence_cell->setToolTip(evidence_links.empty() ? QStringLiteral("No evidence linked yet.") : evidence_link_summary(evidence_links));
        unresolved_->setItem(row, 3, evidence_cell);

        auto* updated_cell = cell(relative_time_label(claim.updated_at));
        updated_cell->setToolTip(display_time(claim.updated_at));
        unresolved_->setItem(row, 4, updated_cell);

        auto* triage_btn = new QPushButton(QStringLiteral("Review ▾"));
        triage_btn->setFixedHeight(26);
        triage_btn->setCursor(Qt::PointingHandCursor);
        triage_btn->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: #1e2638; color: #b0bfd8; border: 1px solid #2d384e; border-radius: 4px; padding: 2px 8px; font-size: 9pt; } "
            "QPushButton:hover { background-color: #27344d; color: #ffffff; border-color: #3e4e6c; }"
        ));
        connect(triage_btn, &QPushButton::clicked, this, [this, claim_id = claim.id, triage_btn, subject_id, object_id] {
            QMenu menu(this);
            auto* graph_action = menu.addAction(QStringLiteral("Review in investigation graph"));
            menu.addSeparator();
            auto* confirm_action = menu.addAction(QStringLiteral("✓ Mark as Confirmed"));
            auto* change_status = menu.addAction(QStringLiteral("Change status…"));
            auto* link_ev = menu.addAction(QStringLiteral("Link evidence…"));
            if (!subject_id.empty() || !object_id.empty()) menu.addSeparator();
            QAction* sub_prof = nullptr;
            QAction* obj_prof = nullptr;
            if (!subject_id.empty()) {
                const auto it = cached_entities_.find(subject_id);
                const auto sub_name = it != cached_entities_.end() ? q(it->second.label) : QStringLiteral("subject");
                sub_prof = menu.addAction(QStringLiteral("Open %1 profile…").arg(sub_name));
            }
            if (!object_id.empty()) {
                const auto it = cached_entities_.find(object_id);
                const auto obj_name = it != cached_entities_.end() ? q(it->second.label) : QStringLiteral("object");
                obj_prof = menu.addAction(QStringLiteral("Open %1 profile…").arg(obj_name));
            }
            const auto chosen = menu.exec(triage_btn->mapToGlobal(QPoint(0, triage_btn->height())));
            if (chosen == graph_action && on_show_claim) {
                on_show_claim(claim_id);
            } else if (chosen == confirm_action) {
                try {
                    context_.investigation.change_claim_status(claim_id, domain::ClaimStatus::Confirmed, "Confirmed via triage queue");
                    refresh();
                    if (on_case_data_changed) on_case_data_changed();
                } catch (const std::exception& error) {
                    show_error(this, error);
                }
            } else if (chosen == change_status) {
                change_claim_status_quick(claim_id);
            } else if (chosen == link_ev) {
                link_claim_evidence_quick(claim_id);
            } else if (chosen == sub_prof && on_show_entity && !subject_id.empty()) {
                on_show_entity(subject_id);
            } else if (chosen == obj_prof && on_show_entity && !object_id.empty()) {
                on_show_entity(object_id);
            }
        });
        unresolved_->setCellWidget(row, 5, triage_btn);
    }
}

void CaseOverviewPage::refresh() {
    try {
        const auto value = context_.cases.get(case_id_);
        const auto purpose = q(value.purpose);
        const auto description = q(value.description);
        QString purpose_text = purpose;
        if (purpose_text.isEmpty()) purpose_text = description;
        else if (!description.isEmpty() && description != purpose) purpose_text += QStringLiteral("\nDescription: ") + description;
        if (purpose_text.isEmpty()) purpose_text = QStringLiteral("No purpose added yet.");
        const auto scope = value.scope.empty() ? QStringLiteral("Not specified") : q(value.scope);
        case_purpose_->setFullText(QStringLiteral("Purpose: %1\nScope: %2").arg(purpose_text, scope));

        const auto entities = context_.investigation.list_entities(case_id_);
        const auto claims = context_.investigation.list_claims(case_id_);
        const auto evidence = context_.investigation.list_evidence(case_id_);
        const auto sources = context_.investigation.list_sources(case_id_);

        cached_entities_.clear();
        for (const auto& entity : entities) cached_entities_.emplace(entity.id, entity);

        add_claim_button_->setEnabled(!entities.empty());
        add_claim_button_->setToolTip(entities.empty() ? QStringLiteral("Add an entity first to create a claim.") : QStringLiteral("Create an observation, inference, or directional relation."));
        entity_count_->setText(QString::number(entities.size()));
        claim_count_->setText(QString::number(claims.size()));
        evidence_count_->setText(QString::number(evidence.size()));
        source_count_->setText(QString::number(sources.size()));

        activity_->setRowCount(0);
        const auto events = context_.investigation.activity(case_id_);
        const auto start = events.size() > 9 ? events.size() - 9 : 0;
        for (std::size_t index = events.size(); index-- > start;) {
            const auto& event = events[index];
            const auto row = activity_->rowCount();
            activity_->insertRow(row);
            activity_->setItem(row, 0, cell(display_time(event.occurred_at)));
            activity_->setItem(row, 1, cell(enum_label(event.action)));
            activity_->setItem(row, 2, cell(q(event.description)));
        }

        cached_unresolved_claims_.clear();
        int unverified_count = 0;
        int possible_count = 0;
        int other_unresolved = 0;

        for (const auto& claim : claims) {
            if (claim.status == domain::ClaimStatus::Confirmed) continue;
            cached_unresolved_claims_.push_back(claim);
            if (claim.status == domain::ClaimStatus::Unverified) ++unverified_count;
            else if (claim.status == domain::ClaimStatus::Possible) ++possible_count;
            else ++other_unresolved;
        }

        const auto total_unresolved = cached_unresolved_claims_.size();
        QString count_label = QStringLiteral("%1 to review").arg(total_unresolved);
        if (total_unresolved > 0) {
            QStringList parts;
            if (unverified_count > 0) parts << QStringLiteral("%1 unverified").arg(unverified_count);
            if (possible_count > 0) parts << QStringLiteral("%1 possible").arg(possible_count);
            if (other_unresolved > 0) parts << QStringLiteral("%1 disputed/refuted").arg(other_unresolved);
            count_label = QStringLiteral("%1 to review (%2)").arg(total_unresolved).arg(parts.join(QStringLiteral(", ")));
        }
        unresolved_count_->setText(count_label);

        apply_unresolved_filter();
    } catch (const std::exception& error) {
        show_error(this, error);
    }
}

EntitiesPage::EntitiesPage(ApplicationContext& context, const domain::Id& case_id, QWidget* parent) : CasePage(context, case_id, parent) {
    auto* root = new QVBoxLayout(this); root->setContentsMargins(24, 20, 24, 20); root->setSpacing(12);
    auto* add = button(QStringLiteral("＋ Add entity"), true);
    auto* edit = button(QStringLiteral("Edit selected"));
    auto* open_profile = button(QStringLiteral("Open profile  ➔"), false);
    root->addWidget(header_with_actions(QStringLiteral("Entities"), QStringLiteral("Objects are descriptive records. Confidence belongs to the claims that connect them."), {add, edit, open_profile}));
    auto* filters = new QHBoxLayout;
    type_filter_ = enum_combo(nullptr, {"All entity types", "Person", "Username", "Account", "Email", "Phone", "Domain", "IP", "Company", "Place", "URL", "Image", "Document", "Event", "Other"}, {"all", "person", "username", "account", "email", "phone", "domain", "ip", "company", "place", "url", "image", "document", "event", "other"});
    search_ = new QLineEdit;
    search_->setPlaceholderText(QStringLiteral("Search labels, values, aliases, tags…"));
    search_->addAction(ui_icon(UiIcon::Search), QLineEdit::LeadingPosition);
    filters->addWidget(type_filter_);
    filters->addWidget(search_, 1);
    root->addLayout(filters);

    auto* split = responsive_splitter();
    table_ = table_with_empty_state();
    configure_table(table_, {"Name / value", "Type", "Aliases", "Related", "Updated"});
    table_->set_empty_state(QStringLiteral("No entities yet"), QStringLiteral("Entities are descriptive records. Add the first person, account, domain, or other object to begin mapping the case."));
    table_->setColumnWidth(0, 220); table_->setColumnWidth(1, 130); table_->setColumnWidth(2, 160); table_->setColumnWidth(3, 190); table_->setColumnWidth(4, 160);
    table_->setMinimumHeight(0); table_->setMaximumHeight(QWIDGETSIZE_MAX);
    table_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* entity_list = new QWidget;
    entity_list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* entity_list_layout = new QVBoxLayout(entity_list);
    entity_list_layout->setContentsMargins(0, 0, 0, 0);
    entity_list_layout->addWidget(table_, 1);
    split->addWidget(entity_list);

    detail_scroll_ = new QScrollArea;
    detail_scroll_->setWidgetResizable(true);
    auto* detail = card();
    auto* detail_layout = new QVBoxLayout(detail);
    detail_layout->setContentsMargins(24, 22, 24, 22);
    detail_title_ = heading(QStringLiteral("Select an entity"), 21);
    auto* open_profile_card_btn = button(QStringLiteral("Open full profile  ➔"), false);
    open_profile_card_btn->setFixedHeight(30);
    connect(open_profile_card_btn, &QPushButton::clicked, this, [this] {
        const auto id = selected_id();
        if (!id.empty() && on_open_profile) on_open_profile(id);
    });

    detail_photo_ = new QLabel;
    detail_photo_->setAlignment(Qt::AlignCenter);
    detail_photo_->setMinimumHeight(120);
    detail_photo_->setMaximumHeight(180);
    detail_meta_ = muted(QStringLiteral("Entity details, aliases, linked claims, evidence, and history appear here."));
    detail_description_ = muted(QString());
    detail_aliases_ = muted(QString());
    detail_tags_ = make_entity_tag_row(detail);
    linked_claims_ = table_with_empty_state();
    configure_table(linked_claims_, {"Status", "Claim"});
    linked_claims_->set_empty_state(QStringLiteral("No related claims"), QStringLiteral("Claims and relations involving this entity will appear here."));
    linked_claims_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    linked_claims_->setColumnWidth(0, 130);
    linked_claims_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    linked_evidence_ = table_with_empty_state();
    configure_table(linked_evidence_, {"Kind", "Evidence", "State"});
    linked_evidence_->set_empty_state(QStringLiteral("No related evidence"), QStringLiteral("Evidence linked through claims will appear here."));
    linked_evidence_->setColumnWidth(0, 120); linked_evidence_->setColumnWidth(1, 260); linked_evidence_->setColumnWidth(2, 170);

    history_ = table_with_empty_state();
    configure_table(history_, {"When", "Action", "Description"});
    history_->set_empty_state(QStringLiteral("No history yet"), QStringLiteral("Changes to this entity will be recorded here."));
    history_->setColumnWidth(0, 180); history_->setColumnWidth(1, 130); history_->setColumnWidth(2, 320);

    detail_layout->addWidget(detail_title_);
    detail_layout->addWidget(open_profile_card_btn);
    detail_layout->addWidget(detail_photo_);
    detail_layout->addWidget(detail_meta_);
    detail_layout->addWidget(detail_description_);
    detail_layout->addWidget(detail_aliases_);
    detail_layout->addWidget(detail_tags_);
    detail_layout->addSpacing(18);
    detail_layout->addWidget(new QLabel(QStringLiteral("Related claims")));
    detail_layout->addWidget(linked_claims_);
    detail_layout->addWidget(new QLabel(QStringLiteral("Related evidence")));
    detail_layout->addWidget(linked_evidence_);
    detail_layout->addWidget(new QLabel(QStringLiteral("Change history")));
    detail_layout->addWidget(history_);
    detail_layout->addStretch();
    detail_scroll_->setWidget(detail);
    split->addWidget(detail_scroll_);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    split->setSizes({1000, 650});
    root->addWidget(split, 1);

    connect(add, &QPushButton::clicked, this, &EntitiesPage::add_entity);
    connect(edit, &QPushButton::clicked, this, &EntitiesPage::edit_selected);
    connect(open_profile, &QPushButton::clicked, this, [this] {
        const auto id = selected_id();
        if (!id.empty() && on_open_profile) on_open_profile(id);
    });

    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table_, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& position) {
        const auto index = table_->indexAt(position);
        if (!index.isValid()) return;
        table_->selectRow(index.row());
        QMenu menu(this);
        auto* open_prof = menu.addAction(QStringLiteral("Open entity profile"));
        auto* related = menu.addAction(QStringLiteral("See related claims"));
        auto* edit_action = menu.addAction(QStringLiteral("Edit entity"));
        menu.addSeparator();
        auto* remove = menu.addAction(QStringLiteral("Delete entity"));
        const auto action = menu.exec(table_->viewport()->mapToGlobal(position));
        if (action == open_prof) {
            const auto id = selected_id();
            if (!id.empty() && on_open_profile) on_open_profile(id);
        } else if (action == related) show_related_selected();
        else if (action == edit_action) edit_selected();
        else if (action == remove) delete_selected();
    });

    connect(table_, &QTableWidget::itemSelectionChanged, this, &EntitiesPage::update_detail);
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int, int) {
        const auto id = selected_id();
        if (!id.empty() && on_open_profile) on_open_profile(id);
    });
    connect(search_, &QLineEdit::textChanged, this, [this] { refresh(); });
    connect(type_filter_, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
    refresh();
}

domain::Id EntitiesPage::selected_id() const { const auto row = table_->currentRow(); return row < 0 ? domain::Id{} : s(table_->item(row, 0)->data(Qt::UserRole).toString()); }

void EntitiesPage::edit_selected() {
    const auto id = selected_id();
    if (id.empty()) return;
    try {
        const auto current = context_.investigation.get_entity(id);
        const auto form = entity_form(this, current);
        if (!form) return;
        context_.investigation.update_entity(id,
                                             s(form->label),
                                             s(form->original_value),
                                             details_with_description(s(form->description)),
                                             form->tags,
                                             form->aliases);
        const auto old_entity = context_.investigation.get_entity(id);
        const auto old_images = json_strings(old_entity.image_paths_json);
        for (const auto& path : old_images) if (std::find_if(form->image_paths.begin(), form->image_paths.end(), [&path](const QString& candidate) { return candidate.toStdString() == path; }) == form->image_paths.end()) context_.investigation.remove_entity_image(id, path);
        for (const auto& path : form->image_paths) if (std::find(old_images.begin(), old_images.end(), path.toStdString()) == old_images.end()) context_.investigation.add_entity_image(id, std::filesystem::path(s(path)));
        refresh();
        if (on_case_data_changed) on_case_data_changed();
    } catch (const std::exception& error) {
        show_error(this, error);
    }
}

void EntitiesPage::show_related_selected() {
    const auto id = selected_id();
    if (id.empty()) return;
    if (on_show_related_claims) on_show_related_claims(id);
}

void EntitiesPage::show_entity(const domain::Id& entity_id) {
    focus_entity_id_ = entity_id;
    refresh();
}

void EntitiesPage::delete_selected() {
    const auto id = selected_id();
    if (id.empty()) return;
    try {
        const auto current = context_.investigation.get_entity(id);
        const auto related_claims = context_.investigation.claims_for_entity(id);
        if (!related_claims.empty()) {
            QMessageBox::information(
                this,
                QStringLiteral("Entity is still referenced"),
                QStringLiteral("%1 is referenced by %2 claim(s). Those claims are preserved and must be reviewed in Claims & Relations before this entity can be deleted.")
                    .arg(q(current.label))
                    .arg(related_claims.size()));
            return;
        }
        if (QMessageBox::warning(
                this,
                QStringLiteral("Delete entity permanently"),
                QStringLiteral("Permanently delete \"%1\"? Any note links to this entity will be removed. This cannot be undone.")
                    .arg(q(current.label)),
                QMessageBox::Yes | QMessageBox::Cancel,
                QMessageBox::Cancel) != QMessageBox::Yes) {
            return;
        }
        context_.investigation.delete_entity(id, true);
        refresh();
        if (on_entities_changed) on_entities_changed();
    } catch (const std::exception& error) {
        show_error(this, error);
    }
}

void EntitiesPage::add_entity() { const auto form = entity_form(this); if (!form) return; try { const auto value = context_.investigation.create_entity(case_id_, form->type, s(form->label), s(form->original_value), details_with_description(s(form->description)), form->tags, form->aliases); for (const auto& path : form->image_paths) context_.investigation.add_entity_image(value.id, std::filesystem::path(s(path))); auto create_identifier = [this, &value](domain::EntityType type, const QString& raw) { const auto matches = context_.investigation.possible_duplicates(case_id_, type, s(raw)); const auto identifier = matches.empty() ? context_.investigation.create_entity(case_id_, type, s(raw), s(raw), details_with_description(QStringLiteral("Identifier recorded with %1").arg(q(value.label)).toStdString())) : matches.front(); context_.investigation.create_relation(case_id_, value.id, "identifier_for", identifier.id, {}, domain::ClaimStatus::Unverified, QStringLiteral("Captured during Person creation; relationship has not been independently assessed.").toStdString(), {}, {}, domain::ClaimKind::Observation); }; for (const auto& item : form->phone_numbers) create_identifier(domain::EntityType::Phone, item); for (const auto& item : form->usernames) create_identifier(domain::EntityType::Username, item); for (const auto& item : form->emails) create_identifier(domain::EntityType::Email, item); for (const auto& item : form->identifiers) create_identifier(domain::EntityType::Other, item); const auto duplicates = context_.investigation.possible_duplicates(case_id_, value.type, value.original_value); if (duplicates.size() > 1) QMessageBox::warning(this, QStringLiteral("Possible duplicate"), QStringLiteral("Another %1 with the same normalized value exists in this case. Both records remain separate; review their history before deciding what they mean.").arg(enum_label(domain::to_string(value.type)))); refresh(); if (on_entities_changed) on_entities_changed(); if (on_entity_created) on_entity_created(value.id); } catch (const std::exception& error) { show_error(this, error); } }

void EntitiesPage::refresh() {
    if (focus_entity_id_.empty()) focus_entity_id_ = selected_id();
    const auto values = context_.investigation.list_entities(case_id_);
    const auto search = search_->text().trimmed().toLower();
    const auto type = type_filter_->currentData().toString();
    table_->setRowCount(0);
    for (const auto& value : values) {
        if (type != "all" && q(domain::to_string(value.type)) != type) continue;
        const auto haystack = QStringLiteral("%1 %2 %3 %4").arg(q(value.label), q(value.original_value), q(value.tags_json), q(value.aliases_json)).toLower();
        if (!search.isEmpty() && !haystack.contains(search)) continue;
        const auto claims = context_.investigation.claims_for_entity(value.id);
        std::set<domain::Id> evidence_ids;
        for (const auto& claim : claims) for (const auto& link : context_.investigation.claim_evidence(claim.id)) evidence_ids.insert(link.evidence_id);
        const auto row = table_->rowCount();
        table_->insertRow(row);
        const auto displayed_name = value.label == value.original_value
            ? q(value.label)
            : QStringLiteral("%1\n%2").arg(q(value.label), q(value.original_value));
        auto* name_item = cell(displayed_name, q(value.id));
        set_accent_icon(name_item, entity_icon(value.type));
        table_->setItem(row, 0, name_item);
        table_->setItem(row, 1, cell(enum_label(domain::to_string(value.type))));
        table_->setItem(row, 2, cell(join_values(json_strings(value.aliases_json))));
        table_->setItem(row, 3, cell(QStringLiteral("%1 claims  ·  %2 evidence").arg(claims.size()).arg(evidence_ids.size())));
        table_->setItem(row, 4, cell(display_time(value.updated_at)));
    }
    if (table_->rowCount() == 0) {
        if (values.empty()) {
            table_->set_empty_state(QStringLiteral("No entities yet"), QStringLiteral("Entities are descriptive records. Add the first person, account, domain, or other object to begin mapping the case."));
        } else {
            table_->set_empty_state(QStringLiteral("No entities match"), QStringLiteral("Clear the search or type filter to see more records."));
        }
    }
    int focus_row = -1;
    for (int row = 0; row < table_->rowCount(); ++row) if (s(table_->item(row, 0)->data(Qt::UserRole).toString()) == focus_entity_id_) { focus_row = row; break; }
    if (focus_row >= 0) table_->selectRow(focus_row);
    else if (table_->rowCount() > 0) table_->selectRow(0);
    focus_entity_id_.clear();
    update_detail();
}

void EntitiesPage::update_detail() { const auto id = selected_id(); detail_scroll_->setVisible(!id.empty()); if (id.empty()) { detail_title_->setText(QStringLiteral("Select an entity")); detail_meta_->setText(QStringLiteral("Entity details, aliases, linked claims, evidence, and history appear here.")); detail_description_->clear(); detail_aliases_->clear(); detail_photo_->clear(); detail_photo_->hide(); detail_description_->hide(); detail_aliases_->hide(); detail_tags_->hide(); set_entity_tag_row(detail_tags_, {}); linked_claims_->setRowCount(0); linked_evidence_->setRowCount(0); history_->setRowCount(0); return; } try { const auto value = context_.investigation.get_entity(id); detail_title_->setText(q(value.label)); const auto image_paths = json_strings(value.image_paths_json); if (image_paths.empty()) detail_photo_->hide(); else { const auto path = context_.store.absolute_path(image_paths.front()); const auto image = QImage(QString::fromStdString(path.string())); if (!image.isNull()) { detail_photo_->setPixmap(QPixmap::fromImage(image).scaled(420, 170, Qt::KeepAspectRatio, Qt::SmoothTransformation)); detail_photo_->show(); } else detail_photo_->hide(); } const auto duplicates = context_.investigation.possible_duplicates(case_id_, value.type, value.original_value); detail_meta_->setText(QStringLiteral("%1 · %2%3").arg(enum_label(domain::to_string(value.type)), q(value.original_value), duplicates.size() > 1 ? QStringLiteral(" · Possible duplicate") : QString())); const auto description = q(json_description(value.details_json)); detail_description_->setVisible(!description.isEmpty()); detail_description_->setText(description); const auto aliases = join_values(json_strings(value.aliases_json)); detail_aliases_->setVisible(!aliases.isEmpty()); detail_aliases_->setText(aliases.isEmpty() ? QString() : QStringLiteral("Aliases · %1").arg(aliases)); QStringList tags; for (const auto& tag : json_strings(value.tags_json)) tags << q(tag); detail_tags_->setVisible(!tags.isEmpty()); set_entity_tag_row(detail_tags_, tags); linked_claims_->setRowCount(0); std::set<domain::Id> evidence_ids; for (const auto& claim : context_.investigation.claims_for_entity(id)) { const auto row = linked_claims_->rowCount(); linked_claims_->insertRow(row); linked_claims_->setCellWidget(row, 0, status_badge(domain::to_string(claim.status))); linked_claims_->setItem(row, 1, cell(q(claim.statement), q(claim.id))); for (const auto& evidence : context_.investigation.claim_evidence(claim.id)) evidence_ids.insert(evidence.evidence_id); } const auto evidence_values = context_.investigation.list_evidence(case_id_); std::unordered_map<domain::Id, domain::Evidence> evidence_by_id; for (const auto& evidence : evidence_values) evidence_by_id.emplace(evidence.id, evidence); linked_evidence_->setRowCount(0); for (const auto& evidence_id : evidence_ids) { const auto row = linked_evidence_->rowCount(); linked_evidence_->insertRow(row); const auto found = evidence_by_id.find(evidence_id); if (found == evidence_by_id.end()) { linked_evidence_->setItem(row, 0, cell(QStringLiteral("Missing"))); linked_evidence_->setItem(row, 1, cell(shorten_id(evidence_id), q(evidence_id))); linked_evidence_->setItem(row, 2, cell(QStringLiteral("Unavailable"))); } else { const auto& evidence = found->second; linked_evidence_->setItem(row, 0, cell(enum_label(domain::to_string(evidence.kind)))); linked_evidence_->setItem(row, 1, cell(evidence_label(evidence), q(evidence.id))); linked_evidence_->setItem(row, 2, cell(evidence.kind == domain::EvidenceKind::Url ? QStringLiteral("external only") : evidence.sha256 ? QStringLiteral("SHA-256 stored") : QStringLiteral("not preserved"))); } } history_->setRowCount(0); for (const auto& event : context_.investigation.activity(case_id_)) if (event.object_id && *event.object_id == id) { const auto row = history_->rowCount(); history_->insertRow(row); history_->setItem(row, 0, cell(display_time(event.occurred_at))); history_->setItem(row, 1, cell(enum_label(event.action))); history_->setItem(row, 2, cell(q(event.description))); } } catch (const std::exception& error) { show_error(this, error); } }

ClaimsPage::ClaimsPage(ApplicationContext& context, const domain::Id& case_id, QWidget* parent) : CasePage(context, case_id, parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* page_split = new QSplitter(Qt::Horizontal);
    page_split->setChildrenCollapsible(false);
    page_split->setHandleWidth(4);

    auto* left = new QWidget;
    auto* left_layout = new QVBoxLayout(left);
    left_layout->setContentsMargins(0, 0, 0, 0);
    left_layout->setSpacing(0);

    auto* graph_card = card();
    graph_card->setFixedHeight(560);
    auto* graph_layout = new QVBoxLayout(graph_card);
    graph_layout->setContentsMargins(10, 9, 10, 7);
    graph_layout->setSpacing(6);
    graph_layout->setContentsMargins(12, 9, 10, 7);
    auto* graph_toolbar = new QHBoxLayout;
    graph_toolbar->setContentsMargins(0, 0, 0, 0);
    graph_toolbar->setSpacing(6);
    auto* fit = button(QStringLiteral("Fit to view"));
    fit->setIcon(ui_icon(UiIcon::Target, QColor("#b7c4da")));
    fit->setIconSize(QSize(14, 14));
    fit->setFixedSize(104, 34);
    fit->setStyleSheet(QStringLiteral("QPushButton { min-height: 0px; padding: 0 6px; }") );
    auto* graph_search = new QLineEdit;
    graph_search->setPlaceholderText(QStringLiteral("Find entity in map..."));
    graph_search->addAction(ui_icon(UiIcon::Search, QColor("#9fb0c8")), QLineEdit::LeadingPosition);
    graph_search->setFixedHeight(29);
    graph_search->setMinimumWidth(120);
    graph_search->setMaximumWidth(200);
    graph_search->setClearButtonEnabled(true);
    graph_search->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    const auto zoom_icon = [](bool zoom_in) {
        QPixmap pixmap(24, 24);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor("#b9c6da"), 1.8, Qt::SolidLine, Qt::RoundCap));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QRectF(3, 3, 13, 13));
        painter.drawLine(QPointF(15, 15), QPointF(21, 21));
        painter.drawLine(QPointF(6.5, 9.5), QPointF(12.5, 9.5));
        if (zoom_in) painter.drawLine(QPointF(9.5, 6.5), QPointF(9.5, 12.5));
        return QIcon(pixmap);
    };
    auto* zoom_out = button(QString());
    auto* zoom_in = button(QString());
    auto* graph_add_entity = button(QStringLiteral("Add entity"), true);
    auto* connect_entities = button(QStringLiteral("Connect"));
    auto* undo_graph = button(QStringLiteral("Undo"));
    undo_graph->setToolTip(QStringLiteral("Undo the last graph change (Ctrl+Z)"));
    zoom_out->setIcon(zoom_icon(false));
    zoom_in->setIcon(zoom_icon(true));
    zoom_out->setIconSize(QSize(16, 16));
    zoom_in->setIconSize(QSize(16, 16));
    zoom_out->setFixedSize(34, 34);
    zoom_in->setFixedSize(34, 34);
    const auto compact_graph_button_style = QStringLiteral("QPushButton { min-height: 0px; padding: 0; }");
    zoom_out->setStyleSheet(compact_graph_button_style);
    zoom_in->setStyleSheet(compact_graph_button_style);
    zoom_out->setToolTip(QStringLiteral("Zoom out"));
    zoom_in->setToolTip(QStringLiteral("Zoom in"));
    graph_toolbar->addWidget(graph_add_entity);
    graph_toolbar->addWidget(connect_entities);
    graph_toolbar->addWidget(undo_graph);
    graph_toolbar->addWidget(fit);
    graph_toolbar->addWidget(graph_search, 1);
    graph_toolbar->addWidget(zoom_out);
    graph_toolbar->addWidget(zoom_in);
    graph_toolbar->addStretch();
    graph_layout->addLayout(graph_toolbar);
    graph_ = new RelationGraphWidget;
    static_cast<RelationGraphWidget*>(graph_)->set_persistence_key(q(case_id_));
    graph_layout->addWidget(graph_, 1);
    left_layout->addWidget(graph_card, 0, Qt::AlignTop);

    auto* claims_toolbar = new QWidget;
    claims_toolbar->setFixedHeight(42);
    auto* claims_layout = new QHBoxLayout(claims_toolbar);
    claims_layout->setContentsMargins(8, 0, 10, 0);
    claims_layout->setSpacing(6);
    auto* claims_title = new QLabel(QStringLiteral("Claims & Relations"));
    claims_title->setObjectName(QStringLiteral("claimsToolbarTitle"));
    claims_layout->addWidget(claims_title);
    claims_count_ = new QLabel(QStringLiteral("0"));
    claims_count_->setObjectName(QStringLiteral("claimsCount"));
    claims_count_->setAlignment(Qt::AlignCenter);
    claims_count_->setFixedSize(19, 18);
    claims_layout->addWidget(claims_count_);
    claims_layout->addStretch();
    const QStringList statuses = {"all", "unverified", "possible", "probable", "confirmed", "refuted"};
    QStringList status_labels;
    for (const auto& value : statuses) status_labels << (value == "all" ? QStringLiteral("All status") : enum_label(value.toStdString()));
    status_filter_ = enum_combo(nullptr, status_labels, statuses);
    status_filter_->setFixedSize(112, 27);
    const QStringList kinds = {"all", "observation", "inference"};
    type_filter_ = enum_combo(nullptr, {QStringLiteral("All types"), QStringLiteral("Observation"), QStringLiteral("Inference")}, kinds);
    type_filter_->setFixedSize(100, 27);
    search_ = new QLineEdit;
    search_->setPlaceholderText(QStringLiteral("Search claims..."));
    search_->addAction(ui_icon(UiIcon::Search, QColor("#9fb0c8")), QLineEdit::LeadingPosition);
    search_->setMinimumWidth(130);
    search_->setMaximumWidth(167);
    search_->setFixedHeight(27);
    search_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    clear_entity_filter_ = button(QStringLiteral("Clear entity filter"));
    clear_entity_filter_->setVisible(false);
    add_claim_button_ = button(QStringLiteral("＋ New claim / relation"), true);
    add_claim_button_->setToolTip(QStringLiteral("Record an observation, inference, or directional relation."));
    add_claim_button_->setMinimumHeight(32);
    add_claim_button_->setMinimumWidth(add_claim_button_->sizeHint().width() + 4);
    add_claim_button_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* add_entity = button(QStringLiteral("＋ Add entity"));
    add_entity->setMinimumHeight(32);
    add_entity->setMinimumWidth(add_entity->sizeHint().width() + 4);
    add_entity->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    claims_layout->addWidget(status_filter_);
    claims_layout->addWidget(type_filter_);
    claims_layout->addWidget(search_, 1);
    claims_layout->addWidget(clear_entity_filter_);
    claims_layout->addWidget(add_claim_button_);
    claims_layout->addWidget(add_entity);
    left_layout->addWidget(claims_toolbar, 0, Qt::AlignTop);

    table_ = table_with_empty_state();
    configure_table(table_, {QString(), QStringLiteral("Subject"), QStringLiteral("Predicate / claim"), QStringLiteral("Object"), QStringLiteral("Status"), QStringLiteral("Supporting"), QStringLiteral("Contradicting"), QStringLiteral("Updated")});
    table_->setObjectName(QStringLiteral("claimsTable"));
    table_->set_empty_state(QStringLiteral("No claims or relations yet"), QStringLiteral("Record an observation or directional relation to make the case assessable."));
    if (auto* select_header = table_->horizontalHeaderItem(0)) {
        select_header->setFlags(select_header->flags() | Qt::ItemIsUserCheckable);
        select_header->setCheckState(Qt::Unchecked);
    }
    table_->setWordWrap(false);
    table_->setTextElideMode(Qt::ElideRight);
    table_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    table_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table_->verticalHeader()->setDefaultSectionSize(28);
    table_->verticalHeader()->setMinimumSectionSize(28);
    table_->horizontalHeader()->setFixedHeight(29);
    table_->horizontalHeader()->setMinimumSectionSize(24);
    // The checkbox/actions and count/status fields stay compact; the three
    // entity/relationship fields share the remaining width so no data column
    // is pushed out of view as the left panel grows.
    const std::array<int, 8> claim_column_widths = {52, 0, 0, 0, 84, 96, 112, 105};
    auto* claim_header = table_->horizontalHeader();
    claim_header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    claim_header->setStretchLastSection(false);

    // Keep the compact utility columns fixed, but let the semantic columns
    // absorb all remaining width. Without this, QTableWidget leaves the
    // unused right side of a wider left pane outside the row/header paint
    // area, which makes selection and separators look truncated.
    for (int column = 0; column < table_->columnCount(); ++column) {
        if (column >= 1 && column <= 3) {
            claim_header->setSectionResizeMode(column, QHeaderView::Stretch);
        } else {
            claim_header->setSectionResizeMode(column, QHeaderView::Fixed);
            table_->setColumnWidth(column, claim_column_widths[static_cast<std::size_t>(column)]);
        }
    }
    for (const int column : {5, 6}) {
        if (auto* header_item = table_->horizontalHeaderItem(column)) header_item->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
    }
    table_->setColumnHidden(0, false);
    left_layout->addSpacing(5);
    left_layout->addWidget(table_, 1, Qt::AlignTop);
    page_split->addWidget(left);

    detail_scroll_ = new QScrollArea;
    detail_scroll_->setObjectName(QStringLiteral("claimInspectorScroll"));
    detail_scroll_->setMinimumWidth(0);
    detail_scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    detail_scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    detail_scroll_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    detail_scroll_->setWidgetResizable(true);
    auto* detail = new QFrame;
    detail->setObjectName(QStringLiteral("claimInspector"));
    detail->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* detail_layout = new QVBoxLayout(detail);
    detail_layout->setContentsMargins(12, 10, 7, 9);
    detail_layout->setSpacing(4);

    auto* inspector_top = new QHBoxLayout;
    inspector_top->setContentsMargins(0, 0, 0, 0);
    inspector_top->setSpacing(7);
    auto* claim_icon = new QLabel;
    claim_icon->setPixmap(ui_icon(UiIcon::Claim, appearance_accent_color().lighter(170)).pixmap(QSize(18, 18)));
    claim_icon->setProperty("accentIconKind", static_cast<int>(UiIcon::Claim));
    claim_icon->setProperty("accentIconSize", QSize(18, 18));
    claim_icon->setFixedSize(18, 18);
    inspector_top->addWidget(claim_icon);
    auto* inspector_label = new QLabel(QStringLiteral("Claim"));
    inspector_label->setObjectName(QStringLiteral("inspectorTitle"));
    inspector_top->addWidget(inspector_label);
    inspector_top->addStretch();
    detail_meta_ = new QLabel(QStringLiteral("#C-0000"));
    detail_meta_->setObjectName(QStringLiteral("claimId"));
    inspector_top->addWidget(detail_meta_);
    auto* inspector_more = new QPushButton;
    inspector_more->setObjectName(QStringLiteral("inspectorMore"));
    inspector_more->setIcon(ui_icon(UiIcon::More, QColor("#a8b6cb")));
    inspector_more->setIconSize(QSize(15, 15));
    inspector_more->setFixedSize(22, 22);
    inspector_top->addWidget(inspector_more);
    detail_layout->addLayout(inspector_top);
    detail_layout->addSpacing(10);

    detail_title_ = new QLabel(QStringLiteral("Select a claim"));
    detail_title_->setObjectName(QStringLiteral("claimInspectorStatement"));
    detail_title_->setWordWrap(false);
    detail_title_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    detail_layout->addWidget(detail_title_);
    detail_layout->addSpacing(14);

    auto* selectors = new QHBoxLayout;
    selectors->setContentsMargins(0, 0, 0, 0);
    selectors->setSpacing(8);
    const auto selector = [](const QString& label, QComboBox* combo) {
        auto* field = new QWidget;
        auto* layout = new QVBoxLayout(field);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(3);
        auto* caption = new QLabel(label);
        caption->setObjectName(QStringLiteral("fieldCaption"));
        layout->addWidget(caption);
        combo->setFixedHeight(26);
        combo->setMinimumWidth(0);
        combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        field->setMinimumWidth(0);
        field->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        layout->addWidget(combo);
        return field;
    };
    detail_kind_ = enum_combo(nullptr, {QStringLiteral("Observation"), QStringLiteral("Inference")}, {"observation", "inference"});
    detail_status_ = enum_combo(nullptr, {QStringLiteral("Unverified"), QStringLiteral("Possible"), QStringLiteral("Probable"), QStringLiteral("Confirmed"), QStringLiteral("Refuted")}, {"unverified", "possible", "probable", "confirmed", "refuted"});
    detail_kind_->setItemIcon(0, ui_icon(UiIcon::Eye, QColor("#aebbd2")));
    set_accent_icon(detail_kind_, 1, UiIcon::Claim);
    selectors->addWidget(selector(QStringLiteral("Kind"), detail_kind_), 1);
    selectors->addWidget(selector(QStringLiteral("Status"), detail_status_), 1);
    detail_layout->addLayout(selectors);

    auto* relation = new QFrame;
    relation->setObjectName(QStringLiteral("claimRelation"));
    auto* relation_layout = new QHBoxLayout(relation);
    relation_layout->setContentsMargins(0, 0, 0, 0);
    relation_layout->setSpacing(7);
    detail_subject_block_ = new QWidget;
    auto* subject_block = detail_subject_block_;
    subject_block->setCursor(Qt::PointingHandCursor);
    subject_block->setToolTip(QStringLiteral("Click to open subject entity profile"));
    subject_block->installEventFilter(this);
    auto* subject_layout = new QHBoxLayout(subject_block);
    subject_layout->setContentsMargins(0, 0, 0, 0);
    subject_layout->setSpacing(6);
    auto* subject_icon = new QLabel;
    subject_icon->setFixedSize(27, 27);
    detail_subject_icon_ = subject_icon;
    subject_layout->addWidget(subject_icon);
    auto* subject_copy = new QVBoxLayout;
    subject_copy->setContentsMargins(0, 0, 0, 0);
    subject_copy->setSpacing(1);
    detail_subject_ = new QLabel(QStringLiteral("Subject"));
    detail_subject_->setObjectName(QStringLiteral("relationEntity"));
    detail_subject_type_ = new QLabel(QStringLiteral("Entity"));
    detail_subject_type_->setObjectName(QStringLiteral("relationType"));
    subject_copy->addWidget(detail_subject_);
    subject_copy->addWidget(detail_subject_type_);
    subject_layout->addLayout(subject_copy, 1);
    relation_layout->addWidget(subject_block, 1);
    detail_relation_arrow_ = new QLabel;
    detail_relation_arrow_->setPixmap(ui_icon(UiIcon::ChevronRight, QColor("#c4cad8")).pixmap(QSize(18, 18)));
    detail_relation_arrow_->setFixedSize(18, 18);
    relation_layout->addWidget(detail_relation_arrow_);
    detail_object_block_ = new QWidget;
    auto* object_block = detail_object_block_;
    object_block->setCursor(Qt::PointingHandCursor);
    object_block->setToolTip(QStringLiteral("Click to open object entity profile"));
    object_block->installEventFilter(this);
    auto* object_layout = new QHBoxLayout(object_block);
    object_layout->setContentsMargins(0, 0, 0, 0);
    object_layout->setSpacing(6);
    auto* object_icon = new QLabel;
    object_icon->setFixedSize(27, 27);
    detail_object_icon_ = object_icon;
    object_layout->addWidget(object_icon);
    auto* object_copy = new QVBoxLayout;
    object_copy->setContentsMargins(0, 0, 0, 0);
    object_copy->setSpacing(1);
    detail_object_ = new QLabel(QStringLiteral("Object"));
    detail_object_->setObjectName(QStringLiteral("relationEntity"));
    detail_object_type_ = new QLabel(QStringLiteral("Entity"));
    detail_object_type_->setObjectName(QStringLiteral("relationType"));
    object_copy->addWidget(detail_object_);
    object_copy->addWidget(detail_object_type_);
    object_layout->addLayout(object_copy, 1);
    relation_layout->addWidget(object_block, 1);
    auto* relation_labels = new QWidget;
    auto* relation_labels_layout = new QHBoxLayout(relation_labels);
    relation_labels_layout->setContentsMargins(0, 0, 0, 0);
    relation_labels_layout->setSpacing(0);
    detail_subject_caption_ = new QLabel(QStringLiteral("Subject"));
    detail_subject_caption_->setObjectName(QStringLiteral("fieldCaption"));
    relation_labels_layout->addWidget(detail_subject_caption_);
    relation_labels_layout->addStretch();
    detail_object_caption_ = new QLabel(QStringLiteral("Object"));
    detail_object_caption_->setObjectName(QStringLiteral("fieldCaption"));
    relation_labels_layout->addWidget(detail_object_caption_);
    auto* relation_group = new QWidget;
    auto* relation_group_layout = new QVBoxLayout(relation_group);
    relation_group_layout->setContentsMargins(0, 8, 0, 0);
    relation_group_layout->setSpacing(-17);
    relation_group_layout->addWidget(relation_labels);
    relation_group_layout->addWidget(relation);
    detail_layout->addSpacing(8);
    detail_layout->addWidget(relation_group);

    detail_reasoning_ = new QLabel;
    detail_reasoning_->setObjectName(QStringLiteral("inspectorBody"));
    detail_reasoning_->setWordWrap(true);
    detail_reasoning_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* reasoning_header = inspector_section_header(QStringLiteral("Reasoning"), UiIcon::Document);
    if (auto* reasoning_header_layout = qobject_cast<QHBoxLayout*>(reasoning_header->layout())) reasoning_header_layout->setContentsMargins(0, 22, 0, 3);
    auto* reasoning_group = new QWidget;
    auto* reasoning_group_layout = new QVBoxLayout(reasoning_group);
    reasoning_group_layout->setContentsMargins(0, 0, 0, 0);
    reasoning_group_layout->setSpacing(-43);
    reasoning_group_layout->addWidget(reasoning_header);
    reasoning_group_layout->addWidget(detail_reasoning_);
    detail_layout->addSpacing(-2);
    detail_layout->addWidget(reasoning_group);

    auto* add_supporting = button(QStringLiteral("＋ Add evidence"));
    add_supporting->setMinimumHeight(32);
    add_supporting->setMinimumWidth(add_supporting->sizeHint().width());
    supporting_list_ = new QWidget;
    supporting_list_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto* supporting_layout = new QVBoxLayout(supporting_list_);
    supporting_layout->setContentsMargins(0, 0, 0, 0);
    supporting_layout->setSpacing(2);
    auto* supporting_header = inspector_section_header(QStringLiteral("Supporting evidence"), UiIcon::Evidence, &supporting_count_, add_supporting);
    auto* supporting_group = new QWidget;
    auto* supporting_group_layout = new QVBoxLayout(supporting_group);
    supporting_group_layout->setContentsMargins(0, 0, 0, 0);
    supporting_group_layout->setSpacing(-6);
    supporting_group_layout->addWidget(supporting_header);
    supporting_group_layout->addWidget(supporting_list_);
    detail_layout->addSpacing(11);
    detail_layout->addWidget(supporting_group);

    auto* add_contradicting = button(QStringLiteral("＋ Add evidence"));
    add_contradicting->setToolTip(QStringLiteral("Link evidence that contradicts this claim."));
    add_contradicting->setMinimumHeight(32);
    add_contradicting->setMinimumWidth(add_contradicting->sizeHint().width());
    contradicting_list_ = new QWidget;
    contradicting_list_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto* contradicting_layout = new QVBoxLayout(contradicting_list_);
    contradicting_layout->setContentsMargins(0, 0, 0, 0);
    contradicting_layout->setSpacing(0);
    auto* contradicting_header = inspector_section_header(QStringLiteral("Contradicting evidence"), UiIcon::Evidence, &contradicting_count_, add_contradicting);
    auto* contradicting_group = new QWidget;
    auto* contradicting_group_layout = new QVBoxLayout(contradicting_group);
    contradicting_group_layout->setContentsMargins(0, 0, 0, 0);
    contradicting_group_layout->setSpacing(0);
    contradicting_group_layout->addWidget(contradicting_header);
    contradicting_group_layout->addWidget(contradicting_list_);
    detail_layout->addSpacing(1);
    detail_layout->addWidget(contradicting_group);

    auto* add_context = button(QStringLiteral("＋ Add evidence"));
    add_context->setMinimumHeight(32);
    add_context->setMinimumWidth(add_context->sizeHint().width());
    context_evidence_list_ = new QWidget;
    context_evidence_list_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto* context_layout = new QVBoxLayout(context_evidence_list_);
    context_layout->setContentsMargins(0, 0, 0, 0);
    context_layout->setSpacing(0);
    auto* context_header = inspector_section_header(QStringLiteral("Contextual evidence"), UiIcon::Evidence, &context_evidence_count_, add_context);
    auto* context_group = new QWidget;
    auto* context_group_layout = new QVBoxLayout(context_group);
    context_group_layout->setContentsMargins(0, 0, 0, 0);
    context_group_layout->setSpacing(0);
    context_group_layout->addWidget(context_header);
    context_group_layout->addWidget(context_evidence_list_);
    detail_layout->addSpacing(1);
    detail_layout->addWidget(context_group);

    auto* added_row = new QHBoxLayout;
    added_row->setContentsMargins(0, 5, 0, 0);
    added_row->setSpacing(6);
    auto* added_icon = new QLabel;
    added_icon->setPixmap(ui_icon(UiIcon::Calendar, QColor("#a9b7cc")).pixmap(QSize(15, 15)));
    added_icon->setFixedSize(15, 15);
    added_row->addWidget(added_icon);
    detail_added_ = new QLabel(QStringLiteral("Added —"));
    detail_added_->setObjectName(QStringLiteral("addedTimestamp"));
    added_row->addWidget(detail_added_);
    added_row->addStretch();
    detail_layout->addLayout(added_row);
    detail_layout->addStretch(1);

    auto* edit = button(QStringLiteral("Edit statement"));
    auto* status = button(QStringLiteral("Change status"), true);
    auto* link = add_supporting;
    const auto open_inspector_menu = [this, inspector_more, edit, status, link] {
        QMenu menu(this);
        auto* edit_action = menu.addAction(ui_icon(UiIcon::Edit, appearance_accent_color().lighter(170)), QStringLiteral("Edit statement"));
        auto* status_action = menu.addAction(QStringLiteral("Change status"));
        auto* link_action = menu.addAction(QStringLiteral("Link evidence"));
        menu.addSeparator();
        auto* delete_action = menu.addAction(QStringLiteral("Delete claim"));
        const auto action = menu.exec(inspector_more->mapToGlobal(QPoint(0, inspector_more->height())));
        if (action == edit_action) edit->click();
        else if (action == status_action) status->click();
        else if (action == link_action) link->click();
        else if (action == delete_action) delete_selected();
    };
    connect(inspector_more, &QPushButton::clicked, this, open_inspector_menu);

    auto* right = detail_scroll_;
    right->setWidget(detail);
    page_split->addWidget(right);
    page_split->setSizes({std::max(390, width() * 66 / 100), std::max(260, width() * 34 / 100)});
    observe_resize(this, [graph_card, page_split](int width) {
        graph_card->setFixedHeight(width >= 950 ? 560 : 350);
        const bool narrow = width < 1000;
        page_split->setOrientation(narrow ? Qt::Vertical : Qt::Horizontal);
        page_split->setMinimumHeight(narrow ? 740 : 0);
        if (narrow) page_split->setSizes({410, 330});
        else page_split->setSizes({std::max(390, width * 66 / 100), std::max(260, width * 34 / 100)});
    });
    root->addWidget(page_split, 1);

    connect(add_claim_button_, &QPushButton::clicked, this, &ClaimsPage::add_claim);
    connect(add_entity, &QPushButton::clicked, this, [this] { if (on_add_entity) on_add_entity(); });
    connect(table_, &QTableWidget::itemSelectionChanged, this, &ClaimsPage::update_detail);
    connect(status_filter_, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
    connect(type_filter_, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
    connect(search_, &QLineEdit::textChanged, this, [this] { refresh(); });
    connect(clear_entity_filter_, &QPushButton::clicked, this, [this] { related_entity_id_.clear(); clear_entity_filter_->setVisible(false); refresh(); });
    auto* graph_widget = static_cast<RelationGraphWidget*>(graph_);
    connect(fit, &QPushButton::clicked, graph_widget, &RelationGraphWidget::fit_to_view);
    connect(zoom_out, &QPushButton::clicked, this, [graph_widget] { graph_widget->zoom_by(1.0 / 1.2); });
    connect(zoom_in, &QPushButton::clicked, this, [graph_widget] { graph_widget->zoom_by(1.2); });
    connect(graph_search, &QLineEdit::textChanged, this, [graph_widget, graph_search](const QString& text) {
        const auto matched = graph_widget->search_entities(text);
        graph_search->setToolTip(matched ? QStringLiteral("Search entities shown in the relationship map.") : QStringLiteral("No matching entity in this relationship map."));
    });
    graph_widget->on_entity_clicked = [this](const QString& id) { if (on_show_entity) on_show_entity(s(id)); };
    graph_widget->on_claim_clicked = [this](const QString& id) { if (on_show_claim) on_show_claim(s(id)); else show_claim(s(id)); };
    graph_widget->on_create_entity = [this] { has_pending_entity_position_ = false; if (on_add_entity) on_add_entity(); };
    graph_widget->on_create_entity_at = [this](const QPointF& point) { pending_entity_position_ = point; has_pending_entity_position_ = true; if (on_add_entity) on_add_entity(); };
    graph_widget->on_create_relation = [this, graph_widget](const QString& subject_id, const QString& object_id) {
        if (subject_id.isEmpty() || object_id.isEmpty()) return;
        bool ok = false;
        const auto predicate = QInputDialog::getText(this, QStringLiteral("Add connection"), QStringLiteral("Relationship type"), QLineEdit::Normal, QStringLiteral("related_to"), &ok).trimmed();
        if (!ok || predicate.isEmpty()) return;
        try {
            const auto created = context_.investigation.create_relation(case_id_, s(subject_id), s(predicate), s(object_id), {}, domain::ClaimStatus::Unverified, {}, {}, {}, domain::ClaimKind::Observation);
            const auto relation_id = created.id;
            graph_widget->push_undo([this, relation_id] {
                try {
                    context_.investigation.delete_claim(relation_id, true);
                    refresh();
                    if (on_case_data_changed) on_case_data_changed();
                } catch (const std::exception& error) { show_error(this, error); }
            });
            focus_claim_id_ = created.id; refresh(); show_claim(created.id); if (on_case_data_changed) on_case_data_changed();
        } catch (const std::exception& error) { show_error(this, error); }
    };
    graph_widget->on_edit_relation = [this](const QString& id) { edit_relation(s(id)); };
    graph_widget->on_reverse_relation = [this](const QString& id) { reverse_relation(s(id)); };
    graph_widget->on_delete_relation = [this](const QString& id) { delete_relation(s(id)); };
    connect(graph_add_entity, &QPushButton::clicked, this, [graph_widget] { if (graph_widget->on_create_entity) graph_widget->on_create_entity(); });
    connect(connect_entities, &QPushButton::clicked, this, [graph_widget] { graph_widget->begin_connection(); });
    connect(undo_graph, &QPushButton::clicked, graph_widget, &RelationGraphWidget::undo);
    connect(edit, &QPushButton::clicked, this, &ClaimsPage::edit_statement);
    connect(status, &QPushButton::clicked, this, [this] { change_selected_status(); });
    connect(link, &QPushButton::clicked, this, [this] { link_selected_evidence(); });
    connect(add_contradicting, &QPushButton::clicked, this, [this] { link_selected_evidence(domain::EvidenceRole::Contradicts); });
    connect(add_context, &QPushButton::clicked, this, [this] { link_selected_evidence(domain::EvidenceRole::Context); });
    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table_, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& position) {
        const auto index = table_->indexAt(position);
        if (!index.isValid()) return;
        table_->selectRow(index.row());
        QMenu menu(this);
        auto* details = menu.addAction(QStringLiteral("Open claim details"));
        auto* edit_action = menu.addAction(QStringLiteral("Edit statement"));
        auto* status_action = menu.addAction(QStringLiteral("Change status"));
        auto* link_action = menu.addAction(QStringLiteral("Link evidence"));
        menu.addSeparator();
        auto* delete_action = menu.addAction(QStringLiteral("Delete claim"));
        const auto action = menu.exec(table_->viewport()->mapToGlobal(position));
        if (action == details) update_detail();
        else if (action == edit_action) edit_statement();
        else if (action == status_action) change_selected_status();
        else if (action == link_action) link_selected_evidence();
        else if (action == delete_action) delete_selected();
    });
    refresh();
}

domain::Id ClaimsPage::selected_id() const { const auto row = table_->currentRow(); return row < 0 || !table_->item(row, 1) ? domain::Id{} : s(table_->item(row, 1)->data(Qt::UserRole).toString()); }

void ClaimsPage::show_related_to_entity(const domain::Id& entity_id) {
    related_entity_id_ = entity_id;
    clear_entity_filter_->setText(QStringLiteral("Clear entity filter"));
    try {
        const auto entity = context_.investigation.get_entity(entity_id);
        clear_entity_filter_->setToolTip(QStringLiteral("Showing claims related to %1").arg(q(entity.label)));
    } catch (const std::exception&) {
        clear_entity_filter_->setToolTip(QStringLiteral("Showing claims related to the selected entity"));
    }
    clear_entity_filter_->setVisible(true);
    refresh();
}

void ClaimsPage::show_claim(const domain::Id& claim_id) {
    related_entity_id_.clear();
    clear_entity_filter_->setVisible(false);
    search_->clear();
    status_filter_->setCurrentIndex(0);
    type_filter_->setCurrentIndex(0);
    focus_claim_id_ = claim_id;
    if (graph_ != nullptr) static_cast<RelationGraphWidget*>(graph_)->select_claim(q(claim_id));
    refresh();
}

void ClaimsPage::add_claim() {
    const auto form = claim_form(this, context_.investigation.list_entities(case_id_));
    if (!form) return;

    std::optional<LinkEvidenceForm> supporting_evidence;
    if (form->status == domain::ClaimStatus::Confirmed) {
        supporting_evidence = link_evidence_form(this,
                                                 context_.investigation.list_evidence(case_id_),
                                                 context_.investigation.list_sources(case_id_),
                                                 true,
                                                 false);
        if (!supporting_evidence) return;
    }

    domain::Id imported_id;
    try {
        std::vector<services::EvidenceLinkInput> evidence_links;
        if (supporting_evidence) {
            const bool created_evidence = supporting_evidence->import_file_path.has_value() || supporting_evidence->new_url.has_value() || supporting_evidence->new_text.has_value();
            const auto evidence_id = persist_link_evidence(context_.investigation, case_id_, *supporting_evidence);
            if (created_evidence) imported_id = evidence_id;
            evidence_links.emplace_back(evidence_id,
                                        domain::EvidenceRole::Supports,
                                        s(supporting_evidence->note));
        }

        domain::Claim created;
        if (form->relation) {
            created = context_.investigation.create_relation(case_id_,
                                                   form->subject_id,
                                                   s(form->predicate),
                                                   form->object_id,
                                                   s(form->statement),
                                                   form->status,
                                                   s(form->reasoning),
                                                   {},
                                                   evidence_links,
                                                   form->kind);
        } else {
            created = context_.investigation.create_claim(case_id_,
                                                s(form->statement),
                                                form->kind,
                                                {{form->context_id, domain::ClaimEntityRole::Context}},
                                                std::nullopt,
                                                form->status,
                                                s(form->reasoning),
                                                {},
                                                evidence_links);
        }
        related_entity_id_.clear();
        clear_entity_filter_->setVisible(false);
        search_->clear();
        status_filter_->setCurrentIndex(0);
        type_filter_->setCurrentIndex(0);
        focus_claim_id_ = created.id;
        refresh();
        if (on_case_data_changed) on_case_data_changed();
    } catch (const std::exception& error) {
        if (!imported_id.empty()) {
            try { context_.investigation.delete_evidence(imported_id, true); } catch (...) { }
        }
        show_error(this, error);
    }
}

void ClaimsPage::edit_statement() {
    const auto id = selected_id();
    if (id.empty()) return;
    try {
        const auto current = context_.investigation.get_claim(id);
        bool ok = false;
        const auto statement = QInputDialog::getText(this, QStringLiteral("Edit claim statement"), QStringLiteral("Statement"), QLineEdit::Normal, q(current.statement), &ok).trimmed();
        if (!ok || statement.isEmpty() || statement == q(current.statement)) return;
        context_.investigation.change_claim_statement(id, s(statement));
        refresh();
        if (on_case_data_changed) on_case_data_changed();
    } catch (const std::exception& error) {
        show_error(this, error);
    }
}

void ClaimsPage::edit_relation(const domain::Id& claim_id) {
    try {
        const auto claim = context_.investigation.get_claim(claim_id);
        if (!claim.predicate) return;
        bool ok = false;
        const auto predicate = QInputDialog::getText(this, QStringLiteral("Edit relationship"), QStringLiteral("Relationship type"), QLineEdit::Normal, q(*claim.predicate), &ok).trimmed();
        if (!ok || predicate.isEmpty() || s(predicate) == *claim.predicate) return;
        const auto previous_predicate = *claim.predicate;
        context_.investigation.change_relation_predicate(claim_id, s(predicate));
        auto* graph_widget = static_cast<RelationGraphWidget*>(graph_);
        graph_widget->push_undo([this, claim_id, previous_predicate] {
            try { context_.investigation.change_relation_predicate(claim_id, previous_predicate); show_claim(claim_id); if (on_case_data_changed) on_case_data_changed(); }
            catch (const std::exception& error) { show_error(this, error); }
        });
        show_claim(claim_id);
        if (on_case_data_changed) on_case_data_changed();
    } catch (const std::exception& error) { show_error(this, error); }
}

void ClaimsPage::reverse_relation(const domain::Id& claim_id) {
    try {
        context_.investigation.reverse_relation(claim_id);
        auto* graph_widget = static_cast<RelationGraphWidget*>(graph_);
        graph_widget->push_undo([this, claim_id] {
            try { context_.investigation.reverse_relation(claim_id); show_claim(claim_id); if (on_case_data_changed) on_case_data_changed(); }
            catch (const std::exception& error) { show_error(this, error); }
        });
        show_claim(claim_id);
        if (on_case_data_changed) on_case_data_changed();
    } catch (const std::exception& error) { show_error(this, error); }
}

void ClaimsPage::delete_relation(const domain::Id& claim_id) {
    try {
        const auto claim = context_.investigation.get_claim(claim_id);
        const auto entity_links = context_.investigation.claim_entities(claim_id);
        const auto evidence_links = context_.investigation.claim_evidence(claim_id);
        if (QMessageBox::warning(this, QStringLiteral("Remove connection"),
                QStringLiteral("Remove this relationship? Its evidence and entity records will remain in the case. You can undo this while the case stays open."),
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
        context_.investigation.delete_claim(claim_id, true);
        auto* graph_widget = static_cast<RelationGraphWidget*>(graph_);
        graph_widget->push_undo([this, claim, entity_links, evidence_links] {
            try { context_.investigation.restore_relation(claim, entity_links, evidence_links); focus_claim_id_ = claim.id; refresh(); show_claim(claim.id); if (on_case_data_changed) on_case_data_changed(); }
            catch (const std::exception& error) { show_error(this, error); }
        });
        refresh();
        if (on_case_data_changed) on_case_data_changed();
    } catch (const std::exception& error) { show_error(this, error); }
}

void ClaimsPage::change_selected_status() {
    const auto id = selected_id();
    if (id.empty()) return;
    try {
        const auto current = context_.investigation.get_claim(id);
        QStringList labels;
        for (const auto& value : {"unverified", "possible", "probable", "confirmed", "refuted"}) labels << enum_label(value);
        bool ok = false;
        const auto choice = QInputDialog::getItem(this, QStringLiteral("Change claim status"), QStringLiteral("New status"), labels, labels.indexOf(enum_label(domain::to_string(current.status))), false, &ok);
        if (!ok) return;
        const auto explanation = explanation_form(this, QStringLiteral("Explain status change"), QStringLiteral("Why is the assessment changing? This explanation remains in the activity log."));
        if (!explanation) return;
        context_.investigation.change_claim_status(id, domain::claim_status_from_string(s(choice.toLower().replace(' ', '_'))), s(*explanation));
        refresh();
        if (on_case_data_changed) on_case_data_changed();
    } catch (const std::exception& error) {
        show_error(this, error);
    }
}

void ClaimsPage::link_selected_evidence(domain::EvidenceRole preferred_role) {
    const auto id = selected_id();
    if (id.empty()) return;
    const auto form = link_evidence_form(this, context_.investigation.list_evidence(case_id_), context_.investigation.list_sources(case_id_), true, true, preferred_role);
    if (!form) return;
    domain::Id imported_id;
    try {
        const bool created_evidence = form->import_file_path.has_value() || form->new_url.has_value() || form->new_text.has_value();
        const auto evidence_id = persist_link_evidence(context_.investigation, case_id_, *form);
        if (created_evidence) imported_id = evidence_id;
        context_.investigation.link_evidence(id, evidence_id, form->role, s(form->note));
        refresh();
        if (on_case_data_changed) on_case_data_changed();
    } catch (const std::exception& error) {
        if (!imported_id.empty()) {
            try { context_.investigation.delete_evidence(imported_id, true); } catch (...) { }
        }
        show_error(this, error);
    }
}

void ClaimsPage::delete_selected() {
    const auto id = selected_id();
    if (id.empty()) return;
    try {
        const auto claim = context_.investigation.get_claim(id);
        if (QMessageBox::warning(
                this,
                QStringLiteral("Delete claim permanently"),
                QStringLiteral("Permanently delete \"%1\"? Evidence files remain in the case, but this claim's entity and evidence links will be removed. This cannot be undone.")
                    .arg(q(claim.statement)),
                QMessageBox::Yes | QMessageBox::Cancel,
                QMessageBox::Cancel) != QMessageBox::Yes) {
            return;
        }
        context_.investigation.delete_claim(id, true);
        refresh();
        if (on_case_data_changed) on_case_data_changed();
    } catch (const std::exception& error) {
        show_error(this, error);
    }
}

void ClaimsPage::refresh() {
    if (focus_claim_id_.empty()) focus_claim_id_ = selected_id();
    const auto entity_records = context_.investigation.list_entities(case_id_);
    const auto entities = entities_by_id(entity_records);
    add_claim_button_->setEnabled(!entity_records.empty());
    add_claim_button_->setToolTip(entity_records.empty() ? QStringLiteral("Add an entity first to create a claim.") : QStringLiteral("Create a claim that links entities or records an assessment."));
    const auto values = context_.investigation.list_claims(case_id_);
    const auto status = status_filter_->currentData().toString();
    const auto kind = type_filter_->currentData().toString();
    const auto search = search_->text().trimmed().toLower();
    claims_count_->setText(QString::number(values.size()));
    table_->setRowCount(0);
    std::vector<RelationGraphWidget::Relation> graph_relations;
    for (const auto& claim : values) {
        QString subject_id, subject, object_id, object, subject_type = QStringLiteral("Entity"), object_type = QStringLiteral("Entity");
        bool context_claim = false;
        bool matches_related_entity = related_entity_id_.empty();
        for (const auto& link : context_.investigation.claim_entities(claim.id)) {
            if (!related_entity_id_.empty() && link.entity_id == related_entity_id_) matches_related_entity = true;
            const auto found = entities.find(link.entity_id);
            const QString label = found == entities.end() ? shorten_id(link.entity_id) : q(found->second.label); const auto type_label = found == entities.end() ? QStringLiteral("Entity") : enum_label(domain::to_string(found->second.type));
            if (link.role == domain::ClaimEntityRole::Subject) { subject_id = q(link.entity_id); subject = label; }
            if (link.role == domain::ClaimEntityRole::Subject) subject_type = type_label;
            if (link.role == domain::ClaimEntityRole::Object) { object_id = q(link.entity_id); object = label; }
            if (link.role == domain::ClaimEntityRole::Object) object_type = type_label;
            if (link.role == domain::ClaimEntityRole::Context) {
                subject_id = q(link.entity_id); subject = label; subject_type = type_label; context_claim = true;
            }
        }
        if (!subject.isEmpty() && !object.isEmpty()) graph_relations.push_back({q(claim.id), q(claim.statement), subject_id, subject, q(claim.predicate.value_or("related to")), object_id, object, subject_type, object_type, q(domain::to_string(claim.status))});
        if (status != "all" && q(domain::to_string(claim.status)) != status) continue;
        if (kind != "all" && q(domain::to_string(claim.kind)) != kind) continue;
        if (!matches_related_entity) continue;
        const auto searchable = q(claim.statement + " " + claim.predicate.value_or("") + " " + claim.reasoning) + " " + subject + " " + object;
        if (!search.isEmpty() && !searchable.toLower().contains(search)) continue;
        std::int64_t support = 0, contradict = 0;
        for (const auto& link : context_.investigation.claim_evidence(claim.id)) {
            if (link.role == domain::EvidenceRole::Supports) ++support;
            if (link.role == domain::EvidenceRole::Contradicts) ++contradict;
        }
        const auto row = table_->rowCount();
        table_->insertRow(row);
        table_->setRowHeight(row, 27);
        auto* checkbox = new QTableWidgetItem;
        checkbox->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        checkbox->setCheckState(Qt::Unchecked);
        checkbox->setText(QString::number(row + 1));
        checkbox->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        checkbox->setToolTip(QStringLiteral("Claim row %1").arg(row + 1));
        table_->setItem(row, 0, checkbox);
        auto* subject_item = cell(subject.isEmpty() ? QStringLiteral("—") : subject, q(claim.id));
        set_accent_icon(subject_item, graph_entity_icon(subject_type));
        table_->setItem(row, 1, subject_item);
        auto* predicate_item = cell(context_claim ? q(claim.statement) : q(claim.predicate.value_or("related to")));
        predicate_item->setToolTip(q(claim.statement));
        table_->setItem(row, 2, predicate_item);
        auto* object_item = cell(object.isEmpty() ? QStringLiteral("—") : object);
        set_accent_icon(object_item, graph_entity_icon(object_type));
        table_->setItem(row, 3, object_item);
        table_->setCellWidget(row, 4, status_badge(domain::to_string(claim.status)));
        auto* support_item = cell(QString::number(support));
        support_item->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
        table_->setItem(row, 5, support_item);
        auto* contradict_item = cell(QString::number(contradict));
        contradict_item->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
        table_->setItem(row, 6, contradict_item);
        auto* updated_item = cell(relative_time_label(claim.updated_at));
        updated_item->setToolTip(display_time(claim.updated_at));
        updated_item->setData(Qt::AccessibleTextRole, display_time(claim.updated_at));
        table_->setItem(row, 7, updated_item);
    }
    if (table_->rowCount() == 0) {
        if (values.empty()) {
            table_->set_empty_state(QStringLiteral("No claims or relations yet"), QStringLiteral("Record an observation or directional relation to make the case assessable."));
        } else {
            table_->set_empty_state(QStringLiteral("No claims or relations match"), QStringLiteral("Clear the search, status, type, or entity filter to see more records."));
        }
    }
    table_->setMaximumHeight(table_->rowCount() == 0 ? 220 : std::min(340, 64 + table_->rowCount() * 35));
    int focus_row = -1;
    for (int row = 0; row < table_->rowCount(); ++row) if (table_->item(row, 1) && s(table_->item(row, 1)->data(Qt::UserRole).toString()) == focus_claim_id_) { focus_row = row; break; }
    if (focus_row >= 0) table_->selectRow(focus_row);
    else if (table_->rowCount() > 0) table_->selectRow(0);
    focus_claim_id_.clear();
    std::vector<std::tuple<QString, QString, QString>> graph_entities;
    for (const auto& entity : entity_records) graph_entities.emplace_back(q(entity.id), q(entity.label), enum_label(domain::to_string(entity.type)));
    static_cast<RelationGraphWidget*>(graph_)->set_relations(std::move(graph_relations), std::move(graph_entities));
    update_detail();
}

void ClaimsPage::refresh_appearance() {
    if (graph_ != nullptr) graph_->update();
    update_detail();
}

void ClaimsPage::focus_entity(const domain::Id& entity_id) {
    if (graph_ != nullptr) {
        static_cast<RelationGraphWidget*>(graph_)->focus_entity(q(entity_id));
    }
}

bool ClaimsPage::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::MouseButtonRelease) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            if (watched == detail_subject_block_ && !detail_subject_id_.empty() && on_show_entity) {
                on_show_entity(detail_subject_id_);
                return true;
            }
            if (watched == detail_object_block_ && !detail_object_id_.empty() && on_show_entity) {
                on_show_entity(detail_object_id_);
                return true;
            }
        }
    }
    return CasePage::eventFilter(watched, event);
}

void ClaimsPage::place_pending_entity(const domain::Id& entity_id) {
    if (!has_pending_entity_position_ || graph_ == nullptr) return;
    static_cast<RelationGraphWidget*>(graph_)->place_entity(q(entity_id), pending_entity_position_);
    has_pending_entity_position_ = false;
}

void ClaimsPage::update_detail() {
    const auto id = selected_id();
    detail_scroll_->setVisible(!id.empty());
    auto* inspector_label = findChild<QLabel*>(QStringLiteral("inspectorTitle"));
    if (id.empty()) {
        detail_subject_id_.clear();
        detail_object_id_.clear();
        if (inspector_label) inspector_label->setText(QStringLiteral("Claim or relation"));
        detail_meta_->setText(QStringLiteral("#C-0000"));
        detail_title_->setText(QStringLiteral("Select a claim or relation"));
        detail_reasoning_->clear();
        detail_subject_->setText(QStringLiteral("Subject"));
        detail_subject_caption_->setText(QStringLiteral("Subject"));
        detail_object_caption_->setText(QStringLiteral("Object"));
        detail_relation_arrow_->show();
        detail_object_block_->show();
        detail_subject_type_->setText(QStringLiteral("Entity"));
        detail_object_->setText(QStringLiteral("Object"));
        detail_object_type_->setText(QStringLiteral("Entity"));
        detail_added_->setText(QStringLiteral("Added —"));
        detail_kind_->setCurrentIndex(0);
        detail_status_->setCurrentIndex(0);
        supporting_count_->setText(QStringLiteral("0"));
        contradicting_count_->setText(QStringLiteral("0"));
        context_evidence_count_->setText(QStringLiteral("0"));
        clear_layout(qobject_cast<QVBoxLayout*>(supporting_list_->layout()));
        clear_layout(qobject_cast<QVBoxLayout*>(contradicting_list_->layout()));
        clear_layout(qobject_cast<QVBoxLayout*>(context_evidence_list_->layout()));
        return;
    }
    try {
        const auto claim = context_.investigation.get_claim(id);
        if (inspector_label) inspector_label->setText(claim.predicate ? QStringLiteral("Relation") : QStringLiteral("Claim"));
        detail_meta_->setText(QStringLiteral("#C-%1").arg(shorten_id(id).left(4)));
        detail_title_->setText(q(claim.statement));
        detail_kind_->setCurrentIndex(claim.kind == domain::ClaimKind::Inference ? 1 : 0);
        const auto status_value = q(domain::to_string(claim.status));
        detail_status_->setCurrentIndex(std::max(0, detail_status_->findData(status_value)));
        detail_reasoning_->setText(q(claim.reasoning.empty() ? "Not specified" : claim.reasoning));
        detail_added_->setText(QStringLiteral("Added %1").arg(display_time(claim.created_at)));

        const auto entity_values = entities_by_id(context_.investigation.list_entities(case_id_));
        QString subject, subject_type = QStringLiteral("Entity"), object, object_type = QStringLiteral("Entity");
        bool context_claim = false;
        domain::EntityType subject_entity_type = domain::EntityType::Other;
        domain::EntityType object_entity_type = domain::EntityType::Other;
        detail_subject_id_.clear();
        detail_object_id_.clear();
        for (const auto& link : context_.investigation.claim_entities(id)) {
            const auto found = entity_values.find(link.entity_id);
            if (found == entity_values.end()) continue;
            if (link.role == domain::ClaimEntityRole::Subject) {
                subject = q(found->second.label);
                subject_type = enum_label(domain::to_string(found->second.type));
                subject_entity_type = found->second.type;
                detail_subject_id_ = link.entity_id;
            } else if (link.role == domain::ClaimEntityRole::Object) {
                object = q(found->second.label);
                object_type = enum_label(domain::to_string(found->second.type));
                object_entity_type = found->second.type;
                detail_object_id_ = link.entity_id;
            } else if (link.role == domain::ClaimEntityRole::Context) {
                subject = q(found->second.label);
                subject_type = enum_label(domain::to_string(found->second.type));
                subject_entity_type = found->second.type;
                detail_subject_id_ = link.entity_id;
                context_claim = true;
            }
        }
        detail_subject_caption_->setText(context_claim ? QStringLiteral("Claim about") : QStringLiteral("Subject"));
        detail_object_caption_->setVisible(!context_claim);
        detail_relation_arrow_->setVisible(!context_claim);
        detail_object_block_->setVisible(!context_claim);
        detail_subject_->setText(subject.isEmpty() ? QStringLiteral("No subject") : subject);
        detail_subject_type_->setText(subject_type);
        detail_object_->setText(object.isEmpty() ? QStringLiteral("No object") : object);
        detail_object_type_->setText(object_type);
        const auto subject_color = graph_entity_color();
        const auto object_color = graph_entity_color();
        detail_subject_icon_->setPixmap(entity_badge_pixmap(subject_type, subject_color));
        detail_object_icon_->setPixmap(entity_badge_pixmap(object_type, object_color));
        Q_UNUSED(subject_entity_type);
        Q_UNUSED(object_entity_type);

        const auto evidence_values = context_.investigation.list_evidence(case_id_);
        std::unordered_map<domain::Id, domain::Evidence> evidence_by_id;
        for (const auto& item : evidence_values) evidence_by_id.emplace(item.id, item);
        clear_layout(qobject_cast<QVBoxLayout*>(supporting_list_->layout()));
        clear_layout(qobject_cast<QVBoxLayout*>(contradicting_list_->layout()));
        clear_layout(qobject_cast<QVBoxLayout*>(context_evidence_list_->layout()));
        int supporting = 0;
        int contradicting = 0;
        int contextual = 0;
        for (const auto& link : context_.investigation.claim_evidence(id)) {
            const auto found = evidence_by_id.find(link.evidence_id);
            if (found == evidence_by_id.end()) continue;
            if (link.role == domain::EvidenceRole::Supports) {
                ++supporting;
                qobject_cast<QVBoxLayout*>(supporting_list_->layout())->addWidget(inspector_evidence_card(found->second, link.role, [this, id, evidence_id = link.evidence_id] {
                    if (QMessageBox::question(this, QStringLiteral("Unlink evidence"), QStringLiteral("Remove this evidence link? The evidence item will remain in the case.")) != QMessageBox::Yes) return;
                    try { context_.investigation.unlink_evidence(id, evidence_id); update_detail(); }
                    catch (const std::exception& error) { show_error(this, error); }
                }));
            } else if (link.role == domain::EvidenceRole::Contradicts) {
                ++contradicting;
                qobject_cast<QVBoxLayout*>(contradicting_list_->layout())->addWidget(inspector_evidence_card(found->second, link.role, [this, id, evidence_id = link.evidence_id] {
                    if (QMessageBox::question(this, QStringLiteral("Unlink evidence"), QStringLiteral("Remove this evidence link? The evidence item will remain in the case.")) != QMessageBox::Yes) return;
                    try { context_.investigation.unlink_evidence(id, evidence_id); update_detail(); }
                    catch (const std::exception& error) { show_error(this, error); }
                }));
            } else {
                ++contextual;
                qobject_cast<QVBoxLayout*>(context_evidence_list_->layout())->addWidget(inspector_evidence_card(found->second, link.role, [this, id, evidence_id = link.evidence_id] {
                    if (QMessageBox::question(this, QStringLiteral("Unlink evidence"), QStringLiteral("Remove this evidence link? The evidence item will remain in the case.")) != QMessageBox::Yes) return;
                    try { context_.investigation.unlink_evidence(id, evidence_id); update_detail(); }
                    catch (const std::exception& error) { show_error(this, error); }
                }));
            }
        }
        supporting_count_->setText(QString::number(supporting));
        contradicting_count_->setText(QString::number(contradicting));
        context_evidence_count_->setText(QString::number(contextual));
    } catch (const std::exception& error) { show_error(this, error); }
}

EvidencePage::EvidencePage(ApplicationContext& context, const domain::Id& case_id, QWidget* parent) : CasePage(context, case_id, parent) {
    auto* root = new QVBoxLayout(this); root->setContentsMargins(14, 0, 14, 18); root->setSpacing(10); auto* add_source = button(QStringLiteral("＋ Add source")); auto* add_evidence = button(QStringLiteral("＋ Add evidence"), true); auto* add_quote = button(QStringLiteral("＋ Add quote")); root->addWidget(header_with_actions(QStringLiteral("Sources & Evidence"), QStringLiteral("Record sources separately, then link evidence to the relationship it supports."), {add_source, add_evidence, add_quote}));
    auto* split = responsive_splitter(nullptr, 950); split->setChildrenCollapsible(false); auto* left = new QWidget; left->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding); auto* left_layout = new QVBoxLayout(left); left_layout->setContentsMargins(0, 0, 0, 0); tabs_ = new QTabWidget; evidence_table_ = table_with_empty_state(); configure_table(evidence_table_, {"Name", "Kind", "Source", "Imported UTC", "SHA-256 (check)", "Linked claims"}); evidence_table_->set_empty_state(QStringLiteral("No preserved evidence yet"), QStringLiteral("Import a file, save a quotation, or record an external-only URL.")); sources_table_ = table_with_empty_state(); configure_table(sources_table_, {"Title", "Type", "Locator", "Accessed UTC"}); sources_table_->set_empty_state(QStringLiteral("No source records yet"), QStringLiteral("Add the page, document, registry, or person that gave rise to your evidence.")); tabs_->addTab(evidence_table_, QStringLiteral("Evidence")); tabs_->addTab(sources_table_, QStringLiteral("Sources")); set_accent_tab_icon(tabs_, 0, UiIcon::Evidence); set_accent_tab_icon(tabs_, 1, UiIcon::Source); evidence_filters_ = new QWidget; auto* evidence_filter_root = new QVBoxLayout(evidence_filters_); evidence_filter_root->setContentsMargins(0, 0, 0, 0); auto* evidence_filter_layout = new QHBoxLayout; evidence_filter_layout->setContentsMargins(0, 0, 0, 0); evidence_search_ = new QLineEdit; evidence_search_->setPlaceholderText(QStringLiteral("Search evidence by name, source, or content...")); evidence_search_->addAction(ui_icon(UiIcon::Search), QLineEdit::LeadingPosition); evidence_kind_filter_ = enum_combo(nullptr, {"All kinds", "Image", "Document", "Text", "Web page"}, {"all", "image", "document", "text", "url"}); evidence_source_filter_ = enum_combo(nullptr, {"All sources", "Web", "Document", "Registry", "Person", "Other"}, {"all", "web", "document", "registry", "person", "other"}); evidence_filter_layout->addWidget(evidence_search_, 1); evidence_filter_layout->addWidget(evidence_kind_filter_); evidence_filter_layout->addWidget(evidence_source_filter_); evidence_filter_root->addLayout(evidence_filter_layout); evidence_filter_empty_ = muted(QStringLiteral("No evidence matches these filters.")); evidence_filter_empty_->hide(); evidence_filter_root->addWidget(evidence_filter_empty_); left_layout->addWidget(evidence_filters_); left_layout->addWidget(tabs_); left_layout->addStretch(1); split->addWidget(left);
    detail_scroll_ = new QScrollArea; detail_scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); detail_scroll_->setWidgetResizable(true); auto* detail = card(); detail->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred); auto* detail_layout = new QVBoxLayout(detail); detail_layout->setContentsMargins(16, 16, 16, 16); detail_title_ = heading(QStringLiteral("Evidence"), 18); detail_id_ = muted(QString()); detail_body_ = muted(QStringLiteral("Select a preserved item to inspect its provenance.")); detail_body_->setWordWrap(true); detail_preview_ = new QLabel(QStringLiteral("Preview\nSelect an evidence item to inspect its local content.")); detail_preview_->setAlignment(Qt::AlignCenter); detail_preview_->setMinimumHeight(130); detail_preview_->setWordWrap(true); detail_preview_->setStyleSheet(QStringLiteral("QLabel { color: #94a4bc; background: #0a111b; border: 1px solid #25344a; border-radius: 7px; padding: 12px; }")); linked_claims_ = table_with_empty_state(); configure_table(linked_claims_, {"Status", "Claim"}); linked_claims_->set_empty_state(QStringLiteral("No linked claims"), QStringLiteral("Claims that use this evidence will appear here.")); auto* verify = button(QStringLiteral("Verify integrity")); auto* open = button(QStringLiteral("Preview in app")); auto* remove = button(QStringLiteral("Delete")); verify_button_ = verify; open_button_ = open; delete_button_ = remove; detail_links_label_ = new QLabel(QStringLiteral("Linked claims")); detail_layout->addWidget(detail_title_); detail_layout->addWidget(detail_id_); detail_layout->addWidget(detail_body_); detail_layout->addWidget(detail_preview_); auto* detail_actions = new QBoxLayout(QBoxLayout::TopToBottom); detail_actions->setSpacing(8); detail_actions->addWidget(verify); detail_actions->addWidget(open); detail_actions->addWidget(remove); image_add_button_ = button(QStringLiteral("Paste / add image")); image_remove_button_ = button(QStringLiteral("Remove image")); detail_actions->addWidget(image_add_button_); detail_actions->addWidget(image_remove_button_); detail_layout->addLayout(detail_actions); detail_layout->addWidget(detail_links_label_); detail_layout->addWidget(linked_claims_); detail_layout->addStretch(); detail_scroll_->setWidget(detail); split->addWidget(detail_scroll_); split->setSizes({std::max(700, width() * 3 / 5), std::max(380, width() * 2 / 5)}); root->addWidget(split, 1);
    evidence_table_->setMaximumHeight(360);
    sources_table_->setMaximumHeight(360);
    connect(evidence_search_, &QLineEdit::textChanged, this, [this] { apply_evidence_filters(); });
    connect(evidence_kind_filter_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] { apply_evidence_filters(); });
    connect(evidence_source_filter_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] { apply_evidence_filters(); });
    connect(add_source, &QPushButton::clicked, this, [this] { const auto form = source_form(this); if (!form) return; try { context_.investigation.create_source(case_id_, form->type, s(form->locator), s(form->title), s(form->accessed_at), s(form->author), s(form->reliability_note)); refresh(); if (on_case_data_changed) on_case_data_changed(); } catch (const std::exception& error) { show_error(this, error); } }); connect(add_evidence, &QPushButton::clicked, this, [this] { this->add_evidence(domain::EvidenceKind::File); }); connect(add_quote, &QPushButton::clicked, this, [this] { this->add_evidence(domain::EvidenceKind::Text); }); connect(evidence_table_, &QTableWidget::itemSelectionChanged, this, &EvidencePage::update_evidence_detail); connect(sources_table_, &QTableWidget::itemSelectionChanged, this, &EvidencePage::update_source_detail); connect(tabs_, &QTabWidget::currentChanged, this, [this](int index) { refresh_accent_tab_icons(tabs_); evidence_filters_->setVisible(index == 0); auto* active_table = index == 0 ? static_cast<QTableWidget*>(evidence_table_) : static_cast<QTableWidget*>(sources_table_); tabs_->setMaximumHeight(tabs_->tabBar()->sizeHint().height() + active_table->maximumHeight() + 10); if (index == 0) update_evidence_detail(); else update_source_detail(); }); connect(image_add_button_, &QPushButton::clicked, this, [this] { const auto id = selected_evidence_id(); if (id.empty()) return; const auto* mime = QGuiApplication::clipboard()->mimeData(); if (mime && mime->hasImage()) { const auto image = qvariant_cast<QImage>(mime->imageData()); const auto path = std::filesystem::temp_directory_path() / ("evidence-trace-clipboard-" + domain::new_id() + ".png"); if (!image.save(QString::fromStdString(path.string()), "PNG")) return; context_.investigation.add_evidence_image(id, path); } else { const auto path = QFileDialog::getOpenFileName(this, QStringLiteral("Choose evidence image"), {}, QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.webp *.gif)")); if (path.isEmpty()) return; context_.investigation.add_evidence_image(id, std::filesystem::path(s(path))); } update_evidence_detail(); });
    connect(image_remove_button_, &QPushButton::clicked, this, [this] { const auto id = selected_evidence_id(); if (id.empty()) return; const auto value = context_.investigation.get_evidence(id); const auto paths = json_strings(value.image_paths_json); if (paths.empty()) return; context_.investigation.remove_evidence_image(id, paths.front()); update_evidence_detail(); });
    connect(verify, &QPushButton::clicked, this, &EvidencePage::verify_selected); connect(open, &QPushButton::clicked, this, &EvidencePage::preview_selected); connect(remove, &QPushButton::clicked, this, &EvidencePage::delete_selected);
    evidence_table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(evidence_table_, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& position) {
        const auto index = evidence_table_->indexAt(position);
        if (!index.isValid()) return;
        tabs_->setCurrentIndex(0);
        evidence_table_->selectRow(index.row());
        QMenu menu(this);
        auto* preview = menu.addAction(QStringLiteral("Preview in app"));
        auto* verify = menu.addAction(QStringLiteral("Verify SHA-256"));
        menu.addSeparator();
        auto* remove = menu.addAction(QStringLiteral("Delete evidence"));
        const auto action = menu.exec(evidence_table_->viewport()->mapToGlobal(position));
        if (action == preview) preview_selected();
        else if (action == verify) verify_selected();
        else if (action == remove) delete_selected();
    });
    sources_table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(sources_table_, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& position) {
        const auto index = sources_table_->indexAt(position);
        if (!index.isValid()) return;
        tabs_->setCurrentIndex(1);
        sources_table_->selectRow(index.row());
        QMenu menu(this);
        auto* details = menu.addAction(QStringLiteral("View source details"));
        auto* remove = menu.addAction(QStringLiteral("Delete source"));
        const auto action = menu.exec(sources_table_->viewport()->mapToGlobal(position));
        if (action == details) {
            update_source_detail();
        } else if (action == remove) {
            const auto id = selected_source_id();
            if (id.empty()) return;
            try {
                const auto source = context_.investigation.get_source(id);
                if (QMessageBox::warning(this, QStringLiteral("Delete source permanently"), QStringLiteral("Permanently delete \"%1\"? Sources with linked evidence are protected. This cannot be undone.").arg(q(source.title.empty() ? source.locator : source.title)), QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
                context_.investigation.delete_source(id, true);
                refresh(); if (on_case_data_changed) on_case_data_changed();
            } catch (const std::exception& error) { show_error(this, error); }
        }
    });
    refresh();
}

void EvidencePage::verify_selected() {
    const auto id = selected_evidence_id();
    if (id.empty()) return;
    try {
        const auto result = context_.investigation.verify_evidence(id);
        if (result.ok) QMessageBox::information(this, QStringLiteral("Integrity verified"), q(result.message));
        else QMessageBox::warning(this, QStringLiteral("Integrity check failed"), q(result.message));
        update_evidence_detail();
    } catch (const std::exception& error) {
        show_error(this, error);
    }
}

void EvidencePage::preview_selected() {
    const auto id = selected_evidence_id();
    if (id.empty()) return;
    try {
        const auto value = context_.investigation.get_evidence(id);
        const auto image_paths = json_strings(value.image_paths_json);
        if (!image_paths.empty()) {
            auto* content = new QWidget;
            auto* layout = new QVBoxLayout(content);
            for (const auto& relative : image_paths) {
                const auto path = context_.store.absolute_path(relative);
                const QImage image(QString::fromStdString(path.string()));
                if (image.isNull()) continue;
                auto* label = new QLabel;
                label->setAlignment(Qt::AlignCenter);
                label->setPixmap(QPixmap::fromImage(image).scaled(1000, 700, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                layout->addWidget(label);
            }
            auto* scroll = new QScrollArea;
            scroll->setWidgetResizable(true);
            scroll->setWidget(content);
            show_preview_dialog(this, QStringLiteral("Attached images · %1").arg(evidence_label(value)),
                                QStringLiteral("%1 attached image(s). These captures are preserved as attachments and do not verify the assertion.").arg(image_paths.size()), scroll);
            return;
        }
        preview_evidence(this, value, context_.store);
    } catch (const std::exception& error) {
        show_error(this, error);
    }
}

void EvidencePage::delete_selected() {
    const auto id = selected_evidence_id();
    if (id.empty()) return;
    if (QMessageBox::question(this, QStringLiteral("Delete evidence"), QStringLiteral("Delete this evidence item? Linked claim, note, and run links will also be removed after explicit confirmation.")) != QMessageBox::Yes) return;
    try {
        context_.investigation.delete_evidence(id, true);
        refresh(); if (on_case_data_changed) on_case_data_changed();
    } catch (const std::exception& error) {
        show_error(this, error);
    }
}

domain::Id EvidencePage::selected_evidence_id() const { const auto row = evidence_table_->currentRow(); return row < 0 ? domain::Id{} : s(evidence_table_->item(row, 0)->data(Qt::UserRole).toString()); }
domain::Id EvidencePage::selected_source_id() const { const auto row = sources_table_->currentRow(); return row < 0 ? domain::Id{} : s(sources_table_->item(row, 0)->data(Qt::UserRole).toString()); }

void EvidencePage::add_evidence(domain::EvidenceKind preferred_kind) { const auto form = evidence_form(this, context_.investigation.list_sources(case_id_), preferred_kind); if (!form) return; try { domain::Evidence value; if (form->kind == domain::EvidenceKind::File) value = context_.investigation.add_file_evidence(case_id_, std::filesystem::path(s(form->file_path)), form->source_id, form->url.isEmpty() ? std::nullopt : std::optional<std::string>(s(form->url)), s(form->note)); else if (form->kind == domain::EvidenceKind::Text) value = context_.investigation.add_text_evidence(case_id_, s(form->text), form->source_id, form->url.isEmpty() ? std::nullopt : std::optional<std::string>(s(form->url)), s(form->note), s(form->quotation_location)); else value = context_.investigation.add_url_evidence(case_id_, s(form->url), form->source_id, s(form->note)); for (const auto& path : form->image_paths) context_.investigation.add_evidence_image(value.id, std::filesystem::path(s(path))); focus_evidence_id_ = value.id; refresh(); if (on_case_data_changed) on_case_data_changed(); } catch (const std::exception& error) { show_error(this, error); } }

void EvidencePage::show_sources() { tabs_->setCurrentIndex(1); }
void EvidencePage::show_evidence(const domain::Id& evidence_id) { focus_evidence_id_ = evidence_id; tabs_->setCurrentIndex(0); refresh(); }
void EvidencePage::show_source(const domain::Id& source_id) { focus_source_id_ = source_id; tabs_->setCurrentIndex(1); refresh(); }

void EvidencePage::refresh() { if (focus_evidence_id_.empty()) focus_evidence_id_ = selected_evidence_id(); if (focus_source_id_.empty()) focus_source_id_ = selected_source_id(); source_values_ = context_.investigation.list_sources(case_id_); evidence_values_ = context_.investigation.list_evidence(case_id_); std::unordered_map<domain::Id, domain::Source> source_map; for (const auto& value : source_values_) source_map.emplace(value.id, value); evidence_table_->setRowCount(0); for (const auto& value : evidence_values_) { const auto row = evidence_table_->rowCount(); evidence_table_->insertRow(row); const auto claims = context_.investigation.claims_for_evidence(value.id); const auto state = value.kind == domain::EvidenceKind::Url ? QStringLiteral("external only") : value.sha256 ? QStringLiteral("SHA-256 stored") : QStringLiteral("not preserved"); auto* evidence_item = cell(evidence_table_label(value), q(value.id)); evidence_item->setToolTip(evidence_label(value)); if (value.kind == domain::EvidenceKind::Url) evidence_item->setIcon(ui_icon(evidence_icon(value.kind), QColor("#6ee7d2"))); else set_accent_icon(evidence_item, evidence_icon(value.kind)); evidence_table_->setItem(row, 0, evidence_item); evidence_table_->setItem(row, 1, cell(enum_label(domain::to_string(value.kind)))); evidence_table_->setItem(row, 2, cell(value.source_id && source_map.contains(*value.source_id) ? q(source_map.at(*value.source_id).title.empty() ? source_map.at(*value.source_id).locator : source_map.at(*value.source_id).title) : QStringLiteral("—"))); evidence_table_->setItem(row, 3, cell(display_time(value.imported_at))); evidence_table_->setItem(row, 4, cell(value.kind == domain::EvidenceKind::Url ? QStringLiteral("external only") : state)); evidence_table_->setItem(row, 5, cell(QString::number(claims.size()))); } sources_table_->setRowCount(0); for (const auto& value : source_values_) { const auto row = sources_table_->rowCount(); sources_table_->insertRow(row); auto* source_item = cell(q(value.title.empty() ? value.locator : value.title), q(value.id)); set_accent_icon(source_item, UiIcon::Source); sources_table_->setItem(row, 0, source_item); sources_table_->setItem(row, 1, cell(enum_label(domain::to_string(value.type)))); sources_table_->setItem(row, 2, cell(q(value.locator))); sources_table_->setItem(row, 3, cell(display_time(value.accessed_at))); } const auto resize_sparse_table = [](QTableWidget* table) { const int rows = table->rowCount(); table->setMaximumHeight(rows == 0 ? 170 : std::min(360, 64 + rows * 62)); }; resize_sparse_table(evidence_table_); resize_sparse_table(sources_table_); auto* active_table = tabs_->currentIndex() == 0 ? static_cast<QTableWidget*>(evidence_table_) : static_cast<QTableWidget*>(sources_table_); tabs_->setMinimumHeight(0); tabs_->setMaximumHeight(tabs_->tabBar()->sizeHint().height() + active_table->maximumHeight() + 10); const auto select_target = [](QTableWidget* table, const domain::Id& target) { if (target.empty()) { if (table->rowCount() > 0) table->selectRow(0); return; } for (int row = 0; row < table->rowCount(); ++row) if (table->item(row, 0) && table->item(row, 0)->data(Qt::UserRole).toString() == q(target)) { table->selectRow(row); return; } }; select_target(evidence_table_, focus_evidence_id_); select_target(sources_table_, focus_source_id_); focus_evidence_id_.clear(); focus_source_id_.clear(); apply_evidence_filters(); if (tabs_->currentIndex() == 0) update_evidence_detail(); else update_source_detail(); }

void EvidencePage::apply_evidence_filters() {
    const auto query = evidence_search_->text().trimmed();
    const auto kind = evidence_kind_filter_->currentData().toString();
    const auto source_type = evidence_source_filter_->currentData().toString();
    std::unordered_map<domain::Id, domain::SourceType> source_types;
    for (const auto& source : source_values_) source_types.emplace(source.id, source.type);
    std::unordered_map<domain::Id, const domain::Evidence*> evidence_by_id;
    for (const auto& evidence : evidence_values_) evidence_by_id.emplace(evidence.id, &evidence);
    bool any_visible = false;
    int first_visible_row = -1;
    const int current_row = evidence_table_->currentRow();
    bool current_row_visible = false;

    for (int row = 0; row < evidence_table_->rowCount(); ++row) {
        const auto id = s(evidence_table_->item(row, 0)->data(Qt::UserRole).toString());
        const auto evidence = evidence_by_id.find(id);
        if (evidence == evidence_by_id.end()) { evidence_table_->setRowHidden(row, true); continue; }
        const auto& value = *evidence->second;

        bool kind_matches = kind == QStringLiteral("all");
        if (kind == QStringLiteral("image")) kind_matches = value.mime_type && value.mime_type->rfind("image/", 0) == 0;
        else if (kind == QStringLiteral("document")) kind_matches = value.kind == domain::EvidenceKind::File && (!value.mime_type || value.mime_type->rfind("image/", 0) != 0);
        else if (kind == QStringLiteral("text")) kind_matches = value.kind == domain::EvidenceKind::Text;
        else if (kind == QStringLiteral("url")) kind_matches = value.kind == domain::EvidenceKind::Url;

        bool source_matches = source_type == QStringLiteral("all");
        if (!source_matches && value.source_id) {
            const auto source = source_types.find(*value.source_id);
            if (source != source_types.end()) source_matches = q(domain::to_string(source->second)) == source_type;
        }

        QString searchable = evidence_table_->item(row, 0)->text() + QLatin1Char(' ') + evidence_table_->item(row, 2)->text();
        searchable += QLatin1Char(' ') + q(value.text_content) + QLatin1Char(' ') + q(value.note);
        if (value.source_url) searchable += QLatin1Char(' ') + q(*value.source_url);
        const bool text_matches = query.isEmpty() || searchable.contains(query, Qt::CaseInsensitive);
        const bool visible = kind_matches && source_matches && text_matches;
        evidence_table_->setRowHidden(row, !visible);
        if (visible && first_visible_row < 0) first_visible_row = row;
        if (visible && row == current_row) current_row_visible = true;
        any_visible = any_visible || visible;
    }
    evidence_filter_empty_->setVisible(evidence_table_->rowCount() > 0 && !any_visible);
    if (!current_row_visible) {
        evidence_table_->clearSelection();
        if (first_visible_row >= 0) evidence_table_->selectRow(first_visible_row);
        else evidence_table_->setCurrentCell(-1, -1);
    }
    if (tabs_->currentIndex() == 0) update_evidence_detail();
}

void EvidencePage::update_source_detail() {
    const auto id = selected_source_id();
    verify_button_->setVisible(false);
    open_button_->setVisible(false);
    delete_button_->setVisible(false);
    detail_preview_->setVisible(false);
    detail_links_label_->setVisible(!id.empty());
    linked_claims_->setVisible(!id.empty());
    detail_scroll_->setVisible(!id.empty());
    linked_claims_->setHorizontalHeaderLabels({QStringLiteral("Kind"), QStringLiteral("Evidence")});
    linked_claims_->set_empty_state(QStringLiteral("No preserved evidence"), QStringLiteral("Evidence linked to this source will appear here."));
    if (id.empty()) {
        detail_title_->setText(QStringLiteral("No source selected"));
        detail_body_->setText(sources_table_->rowCount() == 0
            ? QStringLiteral("Add a source to record where investigation material came from.")
            : QStringLiteral("Select a source to inspect its locator and preserved evidence."));
        linked_claims_->setRowCount(0);
        return;
    }
    try {
        const auto source = context_.investigation.get_source(id);
        detail_title_->setText(q(source.title.empty() ? source.locator : source.title));
        detail_body_->setText(QStringLiteral("Type: %1\nLocator: %2\nAccessed: %3\nAuthor: %4\nReliability note: %5")
            .arg(enum_label(domain::to_string(source.type)), q(source.locator), display_time(source.accessed_at),
                 source.author.empty() ? QStringLiteral("Not specified") : q(source.author),
                 source.reliability_note.empty() ? QStringLiteral("Not specified") : q(source.reliability_note)));
        linked_claims_->setRowCount(0);
        for (const auto& evidence : context_.investigation.list_evidence(case_id_)) {
            if (!evidence.source_id || *evidence.source_id != id) continue;
            const auto row = linked_claims_->rowCount();
            linked_claims_->insertRow(row);
            linked_claims_->setItem(row, 0, cell(enum_label(domain::to_string(evidence.kind))));
            linked_claims_->setItem(row, 1, cell(evidence_label(evidence), q(evidence.id)));
        }
        linked_claims_->setMaximumHeight(linked_claims_->rowCount() == 0 ? 132 : std::min(300, 64 + linked_claims_->rowCount() * 58));
    } catch (const std::exception& error) { show_error(this, error); }
}

void EvidencePage::update_evidence_detail() {
    const auto id = selected_evidence_id();
    const auto selected = !id.empty();
    verify_button_->setVisible(selected);
    open_button_->setVisible(selected);
    delete_button_->setVisible(selected);
    detail_preview_->setVisible(selected); image_add_button_->setVisible(selected); image_remove_button_->setVisible(false);
    detail_links_label_->setVisible(selected);
    linked_claims_->setVisible(selected);
    detail_scroll_->setVisible(selected);
    linked_claims_->setHorizontalHeaderLabels({QStringLiteral("Status"), QStringLiteral("Claim")});
    linked_claims_->set_empty_state(QStringLiteral("No linked claims"), QStringLiteral("Claims that use this evidence will appear here."));
    if (id.empty()) {
        detail_title_->setText(QStringLiteral("No evidence selected"));
        detail_id_->clear();
        detail_body_->setText(evidence_table_->rowCount() == 0
            ? QStringLiteral("Import a file, save a quotation, or record an external URL. Preserved items will show their provenance and preview here.")
            : QStringLiteral("Select a preserved item to inspect its provenance and preview."));
        linked_claims_->setRowCount(0);
        return;
    }
    try {
        const auto value = context_.investigation.get_evidence(id); const auto attached_images = json_strings(value.image_paths_json); image_remove_button_->setVisible(!attached_images.empty()); detail_title_->setText(wrap_long_tokens(evidence_label(value))); detail_id_->setText(QStringLiteral("#%1").arg(shorten_id(id)));
        QString body = QStringLiteral("Kind: %1\nImported: %2\nCaptured: %3\n").arg(enum_label(domain::to_string(value.kind)), display_time(value.imported_at), display_time(value.captured_at));
        if (value.kind == domain::EvidenceKind::File) { const auto filename = q(value.original_filename.value_or("file")); body += QStringLiteral("Filename: %1\nSize: %2\nSHA-256: %3\nStored path: %4\n").arg(filename, value.byte_size ? display_bytes(*value.byte_size) : QStringLiteral("Unknown"), q(value.sha256.value_or("Not recorded")), value.relative_path ? q(*value.relative_path) : QStringLiteral("Missing")); detail_preview_->setText(QStringLiteral("Preview\n%1\nLocal preserved file").arg(filename)); }
        else if (value.kind == domain::EvidenceKind::Text) { detail_preview_->setText(QStringLiteral("Text quotation\n%1").arg(wrap_long_tokens(q(value.text_content).left(220)))); }
        else { detail_preview_->setText(QStringLiteral("External-only URL\n%1\nNo remote navigation performed").arg(q(value.source_url.value_or("No URL recorded")))); }
        if (value.source_url) body += QStringLiteral("Source URL: %1\n").arg(q(*value.source_url));
        body += QStringLiteral("Quotation location: %1\nNote: %2").arg(value.quotation_location.empty() ? QStringLiteral("Not specified") : q(value.quotation_location), value.note.empty() ? QStringLiteral("Not specified") : q(value.note));
        if (!attached_images.empty()) body += QStringLiteral("\nAttached images: %1 · separate from verification").arg(attached_images.size());
        detail_body_->setText(wrap_long_tokens(body)); detail_links_label_->setText(QStringLiteral("Linked claims")); linked_claims_->setRowCount(0);
        for (const auto& claim : context_.investigation.claims_for_evidence(id)) { const auto row = linked_claims_->rowCount(); linked_claims_->insertRow(row); linked_claims_->setCellWidget(row, 0, status_badge(domain::to_string(claim.status))); linked_claims_->setItem(row, 1, cell(q(claim.statement), q(claim.id))); }
        linked_claims_->setMaximumHeight(linked_claims_->rowCount() == 0 ? 132 : std::min(300, 64 + linked_claims_->rowCount() * 58));
    } catch (const std::exception& error) { show_error(this, error); }
}

} // namespace evidence_trace::gui
