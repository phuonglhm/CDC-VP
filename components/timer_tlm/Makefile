#author: Viet Hoang
#verified: linhtk55-fpt
CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Iinclude -g

SYSTEMC_HOME := /usr/local/systemc-2.3.4
CXXFLAGS     += -isystem $(SYSTEMC_HOME)/include
LDFLAGS      := -L$(SYSTEMC_HOME)/lib -Wl,-rpath,$(SYSTEMC_HOME)/lib

SYSTEMC_LIBS := -lsystemc

SRC_DIR := src
OBJ_DIR := build
TARGET  := tb

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all tb run clean

all: $(TARGET)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Compile source files into object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -MMD -c $< -o $@

# Link object files into final executable (output `tb` per README)
$(TARGET): $(OBJS) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(SYSTEMC_LIBS) -o $@

-include $(DEPS)

run: $(TARGET)
	./$(TARGET)

tb: $(TARGET)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
