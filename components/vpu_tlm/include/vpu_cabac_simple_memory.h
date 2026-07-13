#pragma once

#include "simple_memory.h"

// Backwards-compatible alias: previously `CabacSimpleMemory` was defined in
// `cabac/include/simple_memory.h`. Use the shared `SimpleMemory` from
// `common/include/simple_memory.h` and keep the old name for callers.
using CabacSimpleMemory = SimpleMemory;
