# Project Documentation Policy

# **ONLY THE PROJECT OWNER MAY APPROVE HUMAN-FACING DOCUMENTATION CHANGES.**

> **THIS POLICY IS APPROVED BY THE PROJECT OWNER AND EFFECTIVE FROM 2026-10-01.**

> **AFTER OWNER APPROVAL ACTIVATES THIS POLICY, NO CHANGE, COMMIT, TEST, AUTOMATION, OR WORKFLOW MAY BYPASS ITS RULES.**

> **AN AI AGENT MAY EDIT INTERNAL PROJECT DOCUMENTATION DIRECTLY WHEN THE TASK AUTHORIZES THAT DOCUMENTATION WORK. HUMAN-FACING DOCUMENTATION STILL REQUIRES A `temp.*` PROPOSAL AND OWNER APPROVAL BEFORE APPLICATION.**

| Field | Value |
|---|---|
| Status | Approved — active |
| Version | 0.4.0 |
| Owner and approver | Project owner |
| Scope | This repository's documentation |
| Last updated | 2026-10-15 |

This policy defines how project documentation is organized, written, updated,
and validated. It is the beginning of the project's documentation governance
and defines the foundation on which future documentation will be built. It is
not a replacement for the product specification or the implementation itself.

The project owner approved the agreed changes and authorized marking this policy
Approved in the active conversation on 2026-10-01. This activates version 0.3.0.
Version 0.4.0 changes the editing workflow for internal documentation; the
project owner approved this direct policy edit on 2026-10-15. Only the owner may
approve human-facing documentation changes; agents may edit internal
documentation when authorized by the task.

## 1. Scope and authority

This policy applies to:

- `README.md` and `CHANGELOG.md`;
- all maintained files under `docs/`;
- Markdown guides, specifications, module documentation, design notes, and
  development notes;
- diagrams, code examples, and technical comments when they document project
  behavior or design decisions.

For this policy, human-facing documentation means `README.md` and maintained
content under `docs/user/`. Other documentation, including specifications,
acceptance records, development guides, module behavior documents, and policies,
is internal project documentation unless the owner designates a specific file
as human-facing. `docs/Humans/` is reserved for content maintained by the
project owner and designated human contributors.

AI agents may read files under `docs/Humans/`, but must not create, edit, delete,
rename, or move files there. This restriction applies to direct edits and edits
through scripts or other tools.

The project owner is the sole documentation owner and the only person with
authority to approve this policy or changes to human-facing documentation.
Internal project documentation may be directly edited by an AI agent when the
task authorizes that work. Human-facing proposals require the owner's review and
approval before they become canonical changes.

The project owner is the person responsible for this repository who explicitly
identifies themselves as its owner; an agent must not assume another contributor
has that authority. All paths in this policy are relative to the repository root.
`README.md` means the repository entry point; `docs/README.md` means the
documentation entry point.

For intended product behavior, the canonical language is English. The
canonical product specification is:

1. `docs/spec/evidence-trace-spec.md` for product behavior and requirements;
2. approved module behavior documents in `docs/internal_spec/`;
3. development, design, and user documentation;
4. implementation and tests as evidence of the current behavior.

This approved policy is authoritative for documentation-process rules.

If the specification and implementation disagree, the disagreement must be
made explicit in a temporary proposal, tested, and submitted to the project
owner. Tests must not silently redefine intended behavior.

Files marked as reference material, examples, templates, drafts, or archived
documents are not sources of truth unless they are explicitly adopted by the
project owner into the current project documentation.

## 2. Documentation structure

The maintained documentation should follow this structure:

```text
docs/
├── README.md               # documentation entry point
├── user/                   # user-facing usage documentation
├── spec/                   # canonical English product behavior and requirements
├── internal_spec/          # module behavior and technical knowledge
│   ├── modules_registry.md
│   └── module-name/
│       ├── behavior.md
│       ├── feature-name.md # optional feature detail
│       └── tech_notes.md
├── development/            # build, tests, migrations, architecture
├── design/                 # product and GUI design material
├── policy/                 # project policies
├── Humans/                 # owner-maintained material; agents may read only
├── templates/              # reusable documentation templates
└── mb_remeber/             # frozen non-canonical reference material
```

