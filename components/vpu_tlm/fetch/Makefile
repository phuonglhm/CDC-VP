# Compiler
CXX      := g++
# Common include directory (overrideable from environment)
COMMON_DIR ?= ../common
COMMON_INCLUDE ?= $(COMMON_DIR)/include

CXXFLAGS := -std=c++17 -Wall -Wextra -Iinclude -I$(COMMON_INCLUDE) -I../ime/include -I../fme/include -I../prei/include -I../posi/include -g

## FORCE using the local SystemC 2.3.4 install in the user's home
SYSTEMC_HOME := $(HOME)/.local

ifeq ($(wildcard $(SYSTEMC_HOME)/include/systemc.h),)
$(error SystemC header not found in $(SYSTEMC_HOME)/include. Build/install SystemC 2.3.4 into $(SYSTEMC_HOME) or change this Makefile.)
endif

CXXFLAGS     += -isystem $(SYSTEMC_HOME)/include
LDFLAGS      := -L$(SYSTEMC_HOME)/lib -Wl,-rpath,$(SYSTEMC_HOME)/lib

# Target library
LIBS := -lsystemc

# Directories and targets
SRC_DIR := src
OBJ_DIR := build
BIN_DIR := out
TARGET  := $(BIN_DIR)/main

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all main run clean testbench

# Default build target: build the testbench by default
all: $(TEST_TARGET)

# Testbench dirs/targets
TEST_DIR := test
# Gather test sources, but exclude helper implementation files
TEST_SRC_ALL := $(wildcard $(TEST_DIR)/*.cpp)
TEST_SRC := $(filter-out $(TEST_DIR)/coeff_monitor.cpp $(TEST_DIR)/db_monitor.cpp $(TEST_DIR)/frame_buffer.cpp, $(TEST_SRC_ALL))
TEST_TARGET := $(BIN_DIR)/testbench
TEST_OBJS := $(patsubst $(TEST_DIR)/%.cpp,$(OBJ_DIR)/test_%.o,$(TEST_SRC))
EXTRA_OBJS := $(OBJ_DIR)/common_frame.o $(OBJ_DIR)/ime_ime.o $(OBJ_DIR)/fme_fme.o $(OBJ_DIR)/prei_prei.o $(OBJ_DIR)/posi_posi.o $(OBJ_DIR)/simple_memory.o
TEST_DEPS := $(TEST_OBJS:.o=.d)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Compile source files into object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -MMD -c $< -o $@

# Compile test sources into object files
$(OBJ_DIR)/test_%.o: $(TEST_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -MMD -c $< -o $@

# Link object files into final executable (Objects MUST precede -lsystemc)
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LIBS) -o $@

# Link testbench (link against src objects as well)
$(TEST_TARGET): $(TEST_OBJS) $(OBJS) $(EXTRA_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LIBS) -o $@

$(OBJ_DIR)/common_frame.o: ../common/src/frame.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -MMD -c $< -o $@

$(OBJ_DIR)/ime_ime.o: ../ime/src/ime.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -MMD -c $< -o $@

$(OBJ_DIR)/fme_fme.o: ../fme/src/fme.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -MMD -c $< -o $@

$(OBJ_DIR)/prei_prei.o: ../prei/src/prei.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -MMD -c $< -o $@

$(OBJ_DIR)/posi_posi.o: ../posi/src/posi.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -MMD -c $< -o $@

$(OBJ_DIR)/simple_memory.o: ../common/src/simple_memory.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -MMD -c $< -o $@

-include $(DEPS) $(TEST_DEPS)

run: $(TARGET)
	./$(TARGET)

main: run

# Build and run the testbench executable from test/testbench.cpp
testbench: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)