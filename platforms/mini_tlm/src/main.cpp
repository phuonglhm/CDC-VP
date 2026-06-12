#include "mini_tlm_top.h"

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

    return "platforms/mini_tlm/configs/default.yaml";
}

} // namespace

int sc_main(int argc, char* argv[])
{
    const std::string config_path = parse_config_path(argc, argv);

    cdc::platforms::mini_tlm::mini_tlm_top top("mini_tlm", config_path);
    sc_core::sc_start();

    return 0;
}
