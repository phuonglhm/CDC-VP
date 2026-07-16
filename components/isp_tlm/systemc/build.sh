#!/bin/bash
# build.sh - Build ISP SystemC Architecture Model
#
# Usage:
#   cd <CDC-VP>
#   ./components/isp_tlm/systemc/build.sh [target]
#
# Examples:
#   ./components/isp_tlm/systemc/build.sh              # Build all targets
#   ./components/isp_tlm/systemc/build.sh clean        # Clean build artifacts
#   ./components/isp_tlm/systemc/build.sh isp_run     # Build specific target
#
# Requirements:
#   - SystemC 2.3.4 installed at /opt/systemc-2.3.4
#   - Setup scripts: ./tools/third_party/setup_third_party.sh

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

CDC_VP_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Build directory (matches CDC-VP convention)
BUILD_DIR="$CDC_VP_ROOT/build/bremen"

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}   ISP SystemC Architecture Model Build${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "  Root:  $CDC_VP_ROOT"
echo "  Build: $BUILD_DIR"
echo ""

# Check if in CDC-VP directory
check_cdc_vp() {
    if [ ! -f "$CDC_VP_ROOT/tools/third_party/setup_third_party.sh" ]; then
        echo -e "${RED}ERROR: Must be run from CDC-VP root directory${NC}"
        echo "  Expected to find: $CDC_VP_ROOT/tools/third_party/setup_third_party.sh"
        exit 1
    fi
}

# Setup third party dependencies
setup_dependencies() {
    echo -e "${YELLOW}Setting up third party dependencies...${NC}"
    
    # Ensure SystemC paths are set (use default if not)
    if [ -z "$SYSTEMC_INCLUDE" ]; then
        export SYSTEMC_INCLUDE="/opt/systemc-2.3.4/include"
        export SYSTEMC_LIB="/opt/systemc-2.3.4/lib/libsystemc.so"
    fi

    # Verify SystemC
    if [ ! -f "$SYSTEMC_INCLUDE/systemc.h" ]; then
        echo -e "${RED}ERROR: SystemC not found at $SYSTEMC_INCLUDE${NC}"
        echo "  Install SystemC 2.3.4 or set SYSTEMC_INCLUDE"
        exit 1
    fi
    
    echo -e "${GREEN}  SystemC: OK ($SYSTEMC_INCLUDE)${NC}"
    echo ""
}

# Check dependencies
check_dependencies() {
    echo -e "${YELLOW}Checking dependencies...${NC}"

    if ! command -v cmake &> /dev/null; then
        echo -e "${RED}ERROR: cmake not found${NC}"
        exit 1
    fi

    if ! command -v ninja &> /dev/null; then
        echo -e "${YELLOW}WARNING: ninja not found, will use make${NC}"
    fi

    if ! command -v g++ &> /dev/null; then
        echo -e "${RED}ERROR: g++ not found${NC}"
        exit 1
    fi

    echo -e "${GREEN}  cmake: OK${NC}"
    echo -e "${GREEN}  g++: OK${NC}"
    echo ""
}

# Clean build directory
clean() {
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf "$BUILD_DIR"
    echo -e "${GREEN}Done${NC}"
}

# Configure with cmake (Ninja generator)
configure() {
    echo -e "${YELLOW}Configuring with CMake (Ninja)...${NC}"
    
    # Create build directory
    mkdir -p "$BUILD_DIR"
    
    # Ensure SystemC paths are set
    export SYSTEMC_INCLUDE="${SYSTEMC_INCLUDE:-/opt/systemc-2.3.4/include}"
    export SYSTEMC_LIB="${SYSTEMC_LIB:-/opt/systemc-2.3.4/lib/libsystemc.so}"

    cmake -S "$CDC_VP_ROOT" -B "$BUILD_DIR" -G Ninja \
        -DCMAKE_C_COMPILER=/usr/bin/gcc \
        -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
        -DCDC_BUILD_TESTS=ON \
        -DSYSTEMC_INCLUDE_DIR="$SYSTEMC_INCLUDE" \
        -DSYSTEMC_LIBRARY="$SYSTEMC_LIB" \
        -DCMAKE_BUILD_TYPE=Release

    echo -e "${GREEN}Configuration complete${NC}"
}

# Build all targets
build_all() {
    echo -e "${YELLOW}Building all targets...${NC}"
    echo ""

    cd "$BUILD_DIR"
    ninja

    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}   Build Summary${NC}"
    echo -e "${GREEN}========================================${NC}"

    echo ""
    echo "Built executables:"
    find "$BUILD_DIR" -name "tb_*" -type f -executable 2>/dev/null | while read f; do
        size=$(du -h "$f" | cut -f1)
        echo "  $f ($size)"
    done

    echo ""
    echo -e "${GREEN}Build complete!${NC}"
}

# Build specific target
build_target() {
    local target="$1"
    echo -e "${YELLOW}Building $target...${NC}"

    cd "$BUILD_DIR"
    ninja "$target"

    echo -e "${GREEN}$target built successfully${NC}"
}

# Run target
run_target() {
    local target="$1"
    local binary="$BUILD_DIR/$target"
    
    # Find actual binary path
    if [ -f "$BUILD_DIR/components/isp_tlm/tests/$target" ]; then
        binary="$BUILD_DIR/components/isp_tlm/tests/$target"
    fi
    
    if [ ! -f "$binary" ]; then
        echo -e "${RED}ERROR: $binary not found${NC}"
        echo "  Run: $0 build $target"
        exit 1
    fi
    
    echo -e "${YELLOW}Running $binary...${NC}"
    "$binary"
}

# List available targets
list_targets() {
    echo -e "${YELLOW}Available testbench targets:${NC}"
    echo ""
    echo "  isp_run         - Full ISP pipeline run"
    echo "  tb_arch_pipeline - Architecture-aware timed pipeline"
    echo "  tb_power_metrics - Power estimation for ISP pipeline"
    echo "  tb_arch_sweep   - Architecture sweep runner"
    echo "  tb_dma_integration - DMA and memory model integration"
    echo ""
}

# Main
check_cdc_vp
setup_dependencies

case "${1:-build}" in
    clean)
        clean
        ;;
    configure|config)
        check_dependencies
        configure
        ;;
    build)
        check_dependencies
        if [ ! -f "$BUILD_DIR/build.ninja" ]; then
            configure
        fi
        build_all
        ;;
    run)
        check_dependencies
        if [ "$2" ]; then
            run_target "$2"
        else
            echo -e "${RED}Usage: $0 run <target>${NC}"
            list_targets
            exit 1
        fi
        ;;
    list|ls)
        list_targets
        ;;
    help|-h|--help)
        echo "Usage: $0 [command] [target]"
        echo ""
        echo "Commands:"
        echo "  build [target]  - Build targets (default: all)"
        echo "  run <target>    - Run specific target"
        echo "  clean           - Clean build directory"
        echo "  configure       - Reconfigure cmake"
        echo "  list            - List available targets"
        echo ""
        echo "Examples:"
        echo "  $0                # Build all"
        echo "  $0 build          # Build all"
        echo "  $0 build isp_run  # Build specific target"
        echo "  $0 run isp_run    # Run isp_run"
        echo "  $0 clean          # Clean"
        ;;
    *)
        check_dependencies
        if [ ! -f "$BUILD_DIR/build.ninja" ]; then
            configure
        fi
        build_all
        ;;
esac
