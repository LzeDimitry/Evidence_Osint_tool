# Playbooks and techniques technical notes

Status: Approved

Implementation status: Implemented for the constraints described here.

## Constraints before changes

Template/run independence must survive edits, deletes, and ZIP round trips. Global templates are not included in a case archive. Preserve ordering and snapshot fields when changing persistence.

## Evidence and checks

Read [behavior](behavior.md), [source](../../../src/services/playbook_service.cpp), the [canonical specification](../../spec/evidence-trace-spec.md), and [test guidance](../../development/tests.md) before modifying this module. Current source and tests were inspected on 2026-10-01. The existing integration executable passed; no fresh build or native desktop check is claimed.

Record any new mismatch as a proposal under the [documentation policy](../../policy/project_documentation_policy.md).
