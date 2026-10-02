# Architecture

Status: Approved

Implementation status: Implemented for the current capabilities described below; verification limitations are stated separately.

Module boundaries and interfaces are indexed in the [module registry](../internal_spec/modules_registry.md).

The application has one backend core and two thin frontends. The backend is
compiled as `evidence_trace_core` and has no Qt dependency. The GUI and CLI
depend on that core; they do not own persistence or investigation rules.

## Backend/core

The core is divided into:

- src/domain: stable IDs, enum values, UTC time helpers, and record types.
- src/database: SQLite RAII wrappers and sequential migrations.
- src/storage: immutable attachment staging, opaque paths, and SHA-256.
- src/services: case, investigation, activity log, and playbook business rules.
- src/import_export: JSON representation, ZIP validation, import remapping, and Markdown reports.
## Frontends/adapters

- src/gui: Qt 6 application context, dark visual theme, reusable dialogs, global pages, case pages, and the desktop shell.
- src/cli: scriptable command-line UI over the same application services.

The GUI is split by responsibility: `global_pages` owns cases, global playbooks, and settings; `investigation_pages` owns the overview, entities, claims, and evidence screens; `context_pages` owns notes/log, playbook runs, and export/import. Forms live in `dialogs`, and `main_window` only coordinates navigation and context lifetime. No business rule or SQLite query is implemented in a widget. This boundary is intentional: new functionality should be added to a core service first, then exposed through the GUI or CLI.

The configured data directory contains `evidence_trace.sqlite3` and the `attachments/` directory. Attachment paths are relative to that directory and are resolved through the storage module. Database records use UUID-like opaque IDs and UTC ISO-8601 timestamps. UI labels are English display strings; serialized enum values and user-entered case data remain stable across future localization.

The data directory is application data, not a source or build directory. For an installed application it should live in the operating system's per-user data location; the relative `data/` default is retained for development and scripted examples.
