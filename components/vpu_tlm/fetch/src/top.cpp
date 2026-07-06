#include "top.h"

Top::Top(sc_core::sc_module_name name)
	: sc_module(name),
	  fetch("fetch")
{
	// FetchWrapper uses an embedded SimpleMemory; no binding required.
}