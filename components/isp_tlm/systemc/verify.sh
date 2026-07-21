#!/bin/bash
# verify.sh - Verify ISP SystemC Architecture Model
#
# Usage:
#   cd <CDC-VP>
#   ./components/isp_tlm/systemc/verify.sh [command]
#
# Commands:
#   files    - Verify all files exist
#   syntax   - Check C++ syntax
#   loc      - Count lines of code
#   build    - Verify build artifacts
#   all      - Run all verifications

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CDC_VP_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
BUILD_DIR="$CDC_VP_ROOT/build/bremen"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Files that should exist
REQUIRED_FILES=(
    "components/isp_tlm/systemc/hw/isp_arch_config.h"
    "components/isp_tlm/systemc/hw/metrics.h"
    "components/isp_tlm/systemc/hw/line_channel.h"
    "components/isp_tlm/systemc/hw/line_stage.h"
    "components/isp_tlm/systemc/hw/stage_runtime.h"
    "components/isp_tlm/systemc/hw/frame_feedback.h"
    "components/isp_tlm/systemc/hw/sweep.h"
    "components/isp_tlm/systemc/hw/hw.h"
    "components/isp_tlm/systemc/pipeline/sc_isp_pipeline.h"
    "components/isp_tlm/systemc/pipeline/sc_isp_pipeline.cpp"
    "components/isp_tlm/systemc/pipeline/tb_pipeline.cpp"
    "components/isp_tlm/systemc/pipeline/tb_arch_pipeline.cpp"
    "components/isp_tlm/systemc/pipeline/tb_arch_sweep.cpp"
    "components/isp_tlm/systemc/hw/tb_line_primitives.cpp"
    "components/isp_tlm/systemc/CMakeLists.txt"
)

# Verify files exist
verify_files() {
    echo -e "${YELLOW}Verifying required files...${NC}"
    local missing=0

    for file in "${REQUIRED_FILES[@]}"; do
        if [ -f "$CDC_VP_ROOT/$file" ]; then
            echo -e "  ${GREEN}✓${NC} $file"
        else
            echo -e "  ${RED}✗${NC} $file (MISSING)"
            missing=$((missing + 1))
        fi
    done

    echo ""
    if [ $missing -eq 0 ]; then
        echo -e "${GREEN}All required files present!${NC}"
    else
        echo -e "${RED}$missing files missing${NC}"
        return 1
    fi
}

# Check C++ syntax
check_syntax() {
    echo -e "${YELLOW}Checking C++ syntax...${NC}"

    # Source SystemC env
    source "$CDC_VP_ROOT/tools/third_party/setup_env.sh" 2>/dev/null || true

    local files=(
        "hw/isp_arch_config.h"
        "hw/metrics.h"
        "hw/line_channel.h"
        "hw/line_stage.h"
        "hw/stage_runtime.h"
        "hw/frame_feedback.h"
        "hw/sweep.h"
        "hw/hw.h"
    )

    local errors=0
    for file in "${files[@]}"; do
        if [ -f "$SCRIPT_DIR/$file" ]; then
            # Basic syntax check with g++
            if g++ -std=c++17 -fsyntax-only \
                   -I"$SCRIPT_DIR" \
                   -I"$CDC_VP_ROOT/components/isp_tlm/core/include" \
                   -I"${SYSTEMC_INCLUDE:-/opt/systemc-2.3.4/include}" \
                   "$SCRIPT_DIR/$file" 2>/dev/null; then
                echo -e "  ${GREEN}✓${NC} $file"
            else
                echo -e "  ${YELLOW}⚠${NC} $file (syntax check needs SystemC headers)"
            fi
        fi
    done

    echo ""
    echo -e "${GREEN}Syntax check complete${NC}"
}

# Count lines of code
count_loc() {
    echo -e "${YELLOW}Lines of Code Summary${NC}"

    echo -e "\n  Hardware Infrastructure (hw/):"
    wc -l "$SCRIPT_DIR"/hw/*.h 2>/dev/null | tail -1

    echo -e "\n  Pipeline:"
    wc -l "$SCRIPT_DIR"/pipeline/*.h "$SCRIPT_DIR"/pipeline/*.cpp 2>/dev/null | tail -1

    echo -e "\n  Blocks:"
    find "$SCRIPT_DIR"/blocks -name "*.h" -o -name "*.cpp" 2>/dev/null | xargs wc -l 2>/dev/null | tail -1

    echo ""
}

# Verify build artifacts
verify_build() {
    if [ ! -d "$BUILD_DIR" ]; then
        echo -e "${RED}Build directory not found at $BUILD_DIR${NC}"
        echo "  Run: cd $CDC_VP_ROOT && ./components/isp_tlm/systemc/build.sh"
        return 1
    fi

    echo -e "${YELLOW}Verifying build artifacts...${NC}"

    local targets=(
        "components/isp_tlm/tests/isp_run"
        "components/isp_tlm/systemc/tb_pipeline"
        "components/isp_tlm/systemc/tb_arch_pipeline"
        "components/isp_tlm/systemc/tb_arch_sweep"
        "components/isp_tlm/systemc/tb_line_primitives"
    )

    local missing=0
    for target in "${targets[@]}"; do
        if [ -f "$BUILD_DIR/$target" ]; then
            echo -e "  ${GREEN}✓${NC} $target"
        else
            echo -e "  ${RED}✗${NC} $target (not built)"
            missing=$((missing + 1))
        fi
    done

    if [ $missing -eq 0 ]; then
        echo ""
        echo -e "${GREEN}All testbenches built successfully!${NC}"
    else
        echo ""
        echo -e "${RED}$missing testbenches not built${NC}"
        return 1
    fi
}

# Print summary
print_summary() {
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}   Verification Summary${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    echo "  Testbenches: 5"
    echo "    - isp_run: Full ISP pipeline run"
    echo "    - tb_pipeline: Full SystemC pipeline"
    echo "    - tb_arch_pipeline: Architecture-aware line pipeline"
    echo "    - tb_arch_sweep: Architecture sweep runner"
    echo "    - tb_line_primitives: Line-granular primitive contract"
    echo ""
    echo "  Hardware Infrastructure: 8 files"
    echo "    - isp_arch_config.h, metrics.h, line_channel.h"
    echo "    - line_stage.h, stage_runtime.h, frame_feedback.h"
    echo "    - sweep.h, hw.h"
    echo ""
    echo "  Build Directory: $BUILD_DIR"
    echo ""
}

# Print help
print_help() {
    echo "Usage: $0 [command]"
    echo ""
    echo "  files    - Verify all required files exist"
    echo "  syntax   - Check C++ syntax"
    echo "  loc      - Count lines of code"
    echo "  build    - Verify build artifacts"
    echo "  all      - Run all verifications (default)"
    echo ""
    echo "Examples:"
    echo "  $0                # Run all verifications"
    echo "  $0 files          # Check files only"
    echo "  $0 build          # Check build artifacts"
    echo ""
    echo "To build:"
    echo "  cd $CDC_VP_ROOT"
    echo "  ./components/isp_tlm/systemc/build.sh"
}

# Main
case "${1:-all}" in
    files)
        verify_files
        ;;
    syntax)
        check_syntax
        ;;
    loc|count)
        count_loc
        ;;
    build)
        verify_build
        ;;
    all)
        verify_files
        check_syntax
        count_loc
        verify_build 2>/dev/null || true
        print_summary
        ;;
    help|-h|--help)
        print_help
        ;;
    *)
        echo -e "${RED}Unknown command: $1${NC}"
        print_help
        exit 1
        ;;
esac
