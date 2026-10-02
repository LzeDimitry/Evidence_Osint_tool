# Database migrations

Status: Approved

Implementation status: Implemented for the current capabilities described below; verification limitations are stated separately.

The database uses SQLite PRAGMA user_version:

- Migration 1 creates cases, investigation records, links, playbook templates/runs, notes, activity events, and indexes.
- Migration 2 adds precise quotation locations to evidence.
- Migration 3 adds the persisted JSON alias list to entities.

The current schema version is 3. Opening a newer version is rejected before migrations are applied. Opening an older existing database creates a timestamped `.pre-migration-*.bak` copy before applying sequential migrations. Foreign keys are enabled for every connection. Migration 1 also seeds the English starter `Username investigation` playbook and `Check commit history` technique; global templates are not copied into case archives.
