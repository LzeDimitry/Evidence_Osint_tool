# Module registry

Status: Approved

This map describes the current source layout. Implementation labels are based on source inspection on 2026-10-01, not approval of the underlying documents or a fresh build. Features remain within their owning module when a separate module is unnecessary.

## Domain types

- Classification: Core technical module.
- Purpose: Shared record types, enum serialization, opaque identifiers, and UTC helpers.
- Responsibilities: Provide values exchanged by services and adapters.
- Not responsible for: Persist records, enforce investigation workflows, or render UI.
- Features/submodules: Records; enums; ID/time/string helpers.
- Dependencies: C++ standard library.
- Public interfaces: `domain/types.hpp`: records and conversion/helper functions.
- Implementation status: Implemented; qualifications are recorded in the linked documents.
- Documents: [Behavior](domain/behavior.md), [technical notes](domain/tech_notes.md).

## SQLite database

- Classification: Core technical module.
- Purpose: Own the SQLite connection, prepared statements, transactions, and schema migrations.
- Responsibilities: Apply sequential migrations and enable database integrity constraints.
- Not responsible for: Own claim assessments, file bytes, or GUI navigation.
- Features/submodules: RAII wrappers; migrations; starter template seed.
- Dependencies: SQLite; domain helpers used by migrations.
- Public interfaces: `database/sqlite_database.hpp` and `database/migrations.hpp`.
- Implementation status: Implemented; qualifications are recorded in the linked documents.
- Documents: [Behavior](database/behavior.md), [technical notes](database/tech_notes.md).

## Attachment storage

- Classification: Core technical module.
- Purpose: Preserve local evidence bytes and provide integrity and staged deletion operations.
- Responsibilities: Stage input, resolve safe relative paths, calculate SHA-256, quarantine removals, and recover staged removals.
- Not responsible for: Decide claim confidence, own SQLite records, or preview files.
- Features/submodules: File/byte staging; hashing; safe paths; deletion recovery.
- Dependencies: Filesystem; OpenSSL Crypto; an ownership callback supplied by callers.
- Public interfaces: `storage/attachment_store.hpp`: `AttachmentStore`, `StagedFile`, `StagedRemoval`.
- Implementation status: Implemented; qualifications are recorded in the linked documents.
- Documents: [Behavior](storage/behavior.md), [technical notes](storage/tech_notes.md).

## Case management

- Classification: Application service.
- Purpose: Create and maintain investigation containers and their lifecycle.
- Responsibilities: Create/list/update cases, archive/restore, and permanently delete records and attachments.
- Not responsible for: Create individual claims or evidence; maintain global playbook templates.
- Features/submodules: Metadata and tags; summaries/search; archive/restore; deletion.
- Dependencies: Domain; database; EventLogger; AttachmentStore for deletion.
- Public interfaces: `services/case_service.hpp`: `CaseService`.
- Implementation status: Implemented; qualifications are recorded in the linked documents.
- Documents: [Behavior](cases/behavior.md), [technical notes](cases/tech_notes.md).

## Investigation records

- Classification: Application service.
- Purpose: Maintain case-scoped entities, sources, evidence, claims, notes, and text search.
- Responsibilities: Validate same-case links, preserve evidence, record assessments, and log changes.
- Not responsible for: Own global templates, perform automated external collection, or infer truth.
- Features/submodules: Entities; sources/evidence; claims/relations; notes; search; activity access.
- Dependencies: Domain; database; AttachmentStore; EventLogger.
- Public interfaces: `services/investigation_service.hpp`: `InvestigationService`, entity/evidence link inputs.
- Implementation status: Implemented; qualifications are recorded in the linked documents.
- Documents: [Behavior](investigation/behavior.md), [technical notes](investigation/tech_notes.md).

## Playbooks and techniques

- Classification: Application service.
- Purpose: Maintain reusable global methods and case-specific execution snapshots.
- Responsibilities: Edit ordered templates, start runs, record run-step state/notes, and link resulting evidence.
- Not responsible for: Execute searches or tools automatically, or rewrite prior runs on template changes.
- Features/submodules: Techniques; ordered playbook steps; runs; local run steps; evidence associations.
- Dependencies: Domain; database; EventLogger; case/entity/evidence records.
- Public interfaces: `services/playbook_service.hpp`: `PlaybookService`.
- Implementation status: Implemented; qualifications are recorded in the linked documents.
- Documents: [Behavior](playbooks/behavior.md), [technical notes](playbooks/tech_notes.md).

## Activity logging

