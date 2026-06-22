TARGET = clkmgr_sim
SCPATH = /usr/local/systemc-2.3.3
CXX = g++
CXXFLAGS = -std=c++14 -g -O0 -Wall
CXXFLAGS += -Iinclude -I$(SCPATH)/include
LDFLAGS = -L$(SCPATH)/lib-linux64 -Wl,-rpath $(SCPATH)/lib-linux64
LIBS = -lsystemc -lm

SRC = main.cpp src/clkmgr.cpp
OBJ = $(SRC:.cpp=.o)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LIBS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJ) $(TARGET) src/*.o
