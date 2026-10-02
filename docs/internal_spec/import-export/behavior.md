# Import, export, and reports behavior

Status: Approved

Implementation status: Implemented for the behavior described below. This describes current code; intended requirements remain in the [English product specification](../../spec/evidence-trace-spec.md).

## Purpose

Create portable case archives and readable reports, and validate case imports.

## Responsibilities

Validate archive structure and hashes; import records/files; remap copies; separate report assessments.

## Not responsible for

Merge into an existing case, export global templates, or treat reports as restorable backups.

## Main features and rules

ZIP archives contain case.json, manifest.json, and preserved attachments. Existing IDs can be skipped or imported as copies with internal references remapped. Malformed paths and hash mismatches reject import before creating a case. Reports separate confirmed, refuted, and unresolved assessments.

## Dependencies and interfaces

Domain; database; storage; EventLogger; libzip.

`import_export/import_export_service.hpp`: `ImportExportService`, `ImportMode`, `ImportResult`; JSON support is internal.

## Verification and limitations

Source: [implementation](../../../src/import_export/import_export_service.cpp). See [tests](../../development/tests.md) for verification commands and limitations. Source inspection does not imply that every workflow was executed. [Technical notes](tech_notes.md) record constraints before changes.
