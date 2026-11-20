// prints the stream depth constants that follow HAS_NEO_REV (GvuTopBridge::kDepth); compile-only companion of tb_rtl_ref_gvu_depth_r2.cpp
#include <cstdio>
#include "has/gvu_top_bridge.h"
int main() { std::printf("DEPTH bridge kDepth %d\n", has::GvuTopBridge::kDepth()); return 0; }
