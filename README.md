# Evidence Trace

Status: Approved

Evidence Trace is a local-first OSINT investigation workbench. It keeps cases, entities, sources, preserved evidence, claims, relations, playbook runs, notes, activity history, and portable backups together so an investigation can be resumed and reconstructed later.

The application is C++20 with a SQLite data store, CMake builds, and a Qt 6 desktop interface. The core has no UI dependency: domain records, migrations, attachment storage, business rules, and ZIP/report import/export are reusable from both the GUI and the CLI. English is the source language; stable IDs, enum values, and stored user data are independent of future Ukrainian localization.

## Working functionality

The current implementation provides:

- A dark Qt 6 desktop shell with Cases, Playbooks, and Settings sections.
- A shared responsive visual system: readable 11pt body text, larger 40px controls, 52px table rows, bounded inspector cards, deliberate cold-purple accents, useful empty states, tooltips for elided values, and vertical stacking/scrolling at small window sizes.
- Case creation, editing, search, opening/reopening, archive/restore, permanent deletion through an explicit context-menu confirmation, overview counts, recent activity, unresolved claims, and visible tag chips with full-value tooltips. Case tags accept whitespace separators (commas are also accepted).
- Entity list/detail screens with type filters, label/value/alias/tag search, aliases, tags, descriptions, related claims, history, edit forms, possible-duplicate warnings, and right-click actions for editing, related-claim navigation, and safe permanent deletion.
- Claims and directional relations with subject/predicate/object, categorical status, selectable observation/inference kind (relations default to inference), reasoning, entity links, support/contradiction/context evidence roles, status explanations, status history, filters, and case search.
- A data-driven relationship map for binary claims: stable entity nodes, directed predicate arrows, collision-avoiding labels, fit-to-view, wheel zoom, drag pan, and clicks that open the selected entity or claim record. It is a focused case graph, not an automatic inference engine.
- Separate source records and evidence records for preserved files, text quotations, and external-only URLs. File evidence is copied to opaque local storage and verified with SHA-256.
- Context menus on actionable record tables: claims, evidence, sources, playbooks, techniques, playbook steps, runs, run steps, manual notes, activity events, overview activity, and unresolved claims. Actions are wired to the same services as the visible buttons; destructive actions confirm and protected references explain why deletion is refused.
- The claim and playbook-run association workflows can select an existing preserved evidence item or import a new local file, then persist the exact association. When a new claim or relation is created with initial status `confirmed`, the GUI opens this supporting-evidence step before creation; imported files are copied and hashed before linking, and the association survives case reopen and ZIP export/import.
- Global technique/playbook templates and case-specific runs. Runs snapshot step text, keep independent state/result notes, and can link evidence.
- Manual investigation notes, automatic activity log, and cross-case top-bar search for cases, entities, claims, sources, preserved evidence, notes, and run steps. Case-list search remains title/tag scoped.
- Case ZIP export/import with validation, skip/import-as-copy modes, reference remapping, attachment hashes, and an English Markdown report.
- Safe in-app read-only previews for text quotations, local text files, and images. Unsupported local files show metadata and a no-execution fallback; external-only URLs are displayed without navigation.
- Local data settings, schema/attachment policy display, and explicit integrity/import error states.
- GUI timestamps use one explicit convention throughout the application: `YYYY-MM-DD HH:MM UTC`.
- A small CLI for scripted workflows and recovery while the desktop UI is being developed.
- A CTest integration suite covering persistence, SQLite constraints, evidence integrity, claim history, playbook snapshots, search, reports, ZIP round trips, ID remapping, and malformed archives.

## Current limitations

- The relationship map is now implemented for stored binary claims and relations. Advanced graph work remains planned: richer automatic layout for very dense cases, multi-edge routing beyond the current collision-tested labels, richer graph filters/toolbar actions, and a dedicated timeline view. The graph never creates inferences; it only visualizes stored records.
- There is no automatic web capture, link checking, RDAP/DNS, username search, image search, or other external-service automation.
- There is no encryption, multi-user account model, cloud synchronization, PDF report, or full-text indexing inside PDFs/images. The global search does not inspect the bytes of attached PDFs or images.
- Permanent case deletion is irreversible; archive is the recommended reversible lifecycle action.
- Entity deletion is protected while claims reference the entity; review those claims first so the application never leaves invalid claim records.
- English strings are organized in UI code and stored data remains language-neutral, but a complete external translation catalog and Ukrainian translation are still planned.

The authoritative behavior and acceptance IDs remain in [`docs/spec/evidence-trace-spec.md`](docs/spec/evidence-trace-spec.md). GUI reference images live in [`docs/design/gui/`](docs/design/gui/).

## Requirements

The supported installation path is Linux. To build and install from a GitHub source download, first install the build dependencies. On Debian or Ubuntu:

```sh
sudo apt update
sudo apt install build-essential cmake qt6-base-dev libsqlite3-dev libssl-dev libzip-dev
```

Download the repository with GitHub's **Code → Download ZIP** button and extract it, or clone it with Git. From the extracted repository directory, run:

```sh
./install.sh
```

The script configures a Release build, compiles the project, and installs the GUI and CLI into `~/.local/bin` by default. It does not install system packages or require root privileges. To choose another location or build directory:

```sh
./install.sh --prefix "$HOME/.local" --build-dir ./build-install --jobs 4
```

If `~/.local/bin` is not already on your `PATH`, add it to your shell's startup configuration. Then launch the desktop app with `evidence-trace-gui` or use the CLI with `evidence-trace --help`. After the source is downloaded, building and installing it runs locally without network access.

The required development components are:

