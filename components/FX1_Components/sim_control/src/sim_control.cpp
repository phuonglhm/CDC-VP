#include "fx1/sim_control.h"
#include "fx1/fx1_memory_map.h"
#include <iostream>

namespace fx1 {
namespace {
std::uint32_t load32(const unsigned char* p) {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) | (std::uint32_t(p[2]) << 16) |
           (std::uint32_t(p[3]) << 24);
}
void store32(unsigned char* p, std::uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) p[i] = static_cast<unsigned char>(v >> (8 * i));
}
} // namespace

SimControl::SimControl(sc_core::sc_module_name name, bool stop_on_finish, unsigned test_irqs,
                       sc_core::sc_time access_latency)
    : sc_module(name), stop_on_finish_(stop_on_finish), latency_(access_latency) {
    if (test_irqs > 32) test_irqs = 32;
    test_irq.init(test_irqs);
    socket.register_b_transport(this, &SimControl::b_transport);
    socket.register_transport_dbg(this, &SimControl::transport_dbg);
    if (test_irqs) {
        SC_METHOD(drive_irqs);
        sensitive << irq_changed_;
    }
}

void SimControl::drive_irqs() {
    for (unsigned i = 0; i < test_irq.size(); ++i) test_irq[i].write((irq_lines_ >> i) & 1);
}

void SimControl::b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
    tx.set_dmi_allowed(false);
    if ((!tx.is_read() && !tx.is_write()) || !tx.get_data_ptr() || tx.get_data_length() != 4 ||
        tx.get_streaming_width() < 4 || tx.get_byte_enable_ptr()) {
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    if (tx.get_address() == FX1_SIM_CTRL_IRQ_OFF && test_irq.size() != 0) {
        const std::uint32_t mask =
            test_irq.size() == 32 ? 0xFFFFFFFFu : (std::uint32_t{1} << test_irq.size()) - 1;
        if (tx.is_write()) {
            irq_lines_ = load32(tx.get_data_ptr()) & mask;
            irq_changed_.notify(sc_core::SC_ZERO_TIME);
        } else {
            store32(tx.get_data_ptr(), irq_lines_);
        }
        delay += latency_;  // the bus waits it out: lines have settled when the hart resumes
        tx.set_response_status(tlm::TLM_OK_RESPONSE);
        return;
    }
    if (tx.get_address() != FX1_SIM_CTRL_FINISH_OFF) {
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    if (tx.is_read()) {
        store32(tx.get_data_ptr(), result_ == Result::running ? 0u : result_ == Result::pass ? 1u : 2u);
        tx.set_response_status(tlm::TLM_OK_RESPONSE);
        return;
    }
    const auto value = load32(tx.get_data_ptr());
    if (value != FX1_SIM_CTRL_PASS && (value & 0xFFFF) != FX1_SIM_CTRL_FAIL) {
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
    if (result_ != Result::running) return;  // the first verdict stands
    finish_time_ = sc_core::sc_time_stamp();
    if (value == FX1_SIM_CTRL_PASS) {
        result_ = Result::pass;
        std::cout << "[sim-control] PASS at " << finish_time_ << std::endl;
    } else {
        result_ = Result::fail;
        fail_code_ = value >> 16;
        std::cout << "[sim-control] FAIL code 0x" << std::hex << fail_code_ << std::dec << " at "
                  << finish_time_ << std::endl;
    }
    if (stop_on_finish_) sc_core::sc_stop();
}

unsigned SimControl::transport_dbg(tlm::tlm_generic_payload& tx) {
    if (!tx.is_read() || !tx.get_data_ptr() || tx.get_data_length() != 4 ||
        tx.get_address() != FX1_SIM_CTRL_FINISH_OFF)
        return 0;
    store32(tx.get_data_ptr(), result_ == Result::running ? 0u : result_ == Result::pass ? 1u : 2u);
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
    return 4;
}
} // namespace fx1
