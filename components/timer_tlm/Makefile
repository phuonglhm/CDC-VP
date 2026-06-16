TARGET = timer_tb
SRCS = timer.cpp testbench.cpp
CXXFLAGS = -Wall

OBJS = $(SRCS:.cpp=.o)

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(SYSTEMC_LIBS)

%.o: %.cpp include/timer.h
	$(CXX) $(CXXFLAGS) $(SYSTEMC_CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS)
