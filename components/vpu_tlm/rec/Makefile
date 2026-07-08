# Legacy local build helper. Canonical integrated build/test flow for vpu_tlm
# is driven from the repository top-level CMake + CTest configuration.

# Compiler
CXX      := g++
COMMON_DIR ?= ../common
COMMON_INCLUDE ?= $(COMMON_DIR)/include
CXXFLAGS := -std=c++17 -Wall -Wextra -Iinclude -I$(COMMON_INCLUDE) -g

## FORCE using the local SystemC 2.3.4 install in the user's home
SYSTEMC_HOME := $(HOME)/.local

ifeq ($(wildcard $(SYSTEMC_HOME)/include/systemc.h),)
$(error SystemC header not found in $(SYSTEMC_HOME)/include. Build/install SystemC 2.3.4 into $(SYSTEMC_HOME) or change this Makefile.)
endif

CXXFLAGS     += -isystem $(SYSTEMC_HOME)/include
LDFLAGS      := -L$(SYSTEMC_HOME)/lib -Wl,-rpath,$(SYSTEMC_HOME)/lib

# Target library
LIBS := -lsystemc

# Precompiled header (speeds up repeated inclusion of heavy headers like SystemC)
# Use a copy of the header inside $(OBJ_DIR) so generated .gch stays in build/
PCH_HEADER := include/rec_packet.h
PCH_COPY = $(OBJ_DIR)/rec_packet.h
PCH_GCH = $(PCH_COPY).gch

# Directories and target
SRC_DIR := src
OBJ_DIR := build
BIN_DIR := out
TARGET  := $(BIN_DIR)/main

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

# Optional fetch integration for tests. Enable with: `make testbench USE_FETCH=1`
# When enabled, add the fetch include path and compile the FetchWrapper
# and SimpleMemory sources into the rec build so tests can exercise fetch.
ifeq ($(USE_FETCH),1)
CXXFLAGS += -I../fetch/include -DUSE_FETCH
# Use canonical SimpleMemory from common/src
FETCH_SRCS := ../fetch/src/fetch_wrapper_tlm.cpp ../common/src/simple_memory.cpp
FETCH_OBJS := $(OBJ_DIR)/fetch_wrapper_tlm.o $(OBJ_DIR)/simple_memory.o
FETCH_DEPS := $(FETCH_OBJS:.o=.d)
else
FETCH_SRCS :=
FETCH_OBJS :=
FETCH_DEPS :=
endif

.PHONY: all main run clean testbench

# Testbench dirs/targets
TEST_DIR := test
# Gather test sources, but exclude helper implementation files
TEST_SRC_ALL := $(wildcard $(TEST_DIR)/*.cpp)
TEST_SRC := $(filter-out $(TEST_DIR)/coeff_monitor.cpp $(TEST_DIR)/db_monitor.cpp $(TEST_DIR)/frame_buffer.cpp, $(TEST_SRC_ALL))
TEST_TARGET := $(BIN_DIR)/testbench
TEST_OBJS := $(patsubst $(TEST_DIR)/%.cpp,$(OBJ_DIR)/test_%.o,$(TEST_SRC))
TEST_DEPS := $(TEST_OBJS:.o=.d)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Copy header into obj dir and build precompiled header there so build artifacts
# remain under $(OBJ_DIR).
$(PCH_COPY): $(PCH_HEADER) | $(OBJ_DIR)
	cp $< $@

$(PCH_GCH): $(PCH_COPY) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -x c++-header $< -o $@

# Compile source files into object files (use the PCH copy in $(OBJ_DIR))
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR) $(PCH_GCH)
	$(CXX) $(CXXFLAGS) -include $(PCH_COPY) -MMD -c $< -o $@

# Compile test sources into object files (use the PCH copy in $(OBJ_DIR))
$(OBJ_DIR)/test_%.o: $(TEST_DIR)/%.cpp | $(OBJ_DIR) $(PCH_GCH)
	$(CXX) $(CXXFLAGS) -include $(PCH_COPY) -MMD -c $< -o $@

# Compile selected fetch_tlm sources into the rec build using the
# fetch_tlm include directory so their internal relative includes work.
ifeq ($(USE_FETCH),1)
$(OBJ_DIR)/fetch_wrapper_tlm.o: ../fetch/src/fetch_wrapper_tlm.cpp | $(OBJ_DIR) $(PCH_GCH)
	$(CXX) $(CXXFLAGS) -I../fetch/include -include $(PCH_COPY) -MMD -c $< -o $@

$(OBJ_DIR)/simple_memory.o: ../common/src/simple_memory.cpp | $(OBJ_DIR) $(PCH_GCH)
	$(CXX) $(CXXFLAGS) -include $(PCH_COPY) -MMD -c $< -o $@
endif

# Link object files into final executable (Objects MUST precede -lsystemc)
$(TARGET): $(OBJS) $(FETCH_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LIBS) -o $@

# Link testbench (link against src objects as well)
$(TEST_TARGET): $(TEST_OBJS) $(OBJS) $(FETCH_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LIBS) -o $@

-include $(DEPS)
 -include $(DEPS)
 -include $(DEPS)
 -include $(TEST_DEPS)
 -include $(FETCH_DEPS)

run: $(TARGET)
	./$(TARGET)

main: run

# Build and run the testbench executable from test/testbench.cpp
testbench: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
