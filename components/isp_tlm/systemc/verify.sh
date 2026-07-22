#!/bin/bash
# Verify/build script for ISP TLM SystemC

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

cd "${SCRIPT_DIR}"

# Build if not exists
if [ ! -f "${BUILD_DIR}/isp_tb" ]; then
    echo "Building..."
    ./build.sh
fi

# Run verification
echo "========================================"
echo "Running verification..."
echo "========================================"

cd "${BUILD_DIR}"

if [ -f "./isp_tb" ]; then
    ./isp_tb
else
    echo "Error: isp_tb not found. Please build first."
    exit 1
fi

echo "========================================"
echo "Verification complete!"
echo "========================================"
