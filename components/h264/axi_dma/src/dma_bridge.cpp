#include <h264/dma/dma_bridge.h>
#include <algorithm>
namespace h264 {
DmaBridge::DmaBridge(sc_core::sc_module_name name, unsigned width, unsigned burst)
    : sc_module(name), bus_bytes(width/8), max_beats(burst) {
    if ((width!=32 && width!=64 && width!=128) || !burst || burst>256)
        throw std::invalid_argument("DMA width must be 32/64/128; burst limit 1..256");
    input.register_b_transport(this, &DmaBridge::transport);
}
void DmaBridge::transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
    consume_delay(delay);
    tx.set_dmi_allowed(false);
    const uint64_t base=tx.get_address();
    const unsigned length=tx.get_data_length();
    auto* be=tx.get_byte_enable_ptr();
    const unsigned bel=tx.get_byte_enable_length();
    if (!tx.is_read() && !tx.is_write()) { tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE); return; }
    if (!length || !tx.get_data_ptr() || tx.get_streaming_width()<length) {
        tx.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE); return;
    }
    if (base>0xffffffffULL || length>(uint64_t(1)<<32)-base) {
        tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE); return;
    }
    if (be && !bel) { tx.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE); return; }
    if (be) for (unsigned i=0;i<bel;++i) if (be[i]!=0 && be[i]!=0xff) {
        tx.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE); return;
    }
    for (unsigned offset=0;offset<length;) {
        if (!epoch_valid(tx)) { tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE); return; }
        const uint64_t address=base+offset;
        const unsigned lane=static_cast<unsigned>(address%bus_bytes);
        const unsigned bytes=std::min({length-offset, unsigned(4096-address%4096),max_beats*bus_bytes-lane});
        const unsigned beats=(lane+bytes+bus_bytes-1)/bus_bytes;
        // Padded, aligned external beats model lane placement and WSTRB.
        std::vector<unsigned char> data(beats*bus_bytes,0), enables(beats*bus_bytes,0);
        for (unsigned i=0;i<bytes;++i) {
            data[lane+i]=tx.get_data_ptr()[offset+i];
            enables[lane+i]=be ? be[(offset+i)%bel] : 0xff;
        }
        tlm::tlm_generic_payload segment;
        segment.set_command(tx.get_command()); segment.set_address(address-lane);
        segment.set_data_ptr(data.data()); segment.set_data_length(static_cast<unsigned>(data.size()));
        segment.set_streaming_width(static_cast<unsigned>(data.size()));
        segment.set_byte_enable_ptr(enables.data()); segment.set_byte_enable_length(static_cast<unsigned>(enables.size()));
        auto* epoch=tx.get_extension<EpochExtension>();
        if (epoch) segment.set_extension(epoch);
        memory->b_transport(segment,delay);
        consume_delay(delay);
        if (epoch) segment.clear_extension<EpochExtension>();
        trace.push_back({address-lane,static_cast<unsigned>(data.size()),beats,sc_core::sc_time_stamp()});
        if (segment.is_response_error() || !epoch_valid(tx)) {
            tx.set_response_status(segment.is_response_error() ? segment.get_response_status() : tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }
        if (tx.is_read()) for (unsigned i=0;i<bytes;++i)
            if (enables[lane+i]) tx.get_data_ptr()[offset+i]=data[lane+i];
        offset+=bytes;
    }
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
}
}
