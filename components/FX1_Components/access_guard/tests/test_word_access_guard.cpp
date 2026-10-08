#include "fx1/word_access_guard.h"
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#ifdef FX1_GUARD_TEST_WITH_UART2
#include <uart.h>
#endif

namespace {
unsigned checks = 0, failures = 0;
void expect(bool ok, const std::string& what) {
    ++checks;
    if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; }
}

// Stand-in with the uart2_tlm defect: always copies 4 bytes.
struct FourByteModel : sc_core::sc_module {
    tlm_utils::simple_target_socket<FourByteModel> socket{"socket"};
    unsigned accesses = 0;
    explicit FourByteModel(sc_core::sc_module_name n) : sc_module(n) {
        socket.register_b_transport(this, &FourByteModel::b_transport);
    }
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time&) {
        ++accesses;
        const std::uint32_t value = 0x00000090;
        if (tx.is_read()) std::memcpy(tx.get_data_ptr(), &value, 4);
        tx.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

struct Tb : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<Tb> port{"port"};
    fx1::WordAccessGuard guard{"guard"};
    FourByteModel model{"model"};
#ifdef FX1_GUARD_TEST_WITH_UART2
    tlm_utils::simple_initiator_socket<Tb> uart_port{"uart_port"};
    fx1::WordAccessGuard uart_guard{"uart_guard"};
    UartTLM uart{"uart"};
    sc_core::sc_buffer<unsigned char> tx_line{"tx_line"};
    sc_core::sc_signal<bool> irq{"irq"};
#endif
    SC_HAS_PROCESS(Tb);
    explicit Tb(sc_core::sc_module_name n) : sc_module(n) {
        port.bind(guard.target);
        guard.out.bind(model.socket);
#ifdef FX1_GUARD_TEST_WITH_UART2
        uart_port.bind(uart_guard.target);
        uart_guard.out.bind(uart.bus);
        uart.tx(tx_line);
        uart.irq(irq);
#endif
        SC_THREAD(run);
    }
    template <typename Port>
    tlm::tlm_response_status access(Port& p, tlm::tlm_command cmd, std::uint64_t address,
                                    unsigned char* data, unsigned length,
                                    std::vector<unsigned char> enables = {}) {
        tlm::tlm_generic_payload tx;
        tx.set_command(cmd);
        tx.set_address(address);
        tx.set_data_ptr(data);
        tx.set_data_length(length);
        tx.set_streaming_width(length);
        tx.set_byte_enable_ptr(enables.empty() ? nullptr : enables.data());
        tx.set_byte_enable_length(static_cast<unsigned>(enables.size()));
        tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time delay;
        p->b_transport(tx, delay);
        return tx.get_response_status();
    }
    void run() {
        // Canary layout: payload byte(s) first, then A5 guard bytes.
        auto short_read = [&](auto& p, unsigned length, std::uint64_t address) {
            unsigned char buffer[8];
            std::memset(buffer, 0xA5, sizeof buffer);
            const auto status = access(p, tlm::TLM_READ_COMMAND, address, buffer, length);
            bool intact = true;
            for (unsigned i = length; i < sizeof buffer; ++i) intact &= buffer[i] == 0xA5;
            return std::make_pair(status, intact);
        };
        for (unsigned length : {1u, 2u, 3u}) {
            const auto [status, intact] = short_read(port, length, 0x18);
            expect(status == tlm::TLM_GENERIC_ERROR_RESPONSE, "short read rejected as slave error");
            expect(intact, "canary beyond a short payload untouched");
        }
        unsigned char word[8] = {};
        expect(access(port, tlm::TLM_READ_COMMAND, 0x1A, word, 4) == tlm::TLM_GENERIC_ERROR_RESPONSE,
               "misaligned word rejected");
        expect(access(port, tlm::TLM_WRITE_COMMAND, 0x0, word, 4, {0xFF, 0x00, 0xFF, 0xFF}) ==
                   tlm::TLM_GENERIC_ERROR_RESPONSE, "partial byte enable rejected");
        expect(access(port, tlm::TLM_WRITE_COMMAND, 0x0, nullptr, 4) == tlm::TLM_GENERIC_ERROR_RESPONSE,
               "null data rejected");
        expect(model.accesses == 0, "no rejected access reached the model");
        expect(guard.rejected() == 6, "rejections counted");
        expect(access(port, tlm::TLM_READ_COMMAND, 0x18, word, 4) == tlm::TLM_OK_RESPONSE &&
                   word[0] == 0x90 && model.accesses == 1, "aligned word forwarded");
        expect(access(port, tlm::TLM_WRITE_COMMAND, 0x0, word, 4, {0xFF}) == tlm::TLM_OK_RESPONSE,
               "full byte enable forwarded");

#ifdef FX1_GUARD_TEST_WITH_UART2
        // The real shared UART model behind the guard (reviewer probe G1-R3).
        {
            const auto [status, intact] = short_read(uart_port, 1, 0x18);
            expect(status == tlm::TLM_GENERIC_ERROR_RESPONSE && intact,
                   "uart2_tlm: 1-byte FR read rejected, canary intact");
        }
        unsigned char fr[4] = {};
        expect(access(uart_port, tlm::TLM_READ_COMMAND, 0x18, fr, 4) == tlm::TLM_OK_RESPONSE &&
                   (fr[0] & 0x80), "uart2_tlm: 32-bit FR read reports TX FIFO empty");
        unsigned char ch[4] = {'A', 0, 0, 0};
        expect(access(uart_port, tlm::TLM_WRITE_COMMAND, 0x0, ch, 4) == tlm::TLM_OK_RESPONSE,
               "uart2_tlm: 32-bit DR write accepted");
#endif
        sc_core::sc_stop();
    }
};
} // namespace

int sc_main(int, char**) {
    Tb tb("tb");
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_MS));
    std::cout << "fx1_access_guard: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
