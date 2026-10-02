#!/usr/bin/env bash
set -euo pipefail

source_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
install_prefix="${EVIDENCE_TRACE_PREFIX:-${HOME}/.local}"
build_dir="${EVIDENCE_TRACE_BUILD_DIR:-${source_dir}/build-install}"
build_jobs="${EVIDENCE_TRACE_JOBS:-2}"

usage() {
    cat <<'EOF'
Usage: ./install.sh [--prefix DIR] [--build-dir DIR] [--jobs COUNT]

Build Evidence Trace in Release mode and install the GUI and CLI under DIR/bin.
The default install prefix is ~/.local. Required build dependencies must already
be installed; this script does not use sudo or install system packages.

Options:
  --prefix DIR      Install prefix (default: ~/.local)
  --build-dir DIR   CMake build directory (default: ./build-install)
  --jobs COUNT      Number of parallel build jobs (default: 2)
  -h, --help        Show this help
EOF
}

while (($# > 0)); do
    case "$1" in
        --prefix)
            (($# >= 2)) || { echo "Missing value for --prefix" >&2; exit 2; }
            install_prefix="$2"
            shift 2
            ;;
        --build-dir)
            (($# >= 2)) || { echo "Missing value for --build-dir" >&2; exit 2; }
            build_dir="$2"
            shift 2
            ;;
        --jobs)
            (($# >= 2)) || { echo "Missing value for --jobs" >&2; exit 2; }
            build_jobs="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [[ ! "$build_jobs" =~ ^[1-9][0-9]*$ ]]; then
    echo "--jobs must be a positive integer" >&2
    exit 2
fi

if ! command -v cmake >/dev/null 2>&1; then
    echo "CMake 3.20 or newer is required. Install CMake and try again." >&2
    exit 1
fi

cmake_version="$(cmake --version | head -n 1 | awk '{print $3}')"
if [[ "$(printf '%s\n' 3.20 "$cmake_version" | sort -V | head -n 1)" != "3.20" ]]; then
    echo "CMake 3.20 or newer is required; found $cmake_version." >&2
    exit 1
fi

case "$install_prefix" in
    /*) ;;
    *) install_prefix="${source_dir}/${install_prefix}" ;;
esac
case "$build_dir" in
    /*) ;;
    *) build_dir="${source_dir}/${build_dir}" ;;
esac

echo "Configuring Evidence Trace (Release)..."
cmake -S "$source_dir" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$install_prefix"

echo "Building with $build_jobs job(s)..."
cmake --build "$build_dir" --parallel "$build_jobs"

echo "Installing to $install_prefix/bin..."
cmake --install "$build_dir"

cat <<EOF

Evidence Trace is installed.
GUI: $install_prefix/bin/evidence-trace-gui
CLI: $install_prefix/bin/evidence-trace
EOF

if [[ ":${PATH}:" != *":${install_prefix}/bin:"* ]]; then
    echo "Add $install_prefix/bin to your PATH to run the commands by name."
fi
