CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -pthread
LDFLAGS ?= -pthread

SYSTEMC_CFLAGS := $(shell pkg-config --cflags systemc 2>/dev/null)
SYSTEMC_LIBS := $(shell pkg-config --libs systemc 2>/dev/null)

ifeq ($(strip $(SYSTEMC_LIBS)),)
SYSTEMC_LIBS := -lsystemc
endif

TARGET := watchdog_lt
SRCS := main.cpp watchdog.cpp test.cpp
OBJS := $(SRCS:.cpp=.o)
DEPS := watchdog.h test.h

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) $^ -o $@ $(SYSTEMC_LIBS)

%.o: %.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) $(SYSTEMC_CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS) watchdog_wave.vcd
