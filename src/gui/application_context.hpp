#pragma once

#include "database/migrations.hpp"
#include "database/sqlite_database.hpp"
#include "import_export/import_export_service.hpp"
#include "services/case_service.hpp"
#include "services/event_logger.hpp"
#include "services/investigation_service.hpp"
#include "services/playbook_service.hpp"
#include "storage/attachment_store.hpp"

#include <filesystem>

namespace evidence_trace::gui {

class ApplicationContext {
public:
    explicit ApplicationContext(std::filesystem::path data_directory);

    std::filesystem::path data_directory;
    database::Database database;
    storage::AttachmentStore store;
    services::EventLogger logger;
    services::CaseService cases;
    services::InvestigationService investigation;
    services::PlaybookService playbooks;
    import_export::ImportExportService portability;
};

} // namespace evidence_trace::gui
