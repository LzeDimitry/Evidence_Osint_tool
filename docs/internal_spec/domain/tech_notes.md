# Domain types technical notes

Status: Approved

Implementation status: Implemented for the constraints described here.

## Constraints before changes

Preserve serialized enum spellings when changing display labels. Changes to record shape must be checked against migrations and archive compatibility. These helpers do not replace service validation.

## Evidence and checks

Read [behavior](behavior.md), [source](../../../src/domain/types.hpp), the [canonical specification](../../spec/evidence-trace-spec.md), and [test guidance](../../development/tests.md) before modifying this module. Current source and tests were inspected on 2026-10-01. The existing integration executable passed; no fresh build or native desktop check is claimed.

Record any new mismatch as a proposal under the [documentation policy](../../policy/project_documentation_policy.md).
