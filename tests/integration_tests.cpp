#include "database/migrations.hpp"
#include "database/sqlite_database.hpp"
#include "domain/types.hpp"
#include "import_export/import_export_service.hpp"
#include "services/case_service.hpp"
#include "services/event_logger.hpp"
#include "services/investigation_service.hpp"
#include "services/playbook_service.hpp"
#include "storage/attachment_store.hpp"

#include <zip.h>

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace evidence_trace;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error("FAIL: " + message);
}

template <typename Callable>
void require_throw(Callable&& callable, const std::string& message) {
    try {
        callable();
    } catch (const std::exception&) {
        return;
    }
    throw std::runtime_error("FAIL: expected rejection: " + message);
}

std::filesystem::path make_directory(const std::string& name) {
    const auto path = std::filesystem::temp_directory_path() / ("evidence-trace-" + name + "-" + domain::new_id());
    std::filesystem::create_directories(path);
    return path;
}

struct Context {
    database::Database database;
    storage::AttachmentStore store;
    services::EventLogger logger;
    services::CaseService cases;
    services::InvestigationService investigation;
    services::PlaybookService playbooks;
    import_export::ImportExportService portability;

    explicit Context(const std::filesystem::path& root)
        : database(root / "evidence_trace.sqlite3"),
          store(root),
          logger(database),
          cases(database, logger),
          investigation(database, store, logger),
          playbooks(database, logger),
        portability(database, store, logger) {
        database::apply_migrations(database, root / "evidence_trace.sqlite3");
        store.cleanup_temporary_files();
        store.recover_staged_removals([this](const std::filesystem::path& relative_path) {
            auto statement = database.prepare("SELECT 1 FROM evidence WHERE relative_path = ? LIMIT 1;");
            statement.bind(1, relative_path.generic_string());
            return statement.step();
        });
    }
};

void write_file(const std::filesystem::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("Unable to create test file");
    output << text;
}

std::vector<unsigned char> read_zip_entry(zip_t* archive, zip_uint64_t index) {
    zip_stat_t info{};
    if (zip_stat_index(archive, index, 0, &info) != 0) throw std::runtime_error("Unable to inspect test archive");
    zip_file_t* file = zip_fopen_index(archive, index, 0);
    if (file == nullptr) throw std::runtime_error("Unable to open test archive entry");
    std::vector<unsigned char> bytes(static_cast<std::size_t>(info.size));
    if (!bytes.empty() && zip_fread(file, bytes.data(), bytes.size()) != static_cast<zip_int64_t>(bytes.size())) throw std::runtime_error("Unable to read test archive entry");
    zip_fclose(file);
    return bytes;
}

void make_malformed_archive(const std::filesystem::path& source,
                            const std::filesystem::path& destination,
                            bool modify_attachment,
                            bool add_unsafe_path) {
    int error = 0;
    zip_t* input = zip_open(source.string().c_str(), ZIP_RDONLY, &error);
    if (input == nullptr) throw std::runtime_error("Unable to open source archive");
    zip_t* output = zip_open(destination.string().c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);
    if (output == nullptr) {
        zip_discard(input);
        throw std::runtime_error("Unable to create malformed archive");
    }
    std::vector<std::vector<unsigned char>> storage;
    try {
        const auto count = zip_get_num_entries(input, 0);
        storage.reserve(static_cast<std::size_t>(count) + 1);
        for (zip_uint64_t index = 0; index < static_cast<zip_uint64_t>(count); ++index) {
            zip_stat_t info{};
            zip_stat_index(input, index, 0, &info);
            const std::string name(info.name);
            auto bytes = read_zip_entry(input, index);
            if (modify_attachment && name.rfind("attachments/", 0) == 0) bytes.assign({'m', 'o', 'd', 'i', 'f', 'i', 'e', 'd'});
            storage.push_back(std::move(bytes));
            auto* entry = zip_source_buffer(output, storage.back().data(), storage.back().size(), 0);
            if (entry == nullptr || zip_file_add(output, name.c_str(), entry, ZIP_FL_OVERWRITE) < 0) throw std::runtime_error("Unable to write malformed archive");
        }
        if (add_unsafe_path) {
            static const unsigned char unsafe[] = {'x'};
            auto* entry = zip_source_buffer(output, unsafe, sizeof(unsafe), 0);
            if (entry == nullptr || zip_file_add(output, "../unsafe", entry, ZIP_FL_OVERWRITE) < 0) throw std::runtime_error("Unable to write unsafe archive");
        }
        if (zip_close(input) != 0 || zip_close(output) != 0) throw std::runtime_error("Unable to close malformed archive");
    } catch (...) {
        zip_discard(input);
        zip_discard(output);
        throw;
    }
}

