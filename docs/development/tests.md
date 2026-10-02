# Tests

Status: Approved

Implementation status: Implemented for the current capabilities described below; verification limitations are stated separately.

The integration test is intentionally behavior-oriented. It creates isolated SQLite/attachment directories and checks:

- restart persistence and database CHECK/foreign-key constraints;
- case metadata updates and entity alias persistence across a restart;
- file copying, SHA-256 verification, original-file independence, and corruption detection;
- directional relations, claim evidence roles, confirmation safeguards, status history, and reports;
- search, notes, and playbook run snapshot immutability;
- ZIP export/import round trips, copy remapping, hash mismatch rejection, and traversal rejection.

Run it with:

```sh
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

The GUI smoke path is an executable render check, not a unit-test substitute:

First create a temporary case and keep the printed ID for the screenshot:

```sh
./build/evidence-trace --data-dir /tmp/evidence-trace-gui-demo init
CASE_ID=$(./build/evidence-trace --data-dir /tmp/evidence-trace-gui-demo case create \
  --title "GUI smoke case" | sed -n 's/^CASE_ID=//p')

QT_QPA_PLATFORM=offscreen ./build/evidence-trace-gui \
  --data-dir /tmp/evidence-trace-gui-demo \
  --open-case "$CASE_ID" \
  --section claims --screenshot /tmp/evidence-trace-gui-claims.png
```

The current verification record covers offscreen rendering and automated core
tests. Native X11/Wayland mouse interaction still needs to be checked on a
normal desktop session.


## Documentation checks

See [documentation validation](documentation-validation.md) for migration-aware status, local links, syntax, placeholders, and proposal exclusions.

## Migration verification on 2026-10-01

The existing `build/evidence_trace_tests` executable was run directly and printed `All integration tests passed`. It was not rebuilt. Historical acceptance and rendering results below do not constitute a fresh source-build or native interaction check.
