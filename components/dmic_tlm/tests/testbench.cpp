//author: linhtk55-fpt
//verified: hoangv11

#include "testbench.h"
using namespace sc_core;

void PDM_Source::stimulus_process()
{
    tlm::tlm_generic_payload trans;
    pdm_payload payload;
    sc_time delay = SC_ZERO_TIME;

    trans.set_data_ptr(reinterpret_cast<unsigned char *>(&payload));
    trans.set_data_length(sizeof(pdm_payload));
    trans.set_command(tlm::TLM_WRITE_COMMAND);

    wait(5, SC_US);//wait for cpu to boot

    double sample_count = 0.0;
    const double PI = 3.141592653589793;

    while (sample_count < 6000)
    {
        double analog_signal = std::sin(2.0 * PI * sample_count / 3000.0);
        double random_val = (double)rand() / RAND_MAX * 2.0 - 1.0;
        payload.density = (analog_signal > random_val) ? 1 : -1;

        initiator_socket->b_transport(trans, delay);

        wait(delay);
        delay = SC_ZERO_TIME;
        sample_count++;
    }
    wait(10, SC_US);
    sc_stop();
}

uint32_t Host_CPU::read_reg(uint64_t addr)
{
    tlm::tlm_generic_payload trans;
    uint32_t data = 0;
    sc_time delay = SC_ZERO_TIME;

    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(addr);
    trans.set_data_ptr(reinterpret_cast<unsigned char *>(&data));
    trans.set_data_length(4);

    bus_socket->b_transport(trans, delay);
    wait(delay);
    return data;
}

void Host_CPU::write_reg(uint64_t addr, uint32_t data)
{
    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;

    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(addr);
    trans.set_data_ptr(reinterpret_cast<unsigned char *>(&data));
    trans.set_data_length(4);

    bus_socket->b_transport(trans, delay);
    wait(delay);
}

void Host_CPU::cpu_firmware()
{
    std::cout << "[CPU] Initializing DMIC Reset...\n";
    reset_n.write(false);
    wait(1, SC_US);
    reset_n.write(true);
    wait(1, SC_US);

    std::cout << "[CPU] Initializing DMIC Firmware...\n";

    // 1. Setup Watermark to 16
    write_reg(DMIC_FIFO_WM_REG, 16);

    // 2. Enable DMIC, Enable Interrupts, set Decimation to 64
    uint32_t ctrl_setup = (64 << DMIC_CTRL_DEC_SHIFT) | DMIC_CTRL_INT_EN | DMIC_CTRL_EN;
    write_reg(DMIC_CTRL_REG, ctrl_setup);

    // 3. Main Operating Loop
    while (true)
    {
        wait(irq_in->posedge_event());

        std::cout << "\n[CPU] Hardware Interrupt fired at " << sc_time_stamp() << "\n";

        // Read Status Register
        uint32_t status = read_reg(DMIC_STATUS_REG);

        if (status & DMIC_STATUS_OE)
        { // Check Overrun Error
            std::cout << "[CPU] ERROR: Audio Overrun detected! Data lost.\n";
            // Clear the overrun flag
            write_reg(DMIC_INT_CLR_REG, DMIC_INT_CLR_OE);
        }

        if (status & DMIC_STATUS_WM)
        { // Check Watermark
            std::cout << "[CPU] Watermark Reached. Offloading FIFO...\n";

            // Read out the 16 samples
            for (int i = 0; i < 16; i++)
            {
                int32_t sample = read_reg(DMIC_FIFO_DATA_REG);
                std::cout << "      PCM[" << i << "]: " << sample << "\n";
            }
        }
    }
}