void test_mvp() {
    const auto root = make_directory("integration");
    try {
        domain::Id case_id;
        domain::Id username_id;
        domain::Id person_id;
        domain::Id file_evidence_id;
        domain::Id text_evidence_id;
        domain::Id source_id;
        domain::Id relation_id;
        domain::Id confirmed_relation_id;
        domain::Id run_id;
        domain::Id playbook_id;
        domain::Id technique_id;
        domain::Id deleted_entity_id;
        {
            Context context(root);
            const auto created = context.cases.create("Username investigation", "Establish a possible account relationship", "Keep the source and reasoning together", "username", "Public sources only", {"mvp", "osint"});
            case_id = created.id;
            require(context.cases.get(case_id).title == "Username investigation", "case creation");
            require(context.cases.get(case_id).tags_json.find("osint") != std::string::npos, "case tags are stored");
            context.cases.update(case_id, "Username investigation — revised", created.purpose, created.description, created.primary_target_type, created.scope, std::nullopt);
            require(context.cases.get(case_id).title == "Username investigation — revised", "case metadata update");
            username_id = context.investigation.create_entity(case_id, domain::EntityType::Username, "john1337", "John1337", "{}", {}, {"john1337-old"}).id;
            person_id = context.investigation.create_entity(case_id, domain::EntityType::Person, "John Doe", "John Doe").id;
            require(context.investigation.get_entity(username_id).aliases_json.find("john1337-old") != std::string::npos, "entity aliases persist");
        }
        {
            Context context(root);
            require(context.cases.get(case_id).title == "Username investigation — revised", "case metadata persists after restart");
            require(context.cases.get(case_id).tags_json.find("osint") != std::string::npos, "case tags persist after restart");
            require(context.cases.get(case_id).purpose == "Establish a possible account relationship", "case persists after restart");
            require(context.investigation.list_entities(case_id).size() == 2, "entities persist after restart");
            require(context.investigation.get_entity(username_id).aliases_json.find("john1337-old") != std::string::npos, "entity aliases persist after restart");

            const auto source = context.investigation.create_source(case_id, domain::SourceType::Web, "https://example.test/profile", "Profile");
            source_id = source.id;
            const auto external = context.investigation.add_url_evidence(case_id, "https://example.test/profile", source.id);
            require(context.investigation.get_evidence(external.id).kind == domain::EvidenceKind::Url, "URL evidence");
            const auto text = context.investigation.add_text_evidence(case_id, "A quoted profile excerpt", source.id, std::string("https://example.test/profile"), "Excerpt note", "Profile section");
            text_evidence_id = text.id;
            require(context.investigation.get_evidence(text.id).quotation_location == "Profile section", "text quotation location");

            const auto original = root / "original.txt";
            write_file(original, "preserved bytes\n");
            const auto file = context.investigation.add_file_evidence(case_id, original, source.id);
            file_evidence_id = file.id;
            require(context.investigation.verify_evidence(file.id).ok, "new file hash verifies");
            const auto preserved_bytes = context.store.read(*file.relative_path);
            write_file(original, "changed original outside the application\n");
            require(context.store.read(*file.relative_path) == preserved_bytes, "preserved copy is independent");
            require(context.investigation.verify_evidence(file.id).ok, "original mutation does not change internal hash");
            write_file(context.store.absolute_path(*file.relative_path), "internal mutation\n");
            require(!context.investigation.verify_evidence(file.id).ok, "internal mutation is detected");
            {
                std::ofstream restored(context.store.absolute_path(*file.relative_path), std::ios::binary | std::ios::trunc);
                restored.write(reinterpret_cast<const char*>(preserved_bytes.data()), static_cast<std::streamsize>(preserved_bytes.size()));
            }
            require(context.investigation.verify_evidence(file.id).ok, "test fixture restores the preserved copy");

            const auto relation = context.investigation.create_relation(case_id, username_id, "belongs_to", person_id, {}, domain::ClaimStatus::Possible, "Visible profile indicators are consistent with the person.", "test");
            relation_id = relation.id;
            require(context.investigation.claims_for_entity(username_id).size() == 1, "relation appears on subject entity");
            require(context.investigation.claims_for_entity(person_id).size() == 1, "relation appears on object entity");
            require_throw([&] { context.investigation.change_claim_status(relation.id, domain::ClaimStatus::Confirmed, "No support"); }, "confirmed claim without support");
            context.investigation.link_evidence(relation.id, file.id, domain::EvidenceRole::Supports);
            const auto second_claim = context.investigation.create_claim(case_id, "The profile was reachable at capture time", domain::ClaimKind::Observation, {{username_id, domain::ClaimEntityRole::Context}});
            context.investigation.link_evidence(second_claim.id, file.id, domain::EvidenceRole::Context);
            require(context.investigation.claims_for_evidence(file.id).size() == 2, "one evidence item can support two claims");
            context.investigation.unlink_evidence(relation.id, file.id);
            require(context.investigation.claims_for_evidence(file.id).size() == 1, "unlink leaves evidence on second claim");
            context.investigation.link_evidence(relation.id, file.id, domain::EvidenceRole::Supports);
            write_file(context.store.absolute_path(*file.relative_path), "temporary corruption\n");
            require(!context.investigation.verify_evidence(file.id).ok, "corruption remains detectable after claim creation");
            require(context.investigation.get_claim(relation.id).status == domain::ClaimStatus::Possible, "integrity check does not change claim status");
            {
                std::ofstream restored(context.store.absolute_path(*file.relative_path), std::ios::binary | std::ios::trunc);
                restored.write(reinterpret_cast<const char*>(preserved_bytes.data()), static_cast<std::streamsize>(preserved_bytes.size()));
            }
            context.investigation.change_claim_status(relation.id, domain::ClaimStatus::Refuted, "A later independent check contradicted the hypothesis.");
            const auto events = context.investigation.activity(case_id);
            bool saw_status_change = false;
            for (const auto& event : events) if (event.action == "status_changed" && event.occurred_at.size() > 0 && event.payload_json.find("possible") != std::string::npos && event.payload_json.find("refuted") != std::string::npos && event.payload_json.find("A later independent check") != std::string::npos) saw_status_change = true;
            require(saw_status_change, "status history records explanation");

            const auto confirmed_relation = context.investigation.create_relation(
                case_id,
                username_id,
                "directly_belongs_to",
                person_id,
                {},
                domain::ClaimStatus::Confirmed,
                "The preserved profile excerpt directly identifies the account.",
                "test",
                {{file.id, domain::EvidenceRole::Supports, "Direct identifying text"}},
                domain::ClaimKind::Observation);
            confirmed_relation_id = confirmed_relation.id;
            require(confirmed_relation.status == domain::ClaimStatus::Confirmed &&
                        confirmed_relation.kind == domain::ClaimKind::Observation &&
                        context.investigation.claim_evidence(confirmed_relation.id).size() == 1,
                    "confirmed relation can be created with selected kind and supporting evidence");

            const auto note = context.investigation.add_note(case_id, "Search phrase: john1337 at https://example.test/profile", "Failed lead");
            require(!context.investigation.search(case_id, "john1337").empty(), "search finds username");
            require(!context.investigation.search(case_id, "example.test").empty(), "search finds URL");
            require(!context.investigation.search(case_id, "Failed lead").empty(), "search finds note");

            const auto technique = context.playbooks.create_technique("Check commit history", "Find exact username reuse", "Search commit history for the exact username");
            const auto playbook = context.playbooks.create_playbook("Username investigation");
            technique_id = technique.id;
            playbook_id = playbook.id;
            context.playbooks.add_playbook_step(playbook.id, technique.id);
            const auto run = context.playbooks.start_run(case_id, playbook.id, username_id);
            run_id = run.id;
            auto run_steps = context.playbooks.list_run_steps(run.id);
            require(run_steps.size() == 1 && run_steps[0].name_snapshot == "Check commit history", "run captures template step");
            context.playbooks.link_step_evidence(run_steps[0].id, file.id);
            require(context.playbooks.evidence_for_step(run_steps[0].id).size() == 1 && context.playbooks.evidence_for_step(run_steps[0].id)[0] == file.id, "playbook step evidence association persists");
            context.playbooks.update_run_step(run_steps[0].id, domain::RunStepState::Done, "Checked the repository");
            context.playbooks.update_technique(technique.id, "Changed global technique", "New objective", "Changed instructions", "[]", "[]", "Changed later", "[]");
            run_steps = context.playbooks.list_run_steps(run.id);
            require(run_steps[0].state == domain::RunStepState::Done && run_steps[0].note == "Checked the repository" && run_steps[0].name_snapshot == "Check commit history" && run_steps[0].instructions_snapshot == "Search commit history for the exact username", "run snapshot is immutable");

            const auto report_path = root / "report.md";
            context.portability.write_report(case_id, report_path);
            std::ifstream report(report_path);
            const std::string report_text((std::istreambuf_iterator<char>(report)), std::istreambuf_iterator<char>());
            require(report_text.find("refuted") != std::string::npos, "report separates refuted assessment");
            require(report_text.find("## Confirmed conclusions\n\n- john1337 belongs_to John Doe") == std::string::npos, "refuted claim is not reported as confirmed");

            const auto archive = root / "case.zip";
            context.portability.export_case(case_id, archive);
            const auto exported_activity_count = context.investigation.activity(case_id).size();
            require(std::filesystem::is_regular_file(archive), "case archive created");

            const auto malformed_hash = root / "modified.zip";
            make_malformed_archive(archive, malformed_hash, true, false);
            const auto clean_hash_root = make_directory("bad-hash");
            try {
                Context clean_hash(clean_hash_root);
                require_throw([&] { clean_hash.portability.import_case(malformed_hash, import_export::ImportMode::SkipExisting); }, "modified attachment rejected");
                require(clean_hash.cases.list().empty(), "bad hash import creates no case");
            } catch (...) {
                std::filesystem::remove_all(clean_hash_root);
                throw;
            }

            const auto malformed_path = root / "unsafe.zip";
            make_malformed_archive(archive, malformed_path, false, true);
            const auto clean_path_root = make_directory("bad-path");
            try {
                Context clean_path(clean_path_root);
                require_throw([&] { clean_path.portability.import_case(malformed_path, import_export::ImportMode::SkipExisting); }, "unsafe path rejected");
                require(clean_path.cases.list().empty(), "unsafe path import creates no case");
            } catch (...) {
                std::filesystem::remove_all(clean_path_root);
                throw;
            }

            const auto clean_root = make_directory("roundtrip");
            try {
                Context clean(clean_root);
                const auto imported = clean.portability.import_case(archive, import_export::ImportMode::SkipExisting);
                require(imported.imported && imported.case_id == case_id, "clean import succeeds");
                const auto summary = clean.cases.list();
                require(summary.size() == 1 && summary[0].entity_count == 2 && summary[0].claim_count == 3, "roundtrip counts match");
                require(clean.investigation.list_sources(case_id).size() == 1 && clean.investigation.list_evidence(case_id).size() == 3, "roundtrip source and evidence counts match");
                require(clean.investigation.verify_evidence(file_evidence_id).ok, "roundtrip attachment verifies");
                require(clean.investigation.claim_evidence(relation_id).size() == 1 && clean.investigation.claim_evidence(relation_id)[0].evidence_id == file_evidence_id, "claim evidence association survives ZIP roundtrip");
                require(clean.investigation.get_claim(confirmed_relation_id).status == domain::ClaimStatus::Confirmed &&
                            clean.investigation.get_claim(confirmed_relation_id).kind == domain::ClaimKind::Observation &&
                            clean.investigation.claim_evidence(confirmed_relation_id).size() == 1 &&
                            clean.investigation.claim_evidence(confirmed_relation_id)[0].evidence_id == file_evidence_id,
                        "confirmed relation and its supporting evidence survive ZIP roundtrip");
                require(clean.investigation.get_evidence(text_evidence_id).text_content == "A quoted profile excerpt" && clean.investigation.get_evidence(text_evidence_id).quotation_location == "Profile section", "roundtrip text evidence matches");
                require(clean.investigation.list_notes(case_id).size() == 1, "roundtrip note persists");
                const auto imported_steps = clean.playbooks.list_run_steps(run_id);
                require(imported_steps.size() == 1 && clean.playbooks.evidence_for_step(imported_steps[0].id).size() == 1 && clean.playbooks.evidence_for_step(imported_steps[0].id)[0] == file_evidence_id, "playbook step evidence association survives ZIP roundtrip");
                const auto roundtrip_activity_count = clean.investigation.activity(case_id).size();
                require(roundtrip_activity_count == exported_activity_count + 2,
                        "roundtrip activity log matches plus import and verification events (actual=" + std::to_string(roundtrip_activity_count) + ", expected=" + std::to_string(exported_activity_count + 2) + ")");
                const auto copy = clean.portability.import_case(archive, import_export::ImportMode::ImportAsCopy);
                require(copy.imported && copy.case_id != case_id && clean.cases.list().size() == 2, "import as copy remaps case");
                require(clean.investigation.list_entities(copy.case_id).size() == 2, "copy remaps entity references");
            } catch (...) {
                std::filesystem::remove_all(clean_root);
                throw;
            }
        }
        {
            Context context(root);
            require(context.cases.get(case_id).id == case_id, "case remains readable after all workflows");
            require(context.playbooks.list_runs(case_id).size() == 1 && context.playbooks.list_run_steps(run_id).size() == 1, "playbook run persists");
            require(context.investigation.claim_evidence(relation_id).size() == 1 && context.investigation.claim_evidence(relation_id)[0].evidence_id == file_evidence_id, "claim evidence association persists after restart");
            require(context.investigation.get_claim(confirmed_relation_id).status == domain::ClaimStatus::Confirmed &&
                        context.investigation.get_claim(confirmed_relation_id).kind == domain::ClaimKind::Observation &&
                        context.investigation.claim_evidence(confirmed_relation_id).size() == 1,
                    "confirmed relation persists after restart");
            require(context.playbooks.evidence_for_step(context.playbooks.list_run_steps(run_id)[0].id).size() == 1, "playbook step evidence persists after restart");
            require_throw([&] { context.database.execute("INSERT INTO entities(id, case_id, type, label, created_at, updated_at) VALUES('bad', 'missing', 'person', 'bad', 'now', 'now');"); }, "foreign key constraint");
            require_throw([&] { context.database.execute("INSERT INTO cases(id, title, created_at, updated_at) VALUES('bad-case', '', 'now', 'now');"); }, "required case title constraint");
            require_throw([&] { context.database.execute("INSERT INTO cases(id, title, status, created_at, updated_at) VALUES('bad-status', 'Bad status', 'unknown', 'now', 'now');"); }, "case status constraint");

            require_throw([&] { context.investigation.delete_source(source_id, true); }, "source referenced by evidence cannot be deleted");
            const auto removable_source = context.investigation.create_source(case_id, domain::SourceType::Other, "temporary-source", "Temporary source");
            context.investigation.delete_source(removable_source.id, true);
            require_throw([&] { context.investigation.get_source(removable_source.id); }, "unlinked source is deleted");

            const auto removable_claim = context.investigation.create_claim(case_id, "Temporary claim for deletion", domain::ClaimKind::Observation, {{username_id, domain::ClaimEntityRole::Context}});
            context.investigation.link_evidence(removable_claim.id, text_evidence_id, domain::EvidenceRole::Context);
            require(context.investigation.claims_for_evidence(text_evidence_id).size() == 1, "temporary claim evidence link exists");
            context.investigation.delete_claim(removable_claim.id, true);
            require_throw([&] { context.investigation.get_claim(removable_claim.id); }, "claim is deleted");
            require(context.investigation.get_evidence(text_evidence_id).text_content == "A quoted profile excerpt", "claim deletion preserves evidence");

            context.playbooks.delete_technique(technique_id, true);
            require_throw([&] { context.playbooks.get_technique(technique_id); }, "technique is deleted");
            context.playbooks.delete_playbook(playbook_id, true);
            require_throw([&] { context.playbooks.get_playbook(playbook_id); }, "playbook is deleted");
            require(context.playbooks.list_runs(case_id).size() == 1, "run survives template deletion");

            require_throw([&] { context.investigation.delete_entity(username_id, true); }, "entity referenced by claims cannot be deleted");
            const auto unlinked_entity = context.investigation.create_entity(case_id, domain::EntityType::Other, "Temporary entity", "temporary-entity");
            deleted_entity_id = unlinked_entity.id;
            const auto linked_note = context.investigation.add_note(case_id, "The temporary entity was created for deletion testing", "Deletion test note", {unlinked_entity.id});
            context.investigation.delete_entity(unlinked_entity.id, true);
            require_throw([&] { context.investigation.get_entity(unlinked_entity.id); }, "unlinked entity is deleted");
            auto note_link_count = context.database.prepare("SELECT count(*) FROM note_entities WHERE note_id = ?;");
            note_link_count.bind(1, linked_note.id);
            note_link_count.step();
            require(note_link_count.column_int64(0) == 0 && context.investigation.list_notes(case_id).size() >= 2, "entity deletion removes note link but preserves note");
            bool saw_entity_deletion = false;
            for (const auto& event : context.investigation.activity(case_id)) {
                if (event.action == "deleted" && event.object_type == "entity" && event.object_id == unlinked_entity.id) saw_entity_deletion = true;
            }
            require(saw_entity_deletion, "entity deletion is logged");

            const auto recovery_case = context.cases.create("Attachment recovery", "Crash recovery fixture");
            const auto recovery_file = root / "recovery-attachment.txt";
            write_file(recovery_file, "attachment to restore after a simulated interruption");
            const auto recovery_evidence = context.investigation.add_file_evidence(recovery_case.id, recovery_file);
            const auto recovery_path = context.store.absolute_path(*recovery_evidence.relative_path);
            auto staged_recovery = context.store.stage_removal(*recovery_evidence.relative_path);
            require(!std::filesystem::exists(recovery_path), "staged removal moves attachment to quarantine");
            context.store.recover_staged_removals([&](const std::filesystem::path& relative_path) {
                auto statement = context.database.prepare("SELECT 1 FROM evidence WHERE relative_path = ? LIMIT 1;");
                statement.bind(1, relative_path.generic_string());
                return statement.step();
            });
            require(std::filesystem::is_regular_file(recovery_path), "owned staged attachment is restored");
            staged_recovery.temporary_path.clear();
            auto staged_orphan = context.store.stage_removal(*recovery_evidence.relative_path);
            context.database.execute("DELETE FROM evidence WHERE id = '" + recovery_evidence.id + "';");
            context.store.recover_staged_removals([&](const std::filesystem::path& relative_path) {
                auto statement = context.database.prepare("SELECT 1 FROM evidence WHERE relative_path = ? LIMIT 1;");
                statement.bind(1, relative_path.generic_string());
                return statement.step();
            });
            require(!std::filesystem::exists(recovery_path), "orphaned staged attachment is discarded");
            staged_orphan.temporary_path.clear();
            context.cases.delete_case(recovery_case.id, context.store);

            const auto missing_attachment_case = context.cases.create("Missing attachment safety", "Deletion failure fixture");
            const auto missing_attachment_file = root / "missing-attachment.txt";
            write_file(missing_attachment_file, "attachment that will be removed outside the service");
            const auto missing_attachment_evidence = context.investigation.add_file_evidence(missing_attachment_case.id, missing_attachment_file);
            const auto missing_attachment_path = context.store.absolute_path(*missing_attachment_evidence.relative_path);
            std::filesystem::remove(missing_attachment_path);
            require_throw([&] { context.cases.delete_case(missing_attachment_case.id, context.store); }, "case deletion rejects a missing attachment");
            require(context.cases.get(missing_attachment_case.id).id == missing_attachment_case.id, "missing attachment leaves case in database");
            require(context.investigation.get_evidence(missing_attachment_evidence.id).id == missing_attachment_evidence.id, "missing attachment leaves evidence record");
            write_file(missing_attachment_path, "restored attachment for cleanup");
            context.cases.delete_case(missing_attachment_case.id, context.store);

            const auto missing_evidence_case = context.cases.create("Missing evidence attachment", "Evidence deletion failure fixture");
            const auto missing_evidence_file = root / "missing-evidence.txt";
            write_file(missing_evidence_file, "evidence deletion must not remove the database row first");
            const auto missing_evidence = context.investigation.add_file_evidence(missing_evidence_case.id, missing_evidence_file);
            const auto missing_evidence_path = context.store.absolute_path(*missing_evidence.relative_path);
            std::filesystem::remove(missing_evidence_path);
            require_throw([&] { context.investigation.delete_evidence(missing_evidence.id, true); }, "evidence deletion rejects a missing attachment");
            require(context.investigation.get_evidence(missing_evidence.id).id == missing_evidence.id, "failed evidence deletion leaves its record");
            write_file(missing_evidence_path, "restored evidence attachment");
            context.investigation.delete_evidence(missing_evidence.id, true);
            context.cases.delete_case(missing_evidence_case.id, context.store);

            const auto database_failure_case = context.cases.create("Database failure safety", "Deletion rollback fixture");
            const auto database_failure_file = root / "database-failure.txt";
            write_file(database_failure_file, "attachment must be restored when SQL aborts");
            const auto database_failure_evidence = context.investigation.add_file_evidence(database_failure_case.id, database_failure_file);
            const auto database_failure_path = context.store.absolute_path(*database_failure_evidence.relative_path);
            context.database.execute("CREATE TRIGGER refuse_case_delete BEFORE DELETE ON cases WHEN OLD.id = '" + database_failure_case.id + "' BEGIN SELECT RAISE(ABORT, 'test database failure'); END;");
            require_throw([&] { context.cases.delete_case(database_failure_case.id, context.store); }, "case deletion rolls back on a database failure");
            require(context.cases.get(database_failure_case.id).id == database_failure_case.id, "database failure leaves case in database");
            require(std::filesystem::is_regular_file(database_failure_path), "database failure restores the attachment");
            context.database.execute("DROP TRIGGER refuse_case_delete;");
            context.cases.delete_case(database_failure_case.id, context.store);

            const auto deletion_case = context.cases.create("Case to delete", "Deletion workflow", "", std::nullopt, "", {"temporary"});
            const auto deletion_subject = context.investigation.create_entity(deletion_case.id, domain::EntityType::Username, "delete-user", "delete-user");
            const auto deletion_object = context.investigation.create_entity(deletion_case.id, domain::EntityType::Person, "Delete Person", "Delete Person");
            const auto deletion_source = context.investigation.create_source(deletion_case.id, domain::SourceType::Web, "https://example.test/delete", "Deletion source");
            const auto deletion_file_path = root / "deletion-evidence.txt";
            write_file(deletion_file_path, "evidence for deletion");
            const auto deletion_evidence = context.investigation.add_file_evidence(deletion_case.id, deletion_file_path, deletion_source.id);
            const auto deletion_claim = context.investigation.create_relation(deletion_case.id, deletion_subject.id, "belongs_to", deletion_object.id, "Delete user belongs to Delete Person", domain::ClaimStatus::Possible, "Deletion workflow test", "test", {{deletion_evidence.id, domain::EvidenceRole::Supports, "test"}});
            const auto deletion_run = context.playbooks.start_run(deletion_case.id, context.playbooks.list_playbooks().front().id);
            const auto deletion_step = context.playbooks.add_run_step(deletion_run.id, "Delete step", "Delete test step");
            context.playbooks.link_step_evidence(deletion_step.id, deletion_evidence.id);
            const auto deletion_attachment = context.store.absolute_path(*deletion_evidence.relative_path);
            require(std::filesystem::exists(deletion_attachment), "deletion fixture attachment exists");
            context.cases.delete_case(deletion_case.id, context.store);
            require(!std::filesystem::exists(deletion_attachment), "case deletion removes preserved attachments");
            require_throw([&] { context.cases.get(deletion_case.id); }, "deleted case is no longer readable");
            require_throw([&] { context.investigation.get_claim(deletion_claim.id); }, "deleted case claim is removed");
            require_throw([&] { context.playbooks.list_runs(deletion_case.id); }, "deleted case run is removed");
        }
        {
            Context context(root);
            require_throw([&] { context.investigation.get_entity(deleted_entity_id); }, "entity deletion persists after restart");
        }
    } catch (...) {
        std::filesystem::remove_all(root);
        throw;
    }
    std::filesystem::remove_all(root);
}

} // namespace

int main() {
    try {
        test_mvp();
        std::cout << "All integration tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
