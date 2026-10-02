# Investigation records technical notes

Status: Approved

Implementation status: Implemented for the constraints described here.

## Constraints before changes

Case scoping belongs in service validation as well as import validation. Search uses SQL LIKE and exposes no date-filter parameter. Entity deletion protects claim references. Evidence removal can unlink claims without downgrading their status: confirmation safeguards apply at claim creation/status change, not continuously.

## Evidence and checks

Read [behavior](behavior.md), [source](../../../src/services/investigation_service.cpp), the [canonical specification](../../spec/evidence-trace-spec.md), and [test guidance](../../development/tests.md) before modifying this module. Current source and tests were inspected on 2026-10-01. The existing integration executable passed; no fresh build or native desktop check is claimed.

Record any new mismatch as a proposal under the [documentation policy](../../policy/project_documentation_policy.md).
