// Fault injection on one ISP DMA master (fx1_soc --isp-dma-fault KIND:N, ISP
// stream S4 review): the N-th transaction (1-based, counted from boot) of the
// ISP's IDMA (reads) or ODMA (writes) is answered by the VP instead of the bus.
//
//   idma-error:N  the N-th IDMA read gets TLM_GENERIC_ERROR_RESPONSE (an AXI
//                 read error: the ISP sets DMA_ERR.AXI_RD and abandons the frame)
//   odma-error:N  the N-th ODMA write gets TLM_GENERIC_ERROR_RESPONSE (an AXI
//                 write error: DMA_ERR.AXI_WR)
//   idma-stall:N[:US]  the N-th IDMA read is held US us of simulated time
//                 (default 1000000) and then completes normally: the ISP's input
//                 DMA waits with no error, so the frame in flight does not complete
//                 within the firmware's budget (its completion timeout). A read
//                 that never returned is not modelled on purpose: the DDR3
//                 controller answers a lost read with an error (READ_TIMEOUT), and
//                 a recovery cannot retire while a transaction is outstanding
//                 (W1C writes are ignored until it does: the ISP IRQ stays high;
//                 the stream firmware masks it meanwhile, S4 re-review R3). The
//                 report says "pending" while the held read has not returned.
//
// The DDR3 faults (--ddr3-fault) cannot do this deterministically: one DDR port
// serves every initiator, the firmware runs from DDR, and a stored-bit error in
// an input slot is overwritten by the next SYS_DMA copy. This module exists only
// with the option: without it the ISP sockets are bound to the fabric as before.
// Every other transaction is forwarded unchanged. Not a model of the SoC.
#pragma once

#include <cstdint>
#include <sstream>
#include <string>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

namespace cdc::platforms::fx1_soc {

class IspDmaFault : public sc_core::sc_module {
public:
    enum class Kind { idma_error, odma_error, idma_stall };

    tlm_utils::simple_target_socket<IspDmaFault> target{"target"};  // from the ISP's DMA socket
    tlm_utils::simple_initiator_socket<IspDmaFault> out{"out"};      // to the fabric (ODMA: its write guard)

    IspDmaFault(sc_core::sc_module_name name, Kind kind, std::uint64_t nth, std::uint64_t hold_us = 1000000)
        : sc_module(name), kind_(kind), nth_(nth), hold_(static_cast<double>(hold_us), sc_core::SC_US)
    {
        target.register_b_transport(this, &IspDmaFault::b_transport);
        target.register_transport_dbg(this, &IspDmaFault::transport_dbg);
    }

    bool idma() const noexcept { return kind_ != Kind::odma_error; }
    static const char* name_of(Kind k)
    {
        return k == Kind::idma_error ? "idma-error" : k == Kind::odma_error ? "odma-error" : "idma-stall";
    }

    // "<kind>:N, R transaction(s) seen, fired at #N address 0x... at T ns (...)" or "... not reached".
    // A stall is "held, pending" until the held transaction has returned (the run may end first),
    // then "held, completed at T ns".
    std::string report() const
    {
        std::ostringstream o;
        o << name_of(kind_) << ':' << nth_;
        if (kind_ == Kind::idma_stall) o << ':' << static_cast<std::uint64_t>(hold_.to_seconds() * 1e6);
        o << ", " << seen_ << (idma() ? " IDMA read(s)" : " ODMA write(s)") << " seen, ";
        if (fired_)
        {
            o << "fired at #" << nth_ << " address 0x" << std::hex << fired_addr_ << std::dec << " at " << fired_ns_
              << " ns";
            if (kind_ != Kind::idma_stall)
                o << " (error response)";
            else if (completed_)
                o << " (held, completed at " << completed_ns_ << " ns)";
            else
                o << " (held, pending: not returned when the run ended)";
        }
        else
            o << "not reached";
        return o.str();
    }

private:
    void b_transport(tlm::tlm_generic_payload& t, sc_core::sc_time& d)
    {
        const bool mine = idma() ? t.is_read() : t.is_write();
        if (mine && ++seen_ == nth_) {
            fired_ = true;
            fired_addr_ = t.get_address();
            fired_ns_ = static_cast<std::uint64_t>((sc_core::sc_time_stamp() + d).to_seconds() * 1e9);
            if (kind_ == Kind::idma_stall) {
                wait(d + hold_);  // the ISP's input DMA thread waits in its b_transport
                d = sc_core::SC_ZERO_TIME;
                out->b_transport(t, d);
                completed_ = true;
                completed_ns_ = static_cast<std::uint64_t>((sc_core::sc_time_stamp() + d).to_seconds() * 1e9);
                return;
            }
            t.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }
        out->b_transport(t, d);
    }

    unsigned transport_dbg(tlm::tlm_generic_payload& t) { return out->transport_dbg(t); }

    Kind kind_;
    std::uint64_t nth_;
    std::uint64_t seen_ = 0;
    bool fired_ = false, completed_ = false;
    std::uint64_t fired_addr_ = 0, fired_ns_ = 0, completed_ns_ = 0;
    sc_core::sc_time hold_;
};

}  // namespace cdc::platforms::fx1_soc
