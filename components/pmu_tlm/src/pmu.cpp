#include "pmu.h"
#include <cstring>

using namespace sc_core;
using namespace pwrmgr_reg;

namespace cdc::components {
    Pwrmgr::Pwrmgr(const sc_module_name& name)
        : sc_module(name), tl_socket("tl_socket"), por_rst_n("por_rst_n"), core_sleeping("core_sleeping"), otp_done("otp_done"), lc_done("lc_done"), rom_done("rom_done"), rom_good("rom_good"), flash_idle("flash_idle"), lc_test_state("lc_test_state"), main_pok("main_pok"), wakeups("wakeups"), rstreqs("rstreqs"), ndmreset_req("ndmreset_req"), sw_rst_req("sw_rst_req"), esc_rx("esc_rx"), esc_clk_alive("esc_clk_alive"), ast_main_pd_n("ast_main_pd_n"), rst_lc_n("rst_lc_n"), clk_en_2nd("clk_en_2nd"), fetch_en("fetch_en"), strap_o("strap_o"), low_power_o("low_power_o"), sys_rst_n("sys_rst_n"), wakeup_irq("wakeup_irq"), r_intr_state_(0), r_intr_enable_(0), r_ctrl_cfg_regwen_(1), r_control_(CONTROL_RESET_DEFAULT), r_cfg_cdc_sync_(0), r_wakeup_en_regwen_(1), r_wakeup_en_(0), r_wake_status_(0), r_reset_en_regwen_(1), r_reset_en_(0), r_reset_status_(0), r_escalate_reset_status_(0), r_wake_info_capture_dis_(0), r_wake_info_(0), r_fault_status_(0), slow_state_(SlowFsmState::RESET), fast_state_(FastFsmState::LOW_POWER), terminal_(false), wake_recording_(false), pending_reset_reason_(ResetReason::NONE), esc_timeout_counter_(0)
    {
        tl_socket.register_b_transport(this, &Pwrmgr::b_transport);

        SC_THREAD(slow_fsm_thread);
        SC_THREAD(fast_fsm_thread);
        SC_THREAD(esc_timeout_thread);
        SC_THREAD(main_pd_monitor_thread);
        SC_THREAD(irq_update_thread);
    }

    void Pwrmgr::b_transport(tlm::tlm_generic_payload& trans, sc_time& delay)
    {
        tlm::tlm_command cmd = trans.get_command();
        uint64_t addr = trans.get_address();
        unsigned char* ptr = trans.get_data_ptr();
        unsigned int len = trans.get_data_length();
        unsigned char* be = trans.get_byte_enable_ptr();

        if (len != 4) {
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }

        uint32_t be_mask = 0xffffffffu;
        if (be != nullptr) {
            be_mask = 0;
            for (int i = 0; i < 4; ++i) {
                if (be[i] == TLM_BYTE_ENABLED) {
                    be_mask |= (0xffu << (8 * i));
                }
            }
        }

        if (cmd == tlm::TLM_READ_COMMAND) {
            bool hit = false;
            uint32_t val = reg_read(addr, hit);
            if (!hit) {
                trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
                return;
            }
            std::memcpy(ptr, &val, 4);
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
        } else if (cmd == tlm::TLM_WRITE_COMMAND) {
            uint32_t val = 0;
            std::memcpy(&val, ptr, 4);
            reg_write(addr, val, be_mask);
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
        } else {
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        }

        delay += sc_time(10, SC_NS);
    }

    uint32_t Pwrmgr::reg_read(uint64_t addr, bool& hit)
    {
        hit = true;
        switch (addr) {
            case INTR_STATE:            return r_intr_state_;
            case INTR_ENABLE:           return r_intr_enable_;
            case INTR_TEST:             return 0;
            case ALERT_TEST:            return 0;
            case CTRL_CFG_REGWEN:       return r_ctrl_cfg_regwen_;
            case CONTROL:               return r_control_;
            case CFG_CDC_SYNC:          return r_cfg_cdc_sync_;
            case WAKEUP_EN_REGWEN:      return r_wakeup_en_regwen_;
            case WAKEUP_EN:             return r_wakeup_en_;
            case WAKE_STATUS:           return r_wake_status_;
            case RESET_EN_REGWEN:       return r_reset_en_regwen_;
            case RESET_EN:              return r_reset_en_;
            case RESET_STATUS:          return r_reset_status_;
            case ESCALATE_RESET_STATUS: return r_escalate_reset_status_;
            case WAKE_INFO_CAPTURE_DIS: return r_wake_info_capture_dis_;
            case WAKE_INFO:             return r_wake_info_;
            case FAULT_STATUS:          return r_fault_status_;
            default:
                hit = false;
                return 0;
        }
    }

