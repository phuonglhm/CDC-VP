#include "gated_target.h"

#include <iomanip>
#include <sstream>
#include <utility>

namespace cdc::platforms::fu3_soc {

gated_target::gated_target(sc_core::sc_module_name name, std::string block)
    : sc_core::sc_module(name), block_(std::move(block))
{
    socket.register_b_transport(this, &gated_target::b_transport);
    socket.register_transport_dbg(this, &gated_target::transport_dbg);
}

void gated_target::b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay)
{
    if (state_ == state::available) {
        out->b_transport(tx, delay);
        return;
    }
    std::ostringstream msg;
    msg << block_ << (state_ == state::domain_reset  ? " is in reset (system or DBG domain)"
                      : state_ == state::clock_gated ? " is clock gated"
                                                     : " is held by its SW_RST_N bit")
        << ": " << (tx.is_write() ? "write" : "read") << " at offset 0x" << std::hex << std::setw(8)
        << std::setfill('0') << tx.get_address() << " answers TLM_GENERIC_ERROR_RESPONSE";
    if (state_ == state::domain_reset) {
        ++aborted_;
        msg << " (access aborted by the reset)";
        SC_REPORT_INFO_VERB("fu3_soc/gated", msg.str().c_str(), sc_core::SC_DEBUG);
    } else if (++refused_ <= kReported) {
        if (refused_ == kReported)
            msg << " (further refusals are counted, not logged)";
        SC_REPORT_WARNING("fu3_soc/gated", msg.str().c_str());
    }
    tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
}

unsigned gated_target::transport_dbg(tlm::tlm_generic_payload& tx)
{
    return out->transport_dbg(tx);
}

} // namespace cdc::platforms::fu3_soc
