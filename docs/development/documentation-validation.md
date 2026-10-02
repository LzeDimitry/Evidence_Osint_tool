# Documentation validation

Status: Approved

This guide implements the mechanical validation scope of the [documentation policy](../policy/project_documentation_policy.md); it does not redefine approval rules.

## Validation scope

Check repository README/CHANGELOG and maintained Markdown under docs. Exclude temp.* proposals from canonical discovery and validate their delimited target content relative to its final destination during review. Exclude frozen mb_remeber, reference examples, and binary reference assets from maintained-document lint/status enforcement.

For migrated maintained documents require Draft, Approved, or Archived metadata. Unknown legacy approval remains unverified until owner adoption. Templates may contain intentional placeholders; Approved maintained project documents may not. Draft and archived placeholders are permitted intentionally.

Validate UTF-8, a document title, balanced code fences, headings/list/table syntax, and broken local file links including heading fragments. Canonical navigation must not link to temp.* proposals as current guidance. External link reachability is outside lightweight local checks.

## CI follow-up

Implementation status: Planned for automated CI integration. No repository CI configuration exists in the inspected tree. This documentation migration does not install packages or edit repository-root workflow/configuration files. A separate owner-authorized repository change must provide Markdown lint, local-link checks, status checks for migrated documents, placeholder checks, and proposal exclusions before automated policy compliance is claimed.

Migration review uses a staged overlay of proposed targets plus existing files. Hash the review proposals, validate the overlay, and verify original documents and frozen references remain byte-identical. Checks do not approve content or establish semantic freshness.

## Review evidence

The migration review manifest records targets, revisions, content hashes, checks run, and limitations. Product tests are unnecessary for pure process/wording edits; if behavior documentation changes, inspect the relevant code/tests and record available verification.
