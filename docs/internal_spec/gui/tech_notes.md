# Desktop GUI technical notes

Status: Approved

Implementation status: Implemented for the constraints described here.

## Constraints before changes

ApplicationContext performs migrations, temporary-file cleanup, and quarantine recovery at startup. Keep context lifetime consistent with page callbacks. Offscreen screenshots verify rendering only; native interaction requires manual validation. Timeline and advanced graph tooling remain planned.

## Evidence and checks

Read [behavior](behavior.md), [source](../../../src/gui/application_context.cpp), the [canonical specification](../../spec/evidence-trace-spec.md), and [test guidance](../../development/tests.md) before modifying this module. Current source and tests were inspected on 2026-10-01. The existing integration executable passed; no fresh build or native desktop check is claimed.

Record any new mismatch as a proposal under the [documentation policy](../../policy/project_documentation_policy.md).
