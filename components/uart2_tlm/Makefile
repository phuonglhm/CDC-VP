CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Iinclude -g

# Hardcoded paths to your working C++17 SystemC installation
# NO HARD CODE
SYSTEMC_HOME := /usr/local/systemc-2.3.4
CXXFLAGS     += -isystem $(SYSTEMC_HOME)/include
LDFLAGS      := -L$(SYSTEMC_HOME)/lib -Wl,-rpath,$(SYSTEMC_HOME)/lib

# Target library
LIBS := -lsystemc

# Directories and target
SRC_DIR := src
OBJ_DIR := build
BIN_DIR := out
TARGET  := $(BIN_DIR)/main

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all main run clean

all: $(TARGET)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Compile source files into object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -MMD -c $< -o $@

# Link object files into final executable (Objects MUST precede -lsystemc)
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LIBS) -o $@

-include $(DEPS)

run: $(TARGET)
	./$(TARGET)

main: run

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
