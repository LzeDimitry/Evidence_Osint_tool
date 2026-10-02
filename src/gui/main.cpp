#include "gui/gui_helpers.hpp"
#include "gui/main_window.hpp"

#include <QApplication>
#include <QMessageBox>
#include <QTimer>

#include <filesystem>
#include <iostream>
#include <optional>

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Evidence Trace"));
    QCoreApplication::setApplicationName(QStringLiteral("Evidence Trace"));
    evidence_trace::gui::apply_theme(application);
    std::filesystem::path data_directory = "data";
    std::filesystem::path screenshot;
    std::optional<QSize> window_size;
    std::string open_case_id;
    std::string open_section;
    std::string page;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--data-dir" && index + 1 < argc) data_directory = argv[++index];
        else if (argument == "--screenshot" && index + 1 < argc) screenshot = argv[++index];
        else if (argument == "--window-size" && index + 1 < argc) {
            const std::string value = argv[++index];
            const auto separator = value.find('x');
            if (separator == std::string::npos) { std::cerr << "--window-size must use WIDTHxHEIGHT\n"; return 2; }
            try { window_size = QSize(std::stoi(value.substr(0, separator)), std::stoi(value.substr(separator + 1))); }
            catch (const std::exception&) { std::cerr << "--window-size must use numeric WIDTHxHEIGHT\n"; return 2; }
        }
        else if (argument == "--open-case" && index + 1 < argc) open_case_id = argv[++index];
        else if (argument == "--section" && index + 1 < argc) open_section = argv[++index];
        else if (argument == "--page" && index + 1 < argc) page = argv[++index];
        else if (argument == "--help" || argument == "-h") { std::cout << "Usage: evidence-trace-gui [--data-dir DIR] [--page cases|playbooks|settings] [--open-case ID --section NAME] [--window-size WIDTHxHEIGHT] [--screenshot FILE]\n"; return 0; }
    }
    try {
        evidence_trace::gui::MainWindow window(data_directory);
        if (window_size) window.resize(*window_size);
        window.show();
        if (!page.empty()) QTimer::singleShot(250, &window, [&window, page] {
            if (page == "playbooks") window.show_playbooks();
            else if (page == "settings") window.show_settings();
            else window.show_cases();
        });
        if (!open_case_id.empty()) {
            // Open and select the requested section in the same queued turn.
            // The previous nested timers could let the default Overview page
            // win the race when a screenshot was captured during startup.
            QTimer::singleShot(300, &window, [&window, open_case_id, open_section] {
                window.open_case(open_case_id);
                if (!open_section.empty()) window.show_case_section(QString::fromStdString(open_section));
            });
        }
        if (!screenshot.empty()) QTimer::singleShot(900, &window, [&application, &window, screenshot] { QApplication::processEvents(); window.grab().save(QString::fromStdString(screenshot.string())); application.quit(); });
        return application.exec();
    } catch (const std::exception& error) {
        QMessageBox::critical(nullptr, QStringLiteral("Evidence Trace could not start"), QString::fromUtf8(error.what()));
        return 1;
    }
}
