//author: Viet Hoang
//verified: linhtk55-fpt

#include "timer.h"

namespace cdc::components
{
    void Timer::b_transport(tlm::tlm_generic_payload &trans, sc_time &delay)
    {
        tlm::tlm_command cmd = trans.get_command();
        sc_dt::uint64 adr = trans.get_address();
        unsigned char *ptr = trans.get_data_ptr();
        unsigned int len = trans.get_data_length();
        unsigned char *byt = trans.get_byte_enable_ptr();
        unsigned int wid = trans.get_streaming_width();
        if (len != 4 && len != 8)
        {
            std::cerr << "[Timer] unsupported transfer length: " << len << " addr=0x" << std::hex << adr << std::dec << "\n";
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }
        // Some initiators set streaming width to 0 to indicate 'no restriction'.
        if (byt != 0 || (wid != 0 && wid < len))
        {
            std::cerr << "[Timer] byte-enable or streaming width issue: byt=" << (void*)byt << " wid=" << wid << " len=" << len << " addr=0x" << std::hex << adr << std::dec << "\n";
            trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
            return;
        }

        if (cmd == tlm::TLM_READ_COMMAND)
        {
            switch (adr)
            {
            case ADDR::CTRL:
            *reinterpret_cast<uint32_t *>(ptr) = ctrl_reg;
                break;
            case ADDR::VALUE:
                *reinterpret_cast<uint32_t *>(ptr) = value_reg;
                break;
            case ADDR::RELOAD:
                *reinterpret_cast<uint32_t *>(ptr) = reload_reg;
                break;
            case ADDR::INTSTATUS:
                *reinterpret_cast<uint32_t *>(ptr) = intr_status;
                break;
            case ADDR::PID0:
                *reinterpret_cast<uint32_t*>(ptr) = regPID0;
                break;
            case ADDR::PID1:
                *reinterpret_cast<uint32_t*>(ptr) = regPID1;
                break;
            case ADDR::PID2:
                *reinterpret_cast<uint32_t*>(ptr) = regPID2;
                break;
            case ADDR::PID3:
                *reinterpret_cast<uint32_t*>(ptr) = regPID3;
                break;
            case ADDR::PID4:
                *reinterpret_cast<uint32_t*>(ptr) = regPID4;
                break;
            case ADDR::PID5:
                *reinterpret_cast<uint32_t*>(ptr) = regPID5;
                break;
            case ADDR::PID6:
                *reinterpret_cast<uint32_t*>(ptr) = regPID6;
                break;
            case ADDR::PID7:
                *reinterpret_cast<uint32_t*>(ptr) = regPID7;
                break;
            case ADDR::CID0:
                *reinterpret_cast<uint32_t*>(ptr) = regCID0;
                break;
            case ADDR::CID1:
                *reinterpret_cast<uint32_t*>(ptr) = regCID1;
                break;
            case ADDR::CID2:
                *reinterpret_cast<uint32_t*>(ptr) = regCID2;
                break;
            case ADDR::CID3:
                *reinterpret_cast<uint32_t*>(ptr) = regCID3;
                break;
            default:
                trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
                return;
            }
        }
        else
        {
            // If the initiator performed an 8-byte transfer, only use the
            // lower 32-bits which contain the intended 32-bit MMIO write.
            uint32_t data = *reinterpret_cast<uint32_t *>(ptr);
            switch (adr)
            {
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
                if (data & 0x1)
                {
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

    void Timer::timer_thread()
    {
        while (true)
        {
            if (!prstn.read())
            {
                reset();
                continue;
            }

            if (!(ctrl_reg & OPS::ENABLE))
            {
                wait(update_reg | prstn.negedge_event());
                continue;
            }

            if (ctrl_reg & OPS::EX_CLK)
                wait(extin.posedge_event() | update_reg | prstn.negedge_event() | intr_clear);
            else
                wait(tick_period, update_reg | prstn.negedge_event() | intr_clear);

            if (intr_clear_inc)
            {
                intr_clear_inc = false;
                timerint.write(false);
                continue;
            }

            if (ctrl_reg & OPS::EX_EN && !extin.read())
                continue;

            if (value_reg == 0)
            {
                value_reg = reload_reg;
                intr_status = true;
                if (ctrl_reg & OPS::INTR_EN)
                    timerint.write(true);
            }
            else
                value_reg--;
        }
    }

    void Timer::reset()
    {
        ctrl_reg = 0;
        value_reg = 0;
        reload_reg = 0;
        intr_status = false;
        timerint.write(false);
        wait(prstn.posedge_event());
    }
}
