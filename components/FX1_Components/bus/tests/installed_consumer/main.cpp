#include <bus/bus_system.h>
#include <iostream>
int sc_main(int, char**) {
    bus::BusConfig cfg;
    cfg.validate(); // Link the installed library, not a source-tree target.
    std::cout << "Installed CDC bus headers and library: PASS\n";
    return 0;
}
