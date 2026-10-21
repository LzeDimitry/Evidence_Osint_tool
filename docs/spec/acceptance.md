# MVP acceptance checklist

Status: Approved

Implementation status: Partial for complete acceptance verification. For the 2026-10-16 workflow update, the CMake build and CTest passed; an offscreen GUI launch rendered the Cases page and wrote a screenshot. Native GUI interaction was attempted with XCB, but the process aborted in this environment, so Person creation, graph interactions, details presentation, and clipboard paste are not yet interactively verified. Existing full-screen journeys from 2026-10-08 remain historical evidence only. Smaller layouts and performance targets remain unverified.

The matrix below preserves the historical 2026-09-25 record; the new GUI checks are listed separately below. On 2026-10-01 the existing integration executable passed without a rebuild.

Verified by `tests/integration_tests.cpp` and CTest on 2026-09-25. The GUI rows were checked through Qt 6 offscreen rendering; native mouse interaction is not claimed as verified in this environment. Core rows remain automated:

| ID | Result | Verification |
| --- | --- | --- |
| AC-01 | Pass | Case reopened from the same SQLite database with unchanged metadata, including independently stored whitespace-separated tags; the Cases screen renders those tags as compact chips and the Overview screen opens the persisted record. |
| AC-02 | Pass | Directional relation is linked to both entities and listed in claim data; Entities and Claims & Relations screens show the links. The relation form keeps observation/inference kind selectable (defaulting to observation), and the claim association dialog can select an existing evidence record or import a new preserved file. |
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
| AC-16 | Pass | A full-size native GUI journey created a case with the required name and expanded optional details; all fields and actions remained visible at 1920×1080. |
| AC-17 | Pass | On an empty case, claim creation was disabled with an explanation; after an entity was added, the claim form became available. |
| AC-18 | Pass | Blue, Teal, Amber, and Violet accents updated the visible selection and preview; Blue persisted after restart. Font selection and case-tab glow were exercised. |
| AC-19 | Pass | Changing the data folder to a temporary directory persisted across restart with the same settings profile and no `--data-dir` override. |
| AC-20 | Pass | Native checks confirmed that adding evidence changes the header count/time immediately, relation creation updates the Overview claim count, and notes and run-step updates advance the case timestamp. Runs and Export summaries refresh when reopened. |
| AC-21 | Pass | The relation form defaults to Observation; an unverified observation saves without reasoning. Inference requires written reasoning. A fresh first-user journey confirmed relation statement auto-fill, immediate selection, and visible details after save. |
| AC-22 | Pass | Blue accent changed navigation, claim table/detail icons, and relationship-map node colors. |
| AC-23 | Pass | A CLI smoke created two entities, added a relation using positional CASE_ID, printed CLAIM_ID, and listed the new relation. `help` reports the matching syntax and export/import options. |
| AC-24 | Pass | The second entity remained selected, with its detail panel, after navigating from Overview back to Entities. |
| AC-25 | Pass | CMake install placed the GUI/CLI executables, desktop launcher, and PNG icon under the prefix; desktop-file validation passed and source requests maximized startup with the application icon. |
| AC-26 | Pass | Text search narrowed the evidence table to the text quotation; Web page and Web source filters narrowed it to the URL evidence record. |
| AC-27 | Pass | `Ctrl+K` focused global search; searching “Citrine” returned case, entity, source, and evidence results. |
| AC-28 | Pass | On a fresh profile, Claims search with no matches explained how to clear search/status/type/entity filters; clearing a filter restored rows. The entity filter likewise showed a specific no-match state, and related-claim status badges did not overlap their statements. |

| AC-29 | Pass | Visual checks confirmed Edit Playbook uses an edit glyph while Run Playbook uses play, claim evidence cards use the evidence-kind icon, Export uses an outward arrow, and Event target uses a calendar. |
| AC-30 | Partial | A fresh GUI journey exposed that the inspector count updated but the table did not after evidence linking. The link action now refreshes the claim table while preserving the selected claim; final screenshot verification remains pending. |

GUI FR coverage exercised in the historical build: FR-UI-01, FR-CASE-01–05, FR-ENT-01–05, FR-EVD-01–06, FR-CLM-01–05, FR-PLY-01–04, FR-LOG-01–04, and FR-EXP-01–05. Current full-size visual evidence covers major workflows at 1920×1080 with isolated profiles. Native mouse injection required XCB/xdotool because Hyprland did not deliver injected input to the Wayland surface. The relationship map is implemented for stored binary claims; the dedicated timeline and advanced graph tooling remain extensions. Smaller layouts and performance targets remain unverified. Deferred external-service features, encryption, full-text indexing inside attachments, and the Ukrainian catalog remain outside this release.

The 2026-10-16 workflow changes add schema-v4 image references, Person identifier capture, graph-first entity/relation entry, optional URL/text evidence, and image paste controls. Integration tests cover image-reference persistence after database reopen and archive round-trip; CTest passed. The offscreen smoke verifies startup and rendering only. Native interaction and clipboard paste behavior remain pending GUI acceptance.

The 2026-10-16 graph workspace update makes the graph the default case tab. CMake build and CTest passed; integration coverage verifies predicate editing, direction reversal, preserved evidence links, and direction after database reopen. Offscreen captures verified graph-first opening and empty/populated graph rendering. Context-menu connection, drag positioning, Escape cancellation, and saved view/layout restoration have not been exercised through native pointer input in this environment.
