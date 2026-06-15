# Compiler and flags
CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Iincludes -g

# SystemC detection: prefer pkg-config, fall back to SYSTEMC_HOME
SYSTEMC_HOME ?= /usr/local/systemc
PKG_CFLAGS := $(shell pkg-config --cflags systemc 2>/dev/null)
PKG_LIBS := $(shell pkg-config --libs systemc 2>/dev/null)

ifeq ($(strip $(PKG_CFLAGS)),)
	SYSTEMC_INC := $(SYSTEMC_HOME)/include
	CXXFLAGS += -I$(SYSTEMC_INC)
else
	CXXFLAGS += $(PKG_CFLAGS)
endif

ifeq ($(strip $(PKG_LIBS)),)
	SYSTEMC_LIB := $(SYSTEMC_HOME)/lib
	LDFLAGS := -L$(SYSTEMC_LIB) -lsystemc -Wl,-rpath,$(SYSTEMC_LIB)
else
	LDFLAGS := $(PKG_LIBS)
endif

# Directories and target
SRC_DIR := src
OBJ_DIR := build
BIN_DIR := out
TARGET := $(BIN_DIR)/main

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all main run clean

all: $(TARGET)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -MMD -c $< -o $@

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@

-include $(DEPS)

run: $(TARGET)
	./$(TARGET)

main: run

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
