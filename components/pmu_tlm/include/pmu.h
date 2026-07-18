#pragma once
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <cstdint>

namespace pwrmgr_reg {

constexpr uint64_t INTR_STATE              = 0x00;
constexpr uint64_t INTR_ENABLE             = 0x04;
constexpr uint64_t INTR_TEST               = 0x08;
constexpr uint64_t ALERT_TEST              = 0x0c;
constexpr uint64_t CTRL_CFG_REGWEN         = 0x10;
constexpr uint64_t CONTROL                 = 0x14;
constexpr uint64_t CFG_CDC_SYNC            = 0x18;
constexpr uint64_t WAKEUP_EN_REGWEN        = 0x1c;
constexpr uint64_t WAKEUP_EN               = 0x20;
constexpr uint64_t WAKE_STATUS             = 0x24;
constexpr uint64_t RESET_EN_REGWEN         = 0x28;
constexpr uint64_t RESET_EN                = 0x2c;
constexpr uint64_t RESET_STATUS            = 0x30;
constexpr uint64_t ESCALATE_RESET_STATUS   = 0x34;
constexpr uint64_t WAKE_INFO_CAPTURE_DIS   = 0x38;
constexpr uint64_t WAKE_INFO               = 0x3c;
constexpr uint64_t FAULT_STATUS            = 0x40;

constexpr uint32_t CONTROL_LOW_POWER_HINT_BIT    = 0;
constexpr uint32_t CONTROL_CORE_CLK_EN_BIT       = 4;
constexpr uint32_t CONTROL_IO_CLK_EN_BIT         = 5;
constexpr uint32_t CONTROL_USB_CLK_EN_LP_BIT     = 6;
constexpr uint32_t CONTROL_USB_CLK_EN_ACTIVE_BIT = 7;
constexpr uint32_t CONTROL_MAIN_PD_N_BIT         = 8;
constexpr uint32_t CONTROL_RESET_DEFAULT         = 0x180;
constexpr uint32_t CONTROL_RESET_MASK            = 0x1f1;

constexpr uint32_t WAKE_INFO_REASONS_MASK     = 0x3f;
constexpr uint32_t WAKE_INFO_FALL_THROUGH_BIT = 6;
constexpr uint32_t WAKE_INFO_ABORT_BIT        = 7;
constexpr uint32_t WAKE_INFO_MASK             = 0xff;

constexpr uint32_t FAULT_REG_INTG_ERR_BIT   = 0;
constexpr uint32_t FAULT_ESC_TIMEOUT_BIT    = 1;
constexpr uint32_t FAULT_MAIN_PD_GLITCH_BIT = 2;

constexpr int NUM_WAKEUPS = 6;
constexpr int NUM_RESET_REQS = 2;

}

enum class SlowFsmState {
    RESET,
    PWR_UP_AST,
    REQ_FAST_PWR,
    IDLE,
    PWR_DOWN_AST,
    LOW_POWER,
    INVALID
};

enum class FastFsmState {
    LOW_POWER,
    CLKS_ON,
    OTP_INIT,
    LC_INIT,
    STRAP,
    ROM_CHECK,
    ACTIVE,
    LOW_POWER_PREP,
    NVM_IDLE_CHECK,
    RESET_PREP,
    LOW_POWER_ENTRY,
    INVALID
};

enum class ResetReason {
    NONE,
    POR,
    LOW_POWER_EXIT,
    PERIPHERAL_REQ,
    SW_REQ,
    ESCALATION,
    MAIN_PD_GLITCH,
    NDM_RESET
};

namespace cdc::components {
    class Pwrmgr : public sc_core::sc_module {
    public:
        tlm_utils::simple_target_socket<Pwrmgr> tl_socket;

        sc_core::sc_in<bool>  por_rst_n;
        sc_core::sc_in<bool>  core_sleeping;
        sc_core::sc_in<bool>  otp_done;
        sc_core::sc_in<bool>  lc_done;
        sc_core::sc_in<bool>  rom_done;
        sc_core::sc_in<bool>  rom_good;
        sc_core::sc_in<bool>  flash_idle;
        sc_core::sc_in<bool>  lc_test_state;
        sc_core::sc_in<bool>  main_pok;

        sc_core::sc_in<sc_dt::sc_bv<pwrmgr_reg::NUM_WAKEUPS> > wakeups;
        sc_core::sc_in<sc_dt::sc_bv<pwrmgr_reg::NUM_RESET_REQS> > rstreqs;
        sc_core::sc_in<bool>  ndmreset_req;
        sc_core::sc_in<bool>  sw_rst_req;
        sc_core::sc_in<bool>  esc_rx;
        sc_core::sc_in<bool>  esc_clk_alive;

        sc_core::sc_out<bool> ast_main_pd_n;
        sc_core::sc_out<bool> rst_lc_n;
        sc_core::sc_out<bool> clk_en_2nd;
        sc_core::sc_out<bool> fetch_en;
        sc_core::sc_out<bool> strap_o;
        sc_core::sc_out<bool> low_power_o;
        sc_core::sc_out<bool> sys_rst_n;
        sc_core::sc_out<bool> wakeup_irq;

        SC_HAS_PROCESS(Pwrmgr);
        explicit Pwrmgr(const sc_core::sc_module_name& name);

    private:
        void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
        uint32_t reg_read(uint64_t addr, bool& hit);
        void reg_write(uint64_t addr, uint32_t data, uint32_t be);

        void slow_fsm_thread();
        void fast_fsm_thread();
        void esc_timeout_thread();
        void main_pd_monitor_thread();
        void irq_update_thread();

        bool low_power_requested() const;
        bool any_reset_request() const;
        void record_wakeup_reasons();
        void set_fault(uint32_t bit);
        void enter_terminal_state();
        void do_reset_sequence(ResetReason reason);
        void slow_fsm_transition(SlowFsmState next);
        void fast_fsm_transition(FastFsmState next);

        uint32_t r_intr_state_;
        uint32_t r_intr_enable_;
        uint32_t r_ctrl_cfg_regwen_;
        uint32_t r_control_;
        uint32_t r_cfg_cdc_sync_;
        uint32_t r_wakeup_en_regwen_;
        uint32_t r_wakeup_en_;
        uint32_t r_wake_status_;
        uint32_t r_reset_en_regwen_;
        uint32_t r_reset_en_;
        uint32_t r_reset_status_;
        uint32_t r_escalate_reset_status_;
        uint32_t r_wake_info_capture_dis_;
        uint32_t r_wake_info_;
        uint32_t r_fault_status_;

        SlowFsmState slow_state_;
        FastFsmState fast_state_;

        bool terminal_;
        bool wake_recording_;
        ResetReason pending_reset_reason_;

        int esc_timeout_counter_;
        static constexpr int kEscTimeoutCycles = 128;

        /* Wakes the FSM/IRQ threads when anything they evaluate may have
         * changed (register write, state transition, fault, input edge).
         * Replaces the former 1-100 ns polling loops, which dominated
         * whole-platform simulation cost (~1e9 wakeups per simulated
         * second while completely idle). */
        sc_core::sc_event kick_;
        void input_kick_method();
    };
}