The root `CHANGELOG.md` is the default project change history. A module-specific
`changelog.md` is optional for a large or complex module whose history becomes
uncomfortable to maintain in the root changelog. Adding such a file does not
require changing this policy as long as this rule remains unchanged.

Reference material may remain in the repository when it is useful, but it must
be clearly treated as non-authoritative.

## 3. Module registry

`docs/internal_spec/modules_registry.md` is the entry point for the module map.
It must record, for every module or significant feature:

- name and classification;
- purpose;
- responsibilities;
- explicit non-responsibilities;
- main features or submodules;
- dependencies on other modules or technical components;
- public interfaces between modules;
- current implementation status;
- links to the relevant behavior and technical notes.

The registry must distinguish current implementation from planned work. It must
not require code changes merely to make the documentation tree look complete.

## 4. Module documentation

The primary module behavior file is `behavior.md`. It describes stable,
observable, and important conceptual behavior rather than repeating source
code.

Relevant sections may include:

```text
Purpose
Responsibilities
Not responsible for
Main features
Data/models
Main operations
Rules and constraints
Module lifecycle/state behavior
Dependencies
Interfaces with other modules
Persistence
Important edge cases
Known limitations
```

Sections are added only when they are useful for the module. A module document
must not contain artificial sections for concepts the module does not use.

`tech_notes.md` records information needed before changing a module that is not
already clear from the code and behavior document, including:

- important implementation decisions and their reasons;
- technical debt and known limitations;
- recovery behavior and failure cases;
- non-obvious storage or compatibility constraints;
- edge cases that must be preserved;
- warnings for future maintainers and AI agents.

Feature documents such as `evidence.md` and `claims.md` are appropriate when a
feature is substantial but remains inside its parent module.

## 5. Mandatory change order and documentation file matrix

The following order is mandatory for every change that affects code or
observable behavior:

1. Read this policy and the relevant specification, module documents, code,
   and tests. Identify affected documents and existing discrepancies.
2. Check the requested change against intended behavior. If requirements are
   unclear or conflict with the approved specification, obtain owner direction
   before implementing the conflicting behavior. An explicit owner instruction
   resolving that conflict is sufficient; do not request the same direction again.
3. Implement the authorized code changes.
4. Run relevant tests and record results, including failures or unavailable checks.
5. For human-facing documentation, prepare a `temp.*` proposal using the format
   below. Internal documentation may be edited directly when the task
   authorizes that documentation work.
6. For human-facing proposals, identify every proposal, target, and revision to
   the owner for review.
7. Obtain owner approval of the exact proposal before applying it to
   human-facing documentation. One owner message may provide both approval and
   permission to apply.
8. Validate the resulting documents and report direct internal edits.

For a documentation-only change, first verify the current implementation and
relevant evidence when behavior is affected; code tests are not required for
pure wording or process edits. Follow steps 1–2 and 5–8. A proposal is not a
source of truth. The `temp.*` proposal requirement and owner approval gate apply
to human-facing documentation; internal documentation may be edited directly
when the task authorizes it.

### Proposal format and lifecycle

Create `temp.<original-filename>` beside a Markdown target file. For example,
a proposal for `docs/user/cli.md` is `docs/user/temp.cli.md`. For other file
types, append `.md` to the proposal name. Do not overwrite another proposal;
insert a unique identifier after `temp.` if needed, such as `temp.review2.cli.md`.

Each proposal must contain:

- target repository-relative path and operation: create, replace, delete, or rename;
- proposal revision, incremented whenever any proposal content changes;
- reason, affected documents, discrepancies, and verification results;
- the complete proposed target content in a clearly delimited section for
  create/replace operations, including final metadata;
