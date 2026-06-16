CXX ?= g++

SYSTEMC_HOME ?= /opt/systemc-2.3.4
SYSTEMC_LIB_DIR ?= $(SYSTEMC_HOME)/lib
CXXSTD ?= c++17

CXXFLAGS ?= -std=$(CXXSTD) -g -O0 -Wall
CXXFLAGS += -I$(SYSTEMC_HOME)/include

LDFLAGS += -L$(SYSTEMC_LIB_DIR)
LDFLAGS += -Wl,-rpath,$(SYSTEMC_LIB_DIR)

LDLIBS += -lsystemc -lm

TARGET := i2c_sim
SRCS := i2c.cpp main.cpp
OBJS := $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) $(LDFLAGS) $(LDLIBS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
