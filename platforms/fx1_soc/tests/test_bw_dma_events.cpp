// TrafficCounter bandwidth events (fx1_soc --bw-report, plan CSI_ISP_BW1):
// every read / write gets an ID, an issue event at sc_time_stamp() + incoming
// delay and a response event at sc_time_stamp() + the delay left when
// b_transport returns; metadata is snapshot before forwarding. Expected times
// are built from the targets' own behaviour (block / annotate), not from the
// unit under test.
//
//   test_fx1_bw_dma_events      Exit 0 when every check holds.

#include <cstdint>
#include <iostream>
#include <map>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "../src/traffic_counter.h"

using cdc::platforms::fx1_soc::DmaEventSink;
using cdc::platforms::fx1_soc::TrafficCounter;
using sc_core::sc_time;
using sc_core::SC_NS;
using sc_core::SC_US;

namespace {

int failures = 0;
#define CHECK(cond, what)                                                                  \
    do {                                                                                   \
        if (!(cond)) {                                                                     \
            std::cerr << "CHECK failed: " << (what) << " @ line " << __LINE__ << '\n';     \
            ++failures;                                                                    \
        }                                                                                  \
    } while (0)

// Behaviour per call, in order: wait `block` inside b_transport, then add
// `annotate` to the delay; optionally rewrite address / length (legal TLM-2.0).
struct Step {
    sc_time block, annotate;
    bool error = false, clobber = false;
};

struct Target : sc_core::sc_module {
    tlm_utils::simple_target_socket<Target> socket{"socket"};
    std::vector<Step> steps;
    std::size_t next = 0;
    explicit Target(sc_core::sc_module_name n) : sc_module(n) { socket.register_b_transport(this, &Target::b_transport); }
    void b_transport(tlm::tlm_generic_payload& t, sc_time& d)
    {
        const Step s = steps.at(next++);
        if (s.block != sc_core::SC_ZERO_TIME) sc_core::wait(s.block);
        d += s.annotate;
        if (s.clobber) {
            t.set_address(0xDEAD0000u);
            t.set_data_length(1);
        }
        t.set_response_status(s.error ? tlm::TLM_ADDRESS_ERROR_RESPONSE : tlm::TLM_OK_RESPONSE);
    }
};

struct Sink : DmaEventSink {
    struct rec {
        issue i;
        bool responded = false, ok = false;
        sc_time response;
    };
    std::map<std::uint64_t, rec> txns;
    std::vector<std::uint64_t> order;  // report order of the IDs (issue and response)
    void dma_issue(const issue& i) override
    {
        CHECK(!txns.count(i.id), "issue ID unique");
        txns[i.id].i = i;
        order.push_back(i.id);
    }
    void dma_response(unsigned master, std::uint64_t id, const sc_time& when, bool ok) override
    {
        CHECK(txns.count(id) && !txns[id].responded, "response matches one open issue");
        CHECK(master == 7, "response carries the master");
        auto& r = txns[id];
        r.responded = true;
        r.ok = ok;
        r.response = when;
        order.push_back(id);
    }
};

struct Bench : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<Bench> port{"port"}, port_plain{"port_plain"};
    TrafficCounter tc{"tc"}, plain{"plain"};
    Target t{"t"}, t_plain{"t_plain"};
    Sink sink;
    bool done = false;
    SC_HAS_PROCESS(Bench);
    explicit Bench(sc_core::sc_module_name n) : sc_module(n)
    {
        port.bind(tc.target);
        tc.out.bind(t.socket);
        port_plain.bind(plain.target);
        plain.out.bind(t_plain.socket);
        tc.set_event_sink(&sink, 7);
        SC_THREAD(run);
    }
    // One access with incoming delay `in`; returns the delay left after it.
    sc_time access(tlm_utils::simple_initiator_socket<Bench>& p, tlm::tlm_command cmd, std::uint64_t a, unsigned len,
                   sc_time in, bool be = false)
    {
        unsigned char buf[64] = {}, ben[64];
        for (auto& x : ben) x = 0xFF;
        tlm::tlm_generic_payload g;
        g.set_command(cmd);
        g.set_address(a);
        g.set_data_ptr(buf);
        g.set_data_length(len);
        g.set_streaming_width(len);
        g.set_byte_enable_ptr(be ? ben : nullptr);
        g.set_byte_enable_length(be ? len : 0);
        g.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_time d = in;
        p->b_transport(g, d);
        return d;
    }
    void run()
    {
        const std::vector<Step> steps = {
            {sc_core::SC_ZERO_TIME, sc_time(10, SC_NS)},           // 1 annotated only
            {sc_time(20, SC_US), sc_time(10, SC_NS)},              // 2 blocking, then annotated
            {sc_core::SC_ZERO_TIME, sc_time(5, SC_NS), true},      // 3 error response
            {sc_time(1, SC_US), sc_core::SC_ZERO_TIME, false, true},  // 4 rewrites address / length
        };
        t.steps = steps;
        t_plain.steps = steps;
        wait(sc_time(1, SC_US));
        // 1: issued with a 5 ns local offset, the target annotates 10 ns.
        sc_time d = access(port, tlm::TLM_WRITE_COMMAND, 0x89000000u, 64, sc_time(5, SC_NS));
        access(port_plain, tlm::TLM_WRITE_COMMAND, 0x89000000u, 64, sc_time(5, SC_NS));
        CHECK(d == sc_time(15, SC_NS), "annotated delay passed through unchanged");
        wait(d);  // the initiator waits its delay out, as the ISP's memory_access does
        // 2: the target blocks 20 us inside b_transport, then annotates 10 ns.
        d = access(port, tlm::TLM_READ_COMMAND, 0x8A000000u, 32, sc_core::SC_ZERO_TIME);
        access(port_plain, tlm::TLM_READ_COMMAND, 0x8A000000u, 32, sc_core::SC_ZERO_TIME);
        wait(d);
        // 3: error response.
        access(port, tlm::TLM_WRITE_COMMAND, 0x89000040u, 16, sc_core::SC_ZERO_TIME, true);
        access(port_plain, tlm::TLM_WRITE_COMMAND, 0x89000040u, 16, sc_core::SC_ZERO_TIME, true);
        // 4: the target rewrites the payload's address and length.
        access(port, tlm::TLM_WRITE_COMMAND, 0x89000080u, 48, sc_core::SC_ZERO_TIME);
        access(port_plain, tlm::TLM_WRITE_COMMAND, 0x89000080u, 48, sc_core::SC_ZERO_TIME);
        done = true;
    }
};

}  // namespace

