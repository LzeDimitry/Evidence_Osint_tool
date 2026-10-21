# CLI quick guide

Status: Approved

Implementation status: Implemented for the capabilities listed below. The desktop application remains the primary interactive interface; the CLI supports scripted workflows, test fixtures, and recovery.

Every command can use `--data-dir DIR`. The default data directory is `data/`.

## Typical workflow

1. Create a case with `case create`; the command prints `CASE_ID`.
2. Add entities, sources, and evidence with `entity add`, `source add`, and `evidence file|text|url`.
3. Record an observation or inference with `claim add`, or create a relationship with `relation add`.
4. Link evidence to a claim with `claim link`; use `claim status` to record a status change and explanation.
5. Keep working context with `note add`; retrieve it with `search` and `log`.
6. Export a restorable archive or a reading report with `export archive` or `export report`.

Create commands print stable IDs for later commands. Options in square brackets are optional; the command help lists required fields and accepted values.

## Common commands

```text
case create --title TITLE [--purpose TEXT --description TEXT --target-type TYPE --scope TEXT --tags a,b]
case list [--search TEXT]
case show CASE_ID
case edit CASE_ID [--title TITLE --purpose TEXT --description TEXT --target-type TYPE --scope TEXT --tags a,b]
case archive CASE_ID
case restore CASE_ID

entity add CASE_ID --type TYPE --label LABEL [--value VALUE --details JSON --tags a,b --aliases a,b]
source add CASE_ID --type web|document|registry|person|other [--locator URL --title TITLE --accessed-at UTC --author NAME --reliability TEXT]
evidence file CASE_ID FILE [--source ID --source-url URL --note TEXT --mime TYPE --captured-at UTC]
evidence text CASE_ID --text TEXT [--source ID --source-url URL --location TEXT --note TEXT --captured-at UTC]
evidence url CASE_ID URL [--source ID --note TEXT --captured-at UTC]
evidence list CASE_ID
evidence verify EVIDENCE_ID
evidence delete EVIDENCE_ID [--confirm]

claim add CASE_ID --statement TEXT --entities id:role,... [--predicate TEXT --kind observation|inference --status STATUS --reasoning TEXT --recorded-by ID --evidence id:role[:note],...]
relation add CASE_ID --subject ID --predicate PREDICATE --object ID --reasoning TEXT [--statement TEXT --status STATUS --recorded-by ID]
claim link CLAIM_ID EVIDENCE_ID --role supports|contradicts|context [--note TEXT]
claim status CLAIM_ID STATUS --explanation TEXT [--reasoning TEXT]

note add CASE_ID --body TEXT [--title TITLE --entities ID,ID --evidence ID,ID]
search CASE_ID TEXT
technique create --name NAME [--objective TEXT --steps TEXT --example-queries JSON --tools JSON --limitations TEXT --tags-json JSON]
playbook create --name NAME [--scope TEXT --description TEXT]
playbook start --case CASE_ID --playbook ID [--entity ID]

export archive --case CASE_ID --output FILE.zip
export report --case CASE_ID --output FILE.md
import FILE.zip [--mode skip|copy]
log CASE_ID
```

Evidence deletion requires `--confirm` when the item is linked to a claim or playbook step. URL evidence stores the external URL and metadata; it does not download or archive the remote page.

For the complete command list and accepted values, run:

```sh
./build/evidence-trace help
```
