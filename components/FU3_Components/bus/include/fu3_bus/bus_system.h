#pragma once
#include "fu3_bus/apb_bridge.h"
#include "fu3_bus/config.h"
#include "fu3_bus/router.h"
#include "fu3_bus/target_port.h"
#include <memory>
#include <string>
#include <unordered_map>
namespace fu3_bus {
class BusSystem : public sc_core::sc_module {
public:
    explicit BusSystem(sc_core::sc_module_name name,bool trace=false);
    BusSystem(sc_core::sc_module_name name,const BusConfig& cfg,bool trace=false);
    tlm_utils::simple_target_socket_optional<InitiatorPort>& initiator(const std::string& name);
    tlm_utils::simple_initiator_socket<TargetPort>& target(const std::string& name);
    const BusConfig& configuration() const{return config_;}
    // Platform boot controller closes the staging window before CVA6 release.
    void finish_staging();
    // SC_THREAD form for a release that an IBEX request commits (CRM FCLK_SW
    // write): closes staging at once for every later request, then waits until
    // the IBEX requests accepted before the call have completed. New requests
    // are neither rejected nor stalled.
    void close_staging();
    bool staging() const { return staging_; }
    // Bus clock while running (the platform follows its clock controller):
    // AXI pipeline stage cycle, APB cycle (two per APB beat) and router
    // decode/dispatch latency, replacing the BusConfig nanosecond values.
    // A change applies to the next wait of each stage.
    void set_clock(sc_core::sc_time axi_cycle, sc_core::sc_time apb_cycle, sc_core::sc_time router_latency);
    // Functional bus reset after platform quiescence; IP resets remain platform-owned.
    void reset();
    // SC_THREAD control path: close functional ingress, then drain admitted work.
    void quiesce();
    void resume();
    bool accepting() const { return accepting_; }
    unsigned peak_outstanding(const std::string& name, bool write=false) const;
private:
    bool staging_ = true;
    bool accepting_ = true;
    sc_core::sc_event activity_;
    BusConfig config_;
    std::unique_ptr<Router> sysbus1_,sysbus0_,peribus0_,peribus1_;
    std::unique_ptr<ApbBridge> a2p0_,a2p1_;
    std::unique_ptr<TargetPort> ap_sb0_,ap_pp0_,ap_pp1_;
    sc_core::sc_vector<InitiatorPort> initiator_ports_;
    sc_core::sc_vector<TargetPort> master_pipelines_;
    sc_core::sc_vector<TargetPort> target_ports_;
    std::unordered_map<std::string,std::size_t> initiator_index_,target_index_;
};
} // namespace fu3_bus
