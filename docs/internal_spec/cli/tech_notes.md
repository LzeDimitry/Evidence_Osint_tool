# Command-line interface technical notes

Status: Approved

Implementation status: Implemented for the constraints described here.

## Constraints before changes

Output labels and serialized enum values matter to scripts. Keep argument validation and service validation consistent. Startup recovery must remain aligned with GUI startup. See [CLI guide](../../user/cli.md).

## Evidence and checks

Read [behavior](behavior.md), [source](../../../src/cli/main.cpp), the [canonical specification](../../spec/evidence-trace-spec.md), and [test guidance](../../development/tests.md) before modifying this module. Current source and tests were inspected on 2026-10-01. The existing integration executable passed; no fresh build or native desktop check is claimed.

Record any new mismatch as a proposal under the [documentation policy](../../policy/project_documentation_policy.md).
