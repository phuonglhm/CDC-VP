#include "not_integrated_target.h"

#include <iomanip>
#include <sstream>
#include <utility>

namespace cdc::platforms::fu3_soc {

not_integrated_target::not_integrated_target(sc_core::sc_module_name name, std::string block)
    : sc_core::sc_module(name), block_(std::move(block))
{
    socket.register_b_transport(this, &not_integrated_target::b_transport);
    socket.register_transport_dbg(this, &not_integrated_target::transport_dbg);
}

void not_integrated_target::b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time&)
{
    last_offset_ = tx.get_address();
    // The first access per block is a warning; later ones only count, so a
    // polling loop cannot flood the log. The run summary reports the totals.
    if (accesses_++ == 0) {
        std::ostringstream msg;
        msg << block_ << " is not integrated: " << (tx.is_write() ? "write" : "read") << " at offset 0x"
            << std::hex << std::setw(8) << std::setfill('0') << tx.get_address()
            << " answers TLM_GENERIC_ERROR_RESPONSE (further accesses are counted, not logged)";
        SC_REPORT_WARNING("fu3_soc/not_integrated", msg.str().c_str());
    }
    tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
}

unsigned not_integrated_target::transport_dbg(tlm::tlm_generic_payload& tx)
{
    last_offset_ = tx.get_address();
    ++debug_accesses_;
    return 0;
}

} // namespace cdc::platforms::fu3_soc