- source and destination paths for renames, and the reason for deletions.

For multiple targets, prepare one proposal per target and list all revisions
in the review request. Renames with content changes include the final content.
No original file may be deleted or renamed without explicit owner permission
for that operation. Proposal wrapper metadata must not be copied into the target.

Proposals may be committed as non-canonical review artifacts when committing
is authorized. Retain them after application unless the owner authorizes their
removal or archival. Canonical navigation must not present proposals as current
guidance. Local links in proposed target content are checked relative to the
target's final location.

### Change type → files to update

The proposal requirements in this matrix apply to human-facing documentation.
Internal documentation may be edited directly when the task authorizes it.

| Change type | Files to update |
|---|---|
| Product behavior, business rule, public interface, or data format | After code and tests, edit the canonical English `docs/spec/evidence-trace-spec.md` directly when authorized; include related user/module/development documents, acceptance criteria, and tests when affected. |
| Internal architecture or implementation constraint | Edit the relevant file under `docs/development/` or `docs/internal_spec/` directly when authorized; update tests or technical notes when the constraint is behaviorally important. |
| User workflow, GUI behavior, or CLI command | Prepare a `temp.*` proposal for the relevant file under `docs/user/`; include the canonical specification when the behavior or requirement changed. |
| Installation, build prerequisites, startup commands, runtime setup, or recovery instructions | Prepare a `temp.*` proposal for `README.md`; edit the relevant file under `docs/development/` directly when authorized. |
| Release feature matrix, verified limitation, or acceptance result | Edit the canonical English specification, `docs/spec/acceptance.md`, and `CHANGELOG.md` directly when authorized and when the release history changes. |
| Documentation process, approval, status, or structure | Edit the relevant file under `docs/policy/` directly when authorized; no other document may redefine this policy. |
| Ukrainian translation or localization | No update in the current phase. English is the only maintained and canonical documentation language; `docs/mb_remeber/` remains frozen. |

### README.md update rule

`README.md` is the project entry point and must be changed only when one of
the following changes:

- installation or build prerequisites;
- installation, startup, or essential recovery commands;
- top-level implemented functionality or current limitations;
- critical data-safety or backup instructions;
- links needed to reach the current canonical documentation.

Detailed product behavior, module rules, and routine development instructions
belong in their dedicated documents and must not be duplicated in
`README.md` without the project owner's explicit approval.

### Implementation status

Every material behavior statement must be marked as `Implemented`,
`Partial`, or `Planned` when its implementation state is relevant. A
specification change must not present planned or partial behavior as
implemented. If the code, tests, and specification disagree, record the
discrepancy in the applicable human-facing `temp.*` proposal or internal
documentation change summary.

Status is relevant whenever a reader could otherwise mistake planned or partial
behavior for available functionality. A section-level label may cover all its
statements when they share the same status. A significant feature is one with
its own observable workflow, public interface, persistence rule, or substantial
constraints; document it within its parent module when a separate module is
unnecessary.

## 6. Ownership and approval

**ONLY THE PROJECT OWNER MAY APPROVE CHANGES TO HUMAN-FACING DOCUMENTATION. NO AI AGENT,
CONTRIBUTOR, USER, REVIEWER, OR AUTOMATION MAY APPROVE THEM.**

The project owner is the sole documentation owner and final approver for
human-facing documentation. The project owner must personally review every
proposal before it becomes part of the canonical human-facing documentation.

AI agents may inspect the repository, change code when the task authorizes code
work, run tests, edit internal project documentation when the task authorizes
that work, and prepare `temp.*` proposals for human-facing documentation. They
must notify the owner and wait for approval before applying a proposal to a
human-facing original. An agent must not infer approval from silence, elapsed
time, automated checks, or general encouragement.

