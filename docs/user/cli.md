# CLI quick guide

Status: Approved

Implementation status: Implemented for the current capabilities described below; verification limitations are stated separately.

The Qt 6 desktop application is the primary interactive interface. The CLI remains useful for scripted workflows, test fixtures, and recovery. Every command can use `--data-dir DIR`; the default is `data/`.

The normal workflow is:

1. case create creates a case and prints CASE_ID.
2. entity add, source add, and evidence file|text|url add investigation material.
3. relation add or claim add creates an assessment.
4. claim link attaches evidence; claim status records a status change and explanation.
5. note add, search, and log preserve and retrieve working context.
6. export archive creates a restorable ZIP; export report creates a reading copy.

Use ./build/evidence-trace help for the complete command list. IDs printed by create commands are stable within the case and are used by later commands.
