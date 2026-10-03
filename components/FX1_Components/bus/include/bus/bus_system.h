#pragma once
#include "bus/router.h"
#include "bus/apb_bridge.h"
#include "bus/target_port.h"

namespace bus {
class BusSystem : public sc_core::sc_module {
public:
    Router sysbus1, sysbus0, peribus1, peribus0;
    ApbBridge a2p_pp1, a2p_pp0;
    // VP initiators bind here; bind DBG/CPU/DMA in that order for trace source IDs.
    tlm_utils::multi_passthrough_target_socket<Router>& target;
    // Mandatory external target connections; no IP is instantiated in the bus.
    TargetPort rom, isram, dsram, aes, qspi;
    sc_core::sc_vector<TargetPort> pp1, pp0;
    explicit BusSystem(sc_core::sc_module_name name, bool trace = false);
    BusSystem(sc_core::sc_module_name name, const BusConfig& cfg, bool trace = false);
    // Bind external targets with e.g. bus.rom.socket.bind(rom_ip.target).
    // AP_* / CFG_* labels in the diagram are connection boundaries here.
};
} // namespace bus
