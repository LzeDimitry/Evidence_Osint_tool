# Agent operational knowledge

This is a shared, non-authoritative memory for Codex and Antigravity. It keeps
costly operational discoveries and resumable investigations, not a second copy
of the product specification. Use it selectively when a task touches a listed
finding; otherwise follow the task-oriented map in [`docs/README.md`](../docs/README.md).
Before relying on an entry marked `current`, check its evidence against the
current source and tests; the state describes its last review, not a guarantee
that it remains valid.

Approved specifications describe intended behavior. Source and tests show
current implementation only to the extent they cover it. This memory cannot
settle a conflict: inspect the evidence and report discrepancies. Do not use
these notes to bypass the active documentation policy.

## Find a prior investigation

| Topic | Finding or record | State |
| --- | --- | --- |
| GUI interaction audit | [GUI refinement checklist](../docs/internal/gui-refinement-checklist.md) records the completed interaction pass, its build/test evidence, and workflows still lacking manual interaction coverage. Read the step-7 audit before planning another broad GUI pass. | Needs review against current GUI and test workflows |
| Headless GUI smoke testing | [Headless GUI smoke testing and offscreen capture](findings/headless-gui-smoke-testing.md) records the environment variable (`QT_QPA_PLATFORM=offscreen`) and screenshot flag required to verify GUI pages without an active display server. | current |

## Add or maintain a finding

Add a focused Markdown entry only when the finding is likely to save meaningful
investigation time. Add one row here with a short retrieval phrase and its
current state. Prefer an existing canonical source link over restating its
content. Use source paths and stable symbol or feature names; line numbers alone
are fragile. Include exact commands/results when they are needed to reproduce
the finding.

Each entry should state:

- **State:** `current`, `needs-review`, `resolved`, or `abandoned`.
- **Finding:** what was learned and when it matters.
- **Evidence:** relevant source paths/symbols, tests, commands, and observed result.
- **Verified:** date and conditions that affect reproduction (for example, OS,
  build type, or GUI backend); include the Git revision when available.
- **Related guidance:** links to canonical specifications or module notes, if relevant.

When evidence changes or the result no longer reproduces, mark the note
`needs-review` and correct it after checking current code and tests. Keep useful
resolved discoveries with a brief resolution reference; remove duplicate or
low-value notes. Do not copy secrets, case data, or private evidence into this
directory.
