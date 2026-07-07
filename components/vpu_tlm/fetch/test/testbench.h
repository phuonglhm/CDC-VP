#ifndef TESTBENCH_H
#define TESTBENCH_H


#include <systemc>
#include "top.h"
using namespace sc_core;

// Simplified TestBench declaration for fetch-only tests.
struct FetchFrameLoader;

struct FetchMemMonitor;
struct FetchWrapper;

// Integration test helpers (test-scoped)
struct FetchMemBridge;
struct FetchSimpleMemory;

class TestBench : public sc_core::sc_module {
	public:
		TestBench(sc_core::sc_module_name name);
		SC_HAS_PROCESS(TestBench);
		void run();
		bool passed{true};
		Top* top{nullptr};

	private:
		FetchFrameLoader* loader;
		// Test-only fetch instance and monitor (keeps fetch isolated)
		FetchWrapper* fetch_tb{nullptr};
		FetchMemMonitor* mem_mon{nullptr};

		// Integration path objects (created when FETCH_INTEGRATION is set)
		FetchMemBridge* bridge{nullptr};
		FetchSimpleMemory* ext_mem{nullptr};
};


#endif