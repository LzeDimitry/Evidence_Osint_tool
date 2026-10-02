# Activity logging behavior

Status: Approved

Implementation status: Implemented for the behavior described below. This describes current code; intended requirements remain in the [English product specification](../../spec/evidence-trace-spec.md).

## Purpose

Record and retrieve case activity events.

## Responsibilities

Store readable descriptions, action/object identifiers, JSON payload, and UTC event time.

## Not responsible for

Provide cryptographic tamper evidence or edit existing events through a user workflow.

## Main features and rules

Services add events for meaningful operations. Existing event records are read through case activity workflows; corrections are new events rather than edits in the UI.

## Dependencies and interfaces

Domain; database; calling services.

`services/event_logger.hpp`: `EventLogger::record` and `list`.

## Verification and limitations

Source: [implementation](../../../src/services/event_logger.cpp). See [tests](../../development/tests.md) for verification commands and limitations. Source inspection does not imply that every workflow was executed. [Technical notes](tech_notes.md) record constraints before changes.
