//Author: trangnm20
#include "clkmgr.h"
#include <iostream>
#include <cstring>

Clkmgr::Clkmgr(sc_core::sc_module_name name) : sc_module(name), socket("socket") {
    socket.register_b_transport(this, &Clkmgr::b_transport);
}

void Clkmgr::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time & delay) {
    tlm::tlm_command cmd = trans.get_command();
    sc_dt::uint64 addr = trans.get_address();
    unsigned char* ptr = trans.get_data_ptr();
    unsigned int len = trans.get_data_length();

    if (len != 4) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }

    if (cmd == tlm::TLM_READ_COMMAND) {
        uint32_t value = read_reg(addr);
        std::memcpy(ptr, &value, sizeof(value));
    } else if (cmd == tlm::TLM_WRITE_COMMAND) {
        uint32_t value;
        std::memcpy(&value, ptr, sizeof(value));
        write_reg(addr, value);
    } else {
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }

    trans.set_response_status(tlm::TLM_OK_RESPONSE);
    delay = sc_core::SC_ZERO_TIME;
}

uint32_t Clkmgr::read_reg(sc_dt::uint64 addr) {
    switch (addr) {
        case REG_EXTCLK_CTRL:
            return (static_cast<uint32_t>(extclk_ctrl_hispeed_) << 4) | static_cast<uint32_t>(extclk_ctrl_sel_);
        case REG_EXTCLK_STATUS:
            return static_cast<uint32_t>(extclk_status_ack_);
        case REG_CLK_ENABLES:
            return static_cast<uint32_t>(clk_enables_);
        case REG_CLK_HINTS:
            return static_cast<uint32_t>(clk_hints_);
        case REG_CLK_HINTS_STATUS:
            return static_cast<uint32_t>(clk_hints_status_);
        case REG_EXTCLK_CTRL_REGWEN:
            return static_cast<uint32_t>(extclk_ctrl_regwen_);
        default:
            return 0; 
    }
}
void Clkmgr::write_reg(sc_dt::uint64 addr, uint32_t data) {
    switch (addr) {
        case REG_EXTCLK_CTRL_REGWEN:
            if ((data & 0x1) == 0) {
                extclk_ctrl_regwen_ = 0;
            }
            break;
        case REG_EXTCLK_CTRL:
            if (!extclk_ctrl_regwen_) {
                break;
            }
            handle_extclk_ctrl_write(data);
            break;
        case REG_EXTCLK_STATUS:
            break;
        case REG_CLK_ENABLES:
            clk_enables_ = static_cast<uint8_t>(data & 0xf);
            break;
        case REG_CLK_HINTS:
            handle_clk_hints_write(data);
            break;
        case REG_CLK_HINTS_STATUS:
            break;
        default:
            break;
    }
}
void Clkmgr::handle_extclk_ctrl_write(uint32_t data) {
    uint8_t new_sel = static_cast<uint8_t>(data & 0xf);
    uint8_t new_hispeed = static_cast<uint8_t>((data >> 4) & 0xf);

    // EXTCLK_CTRL is always programmable, but only takes effect
    // when debug functions are enabled in life cycle TEST/DEV/RMA.
    if (!lc_debug_enabled()) {
        std::cout << "[CLKMGR] EXTCLK_CTRL write accepted but has no "
                  << "effect — life cycle state does not allow debug "
                  << "(current state is PROD)" << std::endl;
        return;
    }

    extclk_ctrl_hispeed_ = new_hispeed;
    bool currently_external = mubi4::test_true_strict(extclk_ctrl_sel_);
    bool requesting_external = mubi4::test_true_strict(new_sel);

    if (!currently_external && requesting_external) {
        extclk_ctrl_sel_   = mubi4::True;
        extclk_status_ack_ = mubi4::True;
        std::cout << "[CLKMGR] External clock ENABLED, ack=True" << std::endl;
    } else if (currently_external && !requesting_external) {
        extclk_ctrl_sel_   = mubi4::False;
        extclk_status_ack_ = mubi4::False;
        std::cout << "[CLKMGR] External clock DISABLED, ack=False" << std::endl;
    } else {
        std::cout << "[CLKMGR] EXTCLK_CTRL write ignored — no valid transition"
                  << std::endl;
    }
}

void Clkmgr::handle_clk_hints_write(uint32_t data) {
    clk_hints_ = static_cast<uint8_t>(data & 0xf);
    recompute_hints_status();
}
void Clkmgr::recompute_hints_status() {
    struct Block { int bit; const char* name; bool idle; };
    Block blocks[4] = {
        {0, "AES", aes_idle_},
        {1, "HMAC", hmac_idle_},
        {2, "KMAC", kmac_idle_},
        {3, "OTBN", otbn_idle_},
    };
    for (const auto& b : blocks) {
        bool hint_enable = (clk_hints_ >> b.bit) & 0x1;
        if (hint_enable) {
            clk_hints_status_ |= (1u << b.bit);
        } else {
            if (b.idle) {
                clk_hints_status_ &= ~(1u << b.bit);
            }
        }
    }
}

