#include "dmic.h"
using namespace sc_core;

namespace cdc::components {
SC_HAS_PROCESS(DmicTLM);
DmicTLM::DmicTLM(sc_module_name name) : sc_module(name), integrator(0), prev_integrator(0), counter(0),
                                        ctrl_reg(0x4000), // Default: Decimation=64 (0x40 << 8), Disabled (bit 0=0)
                                        fifo_wm_reg(16),  // Default watermark
                                        overrun_flag(false), watermark_flag(false)
{
    pdm_target_socket.register_b_transport(this, &DmicTLM::pdm_b_transport);
    bus_target_socket.register_b_transport(this, &DmicTLM::bus_b_transport);
    irq_out.initialize(false);
}
void DmicTLM::evaluate_interrupts()
{
    bool irq_enable = (ctrl_reg & DMIC_CTRL_INT_EN) != 0;

    if (pcm_fifo.size() >= fifo_wm_reg)
    {
        watermark_flag = true;
    }

    // Assert IRQ if enabled and a flag is active
    if (irq_enable && (watermark_flag || overrun_flag))
    {
        irq_out.write(true);
    }
    else
    {
        irq_out.write(false);
    }
}

uint32_t DmicTLM::get_status_reg()
{
    uint32_t status = 0;
    if (pcm_fifo.empty())
        status |= DMIC_STATUS_FE;
    if (pcm_fifo.size() >= FIFO_MAX_DEPTH)
        status |= DMIC_STATUS_FF;
    if (overrun_flag)
        status |= DMIC_STATUS_OE;
    if (watermark_flag)
        status |= DMIC_STATUS_WM; 
    return status;
}

void DmicTLM::pdm_b_transport(tlm::tlm_generic_payload &trans, sc_time &delay)
{
    delay += sc_time(1.0 / 3000000.0, SC_SEC); //for this testbench, might break official tests
    // If DMIC is disabled, ignore stream
    if ((ctrl_reg & DMIC_CTRL_EN) == 0)
    {
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return;
    }

    auto *data = reinterpret_cast<pdm_payload *>(trans.get_data_ptr());
    uint32_t decimation_factor = (ctrl_reg >> 8) & 0xFF;
    if (decimation_factor == 0)
        decimation_factor = 64;

    integrator += data->density;

    // Decimation & comb
    counter++;
    if (counter >= decimation_factor)
    {
        counter = 0;
        int pcm_sample = (int)(integrator - prev_integrator);
        prev_integrator = integrator;

        // Push to FIFO if space exists
        if (pcm_fifo.size() < FIFO_MAX_DEPTH)
        {
            pcm_fifo.push(pcm_sample);
        }
        else
        {
            overrun_flag = true;
        }

        evaluate_interrupts();
    }

    // delay += sc_time(1.0 / 3000000.0, SC_SEC);
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

void DmicTLM::bus_b_transport(tlm::tlm_generic_payload &trans, sc_time &delay)
{
    tlm::tlm_command cmd = trans.get_command();
    sc_dt::uint64 addr = trans.get_address() & 0xFF; // Mask to get relative offset
    unsigned char *data = trans.get_data_ptr();
    unsigned int len = trans.get_data_length();

    if (len != 4)
    {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }

    uint32_t temp_data = 0;

    if (cmd == tlm::TLM_READ_COMMAND)
    {
        switch (addr)
        {
        case DMIC_CTRL_REG:
            temp_data = ctrl_reg;
            break;
        case DMIC_STATUS_REG:
            temp_data = get_status_reg();
            break;
        case DMIC_FIFO_DATA_REG:
            if (!pcm_fifo.empty())
            {
                temp_data = pcm_fifo.front();
                pcm_fifo.pop();
                // If we drop below watermark, clear the flag automatically
                if (pcm_fifo.size() < fifo_wm_reg)
                    watermark_flag = false;
            }
            else
            {
                temp_data = 0; // Read from empty FIFO
            }
            break;
        case DMIC_FIFO_WM_REG:
            temp_data = fifo_wm_reg;
            break;
        default:
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        memcpy(data, &temp_data, 4);
    }
    else if (cmd == tlm::TLM_WRITE_COMMAND)
    {
        memcpy(&temp_data, data, 4);
        switch (addr)
        {
        case DMIC_CTRL_REG:
            ctrl_reg = temp_data;
            break;
        case DMIC_FIFO_WM_REG:
            fifo_wm_reg = temp_data;
            break;
        case DMIC_INT_CLR_REG:
            if (temp_data & 0x1)
                overrun_flag = false;
            if (temp_data & 0x2)
                watermark_flag = false;
            break;
        default:
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
    }

    evaluate_interrupts(); // State changed, re-evaluate IRQ line
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
    delay += sc_time(10, SC_NS);
}
} //namepsace cdc::components