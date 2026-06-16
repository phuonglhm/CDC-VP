TARGET = timer_tb
SRCS = timer.cpp testbench.cpp

.PHONY all run clean

all: $(TARGET)

$(TARGET): $(SRCS) timer.h
	$(CXX) $(LDFLAGS) $^ -o $@ $(SYSTEMC_LIBS)

%.o: %.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) $(SYSTEMC_CFLAGS) (SRCS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS)
