// Fault injection on the CSI-2 RX RAW DMA (fx1_soc --csi-dma-fault write-error:N,
// plan DIRECT_CSI_ISP D2b): the N-th write of the camera's DMA (1-based,
// counted from boot) is answered TLM_GENERIC_ERROR_RESPONSE by the VP instead
// of the bus, which the CSI-2 RX reads as AXI SLVERR (BRESP 2). The CSI then
// aborts the capture being written and ends it per DMA err_frame_policy
// (rewound, or released with error); the stream path is not concerned
// (baseline coupling, contract DIRECT_CSI_ISP §11).
//
// The DDR3 faults (--ddr3-fault) cannot target one master's write: one DDR
// port serves every initiator. Topology: CSI dma_socket -> MIPI_DMA traffic
// counter (with --traffic-report) -> this shim -> write guard -> fabric. The
// counter sees the failed write as issued and returned with an error (D2b
// review F1); the write reaches neither the guard nor DDR (nothing is
// written). report() gives the writes it saw: equal to the counter's issued
// writes. It exists only with the option.
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

class CsiDmaFault : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<CsiDmaFault> target{"target"};  // from the CSI's dma_socket
    tlm_utils::simple_initiator_socket<CsiDmaFault> out{"out"};      // to its write guard

    CsiDmaFault(sc_core::sc_module_name name, std::uint64_t nth) : sc_module(name), nth_(nth)
    {
        target.register_b_transport(this, &CsiDmaFault::b_transport);
        target.register_transport_dbg(this, &CsiDmaFault::transport_dbg);
    }

    // "write-error:N, R write(s) seen, fired at #N address 0x... at T ns (error response)" or "... not reached".
    std::string report() const
    {
        std::ostringstream o;
        o << "write-error:" << nth_ << ", " << seen_ << " MIPI_DMA write(s) seen, ";
        if (fired_)
            o << "fired at #" << nth_ << " address 0x" << std::hex << fired_addr_ << std::dec << " at " << fired_ns_
              << " ns (error response)";
        else
            o << "not reached";
        return o.str();
    }

private:
    void b_transport(tlm::tlm_generic_payload& t, sc_core::sc_time& d)
    {
        if (t.is_write() && ++seen_ == nth_) {
            fired_ = true;
            fired_addr_ = t.get_address();
            fired_ns_ = static_cast<std::uint64_t>((sc_core::sc_time_stamp() + d).to_seconds() * 1e9);
            t.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }
        out->b_transport(t, d);
    }

    unsigned transport_dbg(tlm::tlm_generic_payload& t) { return out->transport_dbg(t); }

    std::uint64_t nth_;
    std::uint64_t seen_ = 0;
    bool fired_ = false;
    std::uint64_t fired_addr_ = 0, fired_ns_ = 0;
};

}  // namespace cdc::platforms::fx1_soc
