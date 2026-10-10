#include <h264/top/encoder_vp.h>
#include <h264_dma_tlm.h>
// Link/constructor check for the installed public headers and dependency graph.
// No simulation is started; functional behavior is covered by the other suites.
int sc_main(int,char**) {
    cdc::components::h264_dma_tlm dma("vinh_dma");
    h264::EncoderVp encoder("encoder");
    return encoder.dma.bus_bytes==4 ? 0 : 1;
}
