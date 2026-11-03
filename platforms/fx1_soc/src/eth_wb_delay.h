// Fault injection on the Ethernet DMA path (fx1_soc --eth-wb-delay-us N,
// Ethernet review F08/ER02): every DES0 write-back of the MAC is held N us
// before it reaches the bus, so its completion (DONE, interrupt) is seen
// while the descriptor still reads OWN=1, as MAS 9.3 allows. The firmware
// must keep polling it without a further interrupt.
//
// A DES0 write-back is one bus beat at a 16-byte aligned address with the
// strobes of bytes 0..3 only (EthMacTlm::write_des0). A data write of the
// same shape (a frame tail) is held as well; that only delays the same DMA
// stream, it changes no ordering. 0 (default): a plain pass-through.
#pragma once

#include <cstdint>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

namespace cdc::platforms::fx1_soc {

class EthWbDelay : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<EthWbDelay> target{"target"};        // from the MAC's AXI master
    tlm_utils::simple_initiator_socket<EthWbDelay> out{"out"};           // to the write guard / fabric

    EthWbDelay(sc_core::sc_module_name name, unsigned delay_us) : sc_module(name), delay_(double(delay_us), sc_core::SC_US)
    {
        target.register_b_transport(this, &EthWbDelay::b_transport);
    }

    std::uint64_t held() const { return held_; }

private:
    static bool is_des0(const tlm::tlm_generic_payload& t)
    {
        if (!t.is_write() || (t.get_address() & 15u) || t.get_data_length() < 4) return false;
        const unsigned char* be = t.get_byte_enable_ptr();
        if (!be) return t.get_data_length() == 4;
        for (unsigned i = 0; i < t.get_data_length(); ++i)
            if ((be[i % t.get_byte_enable_length()] != 0) != (i < 4)) return false;
        return true;
    }

    void b_transport(tlm::tlm_generic_payload& t, sc_core::sc_time& d)
    {
        if (delay_ != sc_core::SC_ZERO_TIME && is_des0(t)) {
            ++held_;
            wait(d + delay_);                    // the MAC's DMA thread waits in its b_transport
            d = sc_core::SC_ZERO_TIME;
        }
        out->b_transport(t, d);
    }

    sc_core::sc_time delay_;
    std::uint64_t held_ = 0;
};

}  // namespace cdc::platforms::fx1_soc
