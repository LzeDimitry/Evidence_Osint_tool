# MVP acceptance checklist

Status: Approved

Implementation status: Partial for complete acceptance verification. Core behaviors are present; native GUI interaction and performance targets remain unverified.

The matrix below is the historical 2026-09-25 record, not a fresh acceptance run. On 2026-10-01 the existing integration executable passed without a rebuild. Source inspection confirms current core safeguards; native rendering/interaction was not rerun in this migration.

Verified by `tests/integration_tests.cpp` and CTest on 2026-09-25. The GUI rows were checked through Qt 6 offscreen rendering; native mouse interaction is not claimed as verified in this environment. Core rows remain automated:

| ID | Result | Verification |
| --- | --- | --- |
| AC-01 | Pass | Case reopened from the same SQLite database with unchanged metadata, including independently stored whitespace-separated tags; the Cases screen renders those tags as compact chips and the Overview screen opens the persisted record. |
| AC-02 | Pass | Directional relation is linked to both entities and listed in claim data; Entities and Claims & Relations screens show the links. The relation form keeps observation/inference kind selectable (defaulting to inference), and the claim association dialog can select an existing evidence record or import a new preserved file. |
| AC-03 | Pass | The service rejects confirmed status without a supporting evidence link; the new-claim GUI now opens the supporting-evidence step before creating a confirmed claim or relation. |
| AC-04 | Pass | Original file mutation does not change the stored copy or stored hash; Sources & Evidence exposes the preserved item. |
| AC-05 | Pass | Internal attachment mutation reports a mismatch without changing claim status. |
| AC-06 | Pass | One evidence item remains linked after removing another claim link. |
| AC-07 | Pass | Status-change log retains previous/new status and explanation; report separates refuted claims. |
| AC-08 | Pass | Completed run step retains its template snapshot after a global technique edit; Playbooks and Playbook Runs provide the two workflows. |
| AC-09 | Pass | Clean import preserves counts, links, notes, logs, attachment hashes, claim evidence links, and playbook-run evidence links; copy mode remaps IDs. |
| AC-10 | Pass | Modified attachment and unsafe ZIP path are rejected before case creation. |
| AC-11 | Pass | Case search finds username, URL, and note phrase within the case; the top-bar search also returns cross-case case/entity/evidence results and opens the selected record context. |
| AC-12 | Pass | URL-only evidence is reported as `external only` in Sources & Evidence. |
| AC-13 | Pass | Cases provide a right-click context menu for details, edit, archive/restore, and permanent delete. Deletion requires confirmation and the automated cascade test removes dependent records and preserved attachments. |
| AC-14 | Pass | Entities provide a right-click context menu for editing, opening related claims in Claims & Relations with an entity filter, and permanent deletion. The service test verifies that referenced entities are protected and unreferenced entities delete cleanly. |
| AC-15 | Pass | Actionable rows in Cases, Entities, Claims & Relations, Sources & Evidence, Playbook Runs, Playbooks, Notes & Log, and Overview expose context menus wired to real operations. SQLite/service tests cover protected references and deletion effects; native GUI interaction remains a manual verification task. Settings and Export have no record rows, so no row menu is applicable. |

GUI FR coverage exercised in the current build: FR-UI-01, FR-CASE-01–05, FR-ENT-01–05, FR-EVD-01–06, FR-CLM-01–05, FR-PLY-01–04, FR-LOG-01–04, and FR-EXP-01–05. The shared GUI design system was checked with populated pages at 1920×1080 and 1440×900 and with empty/populated pages at 900×600; it uses readable typography, bounded cards, empty-state guidance, tooltips for elided values, and vertical scrolling/stacking where the smaller viewport cannot show all content at once. Evidence preview was exercised with an in-app local fallback; the implementation also supports read-only text and image previews and never launches an external handler. Timestamps were checked at 1440×900 and 900×600 and show `YYYY-MM-DD HH:MM UTC`; table cells expose full values through tooltips and scroll when needed. The relationship map is implemented for stored binary claims; the dedicated timeline and advanced graph tooling remain extensions. Deferred external-service features, encryption, full-text indexing inside attachments, and the Ukrainian catalog remain outside this release.
