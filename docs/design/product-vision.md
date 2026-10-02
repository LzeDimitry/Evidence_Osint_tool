# Evidence Trace product vision

Status: Approved

Evidence Trace is a local-first OSINT investigation workbench. It helps one investigator collect facts, preserve source material, connect entities, record reasoning, and reconstruct the path from a target to a conclusion.

The application is deliberately an investigation workspace, not an automatic truth engine. External searches and automation may be added later, but every result must remain subject to human review and explicit evidence links.

## Core workspace

Implementation status: Implemented for the listed core concepts; this vision summarizes them rather than defining additional rules. See the [specification](../spec/evidence-trace-spec.md).

- Cases contain entities, sources, evidence, claims, relations, notes, activity history, and playbook runs.
- Entities represent people, accounts, usernames, email addresses, domains, files, places, and other investigation subjects.
- Evidence is preserved locally when possible, linked to its source, and verified with a SHA-256 hash.
- Claims record observations or inferences with a status, reasoning, and supporting or contradicting material.
- Relations connect two entities and are visualized for navigation; the graph does not invent inferences.
- Playbooks and techniques preserve repeatable investigation methods, while each case run keeps its own snapshot.
- Activity history and notes preserve how the investigator reached an assessment.

## Development direction

Implementation status: Planned for the future extensions below. A focused relationship map already exists; richer graph tooling remains planned.

The first release prioritizes reliable local storage, evidence integrity, case portability, and a usable desktop workflow. Future work can add a timeline, richer graph navigation, metadata tools, and controlled external integrations as separate features with their own specifications and tests.
