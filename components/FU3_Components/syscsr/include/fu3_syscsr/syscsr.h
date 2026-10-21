#pragma once
// FU3 SYSCSR (m_fpt_syscsr): APB, 32-bit data, 16-bit address, 4 write strobe.
// Bus (PERIBUS_0, 0x4401_0000) đưa vào offset cục bộ.
#include <cstdint>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include "fu3_syscsr/registers.h"

namespace fu3_syscsr {

// Offset thanh ghi dùng ở platform/FW
constexpr std::uint32_t kVersion       = FU3_SYSCSR_VERSION_OFFSET;
constexpr std::uint32_t kSyscsrStatus  = FU3_SYSCSR_STATUS_OFFSET;
constexpr std::uint32_t kSysCtrl       = FU3_SYSCSR_SYS_CTRL_OFFSET;
constexpr std::uint32_t kCpuCtrl       = FU3_SYSCSR_CPU_CTRL_OFFSET;
constexpr std::uint32_t kCpuSystick    = FU3_SYSCSR_CPU_SYSTICK_OFFSET;
constexpr std::uint32_t kOscPadCtrl    = FU3_SYSCSR_OSC_PAD_CTRL_OFFSET;
constexpr std::uint32_t kRomCtrl       = FU3_SYSCSR_ROM_CTRL_OFFSET;

// syscsr_sys_ctrl
constexpr std::uint32_t kSwResetReq       = FU3_SYSCSR_SW_RESET_REQ;
constexpr std::uint32_t kSwGlobalRstReq   = FU3_SYSCSR_SW_GLOBAL_RST_REQ;
constexpr std::uint32_t kJtagmuxSel       = FU3_SYSCSR_JTAGMUX_SEL;
constexpr std::uint32_t kJtagmuxBlock     = FU3_SYSCSR_JTAGMUX_BLOCK;

class Syscsr : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<Syscsr> socket{"socket"};
    sc_core::sc_in<bool> reset_n{"reset_n"};

    // Strap từ IO_PAD: boot/mode_0/mode_1 được chốt lúc nhả reset_n (HAS 11), test_mode/test_mode_sel đọc live
    // DEBUG_EN không nằm ở đây
    sc_core::sc_in<bool> boot{"boot"}, mode_0{"mode_0"}, mode_1{"mode_1"};
    sc_core::sc_in<bool> test_mode{"test_mode"}, test_mode_sel{"test_mode_sel"};

    // Yêu cầu reset level, không tự xoá gửi CRM (CRM bắt sườn lên)
    sc_core::sc_out<bool> sw_reset_req{"sw_reset_req"};
    sc_core::sc_out<bool> sw_global_rst_req{"sw_global_rst_req"};

    SC_HAS_PROCESS(Syscsr);
    explicit Syscsr(sc_core::sc_module_name name);

    // Read trực tiếp (không qua bus) cho IOSEL/pinmux và test
    // Offset không map trả 0
    std::uint32_t read_register(std::uint32_t offset) const;

private:
    static constexpr unsigned kWords = 0xDC / 4;   // 0x0000..0x00D8
    std::uint32_t regs_[kWords] = {};
    sc_core::sc_event sys_ctrl_changed_;
    std::uint32_t straps_ = 0;   // {mode_1, mode_0, boot} đã chốt
    bool captured_ = false;

    void reset_registers();
    void on_reset();
    void sample_straps();
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay);
    unsigned transport_dbg(tlm::tlm_generic_payload& tx);
    void access(tlm::tlm_generic_payload& tx, bool backdoor);
};

}  // namespace fu3_syscsr
