#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <systemc>

namespace cdc::components {

// Host-side byte source/sink for a UartTLM instance (no bus presence).
//
// Purpose (docs/romcode_boot_hw_plan.md, Phase 2): the VP process needs a way
// to feed bytes INTO a UART's RX FIFO so the ROM-code UART download branch can
// be exercised. Two backends, selectable at run time before sc_start():
//
//   listen_on(port [, wait_for_client])
//       TCP server on 127.0.0.1:<port>. Bytes received from the connected
//       client are injected into RX; bytes the firmware transmits are
//       forwarded back to the client (bidirectional host-tool link).
//       With wait_for_client=true the simulation blocks at t=0 until a
//       client connects (useful interactively: sim time races wall time).
//
//   replay_file(path [, start_delay])
//       Deterministic CI backend: file bytes are injected into RX starting
//       at start_delay, one byte per byte_interval. TX still goes to the
//       platform's stdout monitor (and to a TCP client if one is connected).
//
// Ports: rx_out binds to UartTLM::rx (its public sc_buffer), tx_in binds to
// the same signal as the uart's tx output. Both must be bound even when the
// bridge is left unconfigured (it then idles: the thread exits at t=0).
//
// No line-rate modeling: byte_interval is an abstract pacing delay, only
// there so each injected byte lands in its own delta/evaluation and the
// 16-deep RX FIFO is not slammed in one instant.
class uart_host_bridge : public sc_core::sc_module {
public:
    sc_core::sc_out<unsigned char> rx_out;  // -> UartTLM::rx
    sc_core::sc_in<unsigned char>  tx_in;   // <- uart tx signal

    explicit uart_host_bridge(
        sc_core::sc_module_name name,
        sc_core::sc_time byte_interval = sc_core::sc_time(10, sc_core::SC_US),
        sc_core::sc_time poll_interval = sc_core::sc_time(100, sc_core::SC_US));
    ~uart_host_bridge() override;

    // Configure before sc_start(); both backends may be active at once
    // (file replays first, then the socket poll loop takes over).
    void listen_on(std::uint16_t port, bool wait_for_client = false);
    void replay_file(const std::string& path,
                     sc_core::sc_time start_delay = sc_core::SC_ZERO_TIME);

    // Actual listening port (differs from the request when port 0 was asked
    // for); 0 when the TCP backend is off. For tests.
    std::uint16_t listen_port() const { return listen_port_; }

private:
    void rx_thread();   // file replay + socket accept/recv -> rx_out
    void tx_method();   // tx_in -> connected client
    void inject(const unsigned char* data, std::size_t len);
    void accept_client(bool blocking);
    void drop_client();

    sc_core::sc_time byte_interval_;
    sc_core::sc_time poll_interval_;

    std::vector<unsigned char> replay_;
    sc_core::sc_time replay_delay_ = sc_core::SC_ZERO_TIME;

    int listen_fd_ = -1;
    int client_fd_ = -1;
    std::uint16_t listen_port_ = 0;
    bool wait_for_client_ = false;
};

} // namespace cdc::components
