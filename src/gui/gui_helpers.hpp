#pragma once

#include "domain/types.hpp"

#include <QApplication>
#include <QColor>
#include <QComboBox>
#include <QFont>
#include <QFrame>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QSplitter>
#include <QTableWidget>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <optional>
#include <functional>
#include <string>
#include <vector>

namespace evidence_trace::gui {

class StateTableWidget final : public QTableWidget {
public:
    explicit StateTableWidget(QWidget* parent = nullptr);

    void set_empty_state(const QString& title, const QString& description);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString empty_title_;
    QString empty_description_;
};

class ExpandableText final : public QLabel {
public:
    explicit ExpandableText(QWidget* parent = nullptr);

    void setFullText(const QString& text);
    const QString& fullText() const { return full_text_; }
    bool isExpanded() const { return expanded_; }
    void setExpanded(bool expanded);
    void setShowEllipsis(bool show_ellipsis);
    void setPreviewLineLimit(int lines);
    int contentHeightForWidth(int width) const;

    std::function<void(bool)> on_toggled;

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override;

private:
    QString collapsedText(int width) const;
    QStringList visualLines(int width) const;
    void refreshDisplay();

    QString full_text_;
    bool expanded_{false};
    bool show_ellipsis_{true};
    int preview_line_limit_{2};
};

// Small, deliberately quiet line icons keep the interface self-contained.
// The references use an outlined icon language; drawing these at runtime
// avoids a platform-dependent icon theme and keeps the application portable.
enum class UiIcon {
    Add,
    Archive,
    ArrowDown,
    ArrowUp,
    Book,
    Calendar,
    Check,
    ChevronDown,
    ChevronRight,
    Claim,
    Clipboard,
    Document,
    Download,
    Edit,
    Entity,
    Evidence,
    Export,
    Eye,
    File,
    Filter,
    Folder,
    FolderAdd,
    FolderOpen,
    Globe,
    Grid,
    Graph,
    Image,
    Info,
    Link,
    List,
    More,
    Note,
    Organization,
    Person,
    Play,
    Restore,
    Search,
    Server,
    Settings,
    Shield,
    Source,
    Target,
    Activity,
    Trash,
    Upload,
    Warning,
};

QIcon ui_icon(UiIcon kind, const QColor& color = QColor("#aebbd2"));
QLabel* make_tag_chip(const QString& value, QWidget* parent = nullptr, int maximum_width = 140);
QWidget* tag_chips(const QStringList& values, QWidget* parent = nullptr, int visible_limit = 3, int maximum_chip_width = 140);

// Split list/inspector layouts on narrow desktop windows instead of forcing
// both panels into a width where labels and actions become unreadable.
class ResponsiveSplitter final : public QSplitter {
public:
    explicit ResponsiveSplitter(Qt::Orientation orientation, QWidget* parent = nullptr, int breakpoint = 950);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    Qt::Orientation wide_orientation_;
    int breakpoint_;
};

void apply_theme(QApplication& application);
bool navigation_glow_enabled();
void set_navigation_glow_enabled(bool enabled);

QString q(const std::string& value);
std::string s(const QString& value);

QString display_time(const std::string& utc_iso);
QString display_bytes(std::int64_t bytes);
QString title_case(const QString& value);
QString enum_label(const std::string& value);
QString wrap_long_tokens(const QString& value);
QString two_line_text_preview(const QString& value, const QFont& font, int available_width);

std::vector<std::string> json_strings(const std::string& value);
std::string json_string_array(const std::vector<std::string>& values);
std::string json_description(const std::string& value);
std::string details_with_description(const std::string& description);

QLabel* heading(const QString& text, int point_size = 20);
QLabel* muted(const QString& text);
QLabel* status_badge(const std::string& status);
void update_status_badge(QLabel* label, const std::string& status);
QPushButton* button(const QString& text, bool accent = false);
QComboBox* enum_combo(QWidget* parent, const QStringList& labels, const QStringList& values);
QFrame* card(QWidget* parent = nullptr);
StateTableWidget* table_with_empty_state(QWidget* parent = nullptr);
void set_empty_state(QTableWidget* table, const QString& title, const QString& description);
void configure_table(QTableWidget* table, const QStringList& headers);
void stretch_table_columns(QTableWidget* table);
ResponsiveSplitter* responsive_splitter(QWidget* parent = nullptr, int breakpoint = 950);
void observe_resize(QWidget* widget, std::function<void(int)> callback);
void show_error(QWidget* parent, const std::exception& error);
void show_error(QWidget* parent, const QString& message);
QString shorten_id(const std::string& id);

} // namespace evidence_trace::gui
