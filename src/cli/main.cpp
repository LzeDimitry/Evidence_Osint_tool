#include "database/migrations.hpp"
#include "database/sqlite_database.hpp"
#include "domain/types.hpp"
#include "import_export/import_export_service.hpp"
#include "services/case_service.hpp"
#include "services/event_logger.hpp"
#include "services/investigation_service.hpp"
#include "services/playbook_service.hpp"
#include "storage/attachment_store.hpp"

#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {

using namespace evidence_trace;

struct ParsedOptions {
    std::vector<std::string> positional;
    std::map<std::string, std::string> values;
};

ParsedOptions parse_options(const std::vector<std::string>& arguments, std::size_t start = 0) {
    ParsedOptions result;
    for (std::size_t index = start; index < arguments.size(); ++index) {
        const auto& argument = arguments[index];
        if (argument.rfind("--", 0) != 0) {
            result.positional.push_back(argument);
            continue;
        }
        const auto equals = argument.find('=');
        if (equals != std::string::npos) {
            result.values[argument.substr(2, equals - 2)] = argument.substr(equals + 1);
        } else {
            const auto key = argument.substr(2);
            if (index + 1 < arguments.size() && arguments[index + 1].rfind("--", 0) != 0) result.values[key] = arguments[++index];
            else result.values[key] = "true";
        }
    }
    return result;
}

std::string required(const ParsedOptions& options, const std::string& key) {
    const auto iterator = options.values.find(key);
    if (iterator == options.values.end() || iterator->second.empty()) throw std::invalid_argument("--" + key + " is required");
    return iterator->second;
}

std::string optional(const ParsedOptions& options, const std::string& key, const std::string& fallback = {}) {
    const auto iterator = options.values.find(key);
    return iterator == options.values.end() ? fallback : iterator->second;
}

std::optional<std::string> optional_value(const ParsedOptions& options, const std::string& key) {
    const auto iterator = options.values.find(key);
    return iterator == options.values.end() ? std::nullopt : std::optional<std::string>(iterator->second);
}

bool has(const ParsedOptions& options, const std::string& key) {
    return options.values.contains(key);
}

std::vector<std::string> split(const std::string& value, char separator) {
    std::vector<std::string> result;
    std::string current;
    for (const char character : value) {
        if (character == separator) {
            if (!current.empty()) result.push_back(current);
            current.clear();
        } else current.push_back(character);
    }
    if (!current.empty()) result.push_back(current);
    return result;
}

std::vector<std::string> tags(const ParsedOptions& options) {
    const auto raw = optional(options, "tags");
    return raw.empty() ? std::vector<std::string>{} : split(raw, ',');
}

std::vector<domain::Id> id_list(const ParsedOptions& options, const std::string& key) {
    const auto raw = optional(options, key);
    return raw.empty() ? std::vector<domain::Id>{} : split(raw, ',');
}

std::vector<services::EntityLinkInput> entity_links(const std::string& raw) {
    std::vector<services::EntityLinkInput> result;
    for (const auto& value : split(raw, ',')) {
        const auto separator = value.find(':');
        if (separator == std::string::npos) throw std::invalid_argument("Entity links must use id:subject|object|context");
        result.emplace_back(value.substr(0, separator), domain::claim_entity_role_from_string(value.substr(separator + 1)));
    }
    return result;
}

std::vector<services::EvidenceLinkInput> evidence_links(const std::string& raw) {
    std::vector<services::EvidenceLinkInput> result;
    for (const auto& value : split(raw, ',')) {
        const auto first = value.find(':');
        if (first == std::string::npos) throw std::invalid_argument("Evidence links must use id:supports|contradicts|context[:note]");
        const auto second = value.find(':', first + 1);
        const auto id = value.substr(0, first);
        const auto role = domain::evidence_role_from_string(value.substr(first + 1, second == std::string::npos ? std::string::npos : second - first - 1));
        result.emplace_back(id, role, second == std::string::npos ? std::string{} : value.substr(second + 1));
    }
    return result;
}

