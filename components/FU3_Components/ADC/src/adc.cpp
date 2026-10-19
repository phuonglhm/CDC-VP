#include "fu3/adc/adc.h"
#include <algorithm>

namespace fu3::adc {
Adc::Adc(sc_core::sc_module_name name, sc_core::sc_time latency)
    : sc_module(name), access_latency_(latency) {
    socket.register_b_transport(this, &Adc::b_transport);
    socket.register_transport_dbg(this, &Adc::transport_dbg);
    socket.register_get_direct_mem_ptr(this, &Adc::get_direct_mem_ptr);
    SC_METHOD(tick);
    sensitive << adc_clk.pos() << reset_n.neg();
}

void Adc::reset_registers() {
    ++reset_epoch_;
    ctrl_ = 0; cfg_ = 0x10; timing_ = 0x20040c00; count_cfg_ = 1;
    result_ = sum_ = captured_ = remaining_ = 0;
    active_cfg_ = 0x10;
    samples_.fill(0);
    state_ = State::PowerDown;
}

void Adc::outputs() {
    const bool powered = state_ != State::PowerDown;
    pwd.write(!powered);
    macro_reset.write(!reset_n.read());
    sampclk.write(state_ == State::SampleHigh);
    cs.write(state_ == State::SampleHigh || state_ == State::SampleLow ||
             state_ == State::WaitEoc || state_ == State::Capture ||
             state_ == State::CheckCount);
    channel.write(active_cfg_ & 15);
    samp_sel.write((active_cfg_ & 16) != 0);
}

void Adc::tick() {
    if (!reset_n.read()) {
        reset_registers();
    } else if (!ctrl_) {
        state_ = State::PowerDown;
        remaining_ = 0;
    } else {
        switch (state_) {
        case State::PowerDown:
            active_cfg_ = cfg_;
            setup_ = std::max(32u, (timing_ >> 24) & 255u);
            high_ = std::max(4u, (timing_ >> 16) & 255u);
            low_ = std::max(12u, (timing_ >> 8) & 255u);
            count_ = std::clamp(count_cfg_, 1u, 16u);
            samples_.fill(0); result_ = sum_ = captured_ = 0;
            remaining_ = setup_; state_ = State::SetupWait;
            break;
        case State::SetupWait:
            if (--remaining_ == 0) { state_ = State::SampleHigh; remaining_ = high_; }
            break;
        case State::SampleHigh:
            if (--remaining_ == 0) { state_ = State::SampleLow; remaining_ = low_; }
            break;
        case State::SampleLow:
            if (--remaining_ == 0) state_ = State::WaitEoc;
            break;
        case State::WaitEoc:
            // HDS p10 FSM explicitly uses eoc == 1; no invented timeout.
            if (eoc.read()) state_ = State::Capture;
            break;
        case State::Capture:
            samples_[captured_++] = data_in.read().to_uint();
            sum_ += samples_[captured_ - 1];
            state_ = State::CheckCount;
            break;
        case State::CheckCount:
            if (captured_ == count_) {
                result_ = sum_ / count_; // Rounding unspecified: VP uses floor.
                state_ = State::Done;
            } else {
                state_ = State::SampleHigh; remaining_ = high_;
            }
            break;
        case State::Done: break;
        }
    }
    outputs();
}

std::uint32_t Adc::read(unsigned off) const {
    switch (off) {
    case reg::VERSION: return 0x00010000;
    case reg::CTRL: return ctrl_;
    case reg::CFG: return cfg_;
    case reg::STATUS:
        return state_ == State::PowerDown ? 0 :
            state_ == State::Done ? reg::DONE | reg::READY : reg::BUSY;
    case reg::DATA: return result_;
    case reg::TIMING: return timing_;
    case reg::SAMPLE_CFG: return count_cfg_;
    default: return samples_[(off - reg::sample(0)) / 4];
    }
}

void Adc::write(unsigned off, std::uint32_t v) {
    switch (off) {
    case reg::CTRL: ctrl_ = v & 1; break;
    case reg::CFG: cfg_ = v & 0x1f; break;
    case reg::TIMING: timing_ = v & 0xffffff00; break;
    case reg::SAMPLE_CFG: count_cfg_ = v & 0x1f; break;
    default: break; // RO writes ignored; no APB PSLVERR signal in HDS.
    }
}

bool Adc::validate(tlm::tlm_generic_payload& tx, bool debug) const {
    tx.set_dmi_allowed(false);
    auto fail = [&](tlm::tlm_response_status s) { tx.set_response_status(s); return false; };
    if ((!tx.is_read() && !tx.is_write()) || (debug && !tx.is_read()))
        return fail(tlm::TLM_COMMAND_ERROR_RESPONSE);
    if (!tx.get_data_ptr()) return fail(tlm::TLM_GENERIC_ERROR_RESPONSE);
    if (tx.get_data_length() != 4 || tx.get_streaming_width() < 4)
        return fail(tlm::TLM_BURST_ERROR_RESPONSE);
    if (tx.get_address() > reg::sample(15) || (tx.get_address() & 3))
        return fail(tlm::TLM_ADDRESS_ERROR_RESPONSE);
    // PSTRB is unsupported. Reject masked transfers instead of silently
    // interpreting them as full writes. These errors are VP diagnostics.
    if (tx.get_byte_enable_ptr()) return fail(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
    return true;
}

void Adc::transfer(tlm::tlm_generic_payload& tx) {
    auto* p = tx.get_data_ptr();
    const auto off = static_cast<unsigned>(tx.get_address());
    if (tx.is_read()) {
        const auto v = read(off);
        for (unsigned i = 0; i < 4; ++i) p[i] = static_cast<unsigned char>(v >> (8 * i));
    } else {
        std::uint32_t v = 0;
        for (unsigned i = 0; i < 4; ++i) v |= std::uint32_t(p[i]) << (8 * i);
        write(off, v);
    }
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
}

void Adc::b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
    if (!validate(tx, false)) return;
    // Call from SC_THREAD. Arrival delay is consumed before bus arbitration.
    if (delay != sc_core::SC_ZERO_TIME) { sc_core::wait(delay); delay = sc_core::SC_ZERO_TIME; }
    const auto epoch = reset_epoch_;
    const bool entered_in_reset = !reset_n.read();
    bus_lock_.lock();
    sc_core::wait(access_latency_);
    if (entered_in_reset || !reset_n.read() || epoch != reset_epoch_)
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
    else transfer(tx);
    bus_lock_.unlock();
}

unsigned Adc::transport_dbg(tlm::tlm_generic_payload& tx) {
    if (!validate(tx, true)) return 0;
    transfer(tx);
    return 4;
}

bool Adc::get_direct_mem_ptr(tlm::tlm_generic_payload& tx, tlm::tlm_dmi&) {
    tx.set_dmi_allowed(false);
    return false;
}
} // namespace fu3::adc
