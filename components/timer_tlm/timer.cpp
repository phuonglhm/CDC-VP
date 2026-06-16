#include "timer.h"

void Timer::b_transport(tlm::tlm_generic_payload& trans, sc_time& delay) {
    tlm::tlm_command cmd = trans.get_command();
    sc_dt::uint64 adr = trans.get_address();
    unsigned char* ptr = trans.get_data_ptr();
    unsigned int len = trans.get_data_length();
    unsigned char* byt = trans.get_byte_enable_ptr();
    unsigned int wid = trans.get_streaming_width();

    if(len!=4) {
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    if(byt!=0 || wid < len) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }

    if(cmd==tlm::TLM_READ_COMMAND) {
        switch(adr) {
            case ADDR::CTRL:
                *reinterpret_cast<uint32_t*>(ptr) = ctrl_reg;
                break;
            case ADDR::VALUE:
                *reinterpret_cast<uint32_t*>(ptr) = value_reg;
                break;
            case ADDR::RELOAD:
                *reinterpret_cast<uint32_t*>(ptr) = reload_reg;
                break;
            case ADDR::INTSTATUS:
                *reinterpret_cast<uint32_t*>(ptr) = intr_status;
                break;
            default:
                trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
                return;
        }
    } else {
        uint32_t data = *reinterpret_cast<uint32_t*>(ptr);
        switch(adr) {
            case ADDR::CTRL:
                ctrl_reg = data & 0xF;
                update_reg.notify();
                break;
            case ADDR::VALUE:
                value_reg = data;
                update_reg.notify();
                break;
            case ADDR::RELOAD:
                reload_reg = data;
                value_reg = data;
                update_reg.notify();
                break;
            case ADDR::INTSTATUS:
                if(data & 0x1) {
                    intr_status = false;
                    intr_clear_inc = true;
                    intr_clear.notify();
                }
                break;
            default:
                trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
                return;
        }
    }
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
    return;
}

void Timer::timer_thread() {
    while(true) {
        if(!prstn.read()) {
            reset();
            continue;
        }
        
        if(!(ctrl_reg & OPS::ENABLE)) {
            wait(update_reg | prstn.negedge_event());
            continue;
        }

        if(ctrl_reg & OPS::EX_CLK) wait(extin.posedge_event() | update_reg | prstn.negedge_event() | intr_clear);
        else wait(tick_period, update_reg | prstn.negedge_event() | intr_clear);

        if(intr_clear_inc) {
            intr_clear_inc = false;
            timerint.write(false);
            continue;
        }

        if(ctrl_reg & OPS::EX_EN && !extin.read()) continue;

        if(value_reg==0) {
            value_reg = reload_reg;
            intr_status = true;
            if(ctrl_reg & OPS::INTR_EN) timerint.write(true);
        } else value_reg--;
    }
}

void Timer::reset() {
    ctrl_reg = 0;
    value_reg = 0;
    reload_reg = 0;
    intr_status = false;
    timerint.write(false);
    wait(prstn.posedge_event());
}
