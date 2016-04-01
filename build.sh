#!/bin/bash
set -euo pipefail
SCRIPT_PATH="$(cd "$(dirname "$0")" && pwd)"

mkdir -p "${SCRIPT_PATH}/build"
cd "${SCRIPT_PATH}/build"
cmake .. -DCMAKE_TOOLCHAIN_FILE=../externals/Toolchain3dsArm9/Toolchain3dsArm9.cmake

make -j5
