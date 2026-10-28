#pragma once

#include <cstdint>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

namespace cdc::platforms::fu3_soc {

// Pending latch for a pulse interrupt source (decision Q03-c), placed between
// the bus and the source IP. `irq` goes high on a rising `raw` level and stays
// high until a successful functional write with at least one enabled byte to
// the acknowledge register (`ack_offset`), or until `reset_n` falls. Reads,
// debug accesses and zero-strobe writes do not acknowledge; a source event at
// the time of the acknowledge wins (the raw level is still high). Used for the
// timers: acknowledge = CMP write (decision Q03-d).
class irq_ack_latch : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<irq_ack_latch> socket{"socket"};
    tlm_utils::simple_initiator_socket<irq_ack_latch> out{"out"};
    sc_core::sc_in<bool> raw{"raw"}, reset_n{"reset_n"};
    sc_core::sc_out<bool> irq{"irq"};

    SC_HAS_PROCESS(irq_ack_latch);
    irq_ack_latch(sc_core::sc_module_name name, std::uint64_t ack_offset)
        : sc_core::sc_module(name), ack_offset_(ack_offset)
    {
        socket.register_b_transport(this, &irq_ack_latch::b_transport);
        socket.register_transport_dbg(this, &irq_ack_latch::transport_dbg);
        SC_METHOD(on_raw);
        sensitive << raw.pos() << reset_n;
        SC_METHOD(drive);
        sensitive << changed_;
    }
    bool pending() const { return pending_; }

private:
    const std::uint64_t ack_offset_;
    bool pending_ = false;
    sc_core::sc_event changed_;

    void on_raw()
    {
        if (!reset_n.read())
            pending_ = false;
        else if (raw.read())
            pending_ = true;
        changed_.notify(sc_core::SC_ZERO_TIME);
    }
    void drive() { irq.write(pending_); }
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay)
    {
        out->b_transport(tx, delay);
        if (!tx.is_write() || !tx.is_response_ok() || tx.get_address() - tx.get_address() % 4 != ack_offset_)
            return;
        bool any = !tx.get_byte_enable_ptr() || !tx.get_byte_enable_length();
        for (unsigned i = 0; !any && i < tx.get_data_length(); ++i)
            any = tx.get_byte_enable_ptr()[i % tx.get_byte_enable_length()] != TLM_BYTE_DISABLED;
        if (any) {
            pending_ = raw.read();
            changed_.notify(sc_core::SC_ZERO_TIME);
        }
    }
    unsigned transport_dbg(tlm::tlm_generic_payload& tx) { return out->transport_dbg(tx); }
};

} // namespace cdc::platforms::fu3_soc
