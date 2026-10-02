# Activity logging technical notes

Status: Approved

Implementation status: Implemented for the constraints described here.

## Constraints before changes

The local SQLite owner can alter data; the log is not a signed audit trail. Preserve status-change payloads and their readable explanations. A deleted case does not retain a separate external audit archive.

## Evidence and checks

Read [behavior](behavior.md), [source](../../../src/services/event_logger.cpp), the [canonical specification](../../spec/evidence-trace-spec.md), and [test guidance](../../development/tests.md) before modifying this module. Current source and tests were inspected on 2026-10-01. The existing integration executable passed; no fresh build or native desktop check is claimed.

Record any new mismatch as a proposal under the [documentation policy](../../policy/project_documentation_policy.md).
