# Command-line interface behavior

Status: Approved

Implementation status: Implemented for the behavior described below. This describes current code; intended requirements remain in the [English product specification](../../spec/evidence-trace-spec.md).

## Purpose

Expose scripted core workflows and diagnostic commands.

## Responsibilities

Parse commands/options, initialize local data, call services, and print records/IDs or errors.

## Not responsible for

Own business rules or provide the desktop workflow.

## Main features and rules

Commands use a configurable data directory, default data/. Creation commands print IDs that can be passed into subsequent operations. Run help for the actual command surface.

## Dependencies and interfaces

Core domain, database, storage, and application services; C++ standard library.

`src/cli/main.cpp`: command dispatch and `help`; shared service APIs.

## Verification and limitations

Source: [implementation](../../../src/cli/main.cpp). See [tests](../../development/tests.md) for verification commands and limitations. Source inspection does not imply that every workflow was executed. [Technical notes](tech_notes.md) record constraints before changes.
