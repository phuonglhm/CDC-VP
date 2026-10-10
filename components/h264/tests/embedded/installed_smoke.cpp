#include <h264/top/encoder_vp.h>
// Link/constructor check for the installed public headers and dependency graph.
// No simulation is started; functional behavior is covered by the other suites.
int sc_main(int,char**) {
    h264::EncoderVp encoder("encoder");
    return encoder.bridge.bus_bytes==4 ? 0 : 1;
}
