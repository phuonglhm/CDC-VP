#include "../include/top.h"

CabacTop::CabacTop(sc_core::sc_module_name name)
	: sc_module(name),
	  mem_bridge("mem_bridge"),
	  simple_mem("simple_mem"),
	  cabac("cabac")
{
	mem_bridge.socket.bind(simple_mem.socket);
	cabac.mem_socket.bind(mem_bridge.t_socket);
}
