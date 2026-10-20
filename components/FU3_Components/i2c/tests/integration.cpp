#define I2C_INTEGRATION
#include "support/bench.h"
int sc_main(int,char**){Bench bench{"bench"};sc_start();std::cout<<"I2C bus integration: "<<bench.checks<<" checks, "<<bench.failures<<" failures\n";return bench.failures||!bench.finished?1:0;}
