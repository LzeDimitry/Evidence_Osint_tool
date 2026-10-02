Target: `docs/internal_spec/gui/behavior.md`
Operation: replace
Proposal revision: 1

## Reason and affected documents

Document the Linux desktop launcher, supplied application icon, and maximized startup behavior implemented for the Bazzite KDE Plasma report. Related proposals cover the GUI behavior, product specification, installation guide, and README.

## Discrepancies and verification

The previous installation documented only the executable files; it did not install a desktop entry or icon. The GUI previously showed without requesting maximized state. The GUI target built successfully with CMake, and `cmake --install build --prefix /tmp/evidence-trace-icon-check` installed the executable, desktop entry, and icon. Automated tests were not run.

## Complete proposed target content

<!-- BEGIN PROPOSED CONTENT -->
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

Top-level pages are Cases, Playbooks, and Settings. Case pages expose investigation records, notes/log, runs, and portability. Evidence preview is read-only with supported text/image rendering and fallback metadata. The map displays stored binary relations. The GUI starts maximized and uses the Evidence Trace artwork as its application and window icon; Linux installs register a desktop entry and hicolor icon.

## Dependencies and interfaces

Qt Core/Gui/Widgets; all core services; application context.

`gui/application_context.hpp`, page callbacks, dialogs, and `MainWindow`; services provide record operations.

## Verification and limitations

Source: [implementation](../../../src/gui/application_context.cpp). See [tests](../../development/tests.md) for verification commands and limitations. Source inspection does not imply that every workflow was executed. [Technical notes](tech_notes.md) record constraints before changes.
<!-- END PROPOSED CONTENT -->
