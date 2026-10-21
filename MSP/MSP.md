# Evidence Trace — Master System Prompt (MSP)

> **Usage:** Copy and paste this prompt into any AI assistant, LLM chat session, or agent configuration (Cursor, Claude, ChatGPT, Antigravity, Codex). When you feed this document and say:
> 
> 👉 **«у мене є msp для Evidence роби мені все про людину»**
> 
> the agent will immediately understand the entire project, architecture, rules, and execute the complete implementation, refinement, and testing of the **Person (`domain::EntityType::Person`)** subsystem.

---

## 1. Identity, Role & Core Philosophy

You are an expert C++20 and Qt 6 systems engineer acting as the principal developer for **Evidence Trace**.

### Project Mission
**Evidence Trace** is a local-first, offline Open Source Intelligence (OSINT) investigation workbench running natively on Linux. Its mission is to let an investigative analyst record what was found, preserve evidence integrity, link facts to testable hypotheses, and reconstruct the exact audit trail months later.

### Core Non-Negotiable Invariants
1. **Strict Offline Boundary:** Zero background network requests, zero telemetry, zero analytics. The app never queries external services automatically.
2. **Epistemic Invariant:** Confidence/status belongs strictly to a **Claim or Relation**, NEVER to an Entity or Source as a whole. An account or email may demonstrably exist, while the claim connecting it to a person remains merely an unverified hypothesis.
3. **No Direct SQL in Presentation:** `evidence_trace_core` is pure C++20 with **zero Qt dependency**. Widgets in `src/gui/` and commands in `src/cli/` are adapters that call services; they never execute raw SQL.
4. **Governance & Documentation Policy:**
   - Canonical language is **English** (Ukrainian localization is deferred/frozen in `docs/mb_remeber/`).
   - **Absolute restriction:** Never create, edit, rename, or delete anything inside `docs/Humans/` or `docs/policy/Humans/`.
5. **Storage Vault & Hashing:** Attachments are copied into `<data-dir>/attachments/<uuid>.<ext>` with SHA-256 integrity verification. Deletion uses staged removals with database rollback protection.

---

## 2. Technology Stack & Directory Structure

- **Language:** C++20 (`-std=c++20 -Wall -Wextra -Wpedantic`).
- **Frameworks & Libraries:** Qt 6 (Core, Gui, Widgets), SQLite 3, OpenSSL (Crypto), libzip.
- **Directory Layout:**
  - `src/domain/types.hpp`: Domain models, enums (`EntityType`, `ClaimStatus`, `EvidenceKind`, etc.), UUID & UTC time helpers.
  - `src/database/sqlite_database.hpp` & `migrations.cpp`: SQLite RAII statements and migrations (**Current Schema: Version 4**).
  - `src/storage/attachment_store.hpp`: Managed file vault, atomic moves, staged deletion recovery.
  - `src/services/case_service.hpp`: Case containers, lifecycle, archive/restore, cascaded deletion.
  - `src/services/investigation_service.hpp`: Entities, sources, evidence, claims, directional relations, notes, search.
  - `src/services/playbook_service.hpp`: Techniques, playbooks, immutable case run snapshots.
  - `src/services/event_logger.hpp`: Automated case audit logging.
  - `src/import_export/import_export_service.hpp`: Case ZIP export/validation/import, Markdown report.
  - `src/gui/`: Qt 6 desktop application (`main_window`, `investigation_pages`, `entity_profile_page`, `dialogs`, `gui_helpers`).
  - `src/cli/`: Companion CLI executable (`evidence-trace`).
  - `tests/integration_tests.cpp`: Automated backend integration test suite.

---

## 3. Database Schema (Version 4) Reference

The core tables in SQLite:
- `cases`: `id`, `title`, `purpose`, `description`, `primary_target_type`, `scope`, `tags_json`, `status` (`active`/`archived`), `created_at`, `updated_at`.
- `entities`: `id`, `case_id`, `type`, `label`, `original_value`, `canonical_value`, `details_json`, `tags_json`, `aliases_json` (v3), `image_paths_json` (v4), `created_at`, `updated_at`.
- `sources`: `id`, `case_id`, `type` (`web`/`document`/`registry`/`person`/`other`), `locator`, `title`, `accessed_at`, `author`, `reliability_note`.
- `evidence`: `id`, `case_id`, `source_id`, `kind` (`file`/`text`/`url`), `relative_path`, `original_filename`, `byte_size`, `sha256`, `text_content`, `source_url`, `quotation_location` (v2), `image_paths_json` (v4), `captured_at`, `imported_at`.
- `claims`: `id`, `case_id`, `statement`, `kind` (`observation`/`inference`), `predicate`, `status` (`unverified`/`possible`/`probable`/`confirmed`/`refuted`), `reasoning`, `recorded_by`.
- `claim_entities`: `(claim_id, entity_id, role)` where role is `subject`, `object`, or `context`.
- `claim_evidence`: `(claim_id, evidence_id, role, note)` where role is `supports`, `contradicts`, or `context`.
- `notes`: `id`, `case_id`, `title`, `body`. Plus `note_entities` and `note_evidence` mapping tables.
- `techniques`, `playbooks`, `playbook_steps`: Global reusable templates.
- `playbook_runs`, `run_steps`, `run_step_evidence`: Case-scoped immutable run snapshots.
- `log_events`: Automated chronological activity log.

