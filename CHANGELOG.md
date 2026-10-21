# Changelog

Status: Approved

## Unreleased

- Made the investigation graph the default case workspace and added visual two-click connection mode with cancel/preview, lightweight predicate entry, relationship edit/reverse/remove actions, status-colored edges, and per-case saved graph layout/view state.
- Added service operations and integration coverage for changing relationship type and direction while preserving evidence links.

- Simplified case creation to require only a name, with purpose and other metadata optional.
- Simplified entity, source, evidence, claim, note, technique, and playbook forms by keeping secondary fields optional.
- Added optional entity/evidence links to investigation notes and visible unlink actions for claim evidence.
- Added local appearance controls for accent color, control density, text size, font, and selected case-tab glow.
- Applied the selected accent to navigation, case tabs, record tables, claim details, and relationship-map nodes/edges; made the preview visibly non-interactive.
- Simplified claim and relation entry: Observation is the default, relation statements are suggested from the selected entities and predicate, and the kind selection persists when switching record modes.
- Selected newly created claims and relations immediately; the detail panel identifies directional relations separately from entity claims.
- Clarified empty claim/entity-filter results, reserved room for related-claim status badges, and made statement columns adapt to the entity detail panel width.
- Clarified the Appearance labels for control density and the non-mutating preview.
- Advanced the case last-updated time for logged changes and refreshed evidence counts immediately after source/evidence actions.
- Persisted the selected data folder across application launches and refreshed case overview counts when returning to the page.
- Disabled claim creation until a case has an entity, with guidance and automatic refresh after entity changes.
- Updated entity, claim, source, and run icons to better match their meaning.
- Refreshed Runs and Export summaries when opened, and updated the visible case timestamp after claim, note, and run changes.
- Corrected CLI help and dispatch for relation creation, export arguments, optional import mode, and evidence-deletion confirmation.
- Preserved the selected record and detail panel when a case page refreshes.
- Added text/kind/source filters for evidence and a Ctrl+K shortcut for global search.
- Installed the Linux desktop launcher and application icon, and start the GUI maximized.
- Replaced the full system-font chooser with a short, readable font list and a System default option.
- Matched Playbook edit, claim evidence, Export, and Event target icons to their actions and record types.
- Clarified the Appearance sample panel and expanded the Claims status filter so its selected value is readable.
- Refresh claim-table evidence counts immediately after linking evidence from the inspector.
- Show actionable guidance when Claims or Entities filters return no matches.

## 0.1.0 — 2026-09-24

- Added the C++20/SQLite/CMake foundation and schema migrations.
- Added case lifecycle, entities, sources, evidence preservation, SHA-256 checks, claims, relations, notes, search, activity events, techniques, playbooks, and run snapshots.
- Added ZIP archive export/import validation, copy remapping, Markdown reports, CLI commands, and integration tests for AC-01 through AC-12.
- Added the Qt 6 desktop shell with cases, overview, entities, claims/relations, sources/evidence, notes/log, playbook runs, export/import, global playbooks, and local settings screens.
- Added schema migration 3 for persistent entity aliases and GUI page/screenshot smoke options.
- Deferred the Ukrainian translation catalog, external integrations, graph/timeline views, encryption, and multi-user support.
