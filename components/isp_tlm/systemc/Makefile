#==============================================================================
# ISP TLM SystemC Makefile
# Simple and robust build system for block testbenches
#
# Usage:
#   make blc              - Build BLC testbench
#   make run_blc          - Run BLC testbench
#   make blc_run          - Build and run BLC testbench
#   make all              - Build all testbenches
#   make clean            - Clean build artifacts
#   make help             - Show help
#
# Adding new testbenches:
#   1. Create: tb/<block>_tb.cpp
#   2. Add to TESTBENCHES list below
#   3. Run: make <block>
#==============================================================================

# Configuration
BUILD_DIR  := build
CXX        := g++-15
STD        := c++17
SYSTEMC_INC := /usr/include
SYSTEMC_LIB := /usr/lib/x86_64-linux-gnu

# Flags
INCLUDES := \
    -I. \
    -Isrc \
    -Isrc/common \
    -Isrc/base \
    -Isrc/utils \
    -Isrc/blocks \
    -Isrc/metrics \
    -Isrc/pipeline \
    -I$(SYSTEMC_INC)

CXXFLAGS := -std=$(STD) -Wall -Wextra -g -O0 $(INCLUDES)
LDFLAGS  := -L$(SYSTEMC_LIB) -lsystemc -Wl,-rpath,$(SYSTEMC_LIB)

#==============================================================================
# TESTBENCHES
# Format: target_name source_file [more_sources...]
#==============================================================================
TESTBENCHES := \
    blc tb/blc_tb.cpp

# Future testbenches (uncomment as you add them):
# csc  tb/csc_tb.cpp
# crop tb/crop_tb.cpp
# dpc  tb/dpc_tb.cpp
# wb   tb/wb_tb.cpp
# gamma tb/gamma_tb.cpp
# demosaic tb/demosaic_tb.cpp
# ccm  tb/ccm_tb.cpp
# sharpen tb/sharpen_tb.cpp
# 2dnr tb/2dnr_tb.cpp
# bnr  tb/bnr_tb.cpp

# Extract target names
TB_NAMES := $(word 1,$(TESTBENCHES))

#==============================================================================
# Help
#==============================================================================
.PHONY: help
help:
	@echo ""
	@echo "ISP TLM SystemC Build System"
	@echo "============================="
	@echo ""
	@echo "Targets:"
	@echo "  make blc             - Build BLC testbench"
	@echo "  make run_blc         - Run BLC testbench"
	@echo "  make blc_run         - Build and run BLC testbench"
	@echo "  make all             - Build all testbenches"
	@echo "  make clean           - Remove build artifacts"
	@echo ""
	@echo "Available testbenches:"
	@$(foreach n,$(TB_NAMES),echo "  $(n)";)
	@echo ""
	@echo "To add a new testbench:"
	@echo "  1. Create: tb/<block>_tb.cpp"
	@echo "  2. Add to TESTBENCHES list in this file:"
	@echo "  3. Run: make <block>"
	@echo ""

#==============================================================================
# Build single testbench
#==============================================================================
.PHONY: all clean

all:
	@echo "Building all testbenches..."
	@mkdir -p $(BUILD_DIR)
	@$(foreach tb,$(TESTBENCHES),$(call build_tb,$(tb)))
	@echo ""
	@echo "Build complete. Executables in $(BUILD_DIR)/"
	@ls -la $(BUILD_DIR)/

clean:
	@echo "Cleaning..."
	@rm -rf $(BUILD_DIR)
	@echo "Done."

#==============================================================================
# Per-target build rules
# Each testbench gets its own explicit rule
#==============================================================================

# BLC testbench
blc: $(BUILD_DIR)/blc
$(BUILD_DIR)/blc: tb/blc_tb.cpp | $(BUILD_DIR)
	@echo "[BUILD] blc <- tb/blc_tb.cpp"
	@$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)
	@echo "[OK]   $(BUILD_DIR)/blc"

# Add more testbenches below using the same pattern:
#
# csc: $(BUILD_DIR)/csc
# $(BUILD_DIR)/csc: tb/csc_tb.cpp | $(BUILD_DIR)
# 	@echo "[BUILD] csc <- tb/csc_tb.cpp"
# 	@$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)
#
# crop: $(BUILD_DIR)/crop
# $(BUILD_DIR)/crop: tb/crop_tb.cpp | $(BUILD_DIR)
# 	...

#==============================================================================
# Run targets
#==============================================================================

run_blc: $(BUILD_DIR)/blc
	@echo "[RUN] $(BUILD_DIR)/blc"
	@./$(BUILD_DIR)/blc

# Add: run_csc run_crop run_dpc etc. using same pattern

#==============================================================================
# Build-and-run combined targets
#==============================================================================

blc_run: blc
	@echo "[RUN] $(BUILD_DIR)/blc"
	@./$(BUILD_DIR)/blc

# Add: csc_run crop_run etc.

#==============================================================================
# Build directory helper
#==============================================================================
$(BUILD_DIR):
	@mkdir -p $@

#==============================================================================
