# Using the desktop application

Status: Approved

Evidence Trace keeps each investigation and its files on this device. Network access is disabled.

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

## Change appearance and storage

Open **Settings**. **General** shows the local case folder, the per-file size limit, and the network boundary. Use **Change folder…** to select another local data directory; the choice remains active after the next launch. Use **Open folder** to view the current one.

**Appearance** applies changes immediately. Choose an accent color, comfortable or compact control density, a text size, and a font from the compact list of readable installed families or use System default. The **Sample panel** shows an example layout; the subtitle explains that appearance settings apply to the interface. **Highlight the selected case tab with a glow** controls the highlight under the active case tab. Choose **Restore appearance defaults** to return to the default appearance and system font. These preferences are stored locally.