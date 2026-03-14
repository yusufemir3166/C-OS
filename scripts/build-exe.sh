#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build-mingw"
TOOLCHAIN_FILE="${ROOT_DIR}/cmake/toolchains/mingw64.cmake"

if ! command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1; then
  echo "[ERROR] MinGW toolchain not found: x86_64-w64-mingw32-g++" >&2
  echo "Install example (Debian/Ubuntu): sudo apt-get update && sudo apt-get install -y mingw-w64" >&2
  exit 1
fi

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" --config Release

EXE_PATH="${BUILD_DIR}/C-OS.exe"
if [[ ! -f "${EXE_PATH}" ]]; then
  EXE_PATH="${BUILD_DIR}/Release/C-OS.exe"
fi

if [[ -f "${EXE_PATH}" ]]; then
  echo "[OK] EXE created: ${EXE_PATH}"
else
  echo "[ERROR] Build finished but C-OS.exe not found." >&2
  exit 1
fi
