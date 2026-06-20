//author: linhtk55-fpt

#include "testbench.h"

void TestBench::stimulus_process()
{
    tlm::tlm_generic_payload trans;
    pdm_payload payload;
    sc_time delay = SC_ZERO_TIME;

    trans.set_data_ptr(reinterpret_cast<unsigned char *>(&payload));
    trans.set_data_length(sizeof(pdm_payload));
    trans.set_command(tlm::TLM_WRITE_COMMAND);

    double sample_count = 0.0;
    const double PI = 3.14159265358979323846;

    // Generate a 1 kHz sine wave at a 3 MHz PDM clock rate
    while (sample_count < 6000)
    {
        double analog_signal = std::sin(2.0 * PI * sample_count / 3000.0);

        // Simple Delta-Sigma modulation placeholder to generate +1/-1 density
        // Higher analog value -> more likely to be +1
        double random_val = (double)rand() / RAND_MAX * 2.0 - 1.0;
        payload.density = (analog_signal > random_val) ? 1 : -1;

        // Send transaction to the PDM Interface
        initiator_socket->b_transport(trans, delay);

        // Real-time synchronization
        wait(delay);
        delay = SC_ZERO_TIME;

        sample_count++;
    }

    sc_stop(); // Stop simulation after sending data
}

void PCM_Monitor::monitor_process()
{
    while (true)
    {
        int sample = pcm_in_port->read();
        std::cout << "@Time " << sc_time_stamp()
                  << " | Decimated PCM Sample: " << sample << std::endl;
    }
}