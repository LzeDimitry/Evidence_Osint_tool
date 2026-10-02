# Investigation records behavior

Status: Approved

Implementation status: Implemented for the behavior described below. This describes current code; intended requirements remain in the [English product specification](../../spec/evidence-trace-spec.md).

## Purpose

Maintain case-scoped entities, sources, evidence, claims, notes, and text search.

## Responsibilities

Validate same-case links, preserve evidence, record assessments, and log changes.

## Not responsible for

Own global templates, perform automated external collection, or infer truth.

## Main features and rules

Entity normalization supports search without automatic merging. Relations preserve subject/object direction. Evidence roles are supports/contradicts/context. Notes may link entities and evidence. Search matches stored text fields and run-step snapshots, not attachment contents.

## Dependencies and interfaces

Domain; database; AttachmentStore; EventLogger.

`services/investigation_service.hpp`: `InvestigationService`, entity/evidence link inputs.

## Verification and limitations

Source: [implementation](../../../src/services/investigation_service.cpp). See [tests](../../development/tests.md) for verification commands and limitations. Source inspection does not imply that every workflow was executed. [Technical notes](tech_notes.md) record constraints before changes.

Substantial feature details: [claims and relations](claims.md), [sources and evidence](evidence.md).
