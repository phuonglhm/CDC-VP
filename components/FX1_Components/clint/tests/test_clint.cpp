#include "fx1/clint.h"
#include <tlm_utils/simple_initiator_socket.h>
#include <iostream>
#include <string>

namespace {
unsigned checks = 0, failures = 0;
void expect(bool ok, const std::string& what) {
    ++checks;
    if (!ok) { ++failures; std::cerr << "FAIL: " << what << " @" << sc_core::sc_time_stamp() << '\n'; }
}
using sc_core::sc_time;
using sc_core::SC_NS;
using sc_core::SC_US;

struct Tb : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<Tb> port{"port"};
    fx1::Clint clint{"clint", 2, 20000000};  // 20 MHz: one tick per 50 ns
    sc_core::sc_vector<sc_core::sc_signal<bool>> msip{"msip", 2}, mtip{"mtip", 2};
    sc_core::sc_time mtip0_rise;
    unsigned mtip0_edges = 0;
    SC_HAS_PROCESS(Tb);
    explicit Tb(sc_core::sc_module_name n) : sc_module(n) {
        port.bind(clint.socket);
        for (unsigned h = 0; h < 2; ++h) {
            clint.msip_irq[h].bind(msip[h]);
            clint.mtip_irq[h].bind(mtip[h]);
        }
        SC_METHOD(watch);
        sensitive << mtip[0];
        dont_initialize();
        SC_THREAD(run);
    }
    void watch() {
        if (mtip[0].read()) { ++mtip0_edges; mtip0_rise = sc_core::sc_time_stamp(); }
    }
    tlm::tlm_response_status rw(bool write, std::uint64_t offset, std::uint64_t& value, unsigned length = 4) {
        unsigned char data[8] = {};
        for (unsigned i = 0; i < length; ++i) data[i] = static_cast<unsigned char>(value >> (8 * i));
        tlm::tlm_generic_payload tx;
        tx.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
        tx.set_address(offset);
        tx.set_data_ptr(data);
        tx.set_data_length(length);
        tx.set_streaming_width(length);
        tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_time delay;
        port->b_transport(tx, delay);
        expect(!tx.is_response_ok() || delay == sc_time(10, SC_NS), "access latency annotated");
        wait(delay);  // the bus consumes it; emulate that here
        if (!write) {
            value = 0;
            for (unsigned i = 0; i < length; ++i) value |= std::uint64_t(data[i]) << (8 * i);
        }
        return tx.get_response_status();
    }
    std::uint64_t read32(std::uint64_t offset) { std::uint64_t v = 0; rw(false, offset, v); return v; }
    void write32(std::uint64_t offset, std::uint64_t v) { rw(true, offset, v); }
    std::uint64_t read_mtime() {
        // RV32 sequence: hi, lo, hi again until hi is stable.
        for (;;) {
            const auto hi = read32(0xBFFC), lo = read32(0xBFF8), hi2 = read32(0xBFFC);
            if (hi == hi2) return (hi << 32) | lo;
        }
    }
    void run() {
        wait(sc_time(1, SC_NS));
        expect(!mtip[0].read() && !mtip[1].read() && !msip[0].read(), "reset levels low");
        expect(read32(0x4000) == 0xFFFFFFFF && read32(0x4004) == 0xFFFFFFFF, "mtimecmp resets to all ones");

        const auto t0 = read_mtime();
        wait(sc_time(1, SC_US));
        const auto t1 = read_mtime();
        expect(t1 - t0 >= 20 && t1 - t0 <= 22, "mtime counts 20 per microsecond");
        expect(clint.mtime() == read_mtime(), "mtime() matches the register view");

        // Timer on hart 0: raise exactly at the compare tick.
        const auto now = read_mtime();
        const auto cmp = now + 40;  // 2 us ahead
        write32(0x4004, 0);         // hi first, as RV32 firmware should
        write32(0x4000, cmp & 0xFFFFFFFF);
        const auto armed = sc_core::sc_time_stamp();
        expect(!mtip[0].read(), "not pending before deadline");
        wait(sc_time(3, SC_US));
        expect(mtip[0].read() && !mtip[1].read(), "hart 0 MTIP raised, hart 1 untouched");
        expect(mtip0_edges == 1, "single rising edge");
        const auto expected_rise = sc_time::from_value(cmp * clint.tick().value());
        expect(mtip0_rise == expected_rise, "MTIP rises at mtimecmp * tick");
        expect(mtip0_rise > armed, "rise after arming");
        write32(0x4000, 0xFFFFFFFF);
        write32(0x4004, 0xFFFFFFFF);
        expect(!mtip[0].read(), "rewriting mtimecmp lowers MTIP before the writer resumes");

        // Already-expired compare asserts immediately.
        write32(0x400C, 0);
        write32(0x4008, 0);
        expect(mtip[1].read(), "hart 1 compare in the past asserts at once");
        std::uint64_t all_ones = ~0ull;
        expect(rw(true, 0x4008, all_ones, 8) == tlm::TLM_OK_RESPONSE, "64-bit write accepted");
        expect(!mtip[1].read(), "64-bit write clears hart 1");

        // Software interrupts.
        write32(0x0004, 1);
        expect(msip[1].read() && !msip[0].read(), "msip[1] only");
        expect(read32(0x0004) == 1, "msip readback");
        write32(0x0004, 0xFFFFFFFE);
        expect(!msip[1].read() && read32(0x0004) == 0, "only bit 0 is writable");

        // mtime write re-bases the counter.
        write32(0xBFFC, 0);
        write32(0xBFF8, 1000);
        const auto rebased = read_mtime();
        expect(rebased >= 1000 && rebased < 1010, "mtime write re-bases");

        // Errors.
        std::uint64_t v = 1;
        expect(rw(true, 0x0008, v) == tlm::TLM_GENERIC_ERROR_RESPONSE, "msip of a third hart is unmapped");
        expect(rw(false, 0x0002, v, 2) == tlm::TLM_GENERIC_ERROR_RESPONSE, "16-bit access rejected");
        expect(rw(false, 0x4002, v) == tlm::TLM_GENERIC_ERROR_RESPONSE, "misaligned mtimecmp rejected");
        expect(rw(false, 0x8000, v) == tlm::TLM_GENERIC_ERROR_RESPONSE, "reserved offset rejected");
        sc_core::sc_stop();
    }
};
} // namespace

int sc_main(int, char**) {
    Tb tb("tb");
    sc_core::sc_start();
    std::cout << "fx1_clint: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
