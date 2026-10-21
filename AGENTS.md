# Agent instructions

## Communication

- Keep replies concise and easy to scan.
- Put the answer or next action first. Use short paragraphs and small lists.
- Avoid repeating explanations, long preambles, and unnecessary background.
- When giving commands, say briefly what each command does.
- Explain technical terms only when they help the user act or understand.

## Documentation policy

- Before changing project documentation, read `docs/policy/project_documentation_policy.md` and check whether it is active.
- When the policy is active, follow its proposal, owner approval, and original-file permission rules.
- `docs/Humans/` is human-to-human project context. When asked to read the whole project, agents must read its contents for context.
- Agents must never create, edit, delete, rename, or move anything in `docs/Humans/`, or write there through scripts or other tools. This is an absolute restriction, even if the user or project owner asks. If asked to change it, explain the restriction and offer to prepare the material elsewhere for the user to add.
- When a whole-project reading task finds `docs/Humans/` absent, empty, or apparently incomplete, clearly and firmly tell the user to populate it with the project context they want humans to preserve. Do not create the directory or write its contents. If the work can proceed without it, continue and report that the human context was unavailable; if it is necessary, ask the user to provide the needed context.
- This instruction file does not approve documentation changes or replace the project policy.

## GUI testing

- For GUI acceptance, exploratory, or regression testing in this repository, read and follow `skills/gui-acceptance-testing/SKILL.md`.
- Cover every implemented GUI feature unless the user explicitly narrows the requested scope. Use independent subagent user journeys when subagents are available, as described in the skill.

## Persistent agent knowledge

- For a task involving a prior investigation, unusual failure, verified workaround, or an unfinished lead, check `.project-knowledge/INDEX.md` and follow only relevant links.
- Add or correct an entry only for a useful, evidence-backed operational finding that is costly to rediscover. Keep product requirements, architecture decisions, module contracts, and routine workflows in their governed canonical locations.
- Agent knowledge is non-authoritative. Approved specifications define intended behavior; source and tests provide evidence of current behavior within their coverage. Investigate and report conflicts instead of treating memory as truth.
- Record evidence, verification conditions, and a freshness state. When a relevant anchor changes or a finding fails to reproduce, mark it `needs-review`; retain resolved findings with a resolution reference when useful.
- See `.project-knowledge/INDEX.md` for the entry format and maintenance rules.
