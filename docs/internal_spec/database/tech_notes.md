# SQLite database technical notes

Status: Approved

Implementation status: Implemented for the constraints described here.

## Constraints before changes

SQLite and attachment operations are not one transaction. Preserve foreign-key and CHECK constraints. See [migration details](../../development/migrations.md) before changing versions or seed data.

## Evidence and checks

Read [behavior](behavior.md), [source](../../../src/database/migrations.cpp), the [canonical specification](../../spec/evidence-trace-spec.md), and [test guidance](../../development/tests.md) before modifying this module. Current source and tests were inspected on 2026-10-01. The existing integration executable passed; no fresh build or native desktop check is claimed.

Record any new mismatch as a proposal under the [documentation policy](../../policy/project_documentation_policy.md).
