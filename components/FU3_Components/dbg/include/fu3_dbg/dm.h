#pragma once
// FU3 DBG Debug Module (RISC-V Debug Spec 0.13.2): 2 hart (0 = CPU0 CVA6, 1 = CPU1 Ibex), chỉ abstract command
// Target trên bus tại cửa sổ DM 0x1A11_0000: debugger đi qua cầu JTAG2AXI4. Không có System Bus Access
#include <cstdint>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <vector>

#include "fu3_dbg/registers.h"

namespace fu3_dbg {

// Hart nhìn từ DM
// Wrapper CVA6/Ibex cài interface này
class DebugHart {
public:
    virtual ~DebugHart() = default;
    virtual void set_halt_request(bool request) = 0;   // debug_req_i
    virtual void resume() = 0;                         // Rời debug mode (step theo dcsr.step của hart)
    virtual bool halted() const = 0;
    // Access Register: regno 0x0000-0x0FFF CSR, 0x1000-0x101F GPR
                        // false = không truy cập được
    virtual bool access_register(std::uint16_t regno, bool write, std::uint32_t& value) = 0;
};

class DebugModule : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<DebugModule> socket{"socket"};   // <- bus target IBEX_DBG, offset cục bộ
    sc_core::sc_out<bool> ndmreset{"ndmreset"};                      // -> CRM cpu_reset_req

    SC_HAS_PROCESS(DebugModule);
    // harts[i] = hart i (0 = CVA6, 1 = Ibex)
    DebugModule(sc_core::sc_module_name name, std::vector<DebugHart*> harts);

    // DMI
    std::uint32_t dmi_read(std::uint8_t address);
    void dmi_write(std::uint8_t address, std::uint32_t value);

    // Platform báo hart vừa reset (đặt havereset)
    void notify_hart_reset(unsigned hart);

private:
    struct HartState {
        bool havereset = true, resumeack = false;
    };

    std::vector<DebugHart*> harts_;
    std::vector<HartState> state_;
    bool dmactive_ = false, ndmreset_ = false;
    std::uint32_t hartsel_ = 0;
    std::uint32_t data_[2] = {};
    std::uint32_t cmderr_ = 0;
    sc_core::sc_event changed_;

    DebugHart* selected() const {
        return hartsel_ < harts_.size() ? harts_[hartsel_] : nullptr;
    }

    void reset_dm();
    std::uint32_t dmstatus() const;
    void write_dmcontrol(std::uint32_t value);
    void execute(std::uint32_t command);
    void update();   // Process duy nhất lái ndmreset
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay);
    unsigned transport_dbg(tlm::tlm_generic_payload& tx);
    void access(tlm::tlm_generic_payload& tx);
};

}  // namespace fu3_dbg
