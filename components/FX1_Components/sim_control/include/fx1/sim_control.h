#pragma once
#include <cstdint>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

namespace fx1 {
// VP-only test finisher (not on silicon), modelled on the QEMU "sifive_test"
// device. One 32-bit register at offset 0 (FX1_SIM_CTRL_FINISH_OFF):
//   write 0x5555                -> PASS, stop the simulation
//   write (code << 16) | 0x3333 -> FAIL with `code`, stop the simulation
//   read                        -> 0 while running, 1 pass, 2 fail
// Optional VP-only test interrupt lines, offset 4 (FX1_SIM_CTRL_IRQ_OFF): bit i
// drives test_irq[i] (read back as written). The platform wires them to spare
// interrupt-controller inputs so firmware can raise and lower interrupts at
// will (PLIC bring-up before the real IPs exist).
// Any other value, offset or size is a slave error. The platform turns the
// result into the simulator exit status.
class SimControl : public sc_core::sc_module {
public:
    enum class Result { running, pass, fail };

    tlm_utils::simple_target_socket<SimControl> socket{"socket"};
    sc_core::sc_vector<sc_core::sc_out<bool>> test_irq{"test_irq"};

    SC_HAS_PROCESS(SimControl);
    explicit SimControl(sc_core::sc_module_name name, bool stop_on_finish = true,
                        unsigned test_irqs = 0,
                        sc_core::sc_time access_latency = sc_core::sc_time(10, sc_core::SC_NS));

    Result result() const noexcept { return result_; }
    std::uint32_t fail_code() const noexcept { return fail_code_; }
    sc_core::sc_time finish_time() const noexcept { return finish_time_; }

private:
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay);
    unsigned transport_dbg(tlm::tlm_generic_payload& tx);

    void drive_irqs();

    bool stop_on_finish_;
    sc_core::sc_time latency_;
    std::uint32_t irq_lines_ = 0;
    sc_core::sc_event irq_changed_;
    Result result_ = Result::running;
    std::uint32_t fail_code_ = 0;
    sc_core::sc_time finish_time_;
};
} // namespace fx1
