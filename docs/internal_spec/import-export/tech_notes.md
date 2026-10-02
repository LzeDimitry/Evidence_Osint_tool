# Import, export, and reports technical notes

Status: Approved

Implementation status: Implemented for the constraints described here.

## Constraints before changes

Archive format version is 1; import currently requires schema version 3. Limits: 100 MiB per entry, 1 GiB total uncompressed bytes, 10,000 entries. Reads are held in memory; bounds are not a streaming-import guarantee. Preserve duplicate-path, traversal, symlink, manifest, and reference validation.

## Evidence and checks

Read [behavior](behavior.md), [source](../../../src/import_export/import_export_service.cpp), the [canonical specification](../../spec/evidence-trace-spec.md), and [test guidance](../../development/tests.md) before modifying this module. Current source and tests were inspected on 2026-10-01. The existing integration executable passed; no fresh build or native desktop check is claimed.

Record any new mismatch as a proposal under the [documentation policy](../../policy/project_documentation_policy.md).