**AN AI AGENT MAY EDIT INTERNAL DOCUMENTATION DIRECTLY WHEN THE TASK AUTHORIZES
THAT WORK. EDITS TO HUMAN-FACING ORIGINALS REQUIRE OWNER APPROVAL AND EXPLICIT
PERMISSION FOR THAT SPECIFIC EDIT OR OPERATION.**

For internal project documentation outside `docs/Humans/`, task authorization
permits direct edits without a separate proposal or approval step. AI agents
may read but never edit `docs/Humans/`. For human-facing documentation,
the owner may explicitly authorize a direct edit without a separate proposal,
specifying the target and scope. For example: "Edit docs/user/gui.md directly
to add the changes we agreed." This authorizes the stated edit, not unrelated
changes or automatic approval of the resulting document. Record the
authorization in the task handoff. For changes awaiting review, preserve the
previously approved version in version control or a review artifact and clearly
identify it as the authoritative version. Mark the edited version Draft until
the owner approves it. Permission to edit alone does not grant approval. The
owner may also explicitly instruct an agent to apply specified, agreed changes
and mark the result Approved in one message; that instruction approves only
those changes. If implementation requires a substantive departure from what
was agreed, submit that departure for review.

After approval, the project owner may apply the proposal personally or
explicitly authorize an agent to apply exactly the approved content. Any
difference from the approved proposal requires a new proposal and a new owner
decision.

Approval of human-facing documentation must identify each proposal path and
revision, or an immutable content hash, plus its target files. Record the
owner's actual decision and application permission in the review conversation
or repository review record and reference that record in the handoff. An agent
may report a real owner decision and apply its approved metadata, including
`Status: Approved`; it cannot grant that status on its own. Authorization
persists for the specified content and scope only.

Example: an agent presents `docs/user/temp.cli.md`, revision 2, targeting
`docs/user/cli.md`. The owner replies: "I approve revision 2 of that proposal
and authorize you to apply it to docs/user/cli.md." The agent applies its target
content, validates it, and reports the result. Editing revision 2 afterward
invalidates that approval for the changed proposal.

Before preparing a human-facing proposal or directly editing internal
documentation, an AI agent must read this policy and the relevant current
canonical specification, module documentation, technical notes, code, and tests
when they exist. Reference files, old drafts, translations, and generic
examples must never be treated as project rules.

When a task asks an agent to read or understand the project for its work, it
must inspect `docs/Humans/`. If the directory is absent, empty, or its contents
appear incomplete for the requested project context, the agent must tell the
user to fill it in. The agent must not create or complete its contents.

## 7. Language, format, and style

English is the only maintained and canonical language of project documentation
and policy for the current phase. Ukrainian translation/localization is frozen:
do not update `docs/mb_remeber/` or create new Ukrainian documentation unless
the project owner explicitly requests it.

Code identifiers, file names, serialized values, and technical API names remain
in their actual project form.

Documentation uses UTF-8 Markdown unless another format is explicitly needed.
Canonical maintained documents must be concise, concrete, and verifiable:

- prefer observable behavior over intentions;
- use precise conditions and outcomes;
- separate implemented behavior from plans and examples;
- avoid duplicating the same rule in multiple documents;
- link to the authoritative document instead of copying long passages;
- use ISO 8601 dates, preferably in UTC, for document metadata and events;
- use diagrams only when they clarify structure, data flow, or lifecycle.

Templates may contain placeholders. Approved project documents must not contain
unresolved placeholders such as `PROJECT_NAME` or `TODO`.

## 8. Document status

Every maintained document must clearly state whether it is **Draft**,
**Approved**, or **Archived**. Only the project owner may approve it.
Draft and archived documents must not be treated as current project rules.
A simple `Status: Draft`, `Status: Approved`, or `Status: Archived` line
below the title is sufficient; a metadata table is optional.

Other metadata, including version, owner, date, and verification references,
is optional when useful. Product implementation labels such as `Planned` and
`Partial` describe behavior, not document approval. An `Approved` label inside
a proposal's target content does not approve the proposal itself.

### Adoption and agent discovery

