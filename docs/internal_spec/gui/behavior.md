# Desktop GUI behavior

Status: Approved

Implementation status: Implemented for the behavior described below. This describes current code; intended requirements remain in the [English product specification](../../spec/evidence-trace-spec.md).

## Purpose

Expose interactive investigation workflows through Qt 6 Widgets.

## Responsibilities

Coordinate navigation/context lifetime, collect user input, invoke services, and render records/previews.

## Not responsible for

Own database schema or replace service business rules.

## Main features and rules

Top-level pages are Cases, Playbooks, and Settings. The Linux application starts maximized with the supplied Evidence Trace application/window icon; CMake installs the desktop entry and hicolor icon. Global search focuses with Ctrl+K. Opening a case selects the Investigation Graph page by default. Entity, source, evidence, claim, note, technique, and playbook forms expose primary fields first and keep secondary fields optional. Person creation accepts multiple identifier values and stores them as typed entities with unverified directional relations. The graph displays every case entity, including isolated nodes. Graph controls and canvas double-click create entities without leaving the graph. Toolbar Connect and node-context Add connection enter a two-click selection mode with a dashed preview, Escape/empty-space cancellation, and a short predicate prompt; new relations are unverified observations. Node dragging, panning, zoom, search, and fit are available. Node positions and zoom/view center persist per case in local QSettings. Edge colors/tooltips communicate status. Relationship context menus inspect, change predicate, reverse direction, and remove the ordinary relation claim. The relation inspector exposes status, reasoning, and evidence. Single node click selects; double-click/Open entity navigates to the specialized entity inspector. Relationship links remain claims and retain evidence/status semantics. Link evidence accepts existing records, a new URL, a text observation, or a preserved file. Entity creation and evidence forms support managed images and clipboard image paste; Ctrl+V shortcut handling activates only when the clipboard contains an image. Claims require at least one entity. Evidence, claim, type, and entity filters show useful no-match guidance. The Appearance page applies and persists accent, control density, text size, font, and case-tab glow. It identifies the example as “Sample panel” and explains that appearance choices affect the interface. Relation statements are suggested from subject, predicate, and object; the selected kind persists across record modes, and saved records appear selected in the inspector. The inspector labels directional relations “Relation” and entity claims “Claim”. Related-claim badges have reserved width and readable statements. Action icons correspond to their operation and evidence type.

## Dependencies and interfaces

Qt Core/Gui/Widgets; all core services; application context.

`gui/application_context.hpp`, page callbacks, dialogs, and `MainWindow`; services provide record operations.

## Verification and limitations

Source: [implementation](../../../src/gui/application_context.cpp), [case forms](../../../src/gui/dialogs.cpp), [appearance helpers](../../../src/gui/gui_helpers.cpp), and [page behavior](../../../src/gui/investigation_pages.cpp). On 2026-10-16 the CMake build and CTest passed; service tests cover relationship type/direction persistence and evidence retention. Offscreen GUI captures verified default graph navigation and rendering for empty and populated cases. Native mouse-driven context menu, connection, and drag workflows remain unverified in this environment. Smaller layouts and performance targets remain unverified. See [tests](../../development/tests.md) for verification commands and limits. [Technical notes](tech_notes.md) record constraints before changes.