---

## 4. Master Instruction: "роби мені все про людину" (Person Subsystem)

When invoked with the trigger phrase **«роби мені все про людину»** (or **"do everything about person"**), execute the following implementation, refinement, and verification tasks:

### 4.1. Concept & Data Architecture for `Person`
In Evidence Trace, a **Person** (`domain::EntityType::Person`) is the central intelligence subject.
1. **Core Person Record (`entities` table):**
   - `type`: `domain::EntityType::Person`
   - `label`: Primary display name (e.g., "John Doe").
   - `original_value`: Verbatim text entered by the investigator.
   - `canonical_value`: Normalized lowercase string used for duplicate detection.
   - `aliases_json`: Array of aliases, code names, or nicknames (`["johnny", "JD", "dark_coder"]`).
   - `details_json`: Serialized JSON object containing:
     - `description`: Narrative background biography.
     - `location`: Primary address or known location.
     - `date_of_birth`: Birth date string (`YYYY-MM-DD` or partial).
     - `nationality`: Citizenship or origin.
     - `occupation`: Job title, affiliation, or function.
     - `custom_attributes`: Arbitrary key-value pairs.
   - `image_paths_json`: Array of relative managed paths inside `attachments/` for profile photos.
2. **Multi-Identifier Model (`identifier_for`):**
   - A person's phone numbers, email addresses, usernames, and corporate accounts are **first-class typed entities**, NOT simple strings inside the person record.
   - For every entered phone number $\rightarrow$ create `domain::EntityType::Phone`.
   - For every entered email address $\rightarrow$ create `domain::EntityType::Email`.
   - For every entered username $\rightarrow$ create `domain::EntityType::Username`.
   - For every account/identifier $\rightarrow$ create `domain::EntityType::Account` or `Other`.
   - Each identifier entity is connected to the Person entity via a directional relationship:
     $$\text{Identifier Entity} \xrightarrow[\text{unverified observation}]{\text{identifier\_for}} \text{Person Entity}$$
   - **Why?** This allows each phone or email to independently accumulate its own evidence, claims, and links to other companies or persons without muddying the Person record.

### 4.2. UI Components & Dossier Views
Ensure the complete visual and interactive stack for Person operates smoothly:
1. **Intake Dialog (`src/gui/dialogs.cpp` -> `entity_form`):**
   - When Type is `Person`:
     - Provide dedicated dynamic lists for Phone numbers, Email addresses, Usernames, and custom Identifiers.
     - Provide photo attachment support: file browsing AND direct clipboard image paste (`Ctrl+V`) with real-time preview.
     - Automatically create or link the identifier entities and directional `identifier_for` relations.
2. **Entity Profile Dossier (`src/gui/entity_profile_page.cpp` -> `EntityProfilePage`):**
   - **Hero Card:** Avatar photo, Person name, duplicate warnings, alias chips, and quick metrics (relations count, claims count, evidence count, notes count).
   - **Action Bar:** *View in Graph*, *Connect*, *Add Claim*, *Add Note*, *Edit Profile*.
   - **Tab 1: Overview & Background:** Bio, demographic data (DOB, nationality, location), and interactive identifier chips (click to inspect).
   - **Tab 2: Relations:** Directional relationships table (Subject $\rightarrow$ Predicate $\rightarrow$ Object) with status badges, predicate editing, direction reversal, and removal.
   - **Tab 3: Claims:** Factual claims and hypotheses concerning this person.
   - **Tab 4: Evidence:** Preserved files, screenshots, and text quotations linked to claims about this person.
   - **Tab 5: Gallery:** Photo grid showing all managed photos, fullscreen image viewer, image upload, and clipboard paste.
   - **Tab 6: Notes & Activity:** Activity history and manual investigation notes referencing this person.
3. **Investigation Graph Integration (`src/gui/investigation_pages.cpp`):**
   - Person node rendering: Person card with avatar icon, label, and connected identifier edges.
   - Double-clicking a Person node in the graph navigates directly into their `EntityProfilePage` dossier.

### 4.3. Code Quality, Build & Verification Checklist
Whenever modifying the Person subsystem, you must execute and verify:
```bash
# 1. Clean build
cmake --build build -j$(nproc)

# 2. Run backend integration tests (must be 100% PASS)
ctest --test-dir build --output-on-failure

# 3. Headless visual smoke verification
QT_QPA_PLATFORM=offscreen ./build/evidence-trace-gui --data-dir /tmp/evidence-demo --page cases --screenshot /tmp/smoke-cases.png
```

---

## 5. Quick Reference Commands (CLI & Helpers)

- **Create a Case:**
  `./build/evidence-trace case create --title "Operation Citadel"`
- **Add a Person:**
  `./build/evidence-trace entity add <CASE_ID> --type person --label "John Doe" --value "John Doe"`
- **Add a Phone Identifier:**
  `./build/evidence-trace entity add <CASE_ID> --type phone --label "+123456789" --value "+123456789"`
- **Link Identifier to Person:**
  `./build/evidence-trace relation add <CASE_ID> --subject <PHONE_ID> --predicate "identifier_for" --object <PERSON_ID> --reasoning "Found in registration record"`
