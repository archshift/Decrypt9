#!/bin/bash
set -euo pipefail
SCRIPT_PATH=$(dirname `which $0`)

mkdir -p "${SCRIPT_PATH}/build"
cd "${SCRIPT_PATH}/build"
cmake .. -DCMAKE_TOOLCHAIN_FILE=../externals/Toolchain3dsArm9/Toolchain3dsArm9.cmake

make -j5
