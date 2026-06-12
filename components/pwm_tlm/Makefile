SYSTEMC_HOME=/usr/local/systemc-2.3.4

CXX=g++

CXXFLAGS= \
-I$(SYSTEMC_HOME)/include \
-std=c++17

LDFLAGS= \
-L$(SYSTEMC_HOME)/lib-linux64 \
-lsystemc

SRC = pwm.cpp tb.cpp

all:
	$(CXX) $(SRC) \
	$(CXXFLAGS) \
	$(LDFLAGS) \
	-o pwm_sim

clean:
	rm -f pwm_sim wave.vcd

run:
	./pwm_sim
