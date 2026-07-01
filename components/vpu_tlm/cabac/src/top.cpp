#include "top.h"

Top::Top(sc_core::sc_module_name name)
	: sc_module(name),
	  mem_bridge("mem_bridge"),
	  simple_mem("simple_mem"),
	  cabac("cabac")
{
	// Bind the bridge initiator socket to the external memory target
	mem_bridge.socket.bind(simple_mem.socket);

	// Expose the bridge as the target for modules that want to read CABAC tables
	cabac.mem_socket.bind(mem_bridge.t_socket);
}