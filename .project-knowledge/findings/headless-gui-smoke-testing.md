# Headless GUI smoke testing and offscreen capture

- **State:** current
- **Finding:** When verifying GUI rendering headlessly or in CI without an active X11/Wayland display, running `evidence-trace-gui` requires `QT_QPA_PLATFORM=offscreen`. Without this variable, the Qt platform plugin aborts on display connection failure. Passing `--screenshot <path>` alongside `--page <page_name>` allows headless visual inspection without a running window manager.
- **Evidence:** 
  - Execution command: `QT_QPA_PLATFORM=offscreen ./build/evidence-trace-gui --data-dir /tmp/evidence-trace-demo --page settings --screenshot /tmp/evidence-trace-settings.png`
  - Output: Exit code 0, generated valid 47 KB PNG file at `/tmp/evidence-trace-settings.png`.
  - Implementation: Entry point and argument parsing in [`src/gui/main.cpp`](../../src/gui/main.cpp).
- **Verified:** 2026-10-12 on Linux x86_64, CMake Debug build, commit adade97.
- **Related guidance:** [`docs/development/build.md`](../../docs/development/build.md#L47-L53), [`skills/gui-acceptance-testing/SKILL.md`](../../skills/gui-acceptance-testing/SKILL.md).