void print_help() {
    std::cout << R"(Evidence Trace CLI

Usage: evidence-trace [--data-dir DIR] COMMAND SUBCOMMAND [options]

Foundation:
  init
  case create --title TITLE [--purpose TEXT --scope TEXT --tags a,b]
  case list [--search TEXT]
  case show CASE_ID
  case edit CASE_ID --title TITLE [--purpose TEXT --scope TEXT --tags a,b]
  case archive CASE_ID
  case restore CASE_ID

Investigation data:
  entity add CASE_ID --type TYPE --label LABEL [--value VALUE]
  entity list CASE_ID
  entity show ENTITY_ID
  source add CASE_ID --type web|document|registry|person|other --locator URL
  source list CASE_ID
  evidence file CASE_ID FILE [--source ID] [--source-url URL]
  evidence text CASE_ID --text TEXT [--source ID] [--location TEXT]
  evidence url CASE_ID URL [--source ID]
  evidence list CASE_ID
  evidence verify EVIDENCE_ID
  evidence delete EVIDENCE_ID --confirm
  claim add CASE_ID --statement TEXT --entities id:role,... [--kind observation|inference]
  relation add CASE_ID --subject ID --predicate PREDICATE --object ID --reasoning TEXT
  claim list CASE_ID
  claim show CLAIM_ID
  claim link CLAIM_ID EVIDENCE_ID --role supports|contradicts|context
  claim unlink CLAIM_ID EVIDENCE_ID
  claim status CLAIM_ID STATUS --explanation TEXT

Context and methodology:
  note add CASE_ID --body TEXT [--title TITLE --entities ID,ID --evidence ID,ID]
  search CASE_ID TEXT
  technique create --name NAME --steps TEXT
  technique list
  playbook create --name NAME
  playbook add-step --playbook ID [--technique ID] [--name NAME --instructions TEXT]
  playbook move-step --step STEP_ID --position NUMBER
  playbook runs --case CASE_ID
  playbook start --case CASE_ID --playbook ID [--entity ID]
  playbook run-steps --run RUN_ID
  playbook step --step STEP_ID --state todo|done|skipped [--note TEXT]

Portability:
  export archive CASE_ID --output FILE.zip
  export report CASE_ID --output FILE.md
  import FILE.zip --mode skip|copy
  log CASE_ID
)";
}

struct Application {
    database::Database database;
    storage::AttachmentStore store;
    services::EventLogger logger;
    services::CaseService cases;
    services::InvestigationService investigation;
    services::PlaybookService playbooks;
    import_export::ImportExportService portability;

    explicit Application(const std::filesystem::path& data_directory)
        : database(data_directory / "evidence_trace.sqlite3"),
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
};

void print_case(const domain::Case& value) {
    std::cout << value.id << "\nTitle: " << value.title << "\nStatus: " << domain::to_string(value.status)
              << "\nPurpose: " << value.purpose << "\nScope: " << value.scope << "\nUpdated: " << value.updated_at << "\n";
}

