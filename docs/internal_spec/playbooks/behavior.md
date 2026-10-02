# Playbooks and techniques behavior

Status: Approved

Implementation status: Implemented for the behavior described below. This describes current code; intended requirements remain in the [English product specification](../../spec/evidence-trace-spec.md).

## Purpose

Maintain reusable global methods and case-specific execution snapshots.

## Responsibilities

Edit ordered templates, start runs, record run-step state/notes, and link resulting evidence.

## Not responsible for

Execute searches or tools automatically, or rewrite prior runs on template changes.

## Main features and rules

Starting a run copies step names and instructions. Later template changes leave run snapshots intact. Run-step states are todo/done/skipped. Step evidence must belong to the run case.

## Dependencies and interfaces

Domain; database; EventLogger; case/entity/evidence records.

`services/playbook_service.hpp`: `PlaybookService`.

## Verification and limitations

Source: [implementation](../../../src/services/playbook_service.cpp). See [tests](../../development/tests.md) for verification commands and limitations. Source inspection does not imply that every workflow was executed. [Technical notes](tech_notes.md) record constraints before changes.
