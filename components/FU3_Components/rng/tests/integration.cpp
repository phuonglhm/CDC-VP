#define RNG_INTEGRATION
#include "bench.h"
int sc_main(int,char**){Bench b{"bench"};sc_start();std::cout<<"RNG integration: "<<b.checks<<" checks, "<<b.failures<<" failures\n";return b.failures||!b.finished?1:0;}
