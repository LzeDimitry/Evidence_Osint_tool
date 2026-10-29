# Proposal: docs/user/gui.md

- Target: `docs/user/gui.md` — operation: replace.
- Proposal revision: 2.
- Reason: The application gained a Tools hub (saved command and link tools) and an integrated terminal. This is human-facing usage documentation, so under the documentation policy it cannot be edited directly; this proposal adds a "Use your tools and the terminal" section describing the new screens and clarifies that user-launched commands and links can access the network.
- Affected documents: `docs/user/gui.md` only. Related internal records (module registry, `docs/internal_spec/tools/`, `CHANGELOG.md`) were updated directly under the policy's internal-edit allowance.
- Discrepancies: none; this documents implemented behavior verified by GUI acceptance runs on 2026-10-25.
- Verification: `cmake --build build` and `ctest --test-dir build` pass; GUI journeys covered tool CRUD, search/filters, run confirmation, link opening, terminal interaction, stop control, exit codes, and restart persistence.

## Proposed content

```markdown
# Using the desktop application

Status: Draft

Evidence Trace stores investigations and preserved files in a local workspace and makes no background network requests or telemetry calls. Links opened in the browser and commands run through Tools or Terminal may access the network under your account and operating-system permissions.

## Create a case

1. Open **Cases** and choose **Create case**.
2. Enter a case name and choose **Create case**. The name is the only required field.
3. Optionally enter a purpose, or open **More details (optional)** to set a target type, scope, tags, or description. You can edit these details later.

## Start an investigation

Open a case to enter the **Investigation Graph**, the default workspace. Add a Person, account, domain, place, or other object with **Add entity** or double-click empty canvas. Entity creation keeps you in the graph. Person creation accepts multiple phone numbers, usernames, email addresses, and other identifiers. Matching identifiers already in the case are reused. Each identifier is a typed entity linked by an unverified `identifier_for` observation, so the connection is available for exploration without being treated as verified.

The graph shows all case entities, including entities without links. Choose **Connect** and click two entities, or right-click an entity and choose **Add connection**, then click the other entity. A preview shows the connection in progress. Press **Esc** or click empty canvas to cancel. Enter a relationship type to create an unverified directional observation; this creates a real case relationship without opening the full claim form. Click an edge to inspect its status, reasoning, and evidence. Right-click an edge to edit its type, reverse its direction, or remove it. Edge color and tooltip indicate assessment status. Drag nodes to organize the canvas, use the middle mouse button to pan, and use the wheel or zoom controls to navigate. **Fit to view** reframes all entities. Node positions and view state are saved per case on this device. Relationships are directional in the current data model.

Use **Sources & Evidence** to record a source or preserve a file, text observation, or URL. When linking evidence to a claim or relation, choose an existing evidence item or create a URL/text item in that workflow. A source describes where information came from; evidence is material linked to an assertion. A URL or image alone does not verify an assertion. Confirmed status requires supporting evidence and remains an explicit analyst decision.

In entity and evidence forms, use **Paste image** or **Ctrl+V** when the clipboard contains an image. You can also choose image files, preview attachments, and remove them. Ordinary text paste continues to work in text fields. Entity images are illustrative attributes; evidence images are preserved evidence material, but are not automatically linked to or treated as verification of a claim.

Use **Entities** for the specialized entity list and details; use the graph to follow relationships and inspect relationship evidence without leaving the workspace. **Sources & Evidence** remains available for the full evidence list. Empty optional fields are omitted. Search and filters in Entities, the graph's relationship list, and Sources & Evidence narrow the current case. When no records match, clear the relevant search and filters. Press **Ctrl+K** to search across cases, entities, and evidence.

Claims and relations default to **Observation** and can be saved unverified without reasoning. Choose **Inference** when drawing a conclusion; it requires written reasoning. For a relation, choose the subject and object and enter a predicate; the statement is suggested automatically and can be edited.

The case header and activity views update after logged changes. The Runs and Export tabs reload their summaries when opened so they include changes made elsewhere in the case.

## Use your tools and the terminal

Open **Tools** to build your own collection of investigation utilities. Tools are shared across all cases and stay saved in the workspace.

Choose **Add tool** to save either kind:

- A **command** tool runs an executable. Enter the executable and its arguments, and the command runs without shell interpretation; arguments are split on spaces and quoted values keep their spaces. If you need pipes, substitution, or other shell features, select **Shell command** instead and enter one command line, which then runs via `/bin/sh -c`. A command tool can also set a working directory.
- A **link** tool stores a URL, including the scheme, for example `https://example.com`. Links open in your default browser.

Both kinds accept an optional description, category, and tags. Use the search box and the category and type dropdowns to find tools as the collection grows. Double-click a command tool to run it or a link tool to open it; **Edit** and **Delete** manage the selected tool.

Running a saved command always shows a confirmation first, with the exact command line, the execution mode, and the working directory. Nothing runs until you choose **Run in terminal**.

Open **Terminal** for command-line work. **New shell tab** starts your shell in the selected working directory, which is remembered for the next launch. The terminal behaves like a standalone terminal: colors and control sequences work, output can be scrolled back with the mouse wheel or **Shift+PageUp** and **Shift+PageDown** (**Shift+End** jumps to the bottom), interactive programs receive your input, and **Ctrl+Shift+C** and **Ctrl+Shift+V** copy and paste. Each tab shows its process state: running, stopping, exited with its code, or stopped by user. **Stop process** terminates the running session, and closing a tab with a running process asks for confirmation. Commands that fail show their own errors in the terminal; the tab's status strip reports the final exit code.

No command or tool runs automatically when the application opens. Closing the application ends its terminal sessions.

## Change appearance and storage

Open **Settings**. **General** shows the local case folder, the per-file size limit, and the network boundary. Use **Change folder…** to select another local data directory; the choice remains active after the next launch. Use **Open folder** to view the current one.

**Appearance** applies changes immediately. Choose an accent color, comfortable or compact control density, a text size, and a font from the compact list of readable installed families or use System default. The **Sample panel** shows an example layout; the subtitle explains that appearance settings apply to the interface. **Highlight the selected case tab with a glow** controls the highlight under the active case tab. Choose **Restore appearance defaults** to return to the default appearance and system font. These preferences are stored locally.
```
