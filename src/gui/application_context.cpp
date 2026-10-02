#include "gui/application_context.hpp"

namespace evidence_trace::gui {

ApplicationContext::ApplicationContext(std::filesystem::path directory)
    : data_directory(std::filesystem::absolute(std::move(directory))),
      database((std::filesystem::create_directories(data_directory), data_directory / "evidence_trace.sqlite3")),
      store(data_directory),
      logger(database),
      cases(database, logger),
      investigation(database, store, logger),
      playbooks(database, logger),
    portability(database, store, logger) {
    database::apply_migrations(database, data_directory / "evidence_trace.sqlite3");
    store.cleanup_temporary_files();
    store.recover_staged_removals([this](const std::filesystem::path& relative_path) {
        auto statement = database.prepare("SELECT 1 FROM evidence WHERE relative_path = ? LIMIT 1;");
        statement.bind(1, relative_path.generic_string());
        return statement.step();
    });
}

} // namespace evidence_trace::gui
