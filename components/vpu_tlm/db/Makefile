# Compiler
CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Iinclude -g

ifneq ("$(wildcard $(CURDIR)/local/include/systemc.h)","")
SYSTEMC_HOME ?= $(CURDIR)/local
else
SYSTEMC_HOME ?= $(HOME)/.local
endif

# Verify SystemC headers exist at the resolved path.
ifeq ($(wildcard $(SYSTEMC_HOME)/include/systemc.h),)
$(error SystemC header not found in $(SYSTEMC_HOME)/include. Build/install SystemC 2.3.4 into $(SYSTEMC_HOME) or set SYSTEMC_HOME when invoking make)
endif

CXXFLAGS     += -isystem $(SYSTEMC_HOME)/include
LDFLAGS      := -L$(SYSTEMC_HOME)/lib -Wl,-rpath,$(SYSTEMC_HOME)/lib

# Target library
LIBS := -lsystemc

# (No precompiled header in this build configuration)

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
# Gather test sources
TEST_SRC := $(wildcard $(TEST_DIR)/*.cpp)
TEST_TARGET := $(BIN_DIR)/testbench
TEST_OBJS := $(patsubst $(TEST_DIR)/%.cpp,$(OBJ_DIR)/test_%.o,$(TEST_SRC))
TEST_DEPS := $(TEST_OBJS:.o=.d)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Compile source files into object files (generate dependency files; include phony targets)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

# Compile test sources into object files (generate dependency files; include phony targets)
$(OBJ_DIR)/test_%.o: $(TEST_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

# Link object files into final executable (Objects MUST precede -lsystemc)
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LIBS) -o $@

# Link testbench (link against src objects as well)
$(TEST_TARGET): $(TEST_OBJS) $(OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LIBS) -o $@

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