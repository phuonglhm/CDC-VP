# Makefile for standalone PMU TLM testbench

CXX ?= g++

SYSTEMC_HOME ?= /opt/systemc-2.3.4
CXXSTD ?= c++17

TARGET := pmu_tb
BUILD_DIR := build

SRC_DIR := src
INC_DIR := include

SRCS := \
	$(SRC_DIR)/pmu.cpp \
	$(SRC_DIR)/testbench.cpp

OBJS := \
	$(BUILD_DIR)/pmu.o \
	$(BUILD_DIR)/testbench.o

# Detect SystemC library directory
ifndef SYSTEMC_LIB_DIR
ifneq ($(wildcard $(SYSTEMC_HOME)/lib-linux64/libsystemc.*),)
SYSTEMC_LIB_DIR := $(SYSTEMC_HOME)/lib-linux64
else ifneq ($(wildcard $(SYSTEMC_HOME)/lib64/libsystemc.*),)
SYSTEMC_LIB_DIR := $(SYSTEMC_HOME)/lib64
else
SYSTEMC_LIB_DIR := $(SYSTEMC_HOME)/lib
endif
endif

CXXFLAGS ?= -std=$(CXXSTD) -g -O0 -Wall -Wextra
CXXFLAGS += -I$(INC_DIR)
CXXFLAGS += -I$(SYSTEMC_HOME)/include

LDFLAGS += -L$(SYSTEMC_LIB_DIR)
LDFLAGS += -Wl,-rpath,$(SYSTEMC_LIB_DIR)

LDLIBS += -lsystemc

.PHONY: all run clean info

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

$(BUILD_DIR)/pmu.o: $(SRC_DIR)/pmu.cpp $(INC_DIR)/pmu.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/testbench.o: $(SRC_DIR)/testbench.cpp $(INC_DIR)/pmu.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

run: all
	./$(TARGET)

info:
	@echo "CXX              = $(CXX)"
	@echo "SYSTEMC_HOME     = $(SYSTEMC_HOME)"
	@echo "SYSTEMC_LIB_DIR  = $(SYSTEMC_LIB_DIR)"
	@echo "TARGET           = $(TARGET)"

clean:
	rm -rf $(BUILD_DIR) $(TARGET) *.vcd