    void Pwrmgr::reg_write(uint64_t addr, uint32_t data, uint32_t be)
    {
        switch (addr) {
            case INTR_STATE: {
                uint32_t clear_mask = data & be;
                r_intr_state_ &= ~clear_mask;
                break;
            }
            case INTR_ENABLE: {
                uint32_t mask = be & 0x1;
                r_intr_enable_ = (r_intr_enable_ & ~mask) | (data & mask);
                break;
            }
            case INTR_TEST: {
                if (data & be & 0x1) {
                    r_intr_state_ |= 0x1;
                }
                break;
            }
            case ALERT_TEST: {
                if (data & be & 0x1) {
                    set_fault(FAULT_REG_INTG_ERR_BIT);
                }
                break;
            }
            case CONTROL: {
                if (r_ctrl_cfg_regwen_ == 0) break;
                uint32_t mask = be & CONTROL_RESET_MASK;
                r_control_ = (r_control_ & ~mask) | (data & mask);
                break;
            }
            case CFG_CDC_SYNC: {
                if ((data & be & 0x1) != 0) {
                    r_cfg_cdc_sync_ = 0;
                }
                break;
            }
            case WAKEUP_EN_REGWEN: {
                if ((data & be & 0x1) == 0 && (be & 0x1)) {
                    r_wakeup_en_regwen_ = 0;
                }
                break;
            }
            case WAKEUP_EN: {
                if (r_wakeup_en_regwen_ == 0) break;
                uint32_t mask = be & 0x3f;
                r_wakeup_en_ = (r_wakeup_en_ & ~mask) | (data & mask);
                break;
            }
            case WAKE_STATUS:
                break;
            case RESET_EN_REGWEN: {
                if ((data & be & 0x1) == 0 && (be & 0x1)) {
                    r_reset_en_regwen_ = 0;
                }
                break;
            }
            case RESET_EN: {
                if (r_reset_en_regwen_ == 0) break;
                uint32_t mask = be & 0x3;
                r_reset_en_ = (r_reset_en_ & ~mask) | (data & mask);
                break;
            }
            case RESET_STATUS:
                break;
            case ESCALATE_RESET_STATUS:
                break;
            case WAKE_INFO_CAPTURE_DIS: {
                uint32_t mask = be & 0x1;
                r_wake_info_capture_dis_ = (r_wake_info_capture_dis_ & ~mask) | (data & mask);
                wake_recording_ = (r_wake_info_capture_dis_ == 0);
                break;
            }
            case WAKE_INFO: {
                uint32_t clear_mask = data & be & WAKE_INFO_MASK;
                r_wake_info_ &= ~clear_mask;
                break;
            }
            case FAULT_STATUS:
                break;
            default:
                break;
        }
    }

    bool Pwrmgr::low_power_requested() const
    {
        bool hint = (r_control_ & (1u << CONTROL_LOW_POWER_HINT_BIT)) != 0;
        return hint && core_sleeping.read();
    }

    bool Pwrmgr::any_reset_request() const
    {
        bool periph = false;
        sc_dt::sc_bv<NUM_RESET_REQS> rr = rstreqs.read();
        for (int i = 0; i < NUM_RESET_REQS; ++i) {
            bool bit_set = (rr.get_bit(i) != 0);
            bool enabled = (r_reset_en_ & (1u << i)) != 0;
            if (bit_set && enabled) periph = true;
        }
        bool sw = sw_rst_req.read();
        bool ndm = ndmreset_req.read();
        bool esc = (r_escalate_reset_status_ & 0x1) != 0;
        bool glitch = (r_fault_status_ & (1u << FAULT_MAIN_PD_GLITCH_BIT)) != 0;
        return periph || sw || ndm || esc || glitch;
    }

    void Pwrmgr::record_wakeup_reasons()
    {
        if (!wake_recording_) return;

        sc_dt::sc_bv<NUM_WAKEUPS> wk = wakeups.read();
        uint32_t masked_reasons = 0;
        for (int i = 0; i < NUM_WAKEUPS; ++i) {
            bool bit_set = (wk.get_bit(i) != 0);
            bool enabled = (r_wakeup_en_ & (1u << i)) != 0;
            if (bit_set && enabled) masked_reasons |= (1u << i);
        }
        r_wake_status_ = masked_reasons;
        r_wake_info_ |= (masked_reasons & WAKE_INFO_REASONS_MASK);
    }

    void Pwrmgr::set_fault(uint32_t bit)
    {
        r_fault_status_ |= (1u << bit);
    }

    void Pwrmgr::enter_terminal_state()
    {
        terminal_ = true;
        slow_state_ = SlowFsmState::INVALID;
        fast_state_ = FastFsmState::INVALID;

        ast_main_pd_n.write(false);
        rst_lc_n.write(false);
        clk_en_2nd.write(false);
        fetch_en.write(false);
        sys_rst_n.write(false);
        low_power_o.write(false);
    }

