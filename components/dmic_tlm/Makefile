CXX ?= g++

SYSTEMC_HOME ?= /opt/systemc-2.3.4
CXXSTD ?= c++17

SYSTEMC_INC := $(SYSTEMC_HOME)/include
SYSTEMC_LIB := $(firstword \
	$(wildcard $(SYSTEMC_HOME)/lib-linux64/libsystemc.so*) \
	$(wildcard $(SYSTEMC_HOME)/lib64/libsystemc.so*) \
	$(wildcard $(SYSTEMC_HOME)/lib/libsystemc.so*) \
	$(wildcard $(SYSTEMC_HOME)/dynlib/Linux64_GCC-11.5/libsystemc.so*) \
	$(wildcard $(SYSTEMC_HOME)/dynlib/Linux64_GCC-12.3/libsystemc.so*) \
	$(wildcard $(SYSTEMC_HOME)/dynlib/Linux64_GCC-10.3/libsystemc.so*) \
	$(wildcard $(SYSTEMC_HOME)/dynlib/Linux64_GCC-9.3/libsystemc.so*) \
)

ifeq ($(SYSTEMC_LIB),)
$(error Cannot find libsystemc.so under $(SYSTEMC_HOME))
endif

SYSTEMC_LIB_DIR := $(dir $(SYSTEMC_LIB))

TARGET := test_dmic_tlm
SRCS := src/dmic.cpp tests/test_dmic_tlm.cpp tests/testbench.cpp

CXXFLAGS := -std=$(CXXSTD) -g -O0 -Wall -Wextra -I$(SYSTEMC_INC) -Iinclude -Itests
LDFLAGS := $(SYSTEMC_LIB) -Wl,-rpath,$(SYSTEMC_LIB_DIR)

all: $(TARGET)

$(TARGET): $(SRCS) include/dmic.h include/pdm_payload.h tests/testbench.h
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) *.o src/*.o tests/*.o *.vcd

.PHONY: all run clean
