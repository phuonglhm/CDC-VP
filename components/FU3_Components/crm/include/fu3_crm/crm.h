#pragma once
// FU3 CRM theo FU3 CRM Specification V1.0: thanh ghi, chọn/chia clock bus, gate và reset từng IP, firmware MCPU (khởi động, xử lý yêu cầu reset) ở mức chức năng
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

    // Yêu cầu reset (mức)
    // Global giữ CRM reset khi còn ở mức cao
    sc_core::sc_in<bool> wdt_reset_req{"wdt_reset_req"};           // WDT0 | WDT1 critical
    sc_core::sc_in<bool> cpu_reset_req{"cpu_reset_req"};           // Yêu cầu reset CPU (DBG ndmreset) 
    sc_core::sc_in<bool> sw_reset_req{"sw_reset_req"};             // SYSCSR sys_ctrl.sw_reset_req
    sc_core::sc_in<bool> sw_global_rst_req{"sw_global_rst_req"};   // SYSCSR sys_ctrl.sw_global_rst_req

    sc_core::sc_out<bool> cva6_rst_n{"cva6_rst_n"};   // FCLK_SW
    sc_core::sc_out<bool> sys_rst_n{"sys_rst_n"};     // SYSTEM_CLK_SW: bus, memory, ngoại vi - riêng VP có cả Ibex
    sc_core::sc_out<bool> dbg_rst_n{"dbg_rst_n"};     // CPU_DEBUG
    sc_core::sc_out<bool> irq{"irq"};                 // CRM.int -> PLIC ID 1, INTMAP kSrcCrm

    // Gate và reset từng IP: giá trị thanh ghi (1 = bật clock / nhả reset)
    sc_core::sc_out<std::uint32_t> clk_gate_0{"clk_gate_0"}, clk_gate_1{"clk_gate_1"};
    sc_core::sc_out<std::uint32_t> sw_rst_n_0{"sw_rst_n_0"}, sw_rst_n_1{"sw_rst_n_1"}, sw_rst_n_2{"sw_rst_n_2"};

    SC_HAS_PROCESS(Crm);
    // Thời gian firmware giữ reset khi xử lý yêu cầu
    // Ttừ lúc nhả miền hệ thống tới lúc nhả CVA6 (spec 4.2 đo được 8,65 us)
    explicit Crm(sc_core::sc_module_name name,
                 sc_core::sc_time hold = sc_core::sc_time(1400, sc_core::SC_NS),
                 sc_core::sc_time startup_delay = sc_core::sc_time(8650, sc_core::SC_NS));

    // Đọc không side effect (offset không map trả 0)
    std::uint32_t read_register(std::uint32_t offset) const;
    // Clock bus hiện tại
    // CPU luôn chạy ở OSC (spec 3.7.1)
    std::uint64_t bus_clock_hz() const;
    // Chỉ báo DONE do firmware đặt 
    bool done() const { return done_; }

private:
    static constexpr unsigned kWords = 0x5C / 4;   // 0x0000..0x0058
    std::uint32_t regs_[kWords] = {};
    bool done_ = false, pending_ = false;
    bool last_req_[3] = {};
    const sc_core::sc_time hold_, startup_delay_;
    sc_core::sc_event changed_, request_, restart_;

    bool in_reset() const { return !prst_n.read() || sw_global_rst_req.read(); }
    std::uint32_t& reg(std::uint32_t offset) { return regs_[offset / 4]; }
    void reset_registers();
    void on_reset_inputs();
    void on_request();
    bool delay(sc_core::sc_time t);   // False nếu CRM bị reset trong lúc chờ
    void firmware();                  // MCPU
    void update();                    // Process duy nhất lái output
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay);
    unsigned transport_dbg(tlm::tlm_generic_payload& tx);
    void access(tlm::tlm_generic_payload& tx, bool backdoor);
};

}  // namespace fu3_crm
