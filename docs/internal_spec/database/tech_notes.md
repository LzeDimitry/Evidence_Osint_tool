# SQLite database technical notes

Status: Approved

Implementation status: Implemented for the constraints described here.

## Constraints before changes

SQLite and attachment operations are not one transaction. Preserve foreign-key and CHECK constraints. See [migration details](../../development/migrations.md) before changing versions or seed data.

## Evidence and checks

Read [behavior](behavior.md), [source](../../../src/database/migrations.cpp), the [canonical specification](../../spec/evidence-trace-spec.md), and [test guidance](../../development/tests.md) before modifying this module. Schema v4 keeps image bytes in the managed attachment store and adds JSON lists of safe relative paths to entity/evidence records. Archive import/export carries image ownership in case JSON and uses the archive manifest for byte integrity. Build and CTest passed on 2026-10-16; native clipboard and graph interaction were unavailable.

Record any new mismatch as a proposal under the [documentation policy](../../policy/project_documentation_policy.md).
