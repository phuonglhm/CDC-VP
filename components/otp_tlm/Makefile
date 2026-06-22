CXX ?= g++

SYSTEMC_HOME ?= /opt/systemc-2.3.4
CXXSTD ?= c++17

TARGET := otp_tlm

SRCS := otp.cpp tb.cpp
OBJS := $(SRCS:.cpp=.o)

SYSTEMC_INCLUDE_DIR := $(SYSTEMC_HOME)/include

ifeq ($(wildcard $(SYSTEMC_HOME)/lib-linux64/libsystemc.so),$(SYSTEMC_HOME)/lib-linux64/libsystemc.so)
SYSTEMC_LIB_DIR := $(SYSTEMC_HOME)/lib-linux64
else ifeq ($(wildcard $(SYSTEMC_HOME)/lib64/libsystemc.so),$(SYSTEMC_HOME)/lib64/libsystemc.so)
SYSTEMC_LIB_DIR := $(SYSTEMC_HOME)/lib64
else
SYSTEMC_LIB_DIR := $(SYSTEMC_HOME)/lib
endif

CXXFLAGS := -std=$(CXXSTD) -g -O0 -Wall -Wextra -I$(SYSTEMC_INCLUDE_DIR)

LDFLAGS := -L$(SYSTEMC_LIB_DIR) -Wl,-rpath,$(SYSTEMC_LIB_DIR)
LDLIBS := -lsystemc -lm -pthread

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

%.o: %.cpp otp.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	LD_LIBRARY_PATH=$(SYSTEMC_LIB_DIR):$$LD_LIBRARY_PATH ./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS) *.vcd

.PHONY: all run clean