- Bash, to run `install.sh`.
- CMake 3.20 or newer.
- A compiler with C++20 support.
- Qt 6 development packages: Core, Gui, and Widgets.
- SQLite3 development headers and library.
- OpenSSL Crypto development headers and library.
- libzip development headers and library.

The application has no runtime network dependency and does not make background requests or telemetry calls.

## Build

From the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
```

This builds:

- `build/evidence-trace-gui` — Qt 6 desktop application.
- `build/evidence-trace` — CLI.
- `build/evidence_trace_tests` — integration test executable.

To install the built GUI and CLI instead of running them from the build tree, use `cmake --install build --prefix "$HOME/.local"`. The `install.sh` instructions above automate a Release build and this installation step.

## Run the desktop application

```sh
./build/evidence-trace-gui --data-dir /tmp/evidence-trace-demo
```

For a smaller window or a repeatable screenshot, pass `--window-size WIDTHxHEIGHT`:

```sh
./build/evidence-trace-gui --data-dir /tmp/evidence-trace-demo --window-size 900x600
```

The GUI also supports repeatable smoke screenshots and opening a known case section:

```sh
QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1 ./build/evidence-trace-gui \
  --data-dir /tmp/evidence-trace-demo \
  --open-case CASE_ID --section claims \
  --screenshot /tmp/evidence-trace-claims.png
```

For global pages, use `--page cases`, `--page playbooks`, or `--page settings`. `--help` prints the complete GUI option form. `--window-size 1440x900` and `--window-size 900x600` were both run against the implementation during the GUI smoke pass.

## Test

Run the integration suite with:

```sh
ctest --test-dir build --output-on-failure
```

The test creates isolated temporary data directories and removes them after each run. It verifies database constraints, case and alias persistence across a restart, evidence hashes and file independence, claim status history, playbook snapshot immutability, evidence links on claims and run steps, search, report generation, ZIP round trips, ID remapping, and malformed archive rejection.

The verified GUI smoke commands include:

```sh
QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1 ./build/evidence-trace-gui \
  --data-dir build/data \
  --open-case f0b3835b-51a8-4782-a429-a4bcb213de8b \
  --section claims --window-size 1440x900 \
  --screenshot /tmp/final11-claims-1440.png

QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1 ./build/evidence-trace-gui \
  --data-dir build/data \
  --page settings --window-size 1920x1080 \
  --screenshot /tmp/final10-settings-1920.png
```

The current verification environment does not provide a usable native Wayland/X11 display: Qt's `xcb` plugin cannot initialize because the available display lacks `xcb-cursor`, and an Xvfb attempt also aborted. The screenshot commands therefore prove fresh offscreen rendering only; native mouse interaction still needs to be checked on a normal desktop.

## CLI quick start

All commands accept `--data-dir DIR`; if it is omitted, data is stored in `data/`. The command creates `evidence_trace.sqlite3` and an `attachments/` directory inside that location.

Create and reopen a case:

```sh
./build/evidence-trace --data-dir /tmp/evidence-trace-demo init
./build/evidence-trace --data-dir /tmp/evidence-trace-demo case create \
  --title "Username investigation" \
  --purpose "Assess a possible account relationship" \
  --scope "Public sources only" --tags osint,mvp
./build/evidence-trace --data-dir /tmp/evidence-trace-demo case list
```

Add investigation material:

```sh
./build/evidence-trace --data-dir /tmp/evidence-trace-demo entity add CASE_ID \
  --type username --label john1337 --value john1337 --aliases john1337-old
./build/evidence-trace --data-dir /tmp/evidence-trace-demo source add CASE_ID \
  --type web --locator https://example.test/profile --title "Profile"
./build/evidence-trace --data-dir /tmp/evidence-trace-demo evidence url CASE_ID \
  https://example.test/profile
./build/evidence-trace --data-dir /tmp/evidence-trace-demo evidence file CASE_ID \
  ./screenshot.png
```

Create and assess a relation:

```sh
./build/evidence-trace --data-dir /tmp/evidence-trace-demo relation add \
  --case CASE_ID --subject USERNAME_ENTITY_ID --predicate belongs_to \
  --object PERSON_ENTITY_ID --reasoning "Visible indicators are consistent."
./build/evidence-trace --data-dir /tmp/evidence-trace-demo claim link \
  CLAIM_ID EVIDENCE_ID --role supports
./build/evidence-trace --data-dir /tmp/evidence-trace-demo claim status \
  CLAIM_ID confirmed --explanation "The preserved item supports the assessment."
```

Export, report, and restore:

```sh
./build/evidence-trace --data-dir /tmp/evidence-trace-demo export archive \
  --case CASE_ID --output /tmp/case.zip
./build/evidence-trace --data-dir /tmp/evidence-trace-demo export report \
  --case CASE_ID --output /tmp/case.md
./build/evidence-trace --data-dir /tmp/clean-evidence-trace import \
  /tmp/case.zip --mode skip
```

Run `./build/evidence-trace help` for the complete command list. IDs printed by create commands are the IDs used by later commands.

## Data and safety notes

The data directory is local application data and should be protected by the operating system. Evidence files are copied into opaque-ID paths; they are not edited in place. Integrity verification detects changes relative to the stored SHA-256 and does not prove authenticity or protect against a machine owner modifying the database.

Archive import validates the schema, manifest, sizes, hashes, path traversal, and ZIP symlink attributes before writing a case. It does not execute archive content or open URLs automatically. Export does not delete the source case.

See [`docs/development/architecture.md`](docs/development/architecture.md), [`docs/development/build.md`](docs/development/build.md), [`docs/development/tests.md`](docs/development/tests.md), and [`docs/spec/acceptance.md`](docs/spec/acceptance.md) for implementation details and the current acceptance matrix.
