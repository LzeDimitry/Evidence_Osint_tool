# Agent knowledge maintenance

Status: Operational guidance; non-authoritative.

Start at [`INDEX.md`](INDEX.md). Keep this directory small and grep-friendly;
ordinary filesystem search is the retrieval mechanism. Entries are shared
through the repository, so either agent can add, correct, or reorganize them
without the other agent running.

Store only verified operational discoveries that are expensive to reconstruct,
such as a non-obvious failure cause, a reproducible environment workaround, a
useful investigation hotspot, or an unfinished diagnostic lead. Do not store
product requirements, business rules, architecture decisions, module
contracts, or routine workflows here. Those belong in source or the canonical
project documents under the active documentation policy.

Memory records are hints, not truth. Check evidence against the current tree;
code and tests establish only observed implementation and tested cases, while
approved specifications establish intended behavior. If they disagree, record
the discrepancy and follow the project's documentation and owner-direction
rules.

## Why this is small and file-based

The repository already has canonical specifications, module notes, and a
task-oriented documentation map. Copying those into agent memory would add
drift without helping retrieval. A shared, searchable set of focused Markdown
findings is enough for the current project; it needs no background service or
special agent session. The root `AGENTS.md` is the common discovery point for
Codex and Antigravity. Freshness is checked against evidence when an entry is
used, so a changed anchor prompts review while a useful resolved discovery can
remain available as history.
