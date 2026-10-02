#pragma once

#include "gui/application_context.hpp"

#include <QMainWindow>
#include <QString>

#include <filesystem>
#include <functional>
#include <memory>

class QLineEdit;
class QStackedWidget;
class QPushButton;
class QLabel;
class QFrame;
class QResizeEvent;
class QWidget;
class QBoxLayout;

namespace evidence_trace::gui {

class CasesPage;
class PlaybooksPage;
class SettingsPage;
class CasePage;
class ExpandableText;
class CaseOverviewPage;
class EntitiesPage;
class ClaimsPage;
class EvidencePage;
class NotesPage;
class RunsPage;
class ExportPage;

class CaseWorkspace : public QWidget {
public:
    CaseWorkspace(ApplicationContext& context, const domain::Id& case_id, QWidget* parent = nullptr);

    void refresh();
    void search(const QString& query);
    void show_section(const QString& section);
    void show_record(const QString& section, const QString& object_type, const domain::Id& object_id);
    void set_navigation_glow_enabled(bool enabled);
    std::function<void()> on_back;

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void show_page(int index);
    void update_case_subtitle();
    void update_navigation_glow();
    void add_entity();
    void add_claim();
    void add_evidence();

    ApplicationContext& context_;
    domain::Id case_id_;
    QStackedWidget* pages_{nullptr};
    QLabel* case_title_{nullptr};
    QLabel* case_breadcrumb_{nullptr};
    ExpandableText* case_subtitle_{nullptr};
    QLabel* case_status_{nullptr};
    QLabel* case_meta_{nullptr};
    QLabel* case_entities_{nullptr};
    QLabel* case_evidence_{nullptr};
    QLabel* case_updated_{nullptr};
    QBoxLayout* case_header_layout_{nullptr};
    QWidget* case_title_block_{nullptr};
    QWidget* case_meta_block_{nullptr};
    QString case_subtitle_full_;
    QWidget* navigation_bar_{nullptr};
    QWidget* navigation_scroll_{nullptr};
    QWidget* navigation_glow_{nullptr};
    bool navigation_glow_enabled_{false};
    std::vector<QPushButton*> navigation_;
    std::vector<CasePage*> case_pages_;
    EntitiesPage* entities_page_{nullptr};
    ClaimsPage* claims_page_{nullptr};
    EvidencePage* evidence_page_{nullptr};
};

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(std::filesystem::path data_directory, QWidget* parent = nullptr);

    void show_cases();
    void show_playbooks();
    void show_settings();
    void open_case(const domain::Id& case_id);
    void show_case_section(const QString& section);
    void reload_data_directory(const std::filesystem::path& directory);

private:
    void resizeEvent(QResizeEvent* event) override;
    void build_shell();
    void rebuild_pages();
    void set_navigation(QPushButton* active);
    void run_global_search(const QString& query);

    std::unique_ptr<ApplicationContext> context_;
    QWidget* shell_{nullptr};
    QFrame* topbar_{nullptr};
    QLabel* global_shortcut_{nullptr};
    QFrame* sidebar_{nullptr};
    QStackedWidget* content_{nullptr};
    QLineEdit* global_search_{nullptr};
    QPushButton* cases_nav_{nullptr};
    QPushButton* playbooks_nav_{nullptr};
    QPushButton* settings_nav_{nullptr};
    CasesPage* cases_page_{nullptr};
    PlaybooksPage* playbooks_page_{nullptr};
    SettingsPage* settings_page_{nullptr};
    CaseWorkspace* workspace_{nullptr};
};

} // namespace evidence_trace::gui