- Classification: Supporting service.
- Purpose: Record and retrieve case activity events.
- Responsibilities: Store readable descriptions, action/object identifiers, JSON payload, and UTC event time.
- Not responsible for: Provide cryptographic tamper evidence or edit existing events through a user workflow.
- Features/submodules: Event recording; case event listing.
- Dependencies: Domain; database; calling services.
- Public interfaces: `services/event_logger.hpp`: `EventLogger::record` and `list`.
- Implementation status: Implemented; qualifications are recorded in the linked documents.
- Documents: [Behavior](activity/behavior.md), [technical notes](activity/tech_notes.md).

## Import, export, and reports

- Classification: Core technical module.
- Purpose: Create portable case archives and readable reports, and validate case imports.
- Responsibilities: Validate archive structure and hashes; import records/files; remap copies; separate report assessments.
- Not responsible for: Merge into an existing case, export global templates, or treat reports as restorable backups.
- Features/submodules: JSON parser/representation; ZIP archive; manifest; import modes; Markdown report.
- Dependencies: Domain; database; storage; EventLogger; libzip.
- Public interfaces: `import_export/import_export_service.hpp`: `ImportExportService`, `ImportMode`, `ImportResult`; JSON support is internal.
- Implementation status: Implemented; qualifications are recorded in the linked documents.
- Documents: [Behavior](import-export/behavior.md), [technical notes](import-export/tech_notes.md).

## Desktop GUI

- Classification: Frontend adapter.
- Purpose: Expose interactive investigation workflows through Qt 6 Widgets.
- Responsibilities: Coordinate navigation/context lifetime, collect user input, invoke services, and render records/previews.
- Not responsible for: Own database schema or replace service business rules.
- Features/submodules: Global pages; investigation pages; context pages; forms; navigation; relationship map; helpers.
- Dependencies: Qt Core/Gui/Widgets; all core services; application context.
- Public interfaces: `gui/application_context.hpp`, page callbacks, dialogs, and `MainWindow`; services provide record operations.
- Implementation status: Implemented; qualifications are recorded in the linked documents.
- Documents: [Behavior](gui/behavior.md), [technical notes](gui/tech_notes.md).

## Command-line interface

- Classification: Frontend adapter.
- Purpose: Expose scripted core workflows and diagnostic commands.
- Responsibilities: Parse commands/options, initialize local data, call services, and print records/IDs or errors.
- Not responsible for: Own business rules or provide the desktop workflow.
- Features/submodules: Case/entity/evidence/claim operations; notes/search/log; playbooks; archive/report commands.
- Dependencies: Core domain, database, storage, and application services; C++ standard library.
- Public interfaces: `src/cli/main.cpp`: command dispatch and `help`; shared service APIs.
- Implementation status: Implemented; qualifications are recorded in the linked documents.
- Documents: [Behavior](cli/behavior.md), [technical notes](cli/tech_notes.md).

## Significant features and planned work

| Feature | Owner and classification | Purpose/responsibility | Non-responsibility | Dependencies and interface | Implementation status | Documents |
| --- | --- | --- | --- | --- | --- | --- |
| Claims and directional relations | Investigation; domain feature | Record assessments, entity roles, status history, evidence links | Automatic truth inference | Domain/database/activity; InvestigationService claim methods | Implemented | [Behavior](investigation/claims.md), [notes](investigation/tech_notes.md) |
| Sources and preserved evidence | Investigation; domain feature | Record provenance, preserved bytes, quotations, integrity results | Automated web capture | Storage/database/activity; evidence methods | Implemented | [Behavior](investigation/evidence.md), [notes](investigation/tech_notes.md) |
| Notes and case text search | Investigation; supporting feature | Store reasoning context and search text fields | Attachment indexing | Database/activity; add_note/search/activity | Partial: type/date search filtering is not established | [Behavior](investigation/behavior.md), [notes](investigation/tech_notes.md) |
| Relationship map | GUI; presentation feature | Navigate stored binary relations | Infer new relations or advanced graph analysis | Investigation records; GUI map rendering | Implemented for focused binary relations | [Behavior](gui/behavior.md), [notes](gui/tech_notes.md) |
| Dedicated timeline and time precision | Planned extension; owner not yet assigned | Review event chronology/precision | Change acquisition timestamps into event timestamps | Future domain/schema/UI interfaces unspecified | Planned | [Specification](../spec/evidence-trace-spec.md), [domain notes](domain/tech_notes.md) |
| External collection and metadata tools | Planned extension; owner not yet assigned | Return candidate information with provenance | Automatically confirm claims | Future integrations unspecified | Planned | [Specification](../spec/evidence-trace-spec.md), [investigation notes](investigation/tech_notes.md) |
| Ukrainian localization | Deferred frontend work | Localize display strings after owner direction | Translate case data or maintain frozen references | Future catalog unspecified | Planned; frozen in the current phase | [Policy](../policy/project_documentation_policy.md), [GUI notes](gui/tech_notes.md) |

Planned entries are not implemented modules and do not require new code or artificial directories.
