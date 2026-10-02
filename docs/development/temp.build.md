Target: `docs/development/build.md`
Operation: replace
Proposal revision: 1

## Reason and affected documents

Document the Linux desktop launcher, supplied application icon, and maximized startup behavior implemented for the Bazzite KDE Plasma report. Related proposals cover the GUI behavior, product specification, installation guide, and README.

## Discrepancies and verification

The previous installation documented only the executable files; it did not install a desktop entry or icon. The GUI previously showed without requesting maximized state. The GUI target built successfully with CMake, and `cmake --install build --prefix /tmp/evidence-trace-icon-check` installed the executable, desktop entry, and icon. Automated tests were not run.

## Complete proposed target content

<!-- BEGIN PROPOSED CONTENT -->
# Development build

Status: Approved

Implementation status: Implemented for the current capabilities described below; verification limitations are stated separately.

Prerequisites: CMake 3.20 or newer, a C++20 compiler, SQLite3 development files, OpenSSL Crypto, libzip development files, and Qt 6 Core/Gui/Widgets development files. These are required by `CMakeLists.txt`.

## Install from a GitHub source download

On Debian or Ubuntu, install the compiler and library development packages first:

```sh
sudo apt update
sudo apt install build-essential cmake qt6-base-dev libsqlite3-dev libssl-dev libzip-dev
```

After downloading and extracting the source archive, run `./install.sh` from the repository directory. The script configures and builds a Release version, then installs `evidence-trace-gui` and `evidence-trace` under `~/.local/bin`, plus a desktop menu entry and the Evidence Trace icon. The GUI opens maximized. It does not install system packages or use `sudo`; use the commands above to prepare dependencies. Set `EVIDENCE_TRACE_PREFIX`, `EVIDENCE_TRACE_BUILD_DIR`, or `EVIDENCE_TRACE_JOBS`, or pass `--prefix`, `--build-dir`, and `--jobs` to customize the install location, build directory, or parallelism. Run `./install.sh --help` for details.

Alternatively, configure a development build as below and install it with:

```sh
cmake --install build --prefix "$HOME/.local"
```

Add `~/.local/bin` to `PATH` if needed. This source build flow is currently documented for Linux; package-specific instructions are provided for Debian and Ubuntu.

`build/` is an out-of-source CMake build tree. It contains compiler output,
CMake's cache, test output, and temporary demo data; it is not part of the
application source and must not be committed. One build directory is enough.
Use another directory only when you intentionally need a clean configuration
or a different toolchain.

Run these commands from the repository root. On 2026-10-02, a fresh Release configuration and build completed through `install.sh`; the CTest suite was not run as part of that installation check.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

The data passed with `--data-dir` is separate from the build tree. Keep real
investigations outside the repository and use a case ZIP export for backups.

The CMake target `evidence_trace_core` contains the domain, database, storage, services, and import/export modules. `evidence-trace-gui` links that core to Qt 6 Core/Gui/Widgets; `evidence-trace` is the CLI; `evidence_trace_tests` is the CTest integration executable.

For a headless render smoke test:

```sh
QT_QPA_PLATFORM=offscreen ./build/evidence-trace-gui \
  --data-dir /tmp/evidence-trace-demo \
  --page settings --screenshot /tmp/evidence-trace-settings.png
```
<!-- END PROPOSED CONTENT -->
