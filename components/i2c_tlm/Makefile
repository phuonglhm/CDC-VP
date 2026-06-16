CXX ?= g++

SYSTEMC_HOME ?= /opt/systemc-2.3.4
CXXSTD ?= c++17

PKG_CFLAGS := $(shell pkg-config --cflags systemc 2>/dev/null)
PKG_LIBS := $(shell pkg-config --libs systemc 2>/dev/null)

ifndef SYSTEMC_LIB_DIR
ifneq ($(wildcard $(SYSTEMC_HOME)/lib-linux64/libsystemc.*),)
SYSTEMC_LIB_DIR := $(SYSTEMC_HOME)/lib-linux64
else ifneq ($(wildcard $(SYSTEMC_HOME)/lib64/libsystemc.*),)
SYSTEMC_LIB_DIR := $(SYSTEMC_HOME)/lib64
else
SYSTEMC_LIB_DIR := $(SYSTEMC_HOME)/lib
endif
endif

CXXFLAGS ?= -std=$(CXXSTD) -g -O0 -Wall
ifeq ($(strip $(PKG_CFLAGS)),)
CXXFLAGS += -I$(SYSTEMC_HOME)/include
else
CXXFLAGS += $(PKG_CFLAGS)
endif

ifeq ($(strip $(PKG_LIBS)),)
LDFLAGS += -L$(SYSTEMC_LIB_DIR)
LDFLAGS += -Wl,-rpath,$(SYSTEMC_LIB_DIR)

LDLIBS += -lsystemc -lm
else
LDLIBS += $(PKG_LIBS) -lm
endif

TARGET := i2c_sim
SRCS := src/i2c.cpp main.cpp
OBJS := $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) $(LDFLAGS) $(LDLIBS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
