# Case management behavior

Status: Approved

Implementation status: Implemented for the behavior described below. This describes current code; intended requirements remain in the [English product specification](../../spec/evidence-trace-spec.md).

## Purpose

Create and maintain investigation containers and their lifecycle.

## Responsibilities

Create/list/update cases, archive/restore, and permanently delete records and attachments.

## Not responsible for

Create individual claims or evidence; maintain global playbook templates.

## Main features and rules

Cases have active/archived state. Metadata updates preserve investigation history. Permanent deletion removes dependent records and preserved files; archive/restore is the reversible lifecycle path.

## Dependencies and interfaces

Domain; database; EventLogger; AttachmentStore for deletion.

`services/case_service.hpp`: `CaseService`.

## Verification and limitations

Source: [implementation](../../../src/services/case_service.cpp). See [tests](../../development/tests.md) for verification commands and limitations. Source inspection does not imply that every workflow was executed. [Technical notes](tech_notes.md) record constraints before changes.
