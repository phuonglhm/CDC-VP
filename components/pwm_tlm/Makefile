CXX ?= g++

SYSTEMC_HOME ?= /opt/systemc-2.3.4

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

TARGET := pwm_tb

SRCS := tests/tb.cpp src/pwm.cpp

CXXFLAGS := -std=c++17 -g -O0 -Wall -I$(SYSTEMC_INC) -Iinclude

LDFLAGS := $(SYSTEMC_LIB) -Wl,-rpath,$(SYSTEMC_LIB_DIR)

all: $(TARGET)

$(TARGET): $(SRCS) include/pwm.h
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) *.o *.vcd wave.vcd

show:
	@echo "SYSTEMC_HOME    = $(SYSTEMC_HOME)"
	@echo "SYSTEMC_INC     = $(SYSTEMC_INC)"
	@echo "SYSTEMC_LIB     = $(SYSTEMC_LIB)"
	@echo "SYSTEMC_LIB_DIR = $(SYSTEMC_LIB_DIR)"
