#pragma once

#include "database/sqlite_database.hpp"
#include "domain/types.hpp"
#include "services/event_logger.hpp"
#include "storage/attachment_store.hpp"

#include <filesystem>
#include <string>

namespace evidence_trace::import_export {

enum class ImportMode { SkipExisting, ImportAsCopy };

struct ImportResult {
    bool imported{false};
    bool skipped{false};
    domain::Id case_id;
    std::string message;
};

class ImportExportService {
public:
    ImportExportService(database::Database& database,
                        storage::AttachmentStore& attachment_store,
                        services::EventLogger& logger)
        : database_(database), attachment_store_(attachment_store), logger_(logger) {}

    void export_case(const domain::Id& case_id, const std::filesystem::path& archive_path);
    void write_report(const domain::Id& case_id, const std::filesystem::path& report_path);
    ImportResult import_case(const std::filesystem::path& archive_path, ImportMode mode);

private:
    database::Database& database_;
    storage::AttachmentStore& attachment_store_;
    services::EventLogger& logger_;
};

} // namespace evidence_trace::import_export
