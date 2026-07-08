#include "top.h"

FetchTop::FetchTop(sc_core::sc_module_name name)
	: sc_module(name),
	  fetch("fetch")
{
	// FetchWrapper uses an embedded SimpleMemory; no binding required.
}
