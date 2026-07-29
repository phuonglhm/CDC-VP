#include "noc_soc_top.h"

#include <iostream>
#include <string>

#include <systemc>

namespace {

std::string parse_config_path(int argc, char* argv[])
{
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
            return argv[++i];
        }
    }

    return "platforms/noc_soc/configs/default.yaml";
}

} // namespace

int sc_main(int argc, char* argv[])
{
    const std::string config_path = parse_config_path(argc, argv);

    std::string firmware;
    for (int index = 1; index < argc; ++index) {
        const std::string arg = argv[index];
        if (arg == "--fw" && index + 1 < argc) {
            firmware = argv[++index];
        }
    }

    cdc::platforms::noc_soc::noc_soc_top top("noc_soc", config_path, firmware);
    sc_core::sc_start();

    return 0;
}
