#include "gui/gui_helpers.hpp"
#include "gui/main_window.hpp"

#include <QApplication>
#include <QIcon>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

#include <charconv>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string_view>

namespace {

void print_usage() {
    std::cout << "Usage: evidence-trace-gui [--data-dir DIR] [--page cases|playbooks|settings] [--open-case ID --section NAME] [--window-size WIDTHxHEIGHT] [--screenshot FILE]\n";
}

struct GuiOptions {
    std::optional<std::filesystem::path> data_directory;
    std::filesystem::path screenshot;
    std::optional<QSize> window_size;
    std::string open_case_id;
    std::string open_section;
    std::string open_entity_id;
    std::string page;
};

bool parse_positive_integer(std::string_view value, int& result) {
    if (value.empty()) return false;
    int parsed = 0;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (error != std::errc{} || end != value.data() + value.size() || parsed <= 0) return false;
    result = parsed;
    return true;
}

bool parse_arguments(int argc, char** argv, GuiOptions& options) {
    const auto value_after = [argc, argv](int& index, const char* name) -> std::optional<std::string> {
        if (index + 1 >= argc || std::string_view(argv[index + 1]).empty() || std::string_view(argv[index + 1]).starts_with("--")) {
            std::cerr << name << " requires a value\n";
            return std::nullopt;
        }
        return argv[++index];
    };
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--data-dir") {
            const auto value = value_after(index, "--data-dir");
            if (!value) return false;
            options.data_directory = *value;
        } else if (argument == "--screenshot") {
            const auto value = value_after(index, "--screenshot");
            if (!value) return false;
            options.screenshot = *value;
        } else if (argument == "--open-case") {
            const auto value = value_after(index, "--open-case");
            if (!value) return false;
            options.open_case_id = *value;
        } else if (argument == "--entity") {
            const auto value = value_after(index, "--entity");
            if (!value) return false;
            options.open_entity_id = *value;
        } else if (argument == "--section") {
            const auto value = value_after(index, "--section");
            if (!value) return false;
            options.open_section = *value;
        } else if (argument == "--page") {
            const auto value = value_after(index, "--page");
            if (!value) return false;
            if (*value != "cases" && *value != "playbooks" && *value != "settings") {
                std::cerr << "--page must be cases, playbooks, or settings\n";
                return false;
            }
            options.page = *value;
        } else if (argument == "--window-size") {
            const auto value = value_after(index, "--window-size");
            if (!value) return false;
            const auto separator = value->find('x');
            int width = 0;
            int height = 0;
            if (separator == std::string::npos || !parse_positive_integer(std::string_view(*value).substr(0, separator), width) ||
                !parse_positive_integer(std::string_view(*value).substr(separator + 1), height)) {
                std::cerr << "--window-size must use positive numeric WIDTHxHEIGHT\n";
                return false;
            }
            options.window_size = QSize(width, height);
        } else {
            std::cerr << "Unknown option: " << argument << '\n';
            return false;
        }
    }
    if (!options.open_section.empty()) {
        if (options.open_case_id.empty()) {
            std::cerr << "--section requires --open-case ID\n";
            return false;
        }
        const auto& section = options.open_section;
        if (section != "overview" && section != "entities" && section != "claims" && section != "relations" &&
            section != "evidence" && section != "sources" && section != "notes" && section != "log" &&
            section != "runs" && section != "playbooks" && section != "export" && section != "profile" && section != "entity-profile") {
            std::cerr << "Unknown case section: " << section << '\n';
            return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--help" || argument == "-h") {
            print_usage();
            return 0;
        }
    }

    GuiOptions options;
    if (!parse_arguments(argc, argv, options)) {
        print_usage();
        return 2;
    }

    QApplication application(argc, argv);
    const QIcon application_icon(QStringLiteral(":/icons/evidence-trace.png"));
    application.setWindowIcon(application_icon);
    QCoreApplication::setOrganizationName(QStringLiteral("Evidence Trace"));
    QCoreApplication::setApplicationName(QStringLiteral("Evidence Trace"));
    evidence_trace::gui::apply_theme(application);
    const auto configured_data_directory = QSettings().value(QStringLiteral("storage/dataDirectory")).toString();
    const auto default_data_directory = configured_data_directory.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation).toStdString()
        : configured_data_directory.toStdString();
    const std::filesystem::path data_directory = options.data_directory.value_or(default_data_directory);
    try {
        evidence_trace::gui::MainWindow window(data_directory);
        window.setWindowIcon(application_icon);
        if (options.window_size) {
            window.resize(*options.window_size);
            window.show();
        } else {
            window.showMaximized();
        }
        if (!options.page.empty()) QTimer::singleShot(250, &window, [&window, page = options.page] {
            if (page == "playbooks") window.show_playbooks();
            else if (page == "settings") window.show_settings();
            else window.show_cases();
        });
        if (!options.open_case_id.empty()) {
            // Open and select the requested section in the same queued turn.
            // The previous nested timers could let the default Overview page
            // win the race when a screenshot was captured during startup.
            QTimer::singleShot(300, &window, [&window, open_case_id = options.open_case_id, open_section = options.open_section, open_entity_id = options.open_entity_id] {
                window.open_case(open_case_id);
                if (!open_entity_id.empty()) {
                    window.show_case_record(QStringLiteral("profile"), QStringLiteral("entity"), open_entity_id);
                } else if (!open_section.empty()) {
                    window.show_case_section(QString::fromStdString(open_section));
                }
            });
        }
        if (!options.screenshot.empty()) QTimer::singleShot(900, &window, [&application, &window, screenshot = options.screenshot] { QApplication::processEvents(); window.grab().save(QString::fromStdString(screenshot.string())); application.quit(); });
        return application.exec();
    } catch (const std::exception& error) {
        QMessageBox::critical(nullptr, QStringLiteral("Evidence Trace could not start"), QString::fromUtf8(error.what()));
        return 1;
    }
}
