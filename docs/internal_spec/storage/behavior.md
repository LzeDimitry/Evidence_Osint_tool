# Attachment storage behavior

Status: Approved

Implementation status: Implemented for the behavior described below. This describes current code; intended requirements remain in the [English product specification](../../spec/evidence-trace-spec.md).

## Purpose

Preserve local evidence bytes and provide integrity and staged deletion operations.

## Responsibilities

Stage input, resolve safe relative paths, calculate SHA-256, quarantine removals, and recover staged removals.

## Not responsible for

Decide claim confidence, own SQLite records, or preview files.

## Main features and rules

Input is copied into application storage. Default maximum file size is 100 MiB. Staged removals can be restored after SQL failure; startup recovery restores files still owned by the database and discards files no longer owned.

## Dependencies and interfaces

Filesystem; OpenSSL Crypto; an ownership callback supplied by callers.

`storage/attachment_store.hpp`: `AttachmentStore`, `StagedFile`, `StagedRemoval`.

## Verification and limitations

Source: [implementation](../../../src/storage/attachment_store.cpp). See [tests](../../development/tests.md) for verification commands and limitations. Source inspection does not imply that every workflow was executed. [Technical notes](tech_notes.md) record constraints before changes.
