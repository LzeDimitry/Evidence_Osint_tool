# Attachment storage technical notes

Status: Approved

Implementation status: Implemented for the constraints described here.

## Constraints before changes

Startup deletes `.tmp-*` material automatically; it does not offer an interactive cleanup choice. Recovery of `.delete-*` quarantine depends on the database ownership callback. Preserve confinement and symlink checks; failure across filesystem/SQL boundaries must remain visible.

## Evidence and checks

Read [behavior](behavior.md), [source](../../../src/storage/attachment_store.cpp), the [canonical specification](../../spec/evidence-trace-spec.md), and [test guidance](../../development/tests.md) before modifying this module. Current source and tests were inspected on 2026-10-01. The existing integration executable passed; no fresh build or native desktop check is claimed.

Record any new mismatch as a proposal under the [documentation policy](../../policy/project_documentation_policy.md).
