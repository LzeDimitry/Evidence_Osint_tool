# Sources and evidence

Status: Approved

Implementation status: Implemented for the behavior below.

Sources record origin and provenance separately from evidence. File evidence copies bytes into managed storage under an opaque relative path, records size and SHA-256, and does not depend on the original file after import. Text evidence stores an excerpt and quotation location. URL evidence has no preserved local bytes and is external only.

Integrity verification reports missing or changed bytes without changing claim confidence. A hash demonstrates agreement with the recorded bytes, not authenticity of the original content. Revised files are new evidence items.

One item may link to multiple claims and run steps. Unlinking from a claim does not delete it. Evidence deletion handles its links and attachment removal explicitly; database/filesystem failure recovery uses staged removal. A source still referenced by evidence is protected from deletion.

The GUI previews supported text/images read-only and displays metadata for unsupported material. Automated page capture is Planned and outside the current service.

See [module behavior](behavior.md), [technical notes](tech_notes.md), [storage behavior](../storage/behavior.md), and [specification](../../spec/evidence-trace-spec.md).