void run(Application& application, const std::vector<std::string>& arguments) {
    if (arguments.empty() || arguments[0] == "help" || arguments[0] == "--help") {
        print_help();
        return;
    }
    const auto command = arguments[0];
    const auto subcommand = arguments.size() > 1 ? arguments[1] : std::string{};
    const auto options = parse_options(arguments, arguments.size() > 1 ? 2 : 1);

    if (command == "init") {
        std::cout << "Database initialized at " << application.database.path() << "\n";
        return;
    }
    if (command == "case") {
        if (subcommand == "create") {
            const auto value = application.cases.create(required(options, "title"), optional(options, "purpose"), optional(options, "description"), optional_value(options, "target-type"), optional(options, "scope"), tags(options));
            std::cout << "CASE_ID=" << value.id << "\n";
        } else if (subcommand == "list") {
            for (const auto& value : application.cases.list(optional_value(options, "search"))) {
                std::cout << value.id << "\t" << value.title << "\t" << domain::to_string(value.status) << "\tupdated=" << value.updated_at << "\tentities=" << value.entity_count << "\tclaims=" << value.claim_count << "\n";
            }
        } else if (subcommand == "show") {
            if (options.positional.empty()) throw std::invalid_argument("case show requires CASE_ID");
            print_case(application.cases.get(options.positional[0]));
        } else if (subcommand == "edit") {
            if (options.positional.empty()) throw std::invalid_argument("case edit requires CASE_ID");
            const auto current = application.cases.get(options.positional[0]);
            const auto target = optional_value(options, "target-type");
            const auto case_tags = has(options, "tags") ? std::optional<std::vector<std::string>>(tags(options)) : std::nullopt;
            application.cases.update(current.id, optional(options, "title", current.title), optional(options, "purpose", current.purpose), optional(options, "description", current.description), target.has_value() ? target : current.primary_target_type, optional(options, "scope", current.scope), case_tags);
            std::cout << "OK\n";
        } else if (subcommand == "archive" || subcommand == "restore") {
            if (options.positional.empty()) throw std::invalid_argument("case archive/restore requires CASE_ID");
            if (subcommand == "archive") application.cases.archive(options.positional[0]); else application.cases.restore(options.positional[0]);
            std::cout << "OK\n";
        } else throw std::invalid_argument("Unknown case subcommand");
        return;
    }
    if (command == "entity") {
        if (subcommand == "add") {
            if (options.positional.empty()) throw std::invalid_argument("entity add requires CASE_ID");
            const auto type = domain::entity_type_from_string(required(options, "type"));
            const auto label = required(options, "label");
            const auto value = application.investigation.create_entity(options.positional[0], type, label, optional(options, "value", label), optional(options, "details", "{}"), tags(options), split(optional(options, "aliases"), ','));
            const auto duplicates = application.investigation.possible_duplicates(value.case_id, value.type, value.original_value);
            if (duplicates.size() > 1) std::cout << "WARNING=Possible duplicate within case\n";
            std::cout << "ENTITY_ID=" << value.id << "\n";
        } else if (subcommand == "list") {
            if (options.positional.empty()) throw std::invalid_argument("entity list requires CASE_ID");
            for (const auto& value : application.investigation.list_entities(options.positional[0])) std::cout << value.id << "\t" << domain::to_string(value.type) << "\t" << value.label << "\t" << value.original_value << "\n";
        } else if (subcommand == "show") {
            if (options.positional.empty()) throw std::invalid_argument("entity show requires ENTITY_ID");
            const auto value = application.investigation.get_entity(options.positional[0]);
            std::cout << value.id << "\nType: " << domain::to_string(value.type) << "\nLabel: " << value.label << "\nValue: " << value.original_value << "\n";
            for (const auto& claim : application.investigation.claims_for_entity(value.id)) std::cout << "Claim: " << claim.id << " [" << domain::to_string(claim.status) << "] " << claim.statement << "\n";
        } else throw std::invalid_argument("Unknown entity subcommand");
        return;
    }
    if (command == "source") {
        if (subcommand == "add") {
            if (options.positional.empty()) throw std::invalid_argument("source add requires CASE_ID");
            const auto value = application.investigation.create_source(options.positional[0], domain::source_type_from_string(required(options, "type")), optional(options, "locator"), optional(options, "title"), optional(options, "accessed-at"), optional(options, "author"), optional(options, "reliability"));
            std::cout << "SOURCE_ID=" << value.id << "\n";
        } else if (subcommand == "list") {
            if (options.positional.empty()) throw std::invalid_argument("source list requires CASE_ID");
            for (const auto& value : application.investigation.list_sources(options.positional[0])) std::cout << value.id << "\t" << domain::to_string(value.type) << "\t" << value.locator << "\t" << value.accessed_at << "\n";
        } else throw std::invalid_argument("Unknown source subcommand");
        return;
    }
    if (command == "evidence") {
        if (subcommand == "file") {
            if (options.positional.size() < 2) throw std::invalid_argument("evidence file requires CASE_ID FILE");
            const auto value = application.investigation.add_file_evidence(options.positional[0], options.positional[1], optional_value(options, "source"), optional_value(options, "source-url"), optional(options, "note"), optional_value(options, "mime"), optional(options, "captured-at"));
            std::cout << "EVIDENCE_ID=" << value.id << "\nSHA256=" << *value.sha256 << "\n";
        } else if (subcommand == "text") {
            if (options.positional.empty()) throw std::invalid_argument("evidence text requires CASE_ID");
            const auto value = application.investigation.add_text_evidence(options.positional[0], required(options, "text"), optional_value(options, "source"), optional_value(options, "source-url"), optional(options, "note"), optional(options, "location"), optional(options, "captured-at"));
            std::cout << "EVIDENCE_ID=" << value.id << "\n";
        } else if (subcommand == "url") {
            if (options.positional.size() < 2) throw std::invalid_argument("evidence url requires CASE_ID URL");
            const auto value = application.investigation.add_url_evidence(options.positional[0], options.positional[1], optional_value(options, "source"), optional(options, "note"), optional(options, "captured-at"));
            std::cout << "EVIDENCE_ID=" << value.id << "\nexternal only\n";
        } else if (subcommand == "list") {
            if (options.positional.empty()) throw std::invalid_argument("evidence list requires CASE_ID");
            for (const auto& value : application.investigation.list_evidence(options.positional[0])) std::cout << value.id << "\t" << domain::to_string(value.kind) << "\t" << (value.kind == domain::EvidenceKind::Url ? "external only" : value.original_filename.value_or(value.source_url.value_or("text excerpt"))) << "\n";
        } else if (subcommand == "verify") {
            if (options.positional.empty()) throw std::invalid_argument("evidence verify requires EVIDENCE_ID");
            const auto result = application.investigation.verify_evidence(options.positional[0]);
            std::cout << (result.ok ? "OK: " : "ERROR: ") << result.message << "\n";
            if (result.actual_sha256) std::cout << "SHA256=" << *result.actual_sha256 << "\n";
        } else if (subcommand == "delete") {
            if (options.positional.empty()) throw std::invalid_argument("evidence delete requires EVIDENCE_ID");
            application.investigation.delete_evidence(options.positional[0], has(options, "confirm"));
            std::cout << "OK\n";
        } else throw std::invalid_argument("Unknown evidence subcommand");
        return;
    }
    if (command == "claim" || command == "relation") {
        if (command == "relation" && subcommand == "add") {
            const auto value = application.investigation.create_relation(required(options, "case"), required(options, "subject"), required(options, "predicate"), required(options, "object"), optional(options, "statement"), domain::claim_status_from_string(optional(options, "status", "unverified")), required(options, "reasoning"), optional(options, "recorded-by"));
            std::cout << "CLAIM_ID=" << value.id << "\n";
            return;
        }
        if (subcommand == "add") {
            if (options.positional.empty()) throw std::invalid_argument("claim add requires CASE_ID");
            const auto value = application.investigation.create_claim(options.positional[0], required(options, "statement"), domain::claim_kind_from_string(optional(options, "kind", "observation")), entity_links(required(options, "entities")), optional_value(options, "predicate"), domain::claim_status_from_string(optional(options, "status", "unverified")), optional(options, "reasoning"), optional(options, "recorded-by"), has(options, "evidence") ? evidence_links(required(options, "evidence")) : std::vector<services::EvidenceLinkInput>{});
            std::cout << "CLAIM_ID=" << value.id << "\n";
        } else if (subcommand == "list") {
            if (options.positional.empty()) throw std::invalid_argument("claim list requires CASE_ID");
            for (const auto& value : application.investigation.list_claims(options.positional[0])) {
                std::int64_t supports = 0;
                std::int64_t contradicts = 0;
                for (const auto& link : application.investigation.claim_evidence(value.id)) {
                    if (link.role == domain::EvidenceRole::Supports) ++supports;
                    if (link.role == domain::EvidenceRole::Contradicts) ++contradicts;
                }
                std::cout << value.id << "\t" << domain::to_string(value.status) << "\t" << value.statement << "\tsupports=" << supports << "\tcontradicts=" << contradicts << "\n";
            }
        } else if (subcommand == "show") {
            if (options.positional.empty()) throw std::invalid_argument("claim show requires CLAIM_ID");
            const auto value = application.investigation.get_claim(options.positional[0]);
            std::cout << value.id << "\n[" << domain::to_string(value.status) << "] " << value.statement << "\nReasoning: " << value.reasoning << "\n";
            for (const auto& link : application.investigation.claim_entities(value.id)) std::cout << "Entity: " << link.entity_id << " (" << domain::to_string(link.role) << ")\n";
            for (const auto& link : application.investigation.claim_evidence(value.id)) std::cout << "Evidence: " << link.evidence_id << " (" << domain::to_string(link.role) << ")\n";
        } else if (subcommand == "link") {
            if (options.positional.size() < 2) throw std::invalid_argument("claim link requires CLAIM_ID EVIDENCE_ID");
            application.investigation.link_evidence(options.positional[0], options.positional[1], domain::evidence_role_from_string(required(options, "role")), optional(options, "note"));
            std::cout << "OK\n";
        } else if (subcommand == "unlink") {
            if (options.positional.size() < 2) throw std::invalid_argument("claim unlink requires CLAIM_ID EVIDENCE_ID");
            application.investigation.unlink_evidence(options.positional[0], options.positional[1]);
            std::cout << "OK\n";
        } else if (subcommand == "status") {
            if (options.positional.size() < 2) throw std::invalid_argument("claim status requires CLAIM_ID STATUS");
            application.investigation.change_claim_status(options.positional[0], domain::claim_status_from_string(options.positional[1]), required(options, "explanation"), optional_value(options, "reasoning"));
            std::cout << "OK\n";
        } else throw std::invalid_argument("Unknown claim subcommand");
        return;
    }
    if (command == "note") {
        if (subcommand != "add") throw std::invalid_argument("Unknown note subcommand");
        if (options.positional.empty()) throw std::invalid_argument("note add requires CASE_ID");
        const auto value = application.investigation.add_note(options.positional[0], required(options, "body"), optional(options, "title"), id_list(options, "entities"), id_list(options, "evidence"));
        std::cout << "NOTE_ID=" << value.id << "\n";
        return;
    }
    if (command == "search") {
        if (arguments.size() < 3) throw std::invalid_argument("search requires CASE_ID TEXT");
        for (const auto& value : application.investigation.search(arguments[1], arguments[2])) std::cout << value.object_type << "\t" << value.object_id << "\t" << value.title << "\t" << value.snippet << "\n";
        return;
    }
    if (command == "technique") {
        if (subcommand == "create") {
            const auto value = application.playbooks.create_technique(required(options, "name"), optional(options, "objective"), optional(options, "steps"), optional(options, "example-queries", "[]"), optional(options, "tools", "[]"), optional(options, "limitations"), optional(options, "tags-json", "[]"));
            std::cout << "TECHNIQUE_ID=" << value.id << "\n";
        } else if (subcommand == "list") {
            for (const auto& value : application.playbooks.list_techniques()) std::cout << value.id << "\t" << value.name << "\t" << value.steps << "\n";
        } else if (subcommand == "update") {
            application.playbooks.update_technique(required(options, "id"), required(options, "name"), optional(options, "objective"), required(options, "steps"), optional(options, "example-queries", "[]"), optional(options, "tools", "[]"), optional(options, "limitations"), optional(options, "tags-json", "[]"));
            std::cout << "OK\n";
        } else throw std::invalid_argument("Unknown technique subcommand");
        return;
    }
    if (command == "playbook") {
        if (subcommand == "create") {
            const auto value = application.playbooks.create_playbook(required(options, "name"), optional(options, "scope"), optional(options, "description"));
            std::cout << "PLAYBOOK_ID=" << value.id << "\n";
        } else if (subcommand == "list") {
            for (const auto& value : application.playbooks.list_playbooks()) std::cout << value.id << "\t" << value.name << "\n";
        } else if (subcommand == "add-step") {
            const auto value = application.playbooks.add_playbook_step(required(options, "playbook"), optional_value(options, "technique"), optional(options, "name"), optional(options, "instructions"));
            std::cout << "STEP_ID=" << value.id << "\n";
        } else if (subcommand == "move-step") {
            application.playbooks.move_playbook_step(required(options, "step"), std::stoll(required(options, "position")));
            std::cout << "OK\n";
        } else if (subcommand == "start") {
            const auto value = application.playbooks.start_run(required(options, "case"), required(options, "playbook"), optional_value(options, "entity"));
            std::cout << "RUN_ID=" << value.id << "\n";
        } else if (subcommand == "run-steps") {
            for (const auto& value : application.playbooks.list_run_steps(required(options, "run"))) std::cout << value.id << "\t" << value.position << "\t" << domain::to_string(value.state) << "\t" << value.name_snapshot << "\t" << value.instructions_snapshot << "\n";
        } else if (subcommand == "runs") {
            for (const auto& value : application.playbooks.list_runs(required(options, "case"))) std::cout << value.id << "\t" << value.name_snapshot << "\t" << value.started_at << "\n";
        } else if (subcommand == "step") {
            application.playbooks.update_run_step(required(options, "step"), domain::run_step_state_from_string(required(options, "state")), optional(options, "note"));
            std::cout << "OK\n";
        } else if (subcommand == "add-run-step") {
            const auto value = application.playbooks.add_run_step(required(options, "run"), required(options, "name"), optional(options, "instructions"));
            std::cout << "RUN_STEP_ID=" << value.id << "\n";
        } else if (subcommand == "link-step-evidence") {
            application.playbooks.link_step_evidence(required(options, "step"), required(options, "evidence"));
            std::cout << "OK\n";
        } else throw std::invalid_argument("Unknown playbook subcommand");
        return;
    }
    if (command == "export") {
        if (subcommand == "archive") application.portability.export_case(required(options, "case"), required(options, "output"));
        else if (subcommand == "report") application.portability.write_report(required(options, "case"), required(options, "output"));
        else throw std::invalid_argument("export requires archive or report");
        std::cout << "OK\n";
        return;
    }
    if (command == "import") {
        const auto mode = optional(options, "mode", "skip") == "copy" ? import_export::ImportMode::ImportAsCopy : import_export::ImportMode::SkipExisting;
        const auto archive = arguments.size() > 1 && arguments[1].rfind("--", 0) != 0 ? arguments[1] : required(options, "archive");
        const auto result = application.portability.import_case(archive, mode);
        std::cout << result.message << "\n";
        if (result.imported) std::cout << "CASE_ID=" << result.case_id << "\n";
        return;
    }
    if (command == "log") {
        if (arguments.size() < 2) throw std::invalid_argument("log requires CASE_ID");
        for (const auto& value : application.investigation.activity(arguments[1])) std::cout << value.occurred_at << "\t" << value.action << "\t" << value.description << "\n";
        return;
    }
    throw std::invalid_argument("Unknown command: " + command);
}

} // namespace

int main(int argc, char** argv) {
    try {
        std::vector<std::string> arguments;
        for (int index = 1; index < argc; ++index) arguments.emplace_back(argv[index]);
        std::filesystem::path data_directory = "data";
        if (arguments.size() >= 2 && arguments[0] == "--data-dir") {
            data_directory = arguments[1];
            arguments.erase(arguments.begin(), arguments.begin() + 2);
        }
        std::filesystem::create_directories(data_directory);
        Application application(data_directory);
        run(application, arguments);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\n";
        return 1;
    }
}
