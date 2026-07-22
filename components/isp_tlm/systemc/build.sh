#!/bin/bash
# Build script for ISP TLM SystemC

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

echo "========================================"
echo "Building ISP TLM SystemC"
echo "========================================"

# Create build directory
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Check for SystemC
if [ -z "$SYSTEMC_ROOT" ]; then
    # Try common locations
    if [ -d "/usr/local/systemc-2.3.3" ]; then
        export SYSTEMC_ROOT="/usr/local/systemc-2.3.3"
    elif [ -d "$HOME/systemc-2.3.3" ]; then
        export SYSTEMC_ROOT="$HOME/systemc-2.3.3"
    else
        echo "Warning: SYSTEMC_ROOT not set"
    fi
fi

# Configure
echo "Configuring..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_TRACE=ON

# Build
echo "Building..."
make -j$(nproc)

echo "========================================"
echo "Build complete!"
echo "Executable: ${BUILD_DIR}/isp_tb"
echo "========================================"