    void Pwrmgr::slow_fsm_transition(SlowFsmState next)
    {
        slow_state_ = next;
    }

    void Pwrmgr::fast_fsm_transition(FastFsmState next)
    {
        fast_state_ = next;
    }

    void Pwrmgr::do_reset_sequence(ResetReason reason)
    {
        sys_rst_n.write(false);
        fetch_en.write(false);
        clk_en_2nd.write(false);

        pending_reset_reason_ = reason;
        fast_fsm_transition(FastFsmState::RESET_PREP);

        wait(sc_time(5, SC_NS));

        sys_rst_n.write(true);
        fast_fsm_transition(FastFsmState::CLKS_ON);
    }

    void Pwrmgr::slow_fsm_thread()
    {
        while (true) {
            if (terminal_) { wait(sc_time(100, SC_NS)); continue; }

            switch (slow_state_) {

            case SlowFsmState::RESET: {
                ast_main_pd_n.write(false);
                sys_rst_n.write(false);
                wait(por_rst_n.posedge_event());
                if (terminal_) break;
                slow_fsm_transition(SlowFsmState::PWR_UP_AST);
                break;
            }

            case SlowFsmState::PWR_UP_AST: {
                ast_main_pd_n.write(true);
                wait(sc_time(20, SC_NS));
                slow_fsm_transition(SlowFsmState::REQ_FAST_PWR);
                break;
            }

            case SlowFsmState::REQ_FAST_PWR: {
                while (fast_state_ != FastFsmState::ACTIVE && !terminal_) {
                    wait(sc_time(10, SC_NS));
                }
                if (terminal_) break;
                slow_fsm_transition(SlowFsmState::IDLE);
                break;
            }

            case SlowFsmState::IDLE: {
                while (fast_state_ != FastFsmState::LOW_POWER_ENTRY && !terminal_) {
                    wait(sc_time(10, SC_NS));
                }
                if (terminal_) break;
                slow_fsm_transition(SlowFsmState::PWR_DOWN_AST);
                break;
            }

            case SlowFsmState::PWR_DOWN_AST: {
                bool main_pd_n = (r_control_ & (1u << CONTROL_MAIN_PD_N_BIT)) != 0;
                if (!main_pd_n) {
                    ast_main_pd_n.write(false);
                }
                low_power_o.write(true);
                wait(sc_time(10, SC_NS));
                slow_fsm_transition(SlowFsmState::LOW_POWER);
                break;
            }

            case SlowFsmState::LOW_POWER: {
                while (true) {
                    if (terminal_) break;
                    bool wake = (wakeups.read().to_uint() & r_wakeup_en_) != 0;
                    bool rst_req = any_reset_request();
                    if (wake || rst_req) {
                        if (rst_req && !wake) {
                            pending_reset_reason_ = ResetReason::PERIPHERAL_REQ;
                        }
                        break;
                    }
                    wait(sc_time(10, SC_NS));
                }
                if (terminal_) break;
                low_power_o.write(false);
                slow_fsm_transition(SlowFsmState::PWR_UP_AST);
                break;
            }

            case SlowFsmState::INVALID:
            default:
                enter_terminal_state();
                wait(sc_time(100, SC_NS));
                break;
            }
        }
    }