int sc_main(int, char**)
{
    Bench b("bench");
    sc_core::sc_start(sc_time(1, sc_core::SC_MS));
    CHECK(b.done, "bench completed");
    const auto& tx = b.sink.txns;
    CHECK(tx.size() == 4, "four transactions reported");
    for (std::uint64_t id = 1; id <= 4; ++id) CHECK(tx.count(id) && tx.at(id).responded, "every ID responded");
    if (tx.size() == 4) {
        const auto& t1 = tx.at(1);
        CHECK(t1.i.write && t1.i.address == 0x89000000u && t1.i.bytes == 64 && t1.i.master == 7, "1: metadata");
        CHECK(t1.i.when == sc_time(1005, SC_NS), "1: issue = now + incoming delay");
        CHECK(t1.response == sc_time(1015, SC_NS) && t1.ok, "1: response = now + delay left (annotated)");
        const auto& t2 = tx.at(2);
        const sc_time i2 = sc_time(1015, SC_NS);
        CHECK(!t2.i.write && t2.i.bytes == 32 && t2.i.when == i2, "2: read issued after the first completes");
        CHECK(t2.response == i2 + sc_time(20, SC_US) + sc_time(10, SC_NS),
              "2: response = block waited inside + annotated remainder, not counted twice");
        const auto& t3 = tx.at(3);
        CHECK(!t3.ok && t3.i.byte_enable && t3.response == t3.i.when + sc_time(5, SC_NS), "3: error response, byte enable");
        const auto& t4 = tx.at(4);
        CHECK(t4.i.address == 0x89000080u && t4.i.bytes == 48, "4: metadata snapshot before forwarding");
        CHECK(t4.response == t4.i.when + sc_time(1, SC_US), "4: blocking target without annotation");
    }
    // Without a sink, the same traffic gives the same counts (events change nothing).
    const auto& a = b.tc;
    const auto& p = b.plain;
    CHECK(a.reads().txns == p.reads().txns && a.reads().bytes == p.reads().bytes && a.writes().txns == p.writes().txns &&
              a.writes().bytes == p.writes().bytes && a.writes().errors == p.writes().errors &&
              a.outstanding() == p.outstanding() && a.report("X") == p.report("X"),
          "counts and report identical with and without the event sink");
    std::cout << "test_fx1_bw_dma_events: " << (failures ? "FAIL" : "PASS") << " (" << failures << " failures)\n";
    return failures ? 1 : 0;
}
