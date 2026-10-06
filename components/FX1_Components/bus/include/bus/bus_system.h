#pragma once
#include "bus/apb_bridge.h"
#include "bus/config.h"
#include "bus/router.h"
#include "bus/target_port.h"
#include <memory>
#include <string>
#include <unordered_map>
namespace bus {
class BusSystem : public sc_core::sc_module {
public:
    explicit BusSystem(sc_core::sc_module_name name,bool trace=false);
    BusSystem(sc_core::sc_module_name name,const BusConfig& cfg,bool trace=false);
    tlm_utils::simple_target_socket_optional<InitiatorPort>& initiator(const std::string& name);
    tlm_utils::simple_initiator_socket<TargetPort>& target(const std::string& name);
    const BusConfig& configuration() const{return config_;}
private:
    BusConfig config_;
    std::unique_ptr<Router> sysbus1_,sysbus0_,peribus0_,peribus1_;
    std::unique_ptr<ApbBridge> a2p0_,a2p1_;
    sc_core::sc_vector<InitiatorPort> initiator_ports_;
    sc_core::sc_vector<TargetPort> target_ports_;
    std::unordered_map<std::string,std::size_t> initiator_index_,target_index_;
};
} // namespace bus
