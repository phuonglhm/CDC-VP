#ifndef TESTBENCH_H
#define TESTBENCH_H


#include <systemc>
#include "top.h"
using namespace sc_core;

// Simplified TestBench declaration for fetch-only tests.
class FetchInitiator;
class FetchOutReceiver;

class TestBench : public sc_core::sc_module {
	public:
		TestBench(sc_core::sc_module_name name);
		SC_HAS_PROCESS(TestBench);
		void run();
		Top top;

	private:
		FetchInitiator* init;
		FetchOutReceiver* out;
};


#endif