---
name: gui-acceptance-testing
description: Exercise this application's GUI as a real user, cover every visible feature, and coordinate independent subagent user journeys. Use for GUI acceptance, exploratory, or regression testing in this repository.
---

# GUI acceptance testing

Test the running application through its visible GUI and report what a user can reproduce. Cover every implemented GUI feature relevant to the requested test scope.

## Prepare the run

1. Read the active project instructions and documentation policy. Check the current GUI behavior document, applicable product acceptance/specification material, build/run guidance, and recent relevant changes. Use these to identify the expected behaviors and the features that exist today.
2. Inspect the application’s visible navigation and controls as well as those sources. Build a coverage checklist that includes each top-level page, feature flow, meaningful dialog/form, important empty/error/loading state, and settings or persistence behavior. Mark each item as covered, failed, blocked, or not applicable, with a reason for exclusions. Do not assume a screenshot or startup smoke check covers interaction.
3. Use an isolated temporary data directory/profile and synthetic records. Preserve any user data. Record build/version, operating system, display/input method, and setup limitations. Prefer actual mouse and keyboard interaction in a normal desktop session; use offscreen rendering only for checks that do not claim pointer-driven behavior.

## Delegate realistic user journeys

Use subagents for parallel GUI coverage when subagents are available. Give each tester a fresh, independent context and an isolated application data profile. Assign missions that divide the coverage checklist without overlapping state, for example: a first-time user setting up a case; an investigator adding and reviewing records; or a user changing preferences and returning to prior work. Adapt missions to the features actually present.

Each tester should behave like an ordinary person trying to complete a useful task:

- Give them a short persona and a goal, not a scripted sequence of clicks or the expected result for each step.
- Do not reveal suspected bugs, implementation details, source-code paths, internal test cases, or the desired conclusion. They should discover navigation and problems independently.
- Ask them to use only the visible interface, try sensible choices, and report confusing labels, dead ends, unexpected changes, and errors in their own words.
- Keep assigned features and profiles separate. Do not let testers share mutable application data or rely on another tester’s discoveries.
- Have each tester return the goal attempted, features visited, outcome, concise reproduction steps for problems, and available screenshots. Ask them to distinguish what they observed from what they inferred.

The coordinating agent owns the feature inventory, assigns disjoint journeys, merges findings, and checks that every checklist item has a disposition. If subagents are unavailable, run the same independent journeys sequentially and report that delegation was unavailable.

## Run and record coverage

Exercise normal and meaningful edge paths, including validation, cancellation, empty data, and recovery where those states are accessible and safe. Verify save/reopen behavior for features that claim persistence. Do not substitute unit/integration tests, source inspection, or CLI operations for GUI interaction; those may supplement the GUI run when relevant and authorized.

Record results in a compact matrix with feature, journey/tester, status, and evidence. For each failure include:

- the user goal and starting state;
- numbered actions using visible labels;
- expected behavior, grounded in the current approved requirements when available;
- actual behavior and impact on the user;
- reproduction rate, environment, and screenshot or log reference when available.

Report blockers, crashes, and data-loss risks first. Separate reproducible defects from usability observations and environment/setup problems. Do not claim full coverage when any feature is blocked or unvisited. Do not silently redefine expected behavior when implementation and approved requirements disagree; report the discrepancy.

Do not change application code or canonical documentation during a test-only task. If asked to fix an issue, keep the fix distinct from the original test result and rerun the affected GUI journey. Follow the active documentation policy before proposing or changing project documentation.