Approval activates this policy; it does not retroactively prove approval of
existing documents. Existing owner-approved documents retain their authority
and add the simple status label when next changed. Documents with unknown
approval remain unverified; report that uncertainty rather than inventing approval.
CI status-label enforcement applies to migrated documents until migration is complete.

Report missing entry points, module registry files, and related documents.
Propose their creation when relevant to the task; missing structure alone does
not authorize unrelated work or require artificial code changes.

The repository root `AGENTS.md` should direct agents to read
`docs/policy/project_documentation_policy.md`, check its effective status, and
follow its approval and original-file permission rules when active. Adding that
instruction is a separate repository edit requiring owner authorization.

## 9. Validation and CI

Documentation CI must provide lightweight mechanical checks for canonical
maintained documents:

- Markdown syntax and linting;
- broken local links;
- unresolved placeholders in documents with status `Approved`;
- the required document status label;
- that `temp.*` proposals are not treated as canonical documents.

Templates, reference files, drafts, and archived documents may contain
placeholders when that is intentional.

CI must not attempt to infer semantic freshness or decide whether the
documentation accurately describes the product. Content accuracy remains a
review responsibility of the project owner and the task author.

## 10. Git commit prefixes

Use the following standard-style prefixes where they clarify the main change:

```text
docs:
policy:
spec:
feat:
fix:
test:
refactor:
```

Examples:

```text
docs: update investigation documentation
policy: define documentation workflow
spec: define evidence linking behavior
feat: add claim filtering
fix: correct evidence hash validation
test: cover malformed archive rejection
refactor: split investigation service helpers
```

The `CODE` prefix is not used; `feat`, `fix`, and `refactor` describe code
changes more precisely.

## 11. Security and external references

Documentation must not contain real case data, personal data, credentials,
API keys, private attachment contents, or sensitive investigation material.
Examples must use synthetic data.

External standards, libraries, designs, and reference documents should be
cited when they materially influence a project decision. A cited external
document does not override the current project policy or approved
specification.

## 12. Definition of done for documentation-impacting tasks

A documentation-impacting task is not complete until:

- the code has been changed when required and the relevant tests have passed;
- a `temp.*` proposal exists for human-facing documentation, unless the owner
  explicitly authorized a direct edit under section 6;
- the project owner has been notified in the active conversation for
  human-facing proposals or direct edits;
- the project owner has personally approved the exact human-facing content;
- only approved content has been applied to human-facing originals; internal
  documentation edits follow the scope authorized by the task;
- implemented, partial, and planned features are distinguished;
- code, tests, and canonical documentation agree, or an explicit discrepancy
  is recorded and submitted to the project owner;
- relevant acceptance references, links, and document status are valid;
  any optional metadata included is accurate;
- no unintended placeholders or unapproved translation changes remain;
- the change uses an appropriate commit prefix.

Proposal preparation and authorized direct editing are alternative paths for
human-facing documentation. Internal documentation may be directly edited when
the task authorizes it. Approval of the exact final human-facing content is
required before canonical completion.
An appropriate commit prefix is required only if a commit is made.

Use these handoff states:

- `Ready for owner review`: proposals or authorized direct edits are ready;
  report targets, revisions, changes, evidence, and outstanding discrepancies.
- `Approved — awaiting application`: the owner approved exact content, but it
  has not been applied or application permission is still missing.
- `Complete`: authorized content has been applied, approval is recorded, and
  required validation has passed.
- `Rejected` or `Deferred`: record the owner's decision and preserve review
  artifacts; do not apply the rejected or deferred content.

For failed or unavailable tests, report the command, result or reason, and
whether the failure was confirmed to predate the change. Do not claim unrun
checks passed. The task remains pending unless the owner explicitly accepts
the stated verification limitation for completion; record that decision.
Waiting for review is a valid handoff, not approval or completion.

After activation, no task, commit, code change, documentation change, test,
automation, or workflow may be used to bypass or violate this policy.
