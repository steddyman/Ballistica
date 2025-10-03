#!/usr/bin/env bash
set -euo pipefail

# Local CIA build helper for macOS (arm64 & x86_64) and Linux (x86_64) without needing the CI workflow.
# Usage: ./scripts/build_cia_local.sh [build-dir]
# Produces: Ballistica.3dsx (from make) and Ballistica.cia in build dir (default: ./dist)
# Requirements:
#  - devkitARM / devkitPro toolchain in PATH (for make building .3dsx)
#  - bin/mac-arm64/makerom (mac Apple Silicon) OR bin/mac-x86_64/makerom (mac Intel/Rosetta)
#  - bin/mac-arm64/bannertool OR bin/mac-x86_64/bannertool
#  - Linux CI uses bin/linux-x86_64/* (you can also use these locally on Linux)
#  - assets/cia/icon.png, assets/cia/banner.png, assets/cia/banner.wav
#  - assets/cia/cia.rsf

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${1:-${ROOT_DIR}/dist}"
CIA_ASSETS_DIR="${ROOT_DIR}/assets/cia"
RSF_FILE="${CIA_ASSETS_DIR}/cia.rsf"
ICON_PNG="${CIA_ASSETS_DIR}/icon.png"
BANNER_PNG="${CIA_ASSETS_DIR}/banner.png"
BANNER_WAV="${CIA_ASSETS_DIR}/banner.wav"
TARGET_NAME="Ballistica"

mkdir -p "${OUT_DIR}"

# Detect platform for tool selection
UNAME_S="$(uname -s)"
ARCH="$(uname -m)"
if [[ "${UNAME_S}" == "Darwin" ]]; then
  if [[ "${ARCH}" == arm64 ]]; then
    TOOL_DIR="${ROOT_DIR}/bin/mac-arm64"
  else
    # Treat any non-arm64 Darwin arch as x86_64/Rosetta
    TOOL_DIR="${ROOT_DIR}/bin/mac-x86_64"
  fi
elif [[ "${UNAME_S}" == "Linux" ]]; then
  TOOL_DIR="${ROOT_DIR}/bin/linux-x86_64"
else
  echo "Unsupported platform ${UNAME_S} ${ARCH}." >&2
  exit 1
fi

BANNERTOOL="${TOOL_DIR}/bannertool"
MAKEROM="${TOOL_DIR}/makerom"

chmod +x "${BANNERTOOL}" "${MAKEROM}" || true

for f in "${ICON_PNG}" "${BANNER_PNG}" "${BANNER_WAV}" "${RSF_FILE}"; do
  [[ -f "$f" ]] || { echo "Missing required asset: $f" >&2; exit 1; }
done

# 1. Build project (expects Makefile to produce *.3dsx in root or build dir)
( cd "${ROOT_DIR}" && make -s )

THREEDSX="${ROOT_DIR}/${TARGET_NAME}.3dsx"
ELF_FILE="${ROOT_DIR}/${TARGET_NAME}.elf"
if [[ ! -f "${THREEDSX}" ]]; then
  echo "Expected ${THREEDSX} not found. Check Makefile target name." >&2
  exit 1
fi

# 2. Generate SMDH
SMDH_FILE="${OUT_DIR}/${TARGET_NAME}.smdh"
"${BANNERTOOL}" makesmdh -s "${TARGET_NAME}" -l "${TARGET_NAME}" -p "Ballistica" -i "${ICON_PNG}" -o "${SMDH_FILE}"

# 3. Generate banner
BANNER_BIN="${OUT_DIR}/banner.bnr"
"${BANNERTOOL}" makebanner -i "${BANNER_PNG}" -a "${BANNER_WAV}" -o "${BANNER_BIN}"

CIA_FILE="${OUT_DIR}/${TARGET_NAME}.cia"
echo "Using RSF: ${RSF_FILE}" >&2
set +e
"${MAKEROM}" -f cia \
  -o "${CIA_FILE}" \
  -rsf "${RSF_FILE}" \
  -target t \
  -exefslogo \
  -elf "${ELF_FILE}" \
  -icon "${SMDH_FILE}" \
  -banner "${BANNER_BIN}" \
  -DAPP_TITLE="${TARGET_NAME}" \
  -DAPP_PRODUCT="Ballistica" \
  -DAPP_UNIQUE_ID=0x12345
RET=$?
set -e
if [[ $RET -ne 0 ]]; then
  echo "makerom failed (exit $RET)." >&2
  echo "Check RSF AccessControlInfo fields (SystemCallAccess range, ServiceAccessControl list) and SaveDataSize format." >&2
  exit $RET
fi
echo "CIA built successfully: ${CIA_FILE}" >&2

echo "Artifacts:"
ls -lh "${CIA_FILE}" "${THREEDSX}" 2>/dev/null || true

echo "Done."
