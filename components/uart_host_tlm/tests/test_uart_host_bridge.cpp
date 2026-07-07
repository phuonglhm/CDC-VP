// Unit test for uart_host_bridge: file replay into a real UartTLM RX FIFO,
// and the bidirectional TCP backend (loopback client in the same process).
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <uart.h>
#include <uart_host_bridge.h>

namespace {

class tb : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<tb> socket;
    cdc::components::uart_host_bridge* bridge = nullptr;

    SC_HAS_PROCESS(tb);
    explicit tb(sc_core::sc_module_name name)
        : sc_core::sc_module(name), socket("socket") {
        SC_THREAD(run);
    }

    std::uint32_t read32(std::uint64_t addr) {
        tlm::tlm_generic_payload trans;
        std::uint32_t data = 0;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        trans.set_data_length(4);
        socket->b_transport(trans, delay);
        assert(trans.is_response_ok());
        return data;
    }

    void write32(std::uint64_t addr, std::uint32_t data) {
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        trans.set_data_length(4);
        socket->b_transport(trans, delay);
        assert(trans.is_response_ok());
    }

    std::string drain_rx() {
        std::string s;
        while ((read32(UARTFR) & UART_RXFE) == 0)
            s.push_back(static_cast<char>(read32(UARTDR) & 0xFF));
        return s;
    }

    void run() {
        // ── File replay backend ─────────────────────────────────────────────
        // Replay starts at t=1ms; give the pacing (10us/byte) room to finish.
        wait(2, sc_core::SC_MS);
        assert(drain_rx() == "REPLAY");
        std::cout << "uart_host_bridge: file replay PASS\n";

        // ── TCP backend ─────────────────────────────────────────────────────
        // Client connects and sends; poll loop should pick it up.
        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        assert(fd >= 0);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(bridge->listen_port());
        assert(::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
        assert(::send(fd, "TCP!", 4, 0) == 4);

        // Sim time must advance for the bridge poll loop to run.
        std::string got;
        for (int i = 0; i < 1000 && got.size() < 4; ++i) {
            wait(1, sc_core::SC_MS);
            got += drain_rx();
        }
        assert(got == "TCP!");
        std::cout << "uart_host_bridge: TCP RX PASS\n";

        // Firmware TX must be forwarded back to the client.
        for (const char c : std::string("PONG"))
            write32(UARTDR, static_cast<std::uint32_t>(c));
        char back[8] = {};
        std::size_t n = 0;
        for (int i = 0; i < 1000 && n < 4; ++i) {
            wait(1, sc_core::SC_MS);
            ssize_t r = ::recv(fd, back + n, sizeof(back) - n, MSG_DONTWAIT);
            if (r > 0) n += static_cast<std::size_t>(r);
        }
        assert(n == 4 && std::memcmp(back, "PONG", 4) == 0);
        std::cout << "uart_host_bridge: TCP TX PASS\n";

        ::close(fd);
        std::cout << "uart_host_bridge test PASS\n";
        sc_core::sc_stop();
    }
};

} // namespace

int sc_main(int, char*[])
{
    UartTLM uart("uart");
    cdc::components::uart_host_bridge bridge("bridge");
    sc_core::sc_buffer<unsigned char> uart_tx("uart_tx");
    sc_core::sc_signal<bool> uart_irq("uart_irq");

    uart.tx(uart_tx);
    uart.irq(uart_irq);
    bridge.rx_out(uart.rx);
    bridge.tx_in(uart_tx);

    tb bench("bench");
    bench.bridge = &bridge;
    bench.socket.bind(uart.bus);

    // File replay stimulus.
    const char* replay_path = "test_uart_host_bridge_replay.bin";
    {
        std::FILE* f = std::fopen(replay_path, "wb");
        assert(f);
        std::fputs("REPLAY", f);
        std::fclose(f);
    }
    bridge.replay_file(replay_path, sc_core::sc_time(1, sc_core::SC_MS));
    bridge.listen_on(0); // ephemeral port, reported via listen_port()

    sc_core::sc_start();
    std::remove(replay_path);
    return 0;
}
