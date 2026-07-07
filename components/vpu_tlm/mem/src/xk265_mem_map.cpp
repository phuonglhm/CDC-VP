#include "xk265_mem_map.h"

namespace cdc::components {

std::vector<mem_config> make_xk265_default_mem_configs()
{
    std::vector<mem_config> configs;

    // Generic RAMs from rtl/mem style.
    configs.push_back({"ram_sp_256x32", 256, 32, mem_port_kind::single_port, false, 1, 1, true});
    configs.push_back({"ram_sp_1024x32", 1024, 32, mem_port_kind::single_port, false, 1, 1, true});
    configs.push_back({"ram_sp_1536x32", 1536, 32, mem_port_kind::single_port, false, 1, 1, true});

    // Byte-enable RAMs.
    configs.push_back({"ram_sp_be_128x64", 128, 64, mem_port_kind::single_port, true, 1, 1, true});
    configs.push_back({"ram_sp_be_192x128", 192, 128, mem_port_kind::single_port, true, 1, 1, true});
    configs.push_back({"ram_sp_be_192x512", 192, 512, mem_port_kind::single_port, true, 1, 1, true});

    // Fetch-related memories.
    configs.push_back({"fetch_rf_1p_128x512", 128, 512, mem_port_kind::register_file, true, 1, 1, true});
    configs.push_back({"fetch_rf_1p_64x256", 64, 256, mem_port_kind::register_file, true, 1, 1, true});
    configs.push_back({"fetch_ram_2p_64x208", 64, 208, mem_port_kind::simple_dual_port, false, 1, 1, true});

    // PREI / POSI memories.
    configs.push_back({"prei_md_ram_sp_85x6", 85, 6, mem_port_kind::single_port, false, 1, 1, true});
    configs.push_back({"prei_ram_dp_16x32", 16, 32, mem_port_kind::simple_dual_port, false, 1, 1, true});
    configs.push_back({"posi_md_ram_sp_64x6", 64, 6, mem_port_kind::single_port, false, 1, 1, true});

    // IME / FME motion-vector memories.
    configs.push_back({"ime_mv_ram_sp_64x13", 64, 13, mem_port_kind::single_port, false, 1, 1, true});
    configs.push_back({"fme_mv_ram_dp_64x20", 64, 20, mem_port_kind::simple_dual_port, false, 1, 1, true});

    // Other VPU memories for future integration.
    configs.push_back({"mc_mv_ram_sp_512x20", 512, 20, mem_port_kind::single_port, false, 1, 1, true});
    configs.push_back({"cabac_ram_sp_1024x8", 1024, 8, mem_port_kind::single_port, false, 1, 1, true});
    configs.push_back({"tq_ram_sp_256x32", 256, 32, mem_port_kind::single_port, false, 1, 1, true});
    configs.push_back({"db_ram_sp_256x32", 256, 32, mem_port_kind::single_port, false, 1, 1, true});

    return configs;
}

mem make_xk265_mem()
{
    mem memory;

    for (const mem_config& config : make_xk265_default_mem_configs()) {
        memory.add_instance(config);
    }

    return memory;
}

} // namespace cdc::components
