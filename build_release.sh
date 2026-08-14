#!/usr/bin/env bash
# ps5-unified-autoloader — Local SDK Versioned Build Script
#
# Usage:
#   ./build_release.sh
#
# Requirements are intentionally not installed by this script. The PS5
# payload SDK and pldmgr.elf must already be available locally.
set -euo pipefail
cd "$(dirname "$0")"

# -----------------------------------------------------------------------
# The shortcut assets are embedded as-is; this script accepts no arguments.
# -----------------------------------------------------------------------
if [[ "$#" -ne 0 ]]; then
    echo "Error: this build script does not accept arguments" >&2
    exit 1
fi

# -----------------------------------------------------------------------
# Extract version from include/autoloader.h
# -----------------------------------------------------------------------
VERSION=$(grep '#define AUTOLOADER_VERSION' include/autoloader.h \
    | awk '{print $3}' | tr -d '"' | tr -d '\r')

if [ -z "$VERSION" ]; then
    echo "Error: Could not find AUTOLOADER_VERSION in include/autoloader.h"
    exit 1
fi

# -----------------------------------------------------------------------
# Compute short commit hash (or DEV_<timestamp> if working tree is dirty)
# -----------------------------------------------------------------------
if git diff --quiet 2>/dev/null && git diff --cached --quiet 2>/dev/null; then
    SHORT_HASH=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
else
    SHORT_HASH="DEV_$(date -u +"%Y%m%d_%H%M%S")"
fi

OUTPUT_ELF="autoloader_v${VERSION}_${SHORT_HASH}.elf"

echo "=== ps5-unified-autoloader v${VERSION} (${SHORT_HASH}) ==="

# -----------------------------------------------------------------------
# Validate preinstalled local build requirements
# -----------------------------------------------------------------------
SDK_DIR="${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}"
CC="$SDK_DIR/bin/prospero-clang"
STRIP="$SDK_DIR/bin/prospero-strip"

if [[ ! -x "$CC" ]]; then
    echo "Error: PS5 SDK compiler not found or not executable: $CC" >&2
    exit 1
fi
if [[ ! -x "$STRIP" ]]; then
    echo "Error: PS5 SDK strip tool not found or not executable: $STRIP" >&2
    exit 1
fi
if [[ ! -s "pldmgr.elf" ]]; then
    echo "Error: required local dependency is missing or empty: $(pwd)/pldmgr.elf" >&2
    exit 1
fi

# -----------------------------------------------------------------------
# Build autoloader.elf directly with the local SDK
# -----------------------------------------------------------------------
echo "Building autoloader.elf with local SDK: $SDK_DIR"
make clean all CC="$CC" STRIP="$STRIP" SDK="$SDK_DIR"

# -----------------------------------------------------------------------
# Rename to versioned output
# -----------------------------------------------------------------------
if [ ! -f "autoloader.elf" ]; then
    echo "Error: autoloader.elf not found after build."
    exit 1
fi

mv autoloader.elf "$OUTPUT_ELF"
echo ""
echo "=== Build complete! ==="
echo "    Output: $OUTPUT_ELF"
