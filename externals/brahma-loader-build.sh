#!/bin/bash
set -euo pipefail

SCRIPT_PATH="$(cd "$(dirname "$0")" && pwd)"

TARGET_DIR="$1"
TARGET_NAME="$2"
APPINFO_FILE="$3"

TARGET_BIN="${TARGET_DIR}/${TARGET_NAME}.bin"

LDR_DIR="${SCRIPT_PATH}/BrahmaLoader"
LDR_APPINFO_FILE="${LDR_DIR}/resources/AppInfo"
LDR_PAYLOAD_FILE="${LDR_DIR}/data/payload.bin"
LDR_OUTPUT_DIR="${LDR_DIR}/output"

cp "$TARGET_BIN" "$LDR_PAYLOAD_FILE"
cp "$APPINFO_FILE" "$LDR_APPINFO_FILE"
( cd "$LDR_DIR" && make )
find "${LDR_OUTPUT_DIR}" -type f -exec cp {} "$TARGET_DIR" \;
rm -r "${LDR_OUTPUT_DIR}"
