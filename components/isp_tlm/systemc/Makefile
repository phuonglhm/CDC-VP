# Makefile for ISP TLM SystemC
# Clean pattern-based build for all testbenches

#=============================================================================
# SystemC Configuration
#=============================================================================
SYSTEMC_HOME ?= /usr
LIB_DIR := $(shell \
    if [ -d $(SYSTEMC_HOME)/lib/x86_64-linux-gnu ]; then \
        echo "lib/x86_64-linux-gnu"; \
    elif [ -d $(SYSTEMC_HOME)/lib-linux64 ]; then \
        echo "lib-linux64"; \
    elif [ -d $(SYSTEMC_HOME)/lib-linux ]; then \
        echo "lib-linux"; \
    else \
        echo "lib"; \
    fi)

SYSTEMC_INCLUDE = -I$(SYSTEMC_HOME)/include
SYSTEMC_LIB = -L$(SYSTEMC_HOME)/$(LIB_DIR)
LDFLAGS = -lsystemc -lm -lpthread

#=============================================================================
# Compiler Configuration
#=============================================================================
GCCDIR = /usr/lib/gcc/x86_64-linux-gnu/15
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g -O2 -B$(GCCDIR)
CXXFLAGS += -I$(SYSTEMC_HOME)/include/c++/15
CXXFLAGS += -I$(SYSTEMC_HOME)/include/x86_64-linux-gnu/c++/15

# Directories
SRC_DIR = src
TB_DIR = tb
BUILD_DIR = build

# Include paths (all blocks + common + metrics)
INCLUDE_DIRS = $(foreach blk,$(BLOCKS),-I./$(SRC_DIR)/blocks/$(blk))
INCLUDE_DIRS += -I./$(SRC_DIR) -I./$(SRC_DIR)/common -I./$(SRC_DIR)/metrics -I./$(SRC_DIR)/axi -I./$(SRC_DIR)/pipeline

# All ISP blocks
BLOCKS = blc crop wb ccm csc demosaic gamma sharpen dpc bnr 2dnr awb ae oecf dgain lsc ldci

# All testbenches (single-block TBs excluded - require Verilator RTL sim)
TESTS = simple_isp_test isp_top_tb isp_integration_tb

# Debug logging (default: off for metrics-only runs)
# Enable with: make DEBUG=1 isp_integration_tb
DEBUG ?= 0
ifeq ($(DEBUG),1)
    CXXFLAGS += -DDEBUG_LOG
endif

#=============================================================================
# Default run arguments (override with make variables)
#=============================================================================
# Default image size and frame count
W ?= 64
H ?= 32
FRAMES ?= 2
# Default input image (empty = synthetic, e.g. IMG=../input/ColorChecker_2592x1536_12bits_RGGB.raw)
IMG ?=

# Build dir
$(shell mkdir -p $(BUILD_DIR))

#=============================================================================
# Pattern Rules: Build any .cpp in tb/ -> binary in build/
#=============================================================================

# Build integration test (most commonly used)
isp_integration_tb: $(BUILD_DIR)/isp_integration_tb

