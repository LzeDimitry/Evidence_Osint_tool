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

Top-level pages are Cases, Playbooks, and Settings. Case pages expose investigation records, notes/log, runs, and portability. Evidence preview is read-only with supported text/image rendering and fallback metadata. The map displays stored binary relations.

## Dependencies and interfaces

Qt Core/Gui/Widgets; all core services; application context.

`gui/application_context.hpp`, page callbacks, dialogs, and `MainWindow`; services provide record operations.

## Verification and limitations

Source: [implementation](../../../src/gui/application_context.cpp). See [tests](../../development/tests.md) for verification commands and limitations. Source inspection does not imply that every workflow was executed. [Technical notes](tech_notes.md) record constraints before changes.
