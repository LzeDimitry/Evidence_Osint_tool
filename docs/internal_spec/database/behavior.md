# SQLite database behavior

Status: Approved

Implementation status: Implemented for the behavior described below. This describes current code; intended requirements remain in the [English product specification](../../spec/evidence-trace-spec.md).

## Purpose

Own the SQLite connection, prepared statements, transactions, and schema migrations.

## Responsibilities

Apply sequential migrations and enable database integrity constraints.

## Not responsible for

Own claim assessments, file bytes, or GUI navigation.

## Main features and rules

Current schema version is 3. Version 2 adds quotation locations; version 3 adds entity aliases. Opening a newer schema rejects migration. Upgrades back up existing data before sequential migration transactions.

## Dependencies and interfaces

SQLite; domain helpers used by migrations.

`database/sqlite_database.hpp` and `database/migrations.hpp`.

## Verification and limitations

Source: [implementation](../../../src/database/migrations.cpp). See [tests](../../development/tests.md) for verification commands and limitations. Source inspection does not imply that every workflow was executed. [Technical notes](tech_notes.md) record constraints before changes.
