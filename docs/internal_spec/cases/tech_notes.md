# Case management technical notes

Status: Approved

Implementation status: Implemented for the constraints described here.

## Constraints before changes

Deletion spans database and filesystem. Preserve quarantined-file restoration on transaction failure. The GUI provides confirmation; `delete_case` has no confirmation parameter, so callers must obtain the appropriate user decision.

## Evidence and checks

Read [behavior](behavior.md), [source](../../../src/services/case_service.cpp), the [canonical specification](../../spec/evidence-trace-spec.md), and [test guidance](../../development/tests.md) before modifying this module. Current source and tests were inspected on 2026-10-01. The existing integration executable passed; no fresh build or native desktop check is claimed.

Record any new mismatch as a proposal under the [documentation policy](../../policy/project_documentation_policy.md).