$(BUILD_DIR)/isp_integration_tb: $(TB_DIR)/isp_integration_tb.cpp \
    $(wildcard $(SRC_DIR)/blocks/*/*.h) \
    $(wildcard $(SRC_DIR)/blocks/*/*/*.h) \
    $(wildcard $(SRC_DIR)/common/*.h) \
    $(wildcard $(SRC_DIR)/metrics/*.h) \
    $(wildcard $(SRC_DIR)/axi/*.h) \
    $(wildcard $(SRC_DIR)/pipeline/*.h) \
    $(wildcard $(SRC_DIR)/utils/*.h)
	@echo "Building $@..."
	$(CXX) $(CXXFLAGS) $(SYSTEMC_INCLUDE) $(INCLUDE_DIRS) -o $@ $< $(SYSTEMC_LIB) $(LDFLAGS)
	@echo "Done: $@"

# Build isp_top test
isp_top_tb: $(BUILD_DIR)/isp_top_tb

$(BUILD_DIR)/isp_top_tb: $(TB_DIR)/isp_top_tb.cpp \
    $(wildcard $(SRC_DIR)/blocks/*/*.h) \
    $(wildcard $(SRC_DIR)/blocks/*/*/*.h) \
    $(wildcard $(SRC_DIR)/common/*.h) \
    $(wildcard $(SRC_DIR)/metrics/*.h) \
    $(wildcard $(SRC_DIR)/axi/*.h) \
    $(wildcard $(SRC_DIR)/pipeline/*.h) \
    $(wildcard $(SRC_DIR)/utils/*.h)
	@echo "Building $@..."
	$(CXX) $(CXXFLAGS) $(SYSTEMC_INCLUDE) $(INCLUDE_DIRS) -o $@ $< $(SYSTEMC_LIB) $(LDFLAGS)
	@echo "Done: $@"

# Build simple_isp_test
simple_isp_test: $(BUILD_DIR)/simple_isp_test

$(BUILD_DIR)/simple_isp_test: $(TB_DIR)/simple_isp_test.cpp \
    $(wildcard $(SRC_DIR)/blocks/blc/*.h) \
    $(wildcard $(SRC_DIR)/blocks/crop/*.h) \
    $(wildcard $(SRC_DIR)/common/*.h) \
    $(wildcard $(SRC_DIR)/metrics/*.h)
	@echo "Building $@..."
	$(CXX) $(CXXFLAGS) $(SYSTEMC_INCLUDE) $(INCLUDE_DIRS) -o $@ $< $(SYSTEMC_LIB) $(LDFLAGS)
	@echo "Done: $@"

#=============================================================================
# Run Rules: Build (if needed) then run with arguments
#=============================================================================

# Integration test (most common)
# Usage:
#   make run-isp_integration_tb                           # default: 64x32 synthetic
#   make run-isp_integration_tb W=2592 H=1536 FRAMES=1   # custom size
#   make run-isp_integration_tb IMG=../input/ColorChecker_2592x1536_12bits_RGGB.raw W=2592 H=1536
run-isp_integration_tb: $(BUILD_DIR)/isp_integration_tb
	@echo ""
	@echo "=== Running isp_integration_tb ==="
	@echo "    W=$(W) H=$(H) FRAMES=$(FRAMES) IMG=$(or $(IMG),synthetic)"
	@echo ""
	$(if $(IMG),./$(BUILD_DIR)/isp_integration_tb -w $(W) -h $(H) -f $(FRAMES) -i $(IMG),\
		./$(BUILD_DIR)/isp_integration_tb -w $(W) -h $(H) -f $(FRAMES))
	@echo ""

# isp_top test
run-isp_top_tb: $(BUILD_DIR)/isp_top_tb
	@echo ""
	@echo "=== Running isp_top_tb ==="
	@echo "    W=$(W) H=$(H) FRAMES=$(FRAMES)"
	@echo ""
	./$(BUILD_DIR)/isp_top_tb -w $(W) -h $(H) -f $(FRAMES)
	@echo ""

# simple_isp_test (no args needed)
run-simple_isp_test: $(BUILD_DIR)/simple_isp_test
	@echo ""
	@echo "=== Running simple_isp_test ==="
	@echo ""
	./$(BUILD_DIR)/simple_isp_test
	@echo ""

#=============================================================================
# Utility targets
#=============================================================================

# Build all testbenches
all-tests: $(foreach t,$(TESTS),$(BUILD_DIR)/$(t))
	@echo ""
	@echo "All tests built successfully."
	@echo ""

# Clean build directory
clean:
	@echo "Cleaning build directory..."
	rm -rf $(BUILD_DIR)
	@echo "Done."

# Help
help:
	@echo ""
	@echo "ISP TLM SystemC Makefile"
	@echo "========================"
	@echo ""
	@echo "BUILD targets (compile only):"
	@echo "  make isp_integration_tb     - Build integration test (full pipeline)"
	@echo "  make isp_top_tb            - Build isp_top test"
	@echo "  make simple_isp_test       - Build simple CROP+BLC test"
	@echo "  make all-tests             - Build all testbenches"
	@echo ""
	@echo "RUN targets (build + execute with args):"
	@echo "  make run-isp_integration_tb                          # default: 64x32 synthetic"
	@echo "  make run-isp_integration_tb W=128 H=64 FRAMES=1    # custom size"
	@echo "  make run-isp_integration_tb IMG=<file.raw> W=2592 H=1536  # real image"
	@echo ""
	@echo "  make run-isp_top_tb           W=64 H=32 FRAMES=2"
	@echo "  make run-simple_isp_test"
	@echo ""
	@echo "NOTE: Single-block tests (crop, dpc, bnr, blc, etc.) require Verilator - excluded"
	@echo ""
	@echo "VARIABLES (pass via W=, H=, FRAMES=, IMG=, DEBUG=):"
	@echo "  W=<width>     Image width (default: 64)"
	@echo "  H=<height>    Image height (default: 32)"
	@echo "  FRAMES=<n>    Number of frames (default: 2)"
	@echo "  IMG=<file>    RAW Bayer image file (default: synthetic)"
	@echo "  DEBUG=1       Enable debug logging (default: 0 - metrics only)"
	@echo ""
	@echo "EXAMPLES:"
	@echo "  # Run with synthetic image (64x32, 2 frames)"
	@echo "  make run-isp_integration_tb"
	@echo ""
	@echo "  # Run with synthetic image (128x64, 1 frame)"
	@echo "  make run-isp_integration_tb W=128 H=64 FRAMES=1"
	@echo ""
	@echo "  # Run with real image (ColorChecker)"
	@echo "  make run-isp_integration_tb IMG=../input/ColorChecker_2592x1536_12bits_RGGB.raw W=2592 H=1536"
	@echo ""
	@echo "  # Run with real image (A_raw)"
	@echo "  make run-isp_integration_tb IMG=../input/A_raw_2688x1520_5376.raw W=2688 H=1520"
	@echo ""
	@echo "  # Run with real image (D65)"
	@echo "  make run-isp_integration_tb IMG=../input/D65_raw_2688x1520_5376.raw W=2688 H=1520"
	@echo ""
	@echo "  # Clean and help"
	@echo "  make clean"
	@echo "  make help"
	@echo ""

# Aliases for common workflows
.PHONY: all-tests clean help run-isp_integration_tb run-isp_top_tb run-simple_isp_test