    void Pwrmgr::fast_fsm_thread()
    {
        while (true) {
            if (terminal_) { wait(sc_time(100, SC_NS)); continue; }

            switch (fast_state_) {

            case FastFsmState::LOW_POWER: {
                fetch_en.write(false);
                clk_en_2nd.write(false);
                low_power_o.write(true);
                while (slow_state_ == SlowFsmState::RESET ||
                    slow_state_ == SlowFsmState::PWR_UP_AST) {
                    if (terminal_) break;
                    wait(sc_time(10, SC_NS));
                }
                if (terminal_) break;
                low_power_o.write(false);
                fast_fsm_transition(FastFsmState::CLKS_ON);
                break;
            }

            case FastFsmState::CLKS_ON: {
                clk_en_2nd.write(true);
                wait(sc_time(5, SC_NS));
                fast_fsm_transition(FastFsmState::OTP_INIT);
                break;
            }

            case FastFsmState::OTP_INIT: {
                rst_lc_n.write(true);
                if (!otp_done.read()) {
                    wait(otp_done.posedge_event());
                }
                fast_fsm_transition(FastFsmState::LC_INIT);
                break;
            }

            case FastFsmState::LC_INIT: {
                if (!lc_done.read()) {
                    wait(lc_done.posedge_event());
                }
                fast_fsm_transition(FastFsmState::STRAP);
                break;
            }

            case FastFsmState::STRAP: {
                strap_o.write(true);
                wait(sc_time(20, SC_NS));
                strap_o.write(false);
                fast_fsm_transition(FastFsmState::ROM_CHECK);
                break;
            }

            case FastFsmState::ROM_CHECK: {
                if (!rom_done.read()) {
                    wait(rom_done.posedge_event());
                }

                bool allow_exec;
                if (lc_test_state.read()) {
                    allow_exec = true;
                } else {
                    allow_exec = rom_good.read();
                }

                fetch_en.write(allow_exec);
                fast_fsm_transition(FastFsmState::ACTIVE);
                break;
            }

            case FastFsmState::ACTIVE: {
                while (true) {
                    if (terminal_) break;

                    if (any_reset_request()) {
                        do_reset_sequence(pending_reset_reason_ == ResetReason::NONE
                                            ? ResetReason::PERIPHERAL_REQ
                                            : pending_reset_reason_);
                        break;
                    }

                    if (low_power_requested()) {
                        r_ctrl_cfg_regwen_ = 0;
                        fast_fsm_transition(FastFsmState::LOW_POWER_PREP);
                        break;
                    }

                    wait(sc_time(10, SC_NS));
                }
                break;
            }

            case FastFsmState::LOW_POWER_PREP: {
                clk_en_2nd.write(false);

                if (any_reset_request()) {
                    r_ctrl_cfg_regwen_ = 1;
                    do_reset_sequence(pending_reset_reason_ == ResetReason::NONE
                                        ? ResetReason::PERIPHERAL_REQ
                                        : pending_reset_reason_);
                    break;
                }

                if (!low_power_requested()) {
                    r_wake_info_ |= (1u << WAKE_INFO_FALL_THROUGH_BIT);
                    r_intr_state_ |= 0x1;
                    r_control_ &= ~(1u << CONTROL_LOW_POWER_HINT_BIT);
                    r_ctrl_cfg_regwen_ = 1;
                    clk_en_2nd.write(true);
                    fast_fsm_transition(FastFsmState::ACTIVE);
                    break;
                }

                fast_fsm_transition(FastFsmState::NVM_IDLE_CHECK);
                break;
            }

            case FastFsmState::NVM_IDLE_CHECK: {
                if (!flash_idle.read()) {
                    r_wake_info_ |= (1u << WAKE_INFO_ABORT_BIT);
                    r_intr_state_ |= 0x1;
                    r_control_ &= ~(1u << CONTROL_LOW_POWER_HINT_BIT);
                    r_ctrl_cfg_regwen_ = 1;
                    clk_en_2nd.write(true);
                    fast_fsm_transition(FastFsmState::ACTIVE);
                    break;
                }

                fast_fsm_transition(FastFsmState::LOW_POWER_ENTRY);
                break;
            }

            case FastFsmState::LOW_POWER_ENTRY: {
                wake_recording_ = (r_wake_info_capture_dis_ == 0);
                fetch_en.write(false);
                sys_rst_n.write(true);
                r_control_ &= ~(1u << CONTROL_LOW_POWER_HINT_BIT);
                fast_fsm_transition(FastFsmState::LOW_POWER);
                break;
            }

            case FastFsmState::RESET_PREP: {
                wait(sc_time(5, SC_NS));
                break;
            }

            case FastFsmState::INVALID:
            default:
                enter_terminal_state();
                wait(sc_time(100, SC_NS));
                break;
            }

            record_wakeup_reasons();
        }
    }

    void Pwrmgr::esc_timeout_thread()
    {
        const sc_time tick(1, SC_NS);
        while (true) {
            wait(tick);
            if (terminal_) { esc_timeout_counter_ = 0; continue; }
            if (esc_rx.read()) {
                r_escalate_reset_status_ = 1;
                esc_timeout_counter_ = 0;
                continue;
            }
            if (esc_clk_alive.read()) {
                esc_timeout_counter_ = 0;
            } else {
                ++esc_timeout_counter_;
                if (esc_timeout_counter_ >= kEscTimeoutCycles) {
                    set_fault(FAULT_ESC_TIMEOUT_BIT);
                    r_escalate_reset_status_ = 1;
                    esc_timeout_counter_ = 0;
                }
            }
        }
    }

    void Pwrmgr::main_pd_monitor_thread()
    {
        while (true) {
            wait(sc_time(1, SC_NS));
            if (terminal_) continue;

            bool power_should_be_on = ast_main_pd_n.read();
            if (power_should_be_on && !main_pok.read()) {
                set_fault(FAULT_MAIN_PD_GLITCH_BIT);
                pending_reset_reason_ = ResetReason::MAIN_PD_GLITCH;
            }
        }
    }

    void Pwrmgr::irq_update_thread()
    {
        while (true) {
            wait(sc_time(1, SC_NS));
            bool irq = ((r_intr_state_ & r_intr_enable_) & 0x1) != 0;
            wakeup_irq.write(irq);
        }
    }
}