#include "fu3/gpio/gpio.h"

namespace fu3::gpio {
Gpio::Gpio(sc_core::sc_module_name name) : sc_module(name) {
    socket.register_b_transport(this, &Gpio::b_transport);
    socket.register_transport_dbg(this, &Gpio::transport_dbg);
    SC_METHOD(tick);
    sensitive << clk.pos() << reset_n.neg();
    SC_METHOD(outputs);
    sensitive << changed_ << gpio_in;
}
void Gpio::tick() {
    if (!reset_n.read()) {
        ++reset_generation_;
        state_.reset();
        if (pending_) {
            pending_->set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            pending_ = nullptr;
            completed_.notify(sc_core::SC_ZERO_TIME);
        }
    } else if (clk.posedge()) {
        // Read data is the pre-edge state, matching combinational APB PRDATA.
        const bool read = pending_ && pending_->is_read();
        const bool write = pending_ && pending_->is_write();
        const unsigned index = pending_ ? (pending_->get_address() >> 2) & 15 : 0;
        std::uint32_t data = 0;
        if (read)
            read_data(*pending_);
        if (write)
            for (unsigned i = 0; i < 4; ++i)
                data |= std::uint32_t(pending_->get_data_ptr()[i]) << (8 * i);
        state_.edge(gpio_in.read().to_uint(), read, write, index, data);
        if (pending_) {
            pending_->set_response_status(tlm::TLM_OK_RESPONSE);
            pending_ = nullptr;
            completed_.notify(sc_core::SC_ZERO_TIME);
        }
    }
    changed_.notify(sc_core::SC_ZERO_TIME);
}
void Gpio::outputs() {
    gpio_out.write(state_.read(reg::PADOUT / 4));
    gpio_dir.write(state_.read(reg::PADDIR / 4));
    gpio_in_sync.write(state_.synced());
    interrupt.write(state_.irq());
    for (unsigned i = 0; i < 32; ++i)
        gpio_padcfg[i].write(state_.padcfg(i));
    power_event.write(state_.wake(gpio_in.read().to_uint()));
}
bool Gpio::validate(tlm::tlm_generic_payload& tx) {
    tx.set_dmi_allowed(false);
    if (!tx.is_read() && !tx.is_write())
        tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
    else if (tx.get_byte_enable_ptr())
        tx.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
    else if (tx.get_data_length() != 4 || tx.get_streaming_width() < 4)
        tx.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
    else if (!tx.get_data_ptr())
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
    else
        return true;
    return false;
}
void Gpio::read_data(tlm::tlm_generic_payload& tx) {
    const unsigned index = (tx.get_address() >> 2) & 15;
    auto* data = tx.get_data_ptr();
    const auto value = state_.read(index);
    for (unsigned i = 0; i < 4; ++i)
        data[i] = (value >> (8 * i)) & 255;
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
}
void Gpio::b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
    if (!validate(tx))
        return;
    if (delay != sc_core::SC_ZERO_TIME) {
        sc_core::wait(delay);
        delay = sc_core::SC_ZERO_TIME;
    }
    if (!reset_n.read()) {
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    const auto generation = reset_generation_;
    bus_lock_.lock();
    // A queued caller must not survive a reset pulse while the mutex is held,
    // even if reset has already been released before it acquires the bus.
    if (!reset_n.read() || generation != reset_generation_)
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
    else {
        pending_ = &tx;
        sc_core::wait(completed_);
        // Allow the single output driver to publish the clocked state.
        sc_core::wait(sc_core::SC_ZERO_TIME);
    }
    bus_lock_.unlock();
}
unsigned Gpio::transport_dbg(tlm::tlm_generic_payload& tx) {
    if (!validate(tx))
        return 0;
    // Debug reads are non-destructive; debug writes are deliberately unsupported.
    if (tx.is_write()) {
        tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return 0;
    }
    read_data(tx);
    return 4;
}
} // namespace fu3::gpio
