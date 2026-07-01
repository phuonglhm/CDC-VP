# Compiler
CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Iinclude -g

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

# Link object files into final executable (Objects MUST precede -lsystemc)
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LIBS) -o $@

# Link testbench (link against src objects as well)
$(TEST_TARGET): $(TEST_OBJS) $(OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LIBS) -o $@

-include $(DEPS)
 -include $(DEPS)
 -include $(TEST_DEPS)

run: $(TARGET)
	./$(TARGET)

main: run

# Build and run the testbench executable from test/testbench.cpp
testbench: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)