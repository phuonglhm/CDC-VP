#pragma once
// FU3 CRM theo HAS v2.0 + FU3 CRM Specification V1.1: thanh ghi, PLL và clock bus/CPU, gate và reset từng IP,
// Starup: nhả hệ thống, rồi Ibex, CVA6 giữ reset tới khi Ibex nhả và reset hệ thống theo HAS v2.0 section 6.5
// Bus (PERIBUS_1, 0x5001_0000) đưa vào offset cục bộ
#include <cstdint>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include "fu3_crm/registers.h"

namespace fu3_crm {

class Crm : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<Crm> socket{"socket"};
    sc_core::sc_in<bool> prst_n{"prst_n"};   // PRSTN pad (POR)

    // Yêu cầu reset theo mức và tính theo sườn lên
    sc_core::sc_in<bool> wdt_reset_req{"wdt_reset_req"};           // WDT0 | WDT1 critical
    sc_core::sc_in<bool> cpu_reset_req{"cpu_reset_req"};           // DBG ndmreset
    sc_core::sc_in<bool> sw_reset_req{"sw_reset_req"};             // SYSCSR sys_ctrl.sw_reset_req
    sc_core::sc_in<bool> sw_global_rst_req{"sw_global_rst_req"};   // SYSCSR sys_ctrl.sw_global_rst_req: giữ CRM reset khi còn mức cao

    sc_core::sc_out<bool> sys_rst_n{"sys_rst_n"};     // SYSTEM_CLK_SW: bus, memory, ngoại vi
    sc_core::sc_out<bool> ibex_rst_n{"ibex_rst_n"};   // CPU1: SYSTEM_CLK_SW & IBEX_SW
    sc_core::sc_out<bool> cva6_rst_n{"cva6_rst_n"};   // CPU0: SYSTEM_CLK_SW & FCLK_SW
    sc_core::sc_out<bool> dbg_rst_n{"dbg_rst_n"};     // DBG
    sc_core::sc_out<bool> irq{"irq"};                 // CRM.int -> PLIC ID 1, INTMAP kSrcCrm

    // Gate và reset từng IP: giá trị thanh ghi (1 = bật clock / nhả reset)
    sc_core::sc_out<std::uint32_t> clk_gate_0{"clk_gate_0"}, clk_gate_1{"clk_gate_1"};
    sc_core::sc_out<std::uint32_t> sw_rst_n_0{"sw_rst_n_0"}, sw_rst_n_1{"sw_rst_n_1"};

    SC_HAS_PROCESS(Crm);
    // Hold: độ dài xung reset hệ thống 
    // Startup_delay: từ lúc nhả hệ thống tới lúc nhả Ibex (24 chu kỳ MRV ở OSC 20 MHz)
    // por_filter: PRSTN reset filter, a level must be stable this long before
    // it takes effect, shorter pulses are rejected (spec 3.2: 4 taps x 25 ns).
    // readiness_cycles: power-readiness qualification after the first release
    // of PRSTN, in OSC cycles, before the controller starts; locked until the
    // next PRSTN (spec 3.1.1: 255, counted even with readiness bypassed).
    explicit Crm(sc_core::sc_module_name name,
                 sc_core::sc_time hold = sc_core::sc_time(1400, sc_core::SC_NS),
                 sc_core::sc_time startup_delay = sc_core::sc_time(1200, sc_core::SC_NS),
                 sc_core::sc_time por_filter = sc_core::sc_time(100, sc_core::SC_NS),
                 unsigned readiness_cycles = 255);

    // Đọc không side effect (offset không map trả 0)
    std::uint32_t read_register(std::uint32_t offset) const;
    std::uint64_t pll_clock_hz() const;   // = 0 khi PLL tắt
    std::uint64_t cpu_clock_hz() const;   // FCLK của cả 2 CPU: không thông qua bộ chia
    std::uint64_t bus_clock_hz() const;
    // Notified after any register write or reset change; the platform re-reads
    // the clock getters and the outputs on it.
    const sc_core::sc_event& changed_event() const { return changed_; }

private:
    static constexpr unsigned kWords = 0x60 / 4;   // 0x0000..0x005C
    std::uint32_t regs_[kWords] = {};
    bool pending_ = false, keep_dbg_ = false, debug_only_ = true;
    bool por_released_ = false;   // PRSTN after the reset filter
    bool ready_ = false;          // power readiness qualified since the last PRSTN
    sc_core::sc_time lock_start_;  // PLL lock counter start (power-up or new dividers)
    bool last_req_[3] = {};
    const sc_core::sc_time hold_, startup_delay_, por_filter_;
    const unsigned readiness_cycles_;
    sc_core::sc_event changed_, request_, restart_, por_changed_;

    bool in_reset() const { return !por_released_ || sw_global_rst_req.read(); }
    void filter_por();                // PRSTN reset filter
    bool pll_locked() const;
    std::uint32_t& reg(std::uint32_t offset) { return regs_[offset / 4]; }
    std::uint64_t branch_hz() const;
    void reset_registers();
    void on_reset_inputs();
    void on_request();
    void set(std::uint32_t offset, std::uint32_t value);
    bool delay(sc_core::sc_time t);   // False nếu CRM bị reset trong lúc chờ
    bool startup();
    void sequencer();                 // Khởi động và reset hệ thống
    void update();                    // Process duy nhất lái output
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay);
    unsigned transport_dbg(tlm::tlm_generic_payload& tx);
    void access(tlm::tlm_generic_payload& tx, bool backdoor);
};

}  // namespace fu3_crm
