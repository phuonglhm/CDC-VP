#include "fx1/plic.h"
#include <tlm_utils/simple_initiator_socket.h>
#include <iostream>
#include <string>

namespace {
unsigned checks = 0, failures = 0;
void expect(bool ok, const std::string& what) {
    ++checks;
    if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; }
}
constexpr unsigned kSources = 8, kContexts = 2;
constexpr std::uint64_t kPending = 0x1000, kEnable = 0x2000, kContext = 0x200000;

struct Tb : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<Tb> port{"port"};
    fx1::Plic plic{"plic", kSources, kContexts};
    sc_core::sc_vector<sc_core::sc_signal<bool>> line{"line", kSources - 1};
    sc_core::sc_vector<sc_core::sc_signal<bool>> eip{"eip", kContexts};
    SC_HAS_PROCESS(Tb);
    explicit Tb(sc_core::sc_module_name n) : sc_module(n) {
        port.bind(plic.socket);
        for (unsigned i = 0; i < kSources - 1; ++i) plic.irq_in[i](line[i]);
        for (unsigned c = 0; c < kContexts; ++c) plic.eip[c](eip[c]);
        SC_THREAD(run);
    }
    tlm::tlm_response_status rw(bool write, std::uint64_t offset, std::uint32_t& value,
                                unsigned length = 4) {
        unsigned char d[4];
        for (unsigned i = 0; i < 4; ++i) d[i] = static_cast<unsigned char>(value >> (8 * i));
        tlm::tlm_generic_payload tx;
        tx.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
        tx.set_address(offset);
        tx.set_data_ptr(d);
        tx.set_data_length(length);
        tx.set_streaming_width(length);
        sc_core::sc_time delay;
        port->b_transport(tx, delay);
        wait(delay + sc_core::sc_time(1, sc_core::SC_NS));  // the bus consumes it
        if (!write) value = d[0] | (d[1] << 8) | (d[2] << 16) | (std::uint32_t(d[3]) << 24);
        return tx.get_response_status();
    }
    std::uint32_t rd(std::uint64_t o) { std::uint32_t v = 0; rw(false, o, v); return v; }
    void wr(std::uint64_t o, std::uint32_t v) { rw(true, o, v); }
    std::uint32_t claim(unsigned c) { return rd(kContext + 0x1000 * c + 4); }
    void complete(unsigned c, std::uint32_t id) { wr(kContext + 0x1000 * c + 4, id); }
    void raise(unsigned id, bool level) { line[id - 1].write(level); wait(sc_core::sc_time(1, sc_core::SC_NS)); }

    void run() {
        wait(sc_core::sc_time(1, sc_core::SC_NS));
        expect(!eip[0].read() && !eip[1].read(), "outputs low after reset");
        for (unsigned s = 1; s < kSources; ++s) wr(4 * s, s);  // priority = id
        expect(rd(4 * 5) == 5, "priority readback");
        wr(4 * 7, 99);
        expect(rd(4 * 7) == 7, "priority clamps to the maximum (WARL)");
        expect(rd(0) == 0, "source 0 priority hardwired to 0");

        // Pending without enable: no output.
        raise(3, true);
        expect((rd(kPending) & (1u << 3)) && !eip[0].read(), "pending but disabled: no eip");
        wr(kEnable, 0xFF);
        expect(rd(kEnable) == 0xFE, "enable bit 0 hardwired to 0");
        expect(eip[0].read() && !eip[1].read(), "enabled on context 0 only");

        // Priority order and tie-breaking.
        raise(6, true);
        wr(4 * 2, 6);  // source 2 has the same priority as source 6
        raise(2, true);
        expect(claim(0) == 2, "equal priority: lowest ID wins");
        expect(!(rd(kPending) & (1u << 2)), "claim clears pending");
        expect(claim(0) == 6, "then the next-highest priority");
        expect(claim(0) == 3, "then the lowest");
        expect(claim(0) == 0 && !eip[0].read(), "nothing left: claim 0, eip low");

        // Level sources still high re-pend after complete; deasserted ones do not.
        raise(6, false);
        complete(0, 6);
        expect(!(rd(kPending) & (1u << 6)), "deasserted source not pending after complete");
        complete(0, 2);
        expect((rd(kPending) & (1u << 2)) && eip[0].read(), "still-high source pending again");
        raise(2, false);
        expect(claim(0) == 2, "pending survives deassert until claimed (spec gateway)");
        complete(0, 2);  // line 2 low: not pending again
        complete(0, 3);  // line 3 still high: pending again
        raise(3, false); // a latched request survives the deassert
        expect(rd(kPending) == (1u << 3), "only the re-armed source 3 is pending");
        expect(claim(0) == 3, "drain source 3");
        complete(0, 3);
        expect(rd(kPending) == 0 && !eip[0].read(), "all quiet");

        // Threshold gates notification only; claim ignores it (PLIC spec,
        // reviewer G2-R1 probe: priority 4, threshold 7 -> claim must return 4).
        wr(kContext, 7);
        raise(4, true);
        expect((rd(kPending) & (1u << 4)) && !eip[0].read(), "threshold 7 masks notification");
        tlm::tlm_generic_payload peek;
        unsigned char pd[4] = {};
        peek.set_command(tlm::TLM_READ_COMMAND);
        peek.set_address(kContext + 4);
        peek.set_data_ptr(pd);
        peek.set_data_length(4);
        expect(port->transport_dbg(peek) == 4 && pd[0] == 4, "debug claim shows a below-threshold candidate");
        expect(claim(0) == 4, "claim returns a source below the threshold");
        expect(!(rd(kPending) & (1u << 4)), "that claim cleared pending");
        raise(4, false);
        complete(0, 4);
        wr(kContext, 4);
        raise(4, true);
        expect(!eip[0].read(), "priority equal to threshold does not notify");
        wr(kContext, 3);
        expect(eip[0].read(), "threshold 3 notifies priority 4");
        expect(claim(0) == 4, "claim 4");
        raise(4, false);
        complete(0, 4);
        wr(kContext, 0);
        // Priority 0 is never claimable nor notifying.
        wr(4 * 4, 0);
        raise(4, true);
        expect((rd(kPending) & (1u << 4)) && !eip[0].read() && claim(0) == 0,
               "priority 0: pending but neither notifies nor claimable");
        wr(4 * 4, 4);
        expect(eip[0].read() && claim(0) == 4, "restored priority: notifies and claims");
        raise(4, false);
        complete(0, 4);

        // Two contexts, one request: exactly one claim succeeds.
        wr(kEnable + 0x80, 1u << 5);
        raise(5, true);
        expect(eip[0].read() && eip[1].read(), "shared source visible to both contexts");
        const auto first = claim(1), second = claim(0);
        expect(first == 5 && second == 0, "the second context gets nothing");
        expect(!eip[0].read() && !eip[1].read(), "both outputs drop after the claim");
        complete(0, 5);  // context 0 has source 5 enabled too, so this completes it
        raise(5, false);
        // Different thresholds: only context 1 is notified, yet context 0 can
        // still claim the same request first; then context 1 gets nothing.
        wr(kContext, 7);
        raise(5, true);
        expect(!eip[0].read() && eip[1].read(), "threshold 7 on context 0, 0 on context 1");
        expect(claim(0) == 5 && claim(1) == 0, "un-notified context claims first, the other gets 0");
        raise(5, false);
        complete(1, 5);
        wr(kContext, 0);
        // Completion from a context without the source enabled is ignored.
        wr(kEnable + 0x80, 1u << 1);
        wr(kEnable, 0);
        raise(1, true);
        expect(claim(1) == 1, "context 1 claims 1");
        complete(0, 1);
        raise(1, false);
        raise(1, true);
        expect(!(rd(kPending) & (1u << 1)), "complete from a non-enabled context ignored");
        complete(1, 1);
        expect((rd(kPending) & (1u << 1)) && eip[1].read(), "proper complete re-arms the source");

        // Debug read of claim has no side effect.
        tlm::tlm_generic_payload dbg;
        unsigned char d[4] = {};
        dbg.set_command(tlm::TLM_READ_COMMAND);
        dbg.set_address(kContext + 0x1000 + 4);
        dbg.set_data_ptr(d);
        dbg.set_data_length(4);
        expect(port->transport_dbg(dbg) == 4 && d[0] == 1, "debug claim shows the candidate");
        expect(rd(kPending) & (1u << 1), "debug claim did not claim");

        // Errors.
        std::uint32_t v = 0;
        expect(rw(false, 2, v) == tlm::TLM_GENERIC_ERROR_RESPONSE, "misaligned rejected");
        expect(rw(false, 0, v, 2) == tlm::TLM_GENERIC_ERROR_RESPONSE, "16-bit rejected");
        expect(rw(false, kContext + 0x2000, v) == tlm::TLM_GENERIC_ERROR_RESPONSE, "context 2 unmapped");
        expect(rw(false, kContext + 8, v) == tlm::TLM_GENERIC_ERROR_RESPONSE, "reserved context word");
        sc_core::sc_stop();
    }
};
} // namespace

int sc_main(int, char**) {
    Tb tb("tb");
    sc_core::sc_start();
    std::cout << "fx1_plic: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
