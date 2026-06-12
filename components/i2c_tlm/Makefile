TARGET = i2c_sim
SCPATH = /usr/local/systemc-2.3.3
CXX = g++
CXXFLAGS = -std=c++14 -g -O0 -Wall
CXXFLAGS += -I$(SCPATH)/include
LDFLAGS = -L$(SCPATH)/lib-linux64 -Wl,-rpath $(SCPATH)/lib-linux64
LIBS = -lsystemc -lm

SRC = $(wildcard *.cpp)
OBJ = $(SRC:.cpp=.o)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LIBS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJ) $(TARGET)
