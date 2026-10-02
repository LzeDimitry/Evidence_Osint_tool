# Claims and relations

Status: Approved

Implementation status: Implemented for the current behavior below.

Claims contain a statement, observation/inference kind, status, reasoning, and entity links. An inference requires reasoning. A relation has a predicate and exactly one subject and one object; direction is preserved. Linked records must belong to the same case.

At creation or a status change to confirmed, the service requires at least one supports link and written reasoning. Status changes require an explanation and retain the previous/new assessment in activity history. The user may select any supported status; the specification diagram illustrates common paths rather than a transition whitelist.

Evidence links have supports/contradicts/context roles. Removing a link preserves the evidence item. A claim deletion preserves evidence bytes. Changing evidence links does not automatically reassess the claim, so an already confirmed claim can later lose its supporting link. The safeguard is a precondition for creation/status changes, not a continuously enforced integrity assertion.

See [module behavior](behavior.md), [technical notes](tech_notes.md), and [specification](../../spec/evidence-trace-spec.md).
