#include "gui/gui_helpers.hpp"

#include "import_export/json.hpp"

#include <QAbstractItemView>
#include <QDateTime>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QObject>
#include <QPainter>
#include <QPalette>
#include <QPaintEvent>
#include <QSettings>
#include <QTextLayout>
#include <QTextOption>
#include <QTimer>

#include <algorithm>
#include <array>
#include <cmath>

namespace evidence_trace::gui {

namespace {

class ResizeObserver final : public QObject {
public:
    ResizeObserver(QWidget* target, std::function<void(int)> callback)
        : QObject(target), callback_(std::move(callback)) {}

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::Resize && callback_) callback_(static_cast<QWidget*>(watched)->width());
        return QObject::eventFilter(watched, event);
    }

private:
    std::function<void(int)> callback_;
};

class ArrowComboBox final : public QComboBox {
public:
    using QComboBox::QComboBox;

protected:
    void paintEvent(QPaintEvent* event) override {
        QComboBox::paintEvent(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const auto color = isEnabled() ? QColor("#9fb0c8") : QColor("#617087");
        painter.setPen(QPen(color, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        const auto center_x = width() - 11.0;
        const auto center_y = height() / 2.0 - 1.0;
        painter.drawLine(QPointF(center_x - 4.0, center_y - 2.0), QPointF(center_x, center_y + 2.0));
        painter.drawLine(QPointF(center_x, center_y + 2.0), QPointF(center_x + 4.0, center_y - 2.0));
    }
};

}

namespace {

QStringList visual_text_lines(const QString& text, const QFont& font, int width) {
    const auto inner_width = std::max(1, width);
    QStringList result;
    const auto logical_lines = text.split(QChar('\n'), Qt::KeepEmptyParts);
    for (const auto& logical_line : logical_lines) {
        if (logical_line.isEmpty()) {
            result << QString();
            continue;
        }

        QTextLayout layout(logical_line, font);
        QTextOption option;
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        layout.setTextOption(option);
        layout.beginLayout();
        bool created_line = false;
        while (true) {
            auto line = layout.createLine();
            if (!line.isValid()) break;
            line.setLineWidth(inner_width);
            result << logical_line.mid(line.textStart(), line.textLength());
            created_line = true;
        }
        layout.endLayout();
        if (!created_line) result << logical_line;
    }
    if (result.isEmpty()) result << QString();
    return result;
}

QString collapse_text_lines(const QString& text, const QFont& font, int width, int max_lines) {
    if (text.isEmpty()) return {};
    const auto lines = visual_text_lines(text, font, width);
    const auto visible_lines = std::min(max_lines, static_cast<int>(lines.size()));
    QString result;
    for (int index = 0; index < visible_lines; ++index) {
        if (index > 0) result += QChar('\n');
        result += lines[static_cast<std::size_t>(index)];
    }
    if (lines.size() > max_lines) {
        const auto last_break = result.lastIndexOf(QChar('\n'));
        const auto prefix = last_break < 0 ? QString() : result.left(last_break + 1);
        const auto last_line = last_break < 0 ? result : result.mid(last_break + 1);
        const QFontMetrics metrics(font);
        const auto ellipsis = QStringLiteral("...");
        const auto remaining_width = std::max(1, width - metrics.horizontalAdvance(ellipsis));
        result = prefix + metrics.elidedText(last_line, Qt::ElideRight, remaining_width) + ellipsis;
    }
    return result;
}

}

QIcon ui_icon(UiIcon kind, const QColor& color) {
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    const auto line = [&painter](qreal x1, qreal y1, qreal x2, qreal y2) {
        painter.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    };
    switch (kind) {
    case UiIcon::Add: line(12, 5, 12, 19); line(5, 12, 19, 12); break;
    case UiIcon::Archive:
        painter.drawRoundedRect(QRectF(4, 7, 16, 13), 1.8, 1.8); painter.drawRect(QRectF(3, 4, 18, 4)); line(9, 13, 15, 13); break;
    case UiIcon::ArrowDown: line(12, 4, 12, 18); line(7, 13, 12, 18); line(17, 13, 12, 18); break;
    case UiIcon::ArrowUp: line(12, 20, 12, 6); line(7, 11, 12, 6); line(17, 11, 12, 6); break;
    case UiIcon::Book:
        painter.drawRoundedRect(QRectF(4, 3.5, 16, 17), 1.5, 1.5); line(12, 4, 12, 20); line(6.5, 8, 10, 8); line(14, 8, 17.5, 8); break;
    case UiIcon::Calendar:
        painter.drawRoundedRect(QRectF(4, 5, 16, 16), 2, 2); line(4, 9, 20, 9); line(8, 2.5, 8, 7); line(16, 2.5, 16, 7); painter.drawEllipse(QRectF(8, 12, 2, 2)); painter.drawEllipse(QRectF(13, 12, 2, 2)); painter.drawEllipse(QRectF(8, 16, 2, 2)); painter.drawEllipse(QRectF(13, 16, 2, 2)); break;
    case UiIcon::Check: painter.drawEllipse(QRectF(3.5, 3.5, 17, 17)); line(7.5, 12, 10.5, 15); line(10.5, 15, 16.8, 8.5); break;
    case UiIcon::ChevronDown: line(7, 9, 12, 14); line(12, 14, 17, 9); break;
    case UiIcon::ChevronRight: line(9, 5, 15, 12); line(15, 12, 9, 19); break;
    case UiIcon::Claim:
        painter.drawEllipse(QRectF(3, 8.5, 5, 5)); painter.drawEllipse(QRectF(16, 3.5, 5, 5)); painter.drawEllipse(QRectF(16, 15.5, 5, 5)); line(8, 10, 16, 6); line(8, 12, 16, 18); break;
    case UiIcon::Clipboard:
        painter.drawRoundedRect(QRectF(5, 4, 14, 17), 1.8, 1.8); painter.drawRoundedRect(QRectF(9, 2, 6, 4), 1, 1); line(8, 10, 16, 10); line(8, 14, 16, 14); line(8, 18, 13, 18); break;
    case UiIcon::Document:
        painter.drawRoundedRect(QRectF(5, 3, 12, 18), 1.5, 1.5); line(8, 8, 14, 8); line(8, 12, 15, 12); line(8, 16, 13, 16); line(14, 3, 19, 8); break;
    case UiIcon::Download: line(12, 3, 12, 15); line(7, 11, 12, 16); line(17, 11, 12, 16); line(5, 20, 19, 20); break;
    case UiIcon::Edit: painter.drawLine(QPointF(5, 18), QPointF(6, 14)); painter.drawLine(QPointF(6, 14), QPointF(16, 4)); painter.drawLine(QPointF(16, 4), QPointF(20, 8)); painter.drawLine(QPointF(20, 8), QPointF(10, 18)); line(5, 18, 10, 18); break;
    case UiIcon::Entity:
        painter.drawEllipse(QRectF(8, 3.5, 8, 8)); painter.drawArc(QRectF(4, 12, 16, 9), 0, 180 * 16); break;
    case UiIcon::Evidence:
        painter.drawRoundedRect(QRectF(5, 3.5, 14, 17), 1.5, 1.5); line(9, 8, 16, 8); line(9, 12, 16, 12); line(9, 16, 14, 16); break;
    case UiIcon::Export: line(12, 20, 12, 5); line(7, 9, 12, 4); line(17, 9, 12, 4); painter.drawRect(QRectF(5, 13, 5, 6)); painter.drawRect(QRectF(14, 13, 5, 6)); break;
    case UiIcon::Eye: painter.drawEllipse(QRectF(3, 7, 18, 10)); painter.drawEllipse(QRectF(10, 10, 4, 4)); break;
    case UiIcon::File: painter.drawRoundedRect(QRectF(5, 3, 12, 18), 1.5, 1.5); line(14, 3, 19, 8); line(14, 3, 14, 8); line(14, 8, 19, 8); break;
    case UiIcon::Filter:
        line(4, 6, 20, 6); line(7, 12, 17, 12); line(10, 18, 14, 18); break;
    case UiIcon::Folder:
        painter.drawRoundedRect(QRectF(3.5, 6.5, 17, 13), 2, 2); line(5, 6.5, 8, 3.8); line(8, 3.8, 13, 3.8); line(13, 3.8, 15, 6.5); break;
    case UiIcon::FolderAdd:
        painter.drawRoundedRect(QRectF(3.5, 7, 14.5, 12.5), 2, 2); line(5, 7, 8, 4.3); line(8, 4.3, 13, 4.3); line(13, 4.3, 15, 7); line(19, 13, 19, 21); line(15, 17, 23, 17); break;
    case UiIcon::FolderOpen:
        painter.drawPolyline(QPolygonF{QPointF(3.5, 8), QPointF(7, 5), QPointF(13, 5), QPointF(15, 8), QPointF(21, 8), QPointF(18, 19), QPointF(4, 19), QPointF(3.5, 8)}); break;
    case UiIcon::Globe: painter.drawEllipse(QRectF(3.5, 3.5, 17, 17)); painter.drawEllipse(QRectF(8, 3.5, 8, 17)); line(4, 12, 20, 12); break;
    case UiIcon::Grid:
        painter.drawRoundedRect(QRectF(4, 4, 6, 6), 1, 1); painter.drawRoundedRect(QRectF(14, 4, 6, 6), 1, 1); painter.drawRoundedRect(QRectF(4, 14, 6, 6), 1, 1); painter.drawRoundedRect(QRectF(14, 14, 6, 6), 1, 1); break;
    case UiIcon::Graph:
        painter.drawEllipse(QRectF(3.5, 9, 5, 5)); painter.drawEllipse(QRectF(15.5, 3.5, 5, 5)); painter.drawEllipse(QRectF(15.5, 15.5, 5, 5)); line(8, 10, 15.5, 6); line(8, 13, 15.5, 18); break;
    case UiIcon::Image:
        painter.drawRoundedRect(QRectF(3.5, 5, 17, 14), 1.5, 1.5); painter.drawEllipse(QRectF(7, 8, 2.5, 2.5)); painter.drawPolyline(QPolygonF{QPointF(5, 17), QPointF(10, 12), QPointF(13, 15), QPointF(16, 11), QPointF(20, 17)}); break;
    case UiIcon::Info: painter.drawEllipse(QRectF(4, 4, 16, 16)); line(12, 10, 12, 16); painter.drawPoint(QPointF(12, 7)); break;
    case UiIcon::Link: painter.drawArc(QRectF(3, 7, 11, 8), 45 * 16, 220 * 16); painter.drawArc(QRectF(10, 9, 11, 8), 225 * 16, 220 * 16); line(9, 12, 15, 12); break;
    case UiIcon::List:
        for (const auto y : {6.0, 12.0, 18.0}) { painter.drawEllipse(QRectF(3.5, y - 1.2, 2.4, 2.4)); line(9, y, 20, y); } break;
    case UiIcon::More: painter.setBrush(color); painter.setPen(Qt::NoPen); painter.drawEllipse(QRectF(4, 10, 3, 3)); painter.drawEllipse(QRectF(10.5, 10, 3, 3)); painter.drawEllipse(QRectF(17, 10, 3, 3)); break;
    case UiIcon::Note: painter.drawRoundedRect(QRectF(4, 4, 16, 16), 2, 2); line(8, 9, 16, 9); line(8, 13, 16, 13); line(8, 17, 13, 17); break;
    case UiIcon::Organization:
        painter.drawRoundedRect(QRectF(8, 3, 8, 5), 1, 1); line(12, 8, 12, 11); line(6, 11, 18, 11); line(6, 11, 6, 15); line(18, 11, 18, 15); painter.drawRoundedRect(QRectF(3, 15, 6, 5), 1, 1); painter.drawRoundedRect(QRectF(9, 15, 6, 5), 1, 1); painter.drawRoundedRect(QRectF(15, 15, 6, 5), 1, 1); break;
    case UiIcon::Person:
        painter.setPen(Qt::NoPen); painter.setBrush(color);
        painter.drawEllipse(QRectF(9, 3.5, 6, 6));
        painter.drawRoundedRect(QRectF(5, 11, 14, 9), 4, 4);
        break;
    case UiIcon::Play: painter.setBrush(color); painter.setPen(Qt::NoPen); painter.drawPolygon(QPolygonF{QPointF(8, 4), QPointF(19, 12), QPointF(8, 20)}); break;
    case UiIcon::Restore: painter.drawArc(QRectF(4, 5, 15, 15), 45 * 16, 285 * 16); line(4, 5, 4, 11); line(4, 5, 10, 5); break;
    case UiIcon::Search: painter.drawEllipse(QRectF(4, 4, 11, 11)); line(13.5, 13.5, 20, 20); break;
    case UiIcon::Server:
        painter.setPen(QPen(color, 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); painter.setBrush(color);
        painter.drawRoundedRect(QRectF(4, 4, 16, 7), 1.5, 1.5);
        painter.drawRoundedRect(QRectF(4, 13, 16, 7), 1.5, 1.5);
        painter.setPen(QPen(QColor("#111b29"), 1.1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QRectF(7, 6.5, 2, 2)); painter.drawEllipse(QRectF(7, 15.5, 2, 2));
        line(12, 7.5, 17, 7.5); line(12, 16.5, 17, 16.5);
        break;
    case UiIcon::Settings:
        painter.drawEllipse(QRectF(8, 8, 8, 8)); for (int i = 0; i < 8; ++i) { const auto angle = i * 45.0 * 3.1415926535 / 180.0; line(12 + std::cos(angle) * 6.2, 12 + std::sin(angle) * 6.2, 12 + std::cos(angle) * 8.5, 12 + std::sin(angle) * 8.5); } break;
    case UiIcon::Shield: painter.drawPolygon(QPolygonF{QPointF(12, 3), QPointF(19, 6), QPointF(18, 14), QPointF(12, 21), QPointF(6, 14), QPointF(5, 6)}); break;
    case UiIcon::Source: painter.drawEllipse(QRectF(4, 6, 7, 7)); painter.drawEllipse(QRectF(13, 6, 7, 7)); line(10, 9.5, 14, 9.5); line(6, 13, 6, 18); line(18, 13, 18, 18); line(6, 18, 18, 18); break;
    case UiIcon::Target:
        painter.drawEllipse(QRectF(5, 5, 14, 14)); painter.drawEllipse(QRectF(9, 9, 6, 6)); line(12, 2.5, 12, 6); line(12, 18, 12, 21.5); line(2.5, 12, 6, 12); line(18, 12, 21.5, 12); break;
    case UiIcon::Activity:
        painter.drawPolyline(QPolygonF{QPointF(2.5, 13), QPointF(6, 13), QPointF(8.2, 7), QPointF(11.2, 18), QPointF(14.4, 9), QPointF(16.5, 13), QPointF(21.5, 13)}); break;
    case UiIcon::Trash: painter.drawRoundedRect(QRectF(6, 7, 12, 14), 1, 1); line(4, 6, 20, 6); line(9, 3.5, 15, 3.5); line(10, 10, 10, 18); line(14, 10, 14, 18); break;
    case UiIcon::Upload: line(12, 20, 12, 7); line(7, 11, 12, 6); line(17, 11, 12, 6); line(5, 4, 19, 4); break;
    case UiIcon::Warning: painter.drawPolygon(QPolygonF{QPointF(12, 3), QPointF(21, 20), QPointF(3, 20)}); line(12, 8, 12, 14); painter.drawPoint(QPointF(12, 17)); break;
    }
    return QIcon(pixmap);
}

QLabel* make_tag_chip(const QString& value, QWidget* parent, int maximum_width) {
    auto* chip = new QLabel(parent);
    chip->setProperty("tagChip", true);
    const auto key = value.toLower();
    const auto tone = key.contains(QStringLiteral("person")) || key.contains(QStringLiteral("identity"))
        ? QStringLiteral("teal")
        : key.contains(QStringLiteral("domain")) || key.contains(QStringLiteral("infrastructure"))
            ? QStringLiteral("blue")
            : key.contains(QStringLiteral("osint")) || key.contains(QStringLiteral("investigation"))
                ? QStringLiteral("purple") : QStringLiteral("neutral");
    chip->setProperty("tagTone", tone);
    chip->setText(QFontMetrics(chip->font()).elidedText(value, Qt::ElideRight, std::max(38, maximum_width - 16)));
    chip->setToolTip(value);
    chip->setAccessibleName(value);
    chip->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    chip->setMaximumWidth(maximum_width);
    chip->setFixedHeight(22);
    chip->style()->unpolish(chip);
    chip->style()->polish(chip);
    return chip;
}

QWidget* tag_chips(const QStringList& values, QWidget* parent, int visible_limit, int maximum_chip_width) {
    auto* container = new QWidget(parent);
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 2, 0, 2);
    layout->setSpacing(4);
    if (values.isEmpty()) {
        auto* empty = new QLabel(QStringLiteral("No tags"), container);
        empty->setObjectName("subtle");
        layout->addWidget(empty);
    } else {
        const int count = std::min(std::max(0, visible_limit), static_cast<int>(values.size()));
        for (int index = 0; index < count; ++index) {
            layout->addWidget(make_tag_chip(values[index], container, maximum_chip_width));
        }
        if (count < values.size()) {
            auto* more = make_tag_chip(QStringLiteral("+%1").arg(values.size() - count), container, 52);
            more->setToolTip(values.mid(count).join(QStringLiteral(" · ")));
            more->setAccessibleName(more->toolTip());
            layout->addWidget(more);
        }
    }
    layout->addStretch();
    container->setToolTip(values.isEmpty() ? QStringLiteral("No tags") : values.join(QStringLiteral(" · ")));
    container->setAccessibleDescription(container->toolTip());
    return container;
}

ResponsiveSplitter::ResponsiveSplitter(Qt::Orientation orientation, QWidget* parent, int breakpoint)
    : QSplitter(orientation, parent), wide_orientation_(orientation), breakpoint_(breakpoint) {}

void ResponsiveSplitter::resizeEvent(QResizeEvent* event) {
    if (wide_orientation_ == Qt::Horizontal && count() > 1) {
        const auto narrow = width() < breakpoint_;
        if (narrow && orientation() != Qt::Vertical) {
            setOrientation(Qt::Vertical);
            const auto total = std::max(1, height() - (count() - 1) * handleWidth());
            QList<int> sizes(count(), std::max(1, total / count()));
            const auto first = std::max(1, total * 2 / 5);
            sizes[0] = first;
            const auto remainder = std::max(1, total - first);
            for (int index = 1; index < count(); ++index) sizes[index] = std::max(1, remainder / (count() - 1));
            setSizes(sizes);
        } else if (!narrow && orientation() != Qt::Horizontal) {
            setOrientation(Qt::Horizontal);
            const auto total = std::max(1, width() - (count() - 1) * handleWidth());
            QList<int> sizes(count(), std::max(1, total / count()));
            if (count() == 2) {
                sizes[0] = std::max(1, total * 2 / 3);
                sizes[1] = std::max(1, total - sizes[0]);
            } else if (count() == 3) {
                sizes[0] = std::max(1, total / 4);
                sizes[1] = std::max(1, total / 2);
                sizes[2] = std::max(1, total - sizes[0] - sizes[1]);
            }
            setSizes(sizes);
        }
    }
    QSplitter::resizeEvent(event);
}

void apply_theme(QApplication& application) {
    application.setStyle("Fusion");
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#080d15"));
    palette.setColor(QPalette::WindowText, QColor("#eef1fb"));
    palette.setColor(QPalette::Base, QColor("#0b131e"));
    palette.setColor(QPalette::AlternateBase, QColor("#101a28"));
    palette.setColor(QPalette::Text, QColor("#eef1fb"));
    palette.setColor(QPalette::Button, QColor("#111c2b"));
    palette.setColor(QPalette::ButtonText, QColor("#eef1fb"));
    palette.setColor(QPalette::Highlight, QColor("#6d3fe1"));
    palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    palette.setColor(QPalette::ToolTipBase, QColor("#172235"));
    palette.setColor(QPalette::ToolTipText, QColor("#eef1f8"));
    application.setPalette(palette);
    application.setFont(QFont("DejaVu Sans", 10));
    application.setStyleSheet(R"CSS(
        QWidget { color: #eef1fb; font-size: 9.5pt; }
        QMainWindow, QDialog { background: #080d15; }
        QDialog#modalDialog { background: #0d1725; border: 1px solid #263852; border-radius: 9px; }
        QFrame#entityHero { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #211944, stop:0.55 #151b31, stop:1 #101a29); border: 1px solid #57408a; border-radius: 10px; }
        QLabel#entityHeroIcon { background: #30215c; border: 1px solid #795add; border-radius: 10px; }
        QLabel#entityDialogTitle { color: #f3efff; }
        QFrame#entityIdentityPanel, QFrame#entityMetadataPanel { background: #0c1521; border: 1px solid #24344a; border-radius: 9px; }
        QFrame#entityIdentityPanel { border-top: 2px solid #7552d4; }
        QFrame#entityMetadataPanel { border-top: 2px solid #287d88; }
        QLabel#entitySectionLabel { color: #bda8ff; font-size: 8pt; font-weight: 700; letter-spacing: 1px; }
        QLabel#entityFieldLabel { color: #d9e0ef; font-size: 8.5pt; font-weight: 600; }
        QLabel#entityFieldHelp { color: #8192aa; font-size: 7.5pt; }
        QDialogButtonBox { border-top: 1px solid #263852; padding-top: 12px; }
        QDialogButtonBox QPushButton { min-width: 92px; }
        QFrame#sidebar { background: #090f17; border-right: 1px solid #1b2839; }
        QFrame#topbar { background: #0a111b; border-bottom: 1px solid #1b2839; }
        QFrame#card { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #101a29, stop:0.32 #0d1622, stop:1 #0c1420); border: 1px solid #26364c; border-radius: 7px; }
        QLabel#appTitle { color: #f0f2fb; font-size: 11pt; font-weight: 700; }
        QLabel#sectionTitle { color: #f1f3fb; font-size: 18pt; font-weight: 700; }
        QLabel#caseTitle { color: #f1f3fb; font-size: 19pt; font-weight: 700; }
        QLabel#caseMeta { color: #aab7ce; font-size: 9pt; }
        QLabel#subtle { color: #9baac2; }
        QLabel#eyebrow { color: #b69aff; font-size: 8pt; font-weight: 700; letter-spacing: 1px; }
        QLabel[tagChip="true"] { color: #cdbaff; background: #21164b; border: 1px solid #6540b5; border-radius: 6px; padding: 1px 7px; font-size: 7.5pt; }
        QLabel[tagChip="true"][tagTone="blue"] { color: #a8ceff; background: #122540; border-color: #285387; }
        QLabel[tagChip="true"][tagTone="teal"] { color: #a0e9df; background: #102c2b; border-color: #276c68; }
        QLabel[tagChip="true"][tagTone="neutral"] { color: #c3ccdd; background: #172233; border-color: #34445b; }
        QPushButton { min-height: 32px; background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #17152a, stop:1 #0e1220); border: 1px solid #704bd2; border-radius: 9px; padding: 5px 12px; color: #e5deff; font-size: 8.5pt; }
        QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #7544ed, stop:0.52 #5b2bcf, stop:1 #421b9f); border-color: #b095ff; color: #ffffff; }
        QPushButton:pressed { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #4c208f, stop:1 #2b145d); border-color: #c3adff; }
        QPushButton:disabled { color: #68718a; background: #10131e; border-color: #30294a; }
        QPushButton[nav="true"] { height: 40px; min-height: 38px; max-height: 42px; text-align: left; border: 1px solid transparent; background: transparent; padding: 0 10px; color: #b8c4d8; font-size: 9pt; }
        QPushButton[nav="true"]:hover { background: #121d2d; color: #f0ecff; border-color: #312b55; }
        QPushButton[navActive="true"] { background: #1d173a; border-color: transparent; border-left: 2px solid #7545e4; color: #d7c8ff; }
        QPushButton[nav="true"][caseTab="true"] { height: 54px; min-height: 54px; max-height: 54px; background: transparent; border: 0; border-bottom: 2px solid transparent; border-radius: 0; padding: 0 12px; color: #b8c4d8; font-size: 9pt; }
        QPushButton[nav="true"][caseTab="true"]:hover { background: #101725; border: 0; border-bottom: 2px solid #55427f; color: #e9e2ff; }
        QPushButton[nav="true"][caseTab="true"]:pressed { background: #17142a; color: #eee8ff; }
        QPushButton[nav="true"][caseTab="true"][navActive="true"] { background: transparent; border: 0; border-bottom: 2px solid #a982ff; color: #d0b9ff; font-weight: 600; }
        QPushButton[nav="true"][caseTab="true"][navActive="true"]:hover { background: #111523; border: 0; border-bottom: 2px solid #c0a7ff; color: #eee5ff; }
        QScrollArea#caseNavigationScroll { background: #090e17; border: 0; border-bottom: 1px solid #202c3d; }
        QPushButton[caseFilter="true"] { min-height: 34px; background: #0e1724; border: 1px solid #263852; border-radius: 7px; padding: 5px 10px; color: #b8c4d8; }
        QPushButton[caseFilter="true"][filterActive="true"] { background: #21174a; border-color: #936cff; color: #d7c8ff; }
        QPushButton[nav="true"]::menu-indicator { image: none; }
        QPushButton[breadcrumb="true"] { height: 16px; min-height: 16px; max-height: 16px; background: transparent; border: 0; padding: 0; color: #9baac2; }
        QPushButton[breadcrumb="true"]:hover { color: #d0bfff; background: transparent; border: 0; }
        QLineEdit, QTextEdit, QPlainTextEdit, QComboBox, QSpinBox { min-height: 20px; background: #101a28; border: 1px solid #263b54; border-radius: 5px; padding: 2px 8px; selection-background-color: #6d3fe1; }
        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus, QComboBox:focus { border-color: #a184ff; background: #151f34; }
        QComboBox::drop-down { border: 0; background: #101a28; width: 20px; border-top-right-radius: 5px; border-bottom-right-radius: 5px; }
        QComboBox QAbstractItemView { background: #121e30; border: 1px solid #3e5070; selection-background-color: #5d38bd; padding: 4px; }
        QComboBox[segmented="true"] { background: #0e1724; border-color: #394b67; padding-left: 12px; }
        QComboBox[segmented="true"]:focus { border-color: #936cff; }
        QTableWidget { background: #09121b; alternate-background-color: #0c1722; border: 1px solid #1c2d40; border-radius: 6px; gridline-color: transparent; selection-background-color: #241650; selection-color: #ffffff; outline: 0; }
        QTableWidget::item { padding: 4px 5px; }
        QTableWidget::item:selected { border-top: 1px solid #7142d8; border-bottom: 1px solid #7142d8; }
        QTableWidget#claimsTable { background: #0b1015; alternate-background-color: #0b1015; selection-background-color: #151630; }
        QTableWidget#claimsTable::item:selected { border-top: 1px solid #333273; border-bottom: 1px solid #333273; }
        QHeaderView::section { background: #0e1926; color: #8f9eb4; border: 0; border-bottom: 1px solid #1d3045; padding: 5px 5px; font-size: 8pt; font-weight: 600; }
        QTabWidget::pane { border: 1px solid #263852; border-radius: 9px; background: #0b141f; }
        QTabBar::tab { background: transparent; border: 0; border-bottom: 2px solid transparent; padding: 9px 14px; color: #9baac2; min-height: 34px; }
        QTabBar::tab:hover { color: #e0d8ff; }
        QTabBar::tab:selected { color: #d0bfff; border-bottom-color: #936cff; }
        QScrollArea { border: 0; background: transparent; }
        QGroupBox { border: 1px solid #263852; border-radius: 9px; margin-top: 14px; padding: 13px; }
        QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 5px; color: #b9a6f4; font-weight: 700; }
        QSplitter::handle { background: #202e42; }
        QSplitter::handle:hover { background: #7158c6; }
        QStatusBar { background: #0b121c; color: #9aa7bd; border-top: 1px solid #202e42; }
        QToolTip { background: #182438; color: #eef1f8; border: 1px solid #6c54b2; padding: 7px; }
        QCheckBox { spacing: 9px; }
        QCheckBox::indicator { width: 17px; height: 17px; }
        QCheckBox::indicator:unchecked { border: 1px solid #52617a; background: #111a27; border-radius: 4px; }
        QCheckBox::indicator:checked { border: 1px solid #9070ee; background: #5d3bcb; border-radius: 4px; }
        QScrollBar:vertical { background: #0a111a; width: 7px; margin: 1px; }
        QScrollBar::handle:vertical { background: #33425b; min-height: 24px; border-radius: 3px; }
        QScrollBar::handle:vertical:hover { background: #6750b3; }
        QScrollBar:horizontal { background: #0c121b; height: 12px; margin: 2px; }
        QScrollBar::handle:horizontal { background: #344158; min-width: 36px; border-radius: 6px; }
        QScrollBar::add-line, QScrollBar::sub-line { background: transparent; border: 0; }
        QFrame#caseHeaderChip { background: #0f1927; border: 1px solid #1b2c40; border-radius: 5px; }
        QLabel#caseChipText { color: #c5cde0; font-size: 8pt; }
        QFrame#caseStatusChip { background: #24164e; border: 1px solid #7544df; border-radius: 5px; }
        QLabel#caseStatusText { color: #d9c9ff; font-size: 8.5pt; }
        QLabel#caseBreadcrumb { color: #d7ddeb; font-size: 9pt; }
        QLabel#caseDescription { color: #8998b0; font-size: 9pt; }
        QLabel#caseUpdated { color: #8998b0; font-size: 8pt; }
        QLineEdit#globalSearch { background: #101927; border: 1px solid #263b54; border-radius: 5px; padding: 4px 48px 4px 31px; color: #ecf0fb; font-size: 8pt; }
        QLabel#searchShortcut { color: #71819a; font-size: 7.5pt; }
        QWidget#caseWorkspace { background: #080d15; }
        QLabel#claimsToolbarTitle { color: #e7eaf4; font-size: 8pt; font-weight: 700; }
        QLabel#claimsCount, QLabel#inspectorCount { color: #d7c9ff; background: #272054; border-radius: 4px; font-size: 7pt; font-weight: 700; }
        QPushButton#inspectorMore:hover { background: #18253a; border-color: #5a43a2; }
        QScrollArea#claimInspectorScroll { background: #0d1622; border-left: 1px solid #1c2d40; }
        QFrame#claimInspector { background: #0d1622; border: 0; }
        QLabel#inspectorTitle { color: #eff1f8; font-size: 9pt; font-weight: 700; }
        QLabel#claimId { color: #8e9cb4; font-size: 8pt; }
        QLabel#claimInspectorStatement { color: #ecedf6; font-size: 9pt; font-weight: 600; }
        QPushButton#inspectorMore { min-height: 22px; padding: 0; background: transparent; border: 0; }
        QLabel#fieldCaption { color: #aab5c8; font-size: 7.5pt; }
        QFrame#claimRelation { background: transparent; }
        QLabel#relationEntity { color: #d9deeb; font-size: 8pt; }
        QLabel#relationType { color: #8392aa; font-size: 7pt; }
        QLabel#inspectorSectionTitle { color: #e3e7f1; font-size: 8pt; font-weight: 600; }
        QLabel#inspectorBody { color: #aab6c9; font-size: 7pt; line-height: 1.25; }
        QFrame#inspectorEvidenceCard { background: #101a28; border: 1px solid #1f3045; border-radius: 5px; }
        QLabel#evidenceCardTitle { color: #e8ebf3; font-size: 7pt; }
        QLabel#evidenceCardSub { color: #8291a8; font-size: 6.8pt; }
        QLabel#supportsBadge { color: #76e7c1; background: #11372f; border: 1px solid #267d68; border-radius: 4px; padding: 2px 5px; font-size: 6.8pt; }
        QLabel#contradictsBadge { color: #ff7b85; background: #401a25; border: 1px solid #8b3340; border-radius: 4px; padding: 2px 5px; font-size: 6.8pt; }
        QLabel#addedTimestamp { color: #8795aa; font-size: 7.5pt; }
    )CSS");
}

bool navigation_glow_enabled() {
    return QSettings().value(QStringLiteral("appearance/navigationGlow"), false).toBool();
}

void set_navigation_glow_enabled(bool enabled) {
    QSettings settings;
    settings.setValue(QStringLiteral("appearance/navigationGlow"), enabled);
    settings.sync();
}

StateTableWidget::StateTableWidget(QWidget* parent) : QTableWidget(parent) {}

void StateTableWidget::set_empty_state(const QString& title, const QString& description) {
    empty_title_ = title;
    empty_description_ = description;
    viewport()->update();
}

void StateTableWidget::paintEvent(QPaintEvent* event) {
    QTableWidget::paintEvent(event);
    if (rowCount() != 0 || empty_title_.isEmpty()) return;
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);
    const auto area = viewport()->rect().adjusted(24, 16, -24, -16);
    auto title_font = painter.font();
    title_font.setPointSize(13);
    title_font.setWeight(QFont::DemiBold);
    const QFontMetrics title_metrics(title_font);
    const auto title_bounds = title_metrics.boundingRect(QRect(0, 0, area.width(), QWIDGETSIZE_MAX), Qt::TextWordWrap, empty_title_);
    const int title_height = std::max(title_metrics.lineSpacing(), title_bounds.height());
    painter.setFont(title_font);
    painter.setPen(QColor("#e0e5f0"));
    auto body_font = painter.font();
    body_font.setPointSize(9);
    body_font.setWeight(QFont::Normal);
    const QFontMetrics body_metrics(body_font);
    const auto body_bounds = body_metrics.boundingRect(QRect(0, 0, area.width(), QWIDGETSIZE_MAX), Qt::TextWordWrap, empty_description_);
    const int body_height = empty_description_.isEmpty() ? 0 : std::max(body_metrics.lineSpacing(), body_bounds.height());
    const int gap = body_height == 0 ? 0 : 7;
    const int group_height = title_height + gap + body_height;
    const int top = area.top() + std::max(0, (area.height() - group_height) / 2);
    const QRect title_rect(area.left(), top, area.width(), title_height);
    painter.drawText(title_rect, Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextWordWrap, empty_title_);
    if (body_height == 0) return;
    painter.setFont(body_font);
    painter.setPen(QColor("#8998b0"));
    const QRect body_rect(area.left(), top + title_height + gap, area.width(), body_height);
    painter.drawText(body_rect, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, empty_description_);
}

ExpandableText::ExpandableText(QWidget* parent) : QLabel(parent) {
    setTextFormat(Qt::PlainText);
    setWordWrap(true);
    setContentsMargins(0, 0, 0, 0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setCursor(Qt::PointingHandCursor);
}

QStringList ExpandableText::visualLines(int width) const {
    const auto margins = contentsMargins();
    const auto inner_width = std::max(1, width - margins.left() - margins.right());
    return visual_text_lines(full_text_, font(), inner_width);
}

QString ExpandableText::collapsedText(int width) const {
    const auto margins = contentsMargins();
    const auto inner_width = std::max(1, width - margins.left() - margins.right());
    if (!show_ellipsis_) {
        const auto lines = visualLines(width);
        const auto visible_lines = std::min(preview_line_limit_, static_cast<int>(lines.size()));
        QString result;
        for (int index = 0; index < visible_lines; ++index) {
            if (index > 0) result += QChar('\n');
            result += lines[static_cast<std::size_t>(index)];
        }
        return result;
    }
    return collapse_text_lines(full_text_, font(), inner_width, preview_line_limit_);
}

int ExpandableText::contentHeightForWidth(int width) const {
    const auto safe_width = width > 0 ? width : 320;
    const auto line_count = expanded_ ? visualLines(safe_width).size() : std::min(preview_line_limit_, static_cast<int>(visualLines(safe_width).size()));
    const auto margins = contentsMargins();
    return static_cast<int>(std::max<std::size_t>(1, line_count)) * QFontMetrics(font()).lineSpacing() + margins.top() + margins.bottom() + 2;
}

int ExpandableText::heightForWidth(int width) const { return contentHeightForWidth(width); }

QString two_line_text_preview(const QString& value, const QFont& font, int available_width) {
    return collapse_text_lines(value, font, std::max(1, available_width), 2);
}

void ExpandableText::refreshDisplay() {
    const auto width = this->width() > 0 ? this->width() : 320;
    QLabel::setText(expanded_ ? full_text_ : collapsedText(width));
    setMaximumHeight(expanded_ ? QWIDGETSIZE_MAX : contentHeightForWidth(width));
    setToolTip(full_text_);
    updateGeometry();
    update();
}

void ExpandableText::setFullText(const QString& text) {
    full_text_ = text;
    expanded_ = false;
    refreshDisplay();
}

void ExpandableText::setExpanded(bool expanded) {
    if (expanded_ == expanded) return;
    expanded_ = expanded;
    refreshDisplay();
}

void ExpandableText::setShowEllipsis(bool show_ellipsis) {
    if (show_ellipsis_ == show_ellipsis) return;
    show_ellipsis_ = show_ellipsis;
    refreshDisplay();
}

void ExpandableText::setPreviewLineLimit(int lines) {
    const int safe_lines = std::clamp(lines, 1, 6);
    if (preview_line_limit_ == safe_lines) return;
    preview_line_limit_ = safe_lines;
    refreshDisplay();
}

void ExpandableText::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && !full_text_.isEmpty()) {
        setExpanded(!expanded_);
        if (on_toggled) on_toggled(expanded_);
        event->accept();
        return;
    }
    QLabel::mousePressEvent(event);
}

void ExpandableText::resizeEvent(QResizeEvent* event) {
    QLabel::resizeEvent(event);
    refreshDisplay();
}

QString q(const std::string& value) { return QString::fromUtf8(value.c_str()); }
std::string s(const QString& value) { return value.toUtf8().toStdString(); }

QString display_time(const std::string& utc_iso) {
    if (utc_iso.empty()) return QStringLiteral("Not specified");
    auto parsed = QDateTime::fromString(q(utc_iso), Qt::ISODateWithMs);
    if (!parsed.isValid()) parsed = QDateTime::fromString(q(utc_iso), Qt::ISODate);
    if (!parsed.isValid()) return q(utc_iso);
    // The database stores UTC ISO-8601 values. Keeping the GUI in UTC avoids
    // ambiguity when a case is moved between machines and makes the zone
    // explicit without exposing implementation-level milliseconds.
    return parsed.toUTC().toString(QStringLiteral("yyyy-MM-dd HH:mm 'UTC'"));
}

QString display_bytes(std::int64_t bytes) {
    if (bytes < 1024) return QString::number(bytes) + QStringLiteral(" B");
    if (bytes < 1024 * 1024) return QString::number(bytes / 1024.0, 'f', 1) + QStringLiteral(" KB");
    if (bytes < 1024LL * 1024LL * 1024LL) return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + QStringLiteral(" MB");
    return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 1) + QStringLiteral(" GB");
}

QString title_case(const QString& value) {
    if (value.isEmpty()) return value;
    auto result = value;
    result[0] = result[0].toUpper();
    return result;
}

QString enum_label(const std::string& value) {
    if (value == "ip") return QStringLiteral("IP Address");
    auto result = title_case(q(value));
    result.replace('_', ' ');
    return result;
}

QString wrap_long_tokens(const QString& value) {
    QString result;
    QString token;
    const auto flush = [&] {
        if (token.size() <= 16) {
            result += token;
        } else {
            for (int index = 0; index < token.size(); ++index) {
                result += token[index];
                if ((index + 1) % 8 == 0 && index + 1 < token.size()) result += QChar(0x200B);
            }
        }
        token.clear();
    };
    for (const auto character : value) {
        if (character.isSpace()) {
            flush();
            result += character;
        } else {
            token += character;
        }
    }
    flush();
    return result;
}

std::vector<std::string> json_strings(const std::string& value) {
    std::vector<std::string> result;
    try {
        const auto parsed = json::Json::parse(value.empty() ? "[]" : value);
        if (!parsed.is_array()) return result;
        for (const auto& item : parsed.as_array()) if (item.is_string() && !item.as_string().empty()) result.push_back(item.as_string());
    } catch (...) {
        // Malformed user-entered metadata is displayed as raw data elsewhere; it must not break the UI.
    }
    return result;
}

std::string json_string_array(const std::vector<std::string>& values) {
    json::Json::Array array;
    for (const auto& value : values) array.emplace_back(value);
    return json::Json(std::move(array)).dump();
}

std::string json_description(const std::string& value) {
    try {
        const auto parsed = json::Json::parse(value.empty() ? "{}" : value);
        if (parsed.is_object()) return parsed.string_or("description");
    } catch (...) {
    }
    return value;
}

std::string details_with_description(const std::string& description) {
    return json::Json(json::Json::Object{{"description", description}}).dump();
}

QLabel* heading(const QString& text, int point_size) {
    auto* label = new QLabel(text);
    label->setObjectName("sectionTitle");
    label->setWordWrap(true);
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    auto font = label->font();
    font.setPointSize(std::max(8, static_cast<int>(std::lround(point_size * 0.9))));
    font.setWeight(QFont::DemiBold);
    label->setFont(font);
    return label;
}

QLabel* muted(const QString& text) {
    auto* label = new QLabel(text);
    label->setObjectName("subtle");
    label->setWordWrap(true);
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

void update_status_badge(QLabel* label, const std::string& status) {
    if (label == nullptr) return;
    const bool green = status == "confirmed" || status == "active";
    const bool red = status == "refuted" || status == "archived";
    const auto border = green ? QStringLiteral("#36c99b") : red ? QStringLiteral("#e35d6a") : status == "possible" ? QStringLiteral("#e6b93f") : status == "probable" ? QStringLiteral("#5dd7c2") : QStringLiteral("#72809a");
    const auto background = green ? QStringLiteral("#12362f") : red ? QStringLiteral("#3e1a25") : status == "possible" ? QStringLiteral("#3b3011") : status == "probable" ? QStringLiteral("#12332e") : QStringLiteral("#1b2432");
    const auto foreground = green ? QStringLiteral("#74f0c0") : red ? QStringLiteral("#ff8994") : status == "possible" ? QStringLiteral("#ffd85c") : status == "probable" ? QStringLiteral("#7af1dc") : QStringLiteral("#bac6da");
    label->setText(QStringLiteral("● %1").arg(enum_label(status)));
    label->setAlignment(Qt::AlignCenter);
    // Cell widgets do not participate in the table's text-elision rules. Give
    // the label enough room for its complete state name so Confirmed and
    // Unverified never lose their first or last characters.
    label->setMinimumWidth(label->sizeHint().width() + 4);
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    label->setFixedHeight(20);
    label->setStyleSheet(QStringLiteral("QLabel { border: 1px solid %1; background: %2; color: %3; border-radius: 4px; padding: 1px 4px; font-size: 7pt; font-weight: 700; }")
        .arg(border)
        .arg(background)
        .arg(foreground));
}

QLabel* status_badge(const std::string& status) {
    auto* label = new QLabel;
    update_status_badge(label, status);
    return label;
}

QPushButton* button(const QString& text, bool accent) {
    // Keep action labels portable across Linux font stacks. Several of the
    // original labels used full-width symbols and Unicode arrows that render
    // as tofu squares on minimal Wayland desktops.
    auto normalized = text;
    normalized.replace(QStringLiteral("＋"), QStringLiteral("+"));
    normalized.replace(QStringLiteral("‹"), QStringLiteral("<"));
    normalized.replace(QStringLiteral("↑"), QStringLiteral("up"));
    normalized.replace(QStringLiteral("↓"), QStringLiteral("down"));
    normalized = normalized.trimmed();
    if (normalized.startsWith(QLatin1Char('+'))) normalized = normalized.mid(1).trimmed();
    auto* result = new QPushButton(normalized);
    const auto lower = normalized.toLower();
    std::optional<UiIcon> icon;
    if (lower.startsWith(QStringLiteral("add ")) || lower.startsWith(QStringLiteral("new "))) icon = UiIcon::Add;
    else if (lower.contains(QStringLiteral("evidence"))) icon = UiIcon::Evidence;
    else if (lower.contains(QStringLiteral("source"))) icon = UiIcon::Source;
    else if (lower.contains(QStringLiteral("claim")) || lower.contains(QStringLiteral("relation"))) icon = UiIcon::Claim;
    else if (lower.contains(QStringLiteral("playbook")) || lower.contains(QStringLiteral("run"))) icon = UiIcon::Play;
    else if (lower.contains(QStringLiteral("export"))) icon = UiIcon::Export;
    else if (lower.contains(QStringLiteral("import")) || lower.contains(QStringLiteral("upload"))) icon = UiIcon::Upload;
    else if (lower.contains(QStringLiteral("preview"))) icon = UiIcon::Eye;
    else if (lower.contains(QStringLiteral("verify"))) icon = UiIcon::Check;
    else if (lower.contains(QStringLiteral("delete")) || lower.contains(QStringLiteral("remove"))) icon = UiIcon::Trash;
    else if (lower.contains(QStringLiteral("archive"))) icon = UiIcon::Archive;
    else if (lower.contains(QStringLiteral("restore"))) icon = UiIcon::Restore;
    else if (lower.contains(QStringLiteral("edit")) || lower.contains(QStringLiteral("update"))) icon = UiIcon::Edit;
    else if (lower.contains(QStringLiteral("open")) || lower.contains(QStringLiteral("browse")) || lower.contains(QStringLiteral("change"))) icon = UiIcon::FolderOpen;
    else if (lower.contains(QStringLiteral("link"))) icon = UiIcon::Link;
    else if (lower.contains(QStringLiteral("move")) && lower.contains(QStringLiteral("up"))) icon = UiIcon::ArrowUp;
    else if (lower.contains(QStringLiteral("move"))) icon = UiIcon::ArrowDown;
    else if (lower.contains(QStringLiteral("add")) || lower.contains(QStringLiteral("new")) || lower.contains(QStringLiteral("create"))) icon = UiIcon::Add;
    if (icon) {
        result->setIcon(ui_icon(*icon, accent ? QColor("#ffffff") : QColor("#bba6ff")));
        result->setIconSize(QSize(18, 18));
    }
    if (accent) result->setProperty("accent", true);
    return result;
}

QComboBox* enum_combo(QWidget* parent, const QStringList& labels, const QStringList& values) {
    auto* combo = new ArrowComboBox(parent);
    for (int index = 0; index < labels.size() && index < values.size(); ++index) combo->addItem(labels[index], values[index]);
    return combo;
}

QFrame* card(QWidget* parent) {
    auto* frame = new QFrame(parent);
    frame->setObjectName("card");
    return frame;
}

StateTableWidget* table_with_empty_state(QWidget* parent) {
    return new StateTableWidget(parent);
}

void set_empty_state(QTableWidget* table, const QString& title, const QString& description) {
    if (auto* state_table = dynamic_cast<StateTableWidget*>(table)) state_table->set_empty_state(title, description);
}

void configure_table(QTableWidget* table, const QStringList& headers) {
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
    table->setWordWrap(true);
    table->setTextElideMode(Qt::ElideNone);
    table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    // Long values wrap in the visible cell and remain available through the
    // full-value tooltip/inspector. A horizontal scrollbar inside every
    // record table is what made the old pages feel like nested workspaces.
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    table->setMinimumWidth(0);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(44);
    table->verticalHeader()->setMinimumSectionSize(34);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setStretchLastSection(true);
    // Fit the visible record columns to the available panel. Individual
    // pages can opt into fixed widths for highly structured tables, but the
    // default must not create a second horizontal workspace scrollbar.
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table->horizontalHeader()->setTextElideMode(Qt::ElideNone);
    table->horizontalHeader()->setMinimumHeight(38);
    table->horizontalHeader()->setDefaultSectionSize(116);
    table->horizontalHeader()->setMinimumSectionSize(56);
    for (int index = 0; index < headers.size(); ++index) {
        if (auto* header = table->horizontalHeaderItem(index)) header->setToolTip(headers[index]);
    }
    table->setSortingEnabled(false);
}

void stretch_table_columns(QTableWidget* table) {
    if (!table) return;
    auto* header = table->horizontalHeader();
    header->setMinimumSectionSize(56);
    for (int index = 0; index < table->columnCount(); ++index) {
        header->setSectionResizeMode(index, QHeaderView::Stretch);
    }
    if (table->columnCount() == 8 && table->horizontalHeaderItem(0) && table->horizontalHeaderItem(0)->text() == QStringLiteral("Statement")) {
        header->setSectionResizeMode(0, QHeaderView::Stretch);
        const std::array<int, 7> fixed_widths = {88, 76, 82, 76, 108, 74, 122};
        for (int index = 1; index < 8; ++index) {
            header->setSectionResizeMode(index, QHeaderView::Fixed);
            table->setColumnWidth(index, fixed_widths[static_cast<std::size_t>(index - 1)]);
        }
    }
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

ResponsiveSplitter* responsive_splitter(QWidget* parent, int breakpoint) {
    auto* splitter = new ResponsiveSplitter(Qt::Horizontal, parent, breakpoint);
    // Page constructors may provide a semantic initial size before the
    // splitter has entered a layout. Rebalance once after geometry exists so
    // the list remains the primary surface and the inspector stays readable.
    QTimer::singleShot(0, splitter, [splitter] {
        if (splitter->orientation() == Qt::Horizontal && splitter->count() > 1) {
            const auto total = std::max(1, splitter->width() - splitter->handleWidth());
            splitter->setSizes({total * 2 / 3, total - total * 2 / 3});
        }
    });
    return splitter;
}

void observe_resize(QWidget* widget, std::function<void(int)> callback) {
    if (!widget) return;
    widget->installEventFilter(new ResizeObserver(widget, std::move(callback)));
}

void show_error(QWidget* parent, const std::exception& error) { show_error(parent, QString::fromUtf8(error.what())); }

void show_error(QWidget* parent, const QString& message) {
    QMessageBox box(QMessageBox::Critical, QStringLiteral("Action could not be completed"), message, QMessageBox::Ok, parent);
    box.setInformativeText(QStringLiteral("No partial change was kept. Review the message and try again."));
    box.exec();
}

QString shorten_id(const std::string& id) {
    const auto value = q(id);
    return value.size() > 8 ? value.left(8) : value;
}

} // namespace evidence_trace::gui
