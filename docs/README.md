# Evidence Trace documentation

Status: Approved

The [documentation policy](policy/project_documentation_policy.md) governs approval and maintenance. English is the maintained language. Product implementation labels and document approval status are separate.

## Read by task

| Task | Document |
| --- | --- |
| Product requirements | [English specification](spec/evidence-trace-spec.md) |
| Release verification and remaining checks | [Acceptance record](spec/acceptance.md) |
| Scripted usage | [CLI guide](user/cli.md) |
| Build and startup | [Development build](development/build.md) |
| Understand boundaries | [Architecture](development/architecture.md) and [module registry](internal_spec/modules_registry.md) |
| Change the database | [Migrations](development/migrations.md) |
| Run checks | [Tests](development/tests.md) and [documentation validation](development/documentation-validation.md) |
| Product and visual direction | [Product vision](design/product-vision.md) and [GUI references](design/gui/README.md) |
| Prepare a document | [Templates](templates/README.md) |

The [repository README](../README.md) is the installation/startup entry point; [CHANGELOG](../CHANGELOG.md) is the default change history. The registry describes current code areas and links to their notes. Draft and archived documents are not current rules; an implementation label does not approve a document.

## Before changing code

Use these documents to answer four separate questions:

| Question | Where to look | How to use it |
| --- | --- | --- |
| What should the product do? | [Product specification](spec/evidence-trace-spec.md) | Treat this as the intended behavior. Requirements can describe work that is not implemented yet. |
| What does it do now? | [Module registry](internal_spec/modules_registry.md), linked behavior notes, source code, and tests | The code shows current implementation. Tests show only the cases they actually run. |
| What is missing or uncertain? | [Implementation status in the specification](spec/evidence-trace-spec.md), [acceptance record](spec/acceptance.md), and module limitations | `Partial` means some behavior exists but a stated part is missing. `Planned` means it is not implemented. A check marked unrun or historical is not current verification. |
| What must a change preserve? | The module registry and its linked technical notes, relevant tests, migrations, and data-format requirements | Check these before editing, then run the relevant checks where available. Examples include evidence hashes and bytes, case-scoped links, serialized values, and import/export compatibility. |

The documents answer different questions and may not agree. For example, the
specification requires search filters by type and date, while the implementation
status records those filters as missing. Treat the requirement as intended
behavior and the code as current behavior. Do not claim the requirement already
works. If a requested change depends on resolving a disagreement or uncertainty,
describe the evidence and ask the project owner for direction instead of guessing.

Before a code change, read the relevant specification sections and module notes,
inspect the affected code and tests, and identify preservation constraints. In
the handoff, state what changed, what checks actually ran, and which gaps remain.
If the repository provides no code or test evidence for a statement, label it
unverified rather than presenting it as fact.

## Reference material

`mb_remeber/` is frozen, non-canonical reference material, including the Ukrainian specification. Do not update it as part of English documentation maintenance.

Legacy examples in `reference/` are non-authoritative examples, not project policies or module contracts. GUI PNGs are visual reference assets, not screenshots proving implemented behavior. Proposals named `temp.*` are review artifacts and are excluded from current guidance. Approval of this migration does not adopt any reference asset as a product requirement.
