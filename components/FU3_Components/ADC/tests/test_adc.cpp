#include "fu3/adc/adc.h"
#include <tlm_utils/simple_initiator_socket.h>
#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef FU3_ADC_BUS_TEST
#include "fu3_bus/bus_system.h"
#endif

using namespace sc_core;
using namespace fu3::adc;

struct Bench : sc_module {
    tlm_utils::simple_initiator_socket<Bench> cpu{"cpu"};
    sc_signal<bool> clk{"clk"}, rst{"rst"}, eoc{"eoc"}, samp{"samp"}, cs{"cs"},
        pwd{"pwd"}, mrst{"mrst"}, mode{"mode"};
    sc_signal<sc_dt::sc_uint<12>> data{"data"};
    sc_signal<sc_dt::sc_uint<4>> channel{"channel"};
    Adc adc{"adc"};
#ifdef FU3_ADC_BUS_TEST
    static fu3_bus::BusConfig bus_config() {
        auto c = fu3_bus::BusConfig::fu3();
        for (auto& t : c.targets) t.enabled = t.name == "ADC";
        return c;
    }
    fu3_bus::BusSystem fabric{"fabric", bus_config()};
    static constexpr std::uint64_t base = 0x500a0000;
#else
    static constexpr std::uint64_t base = 0;
#endif
    std::string scenario;
    bool passed = false, clock_enabled = true, suppress_eoc = false;
    bool early_eoc = false, held_eoc = false;
    sc_time reset_hold{210, SC_NS};
    sc_time macro_sample_delay{SC_ZERO_TIME};
    unsigned spec_checks = 0, policy_checks = 0, sample_index = 0;
    unsigned emitted_channel = 0;
    std::vector<unsigned> input;
    std::vector<sc_time> rises, falls;
    std::vector<unsigned> selected_channels;
    sc_time enabled_at = SC_ZERO_TIME;
    sc_event release_eoc, request_reset, reset_done, start_worker, worker_done;
    bool worker_complete = false;
    tlm::tlm_response_status worker_status = tlm::TLM_INCOMPLETE_RESPONSE;

    SC_HAS_PROCESS(Bench);
    Bench(sc_module_name name, const std::string& s) : sc_module(name), scenario(s) {
#ifdef FU3_ADC_BUS_TEST
        cpu.bind(fabric.initiator("CVA6"));
        fabric.target("ADC").bind(adc.socket);
#else
        cpu.bind(adc.socket);
#endif
        adc.adc_clk(clk); adc.reset_n(rst); adc.eoc(eoc); adc.data_in(data);
        adc.sampclk(samp); adc.cs(cs); adc.pwd(pwd); adc.macro_reset(mrst);
        adc.samp_sel(mode); adc.channel(channel);
        SC_THREAD(clock_driver);
        SC_THREAD(reset_driver);
        SC_THREAD(hardmacro);
        SC_THREAD(worker);
        SC_THREAD(pin_contract);
        SC_THREAD(run);
        SC_METHOD(monitor);
        sensitive << samp << pwd;
        dont_initialize();
    }
    void check(bool ok, const std::string& why, bool spec = false) {
        if (spec) ++spec_checks; else ++policy_checks;
        if (!ok) throw std::runtime_error(std::string(spec ? "SPEC_FAIL: " : "POLICY_FAIL: ") + why);
    }
    void clock_driver() {
        for (;;) { clk = clock_enabled; wait(50, SC_NS); clk = false; wait(50, SC_NS); }
    }
    void reset_driver() {
        rst = false; wait(225, SC_NS); rst = true;
        for (;;) {
            wait(request_reset); rst = false;
            wait(reset_hold); rst = true; reset_done.notify(SC_ZERO_TIME);
        }
    }
    void monitor() {
        if (pwd.event() && !pwd.read()) enabled_at = sc_time_stamp();
        if (samp.event()) {
            if (samp.read()) { rises.push_back(sc_time_stamp()); selected_channels.push_back(channel.read().to_uint()); }
            else if (rst.read() && !pwd.read()) falls.push_back(sc_time_stamp());
        }
    }
    void pin_contract() {
        unsigned previous_channel = channel.read().to_uint();
        for (;;) {
            wait(samp.value_changed_event() | cs.value_changed_event() |
                 channel.value_changed_event());
            wait(SC_ZERO_TIME); // Observe outputs after their signal updates.
            const auto current_channel = channel.read().to_uint();
            if (rst.read()) {
                check(!samp.read() || cs.read(), "CS high while SAMPCLK high", true);
                if (current_channel != previous_channel)
                    check(!cs.read(), "channel changes only while CS low", true);
            }
            previous_channel = current_channel;
        }
    }
    bool cancelled() const { return !rst.read() || pwd.read(); }
    void hardmacro() {
        eoc = false; data = 0;
        for (;;) {
            wait(samp.negedge_event());
            if (cancelled()) continue;
            emitted_channel = channel.read().to_uint();
            const auto value = input.empty() ? (emitted_channel * 257u) :
                input.at(sample_index % input.size());
            // D/EOC at 12/12.5 CLK after the effective sampling edge (HDS p11).
            // macro_phase also exercises a one-cycle ISAMPCLK sampling delay.
            if (early_eoc) {
                eoc = true;
                wait(sc_time(100, SC_NS), rst.negedge_event() | pwd.posedge_event());
                eoc = false;
                if (cancelled()) continue;
            }
            wait(sc_time(early_eoc ? 1100 : 1200, SC_NS) + macro_sample_delay,
                 rst.negedge_event() | pwd.posedge_event());
            if (cancelled()) { eoc = false; continue; }
            data = value;
            wait(sc_time(50, SC_NS), rst.negedge_event() | pwd.posedge_event());
            if (cancelled()) { eoc = false; continue; }
            if (suppress_eoc) {
                wait(release_eoc | rst.negedge_event() | pwd.posedge_event());
                if (cancelled()) { eoc = false; continue; }
            }
            ++sample_index; eoc = true;
            wait(samp.posedge_event() | rst.negedge_event() | pwd.posedge_event());
            if (!cancelled()) wait(sc_time(250, SC_NS) + macro_sample_delay,
                                   rst.negedge_event() | pwd.posedge_event());
            if (!held_eoc || cancelled()) eoc = false;
        }
    }
    tlm::tlm_response_status access(tlm::tlm_command cmd, std::uint64_t off,
        std::uint32_t& value, unsigned len = 4, unsigned width = 4,
        unsigned char* be = nullptr, unsigned be_len = 0, bool debug = false,
        bool null_data = false, sc_time delay = SC_ZERO_TIME) {
        unsigned char bytes[8]{};
        for (unsigned i = 0; i < 4; ++i) bytes[i] = static_cast<unsigned char>(value >> (8 * i));
        tlm::tlm_generic_payload tx;
        tx.set_command(cmd); tx.set_address(base + off); tx.set_data_ptr(null_data ? nullptr : bytes);
        tx.set_data_length(len); tx.set_streaming_width(width);
        tx.set_byte_enable_ptr(be); tx.set_byte_enable_length(be_len); tx.set_dmi_allowed(true);
#ifdef FU3_ADC_BUS_TEST
        fu3_bus::ScopedAccess metadata(tx, len < 4 ? len : 4, fu3_bus::Burst::Increment);
#endif
        const auto before = sc_time_stamp();
        if (debug) {
            const auto n = cpu->transport_dbg(tx);
            check(n == (tx.is_response_ok() ? 4u : 0u), "debug byte count");
            check(sc_time_stamp() == before, "debug consumes no simulation time");
        } else {
            cpu->b_transport(tx, delay);
            if (delay != SC_ZERO_TIME) wait(delay);
        }
        check(tx.get_address() == base + off && tx.get_data_length() == len &&
            tx.get_streaming_width() == width && tx.get_command() == cmd &&
            tx.get_data_ptr() == (null_data ? nullptr : bytes) && tx.get_byte_enable_ptr() == be &&
            tx.get_byte_enable_length() == be_len, "payload metadata preserved");
        check(!tx.is_dmi_allowed(), "CSR DMI denied");
        value = 0;
        for (unsigned i = 0; i < 4; ++i) value |= std::uint32_t(bytes[i]) << (8 * i);
        return tx.get_response_status();
    }
    void wr(unsigned off, std::uint32_t v) {
        check(access(tlm::TLM_WRITE_COMMAND, off, v) == tlm::TLM_OK_RESPONSE, "write " + std::to_string(off));
    }
    std::uint32_t rd(unsigned off, bool debug = false) {
        std::uint32_t v = 0;
        check(access(tlm::TLM_READ_COMMAND, off, v, 4, 4, nullptr, 0, debug) == tlm::TLM_OK_RESPONSE,
              "read " + std::to_string(off));
        return v;
    }
    void cycle(unsigned n = 1) {
        for (unsigned i = 0; i < n; ++i) { wait(clk.posedge_event()); wait(1, SC_NS); }
    }
    void phase(Adc::State state) {
        for (unsigned i = 0; i < 10000; ++i) {
            if (adc.state() == state) return;
            cycle();
        }
        check(false, "phase timeout");
    }
    void done() {
        for (unsigned i = 0; i < 20000; ++i) {
            if (rd(0x0c) & 1) return;
            wait(75, SC_NS);
        }
        check(false, "software polling timeout");
    }
    void stop() {
        wr(4, 0); cycle(2);
        check(rd(0x0c) == 0, "disable clears all operation flags", true);
        check(pwd && !samp && !cs, "power-down outputs", true);
    }
    void start(unsigned n, unsigned ch = 0, std::uint32_t timing = 0x20040c00) {
        stop();
        sample_index = 0; rises.clear(); falls.clear(); selected_channels.clear();
        wr(8, 0x10 | ch); wr(0x14, timing); wr(0x18, n); wr(4, 1);
    }
    void results(unsigned n, unsigned expected_channel = 0) {
        unsigned sum = 0;
        for (unsigned i = 0; i < n; ++i) {
            const auto expected = input.empty() ? expected_channel * 257u : input[i % input.size()];
            sum += expected;
            check(rd(0x1c + 4 * i) == expected, "raw sample " + std::to_string(i), true);
        }
        check(rd(0x10) == sum / n, "average result", sum % n == 0);
        check(rd(0x0c) == 3, "DONE/READY set, BUSY clear (VP bit assignment)");
        check(sample_index == n, "exact number of hardmacro conversions", true);
    }
    void reset_now() { request_reset.notify(SC_ZERO_TIME); wait(reset_done); wait(1, SC_NS); }
    void reset_values() {
        for (unsigned off = 0; off <= 0x58; off += 4) {
            const auto expected = off == 0 ? 0x10000u : off == 8 ? 0x10u :
                off == 0x14 ? 0x20040c00u : off == 0x18 ? 1u : 0u;
            check(rd(off, true) == expected, "reset register " + std::to_string(off), true);
        }
    }
    void test_registers() {
        reset_values();
        for (unsigned off = 0; off <= 0x58; off += 4) {
            if (off == 4 || off == 8 || off == 0x14 || off == 0x18) continue;
            const auto old = rd(off); wr(off, 0xffffffff);
            check(rd(off) == old, "RO write ignored", true);
        }
        for (unsigned bit = 0; bit < 32; ++bit) {
            for (auto off : {8u, 0x14u, 0x18u}) {
                const unsigned mask = off == 0x14 ? 0xffffff00u : 0x1fu;
                wr(off, 1u << bit);
                // Timing/count raw readback is a VP choice (D03).
                check(rd(off) == ((1u << bit) & mask), "RW bit and reserved mask", off == 8);
            }
            wr(4, (1u << bit) & ~1u);
            check(rd(4) == 0, "CTRL reserved bits zero", true);
        }
        wr(4, 0xffffffff); check(rd(4) == 1, "CTRL ADC_EN only", true);
        stop();
    }
    void test_channels() {
        for (unsigned ch = 0; ch < 16; ++ch) {
            input.clear(); start(1, ch); done(); results(1, ch);
            check(channel.read() == ch && mode, "channel and normal mode pins", true);
            check(selected_channels == std::vector<unsigned>{ch}, "selection stable at sample", true);
        }
        stop(); wr(8, 7); wr(4, 1); phase(Adc::State::SampleHigh);
        check(!mode && channel.read() == 7, "SAMP_SEL=0 passes to macro (analog test behavior external)", true);
        stop();
    }
    void test_counts() {
        input = {0, 4095, 1, 4094, 2048, 2047, 17, 63, 255, 256, 1023, 1024, 31, 99, 3777, 4095};
        for (unsigned encoded = 0; encoded < 32; ++encoded) {
            const unsigned effective = std::clamp(encoded, 1u, 16u);
            start(encoded); done(); results(effective);
            check(rd(0x18) == encoded, "raw SAMPLE_COUNT readback policy");
            for (unsigned i = effective; i < 16; ++i) check(rd(0x1c + 4 * i) == 0, "unused sample cleared at start");
        }
    }
    void timing_case(unsigned setup, unsigned high, unsigned low) {
        input = {100, 200, 300};
        const auto encoded = (setup << 24) | (high << 16) | (low << 8);
        start(3, 5, encoded); done(); results(3);
        check(rd(0x14) == encoded, "raw timing readback policy");
        check(rises.size() == 3 && falls.size() == 3, "three sample pulses", true);
        check(rises[0] - enabled_at == sc_time(std::max(setup, 32u) * 100, SC_NS), "setup cycles", true);
        for (unsigned i = 0; i < 3; ++i) {
            check(falls[i] - rises[i] == sc_time(std::max(high, 4u) * 100, SC_NS), "high cycles", true);
            if (i + 1 < 3) {
                check(rises[i + 1] - falls[i] >= sc_time(std::max(low, 12u) * 100, SC_NS), "low minimum", true);
                const auto expected = (std::max(std::max(low, 12u) + 1, 13u) + 2) * 100;
                check(rises[i + 1] - falls[i] == sc_time(expected, SC_NS), "WAIT_EOC/CAPTURE/CHECK_CNT cycle policy");
            }
        }
    }
    void test_averaging() {
        for (const auto value : {0u, 1u, 2047u, 2048u, 4094u, 4095u}) {
            input.assign(16, value); start(16); done(); results(16);
        }
        input = {0, 0, 1}; start(3); done(); results(3);
        check(rd(0x10) == 0, "nonintegral average truncates toward zero");
        input = {4095, 4095, 4094}; start(3); done(); results(3);
        check(rd(0x10) == 4094, "upper-bound nonintegral average");
        input = {0, 4095}; start(2); done(); results(2);
    }
    void test_eoc_wait() {
        input = {0xabc}; suppress_eoc = true; start(1);
        phase(Adc::State::WaitEoc); cycle(50);
        check(rd(0xc) == 4 && rd(0x10) == 0 && rd(0x1c) == 0, "missing EOC cannot complete or capture");
        check(cs && !samp && !pwd, "WAIT_EOC pin state");
        release_eoc.notify(SC_ZERO_TIME); done(); results(1);
        suppress_eoc = false;
    }
    void test_eoc_policy() {
        early_eoc = true;
        test_eoc_wait(); // A pulse ending before WAIT_EOC is not latched.
        early_eoc = false; held_eoc = true;
        input = {100, 200, 300}; start(3); done(); results(3);
        check(rd(0x10) == 200, "FSM accepts held EOC level on subsequent WAIT_EOC entries");
        held_eoc = false; stop();
    }
    void test_macro_phase() {
        // The controller updates SAMPCLK just after an ADC rising edge. A
        // macro sampling it on rising edges observes that change one cycle later.
        macro_sample_delay = sc_time(100, SC_NS);
        input = {1, 2048, 4095}; start(3, 11); done(); results(3);
        check(falls.size() == 3 && rises.size() == 3, "three conversions with delayed macro edge", true);
        for (unsigned i = 0; i + 1 < rises.size(); ++i)
            check(rises[i + 1] - falls[i] == sc_time(1600, SC_NS),
                  "FSM waits for EOC delayed by ISAMPCLK sampling policy");
        stop();
    }
    void test_lifecycle() {
        input = {4095, 1}; start(2); done(); results(2);
        for (unsigned i = 0; i < 20; ++i) {
            wr(4, 1); check(rd(0xc) == 3, "DONE sticky until disable");
            check(rd(0x10) == 2048, "reads and repeated enable preserve result", true);
        }
        check(sample_index == 2, "DONE does not auto-restart", true);
        stop(); check(rd(0x10) == 2048, "disable retains result policy");
        input = {33}; start(1); done(); results(1);
        check(rd(0x20) == 0, "shorter conversion clears old tail policy");
    }
    void test_status() {
        check(rd(0x0c) == 0, "STATUS cleared after reset", true);
        input = {0x123, 0x345}; start(2);
        for (auto state : {Adc::State::SetupWait, Adc::State::SampleHigh,
                           Adc::State::SampleLow, Adc::State::WaitEoc,
                           Adc::State::Capture, Adc::State::CheckCount}) {
            phase(state);
            // Debug reads consume no time, including one-cycle states.
            const auto status = rd(0x0c, true);
            check((status & 1) == 0, "DONE stays low before sequence completion", true);
            check(status == 4, "active BUSY[2], READY[1] ABI policy");
        }
        check(rd(0x10, true) == 0, "average published only at DONE policy");
        done(); results(2);
        check((rd(0x0c) & 1) == 1, "completion is visible at DONE[0]", true);
        check(rd(0x0c) == 3, "DONE BUSY=0 READY=1 with VP bit assignment");
        wr(0x0c, 0xffffffff);
        check((rd(0x0c) & 1) == 1, "RO STATUS write cannot clear DONE", true);
        cycle(20);
        check((rd(0x0c) & 1) == 1, "DONE remains set without disable", true);
        stop();
        check((rd(0x0c) & 7) == 0, "disable clears STATUS[2:0]", true);
        start(2); done(); reset_now();
        check(rd(0x0c) == 0, "reset clears completed STATUS", true);
    }
    void test_snapshot() {
        input = {10, 20, 30}; start(3, 2);
        phase(Adc::State::SampleHigh);
        wr(8, 0x1f); wr(0x18, 1); wr(0x14, 0x40408000);
        done(); results(3);
        check(selected_channels == std::vector<unsigned>({2, 2, 2}), "latched channel across active writes", true);
        check(channel.read() == 2, "channel pin holds original selection", true);
        for (unsigned i = 0; i < 3; ++i) check(falls[i] - rises[i] == sc_time(400, SC_NS), "latched timing", true);
        stop(); sample_index = 0; rises.clear(); falls.clear();
        wr(4, 1); done(); results(1);
        check(channel.read() == 15, "new sequence sees staged config", true);
        check(falls[0] - rises[0] == sc_time(6400, SC_NS), "next sequence uses new high duration", true);
    }
    void test_reset() {
        input = {999}; start(1); done(); reset_now(); reset_values();
        check(pwd && !cs && !samp, "reset pin outputs", true);
        start(1, 15); done(); results(1);
    }
    void test_phases(bool reset) {
        for (auto state : {Adc::State::SetupWait, Adc::State::SampleHigh, Adc::State::SampleLow,
                           Adc::State::WaitEoc, Adc::State::Capture, Adc::State::CheckCount, Adc::State::Done}) {
            input = {123}; start(1); phase(state);
            if (reset) { reset_now(); reset_values(); }
            else stop();
            check(pwd && !cs && !samp, "abort outputs in every phase");
            cycle(30); check(rd(0xc) == 0, "no stale completion after cancellation");
            input = {456}; start(1); done(); results(1);
        }
    }
    void test_protocol() {
        std::uint32_t v = 0xdeadbeef;
        const auto expect = [&](tlm::tlm_response_status got, tlm::tlm_response_status want) {
            check(got == want, "TLM error status");
        };
        for (auto off : {1ull, 2ull, 3ull, 0x59ull, 0x5cull, 0xffffull, 0x10000ull, 0x100000000ull})
            for (auto cmd : {tlm::TLM_READ_COMMAND, tlm::TLM_WRITE_COMMAND})
                expect(access(cmd, off, v), tlm::TLM_ADDRESS_ERROR_RESPONSE);
        for (auto len : {0u, 1u, 2u, 3u, 5u, 8u})
            expect(access(tlm::TLM_WRITE_COMMAND, 8, v, len, 8), tlm::TLM_BURST_ERROR_RESPONSE);
        for (auto width : {0u, 1u, 2u, 3u})
            expect(access(tlm::TLM_READ_COMMAND, 8, v, 4, width), tlm::TLM_BURST_ERROR_RESPONSE);
        unsigned char be[4]{0xff, 0, 0xff, 0};
        for (auto len : {0u, 1u, 4u})
            expect(access(tlm::TLM_WRITE_COMMAND, 8, v, 4, 4, be, len), tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        expect(access(tlm::TLM_IGNORE_COMMAND, 8, v), tlm::TLM_COMMAND_ERROR_RESPONSE);
        expect(access(tlm::TLM_READ_COMMAND, 8, v, 4, 4, nullptr, 0, false, true), tlm::TLM_GENERIC_ERROR_RESPONSE);
        check(rd(8) == 0x10, "malformed writes have no effect");
        v = 0x1f; const auto before = sc_time_stamp();
        expect(access(tlm::TLM_WRITE_COMMAND, 8, v, 4, 4, nullptr, 0, false, false, sc_time(37, SC_NS)), tlm::TLM_OK_RESPONSE);
        check(sc_time_stamp() - before == sc_time(62, SC_NS), "arrival delay plus two APB cycles");
        check(rd(8) == 0x1f, "delayed write applied");
    }
    void test_debug() {
        for (unsigned off = 0; off <= 0x58; off += 4) check(rd(off, true) == rd(off), "debug MMIO read parity");
        std::uint32_t v = 1;
        check(access(tlm::TLM_WRITE_COMMAND, 4, v, 4, 4, nullptr, 0, true) == tlm::TLM_COMMAND_ERROR_RESPONSE, "debug writes rejected");
        check(rd(4) == 0, "debug cannot start conversion");
        check(access(tlm::TLM_READ_COMMAND, 0x5c, v, 4, 4, nullptr, 0, true) == tlm::TLM_ADDRESS_ERROR_RESPONSE, "debug unmapped");
        tlm::tlm_generic_payload tx; tlm::tlm_dmi dmi;
        tx.set_dmi_allowed(true);
        check(!cpu->get_direct_mem_ptr(tx, dmi) && !tx.is_dmi_allowed(), "explicit DMI request denied");
        input = {123}; start(1); done();
        for (unsigned i = 0; i < 50; ++i) { check(rd(0xc, true) == 3, "debug DONE nondestructive"); check(rd(0x1c, true) == 123, "debug sample nondestructive"); }
    }
    void test_randomized() {
        std::mt19937 rng(0xadc2026);
        for (unsigned trial = 0; trial < 160; ++trial) {
            const unsigned n = 1 + rng() % 16, ch = rng() % 16;
            input.resize(n); for (auto& v : input) v = rng() & 4095;
            start(n, ch, ((rng() % 64) << 24) | ((rng() % 16) << 16) | ((rng() % 32) << 8));
            done(); results(n, ch);
            check(channel.read() == ch, "random channel", true);
        }
    }
    void test_clock_pause() {
        input = {55}; start(1); phase(Adc::State::SampleHigh);
        clock_enabled = false; wait(55, SC_NS);
        const auto status = rd(0xc); const auto count = rises.size();
        wait(10, SC_US);
        check(samp && rises.size() == count && rd(0xc) == status && rd(0x10) == 0, "stopped ADC clock freezes state");
        reset_now(); reset_values(); check(!samp && pwd, "reset asynchronous with stopped clock", true);
        clock_enabled = true; start(1); done(); results(1);
    }
    void worker() {
        for (;;) {
            wait(start_worker); std::uint32_t value = 0x1f;
            worker_status = access(tlm::TLM_WRITE_COMMAND, 8, value);
            worker_complete = true; worker_done.notify(SC_ZERO_TIME);
        }
    }
    void test_reset_transport() {
        worker_complete = false; start_worker.notify(SC_ZERO_TIME);
        wait(10, SC_NS); request_reset.notify(SC_ZERO_TIME);
        wait(reset_done);
        check(worker_complete && worker_status == tlm::TLM_GENERIC_ERROR_RESPONSE, "reset cancels in-flight write");
        reset_values();
        reset_hold = sc_time(5, SC_NS);
        worker_complete = false; start_worker.notify(SC_ZERO_TIME);
        wait(10, SC_NS); request_reset.notify(SC_ZERO_TIME);
        wait(reset_done); if (!worker_complete) wait(worker_done);
        check(worker_status == tlm::TLM_GENERIC_ERROR_RESPONSE, "completed reset pulse still cancels old transaction");
        reset_values();
        reset_hold = sc_time(210, SC_NS);
        request_reset.notify(SC_ZERO_TIME); wait(1, SC_NS);
        std::uint32_t v = 31;
        check(access(tlm::TLM_WRITE_COMMAND, 8, v) == tlm::TLM_GENERIC_ERROR_RESPONSE, "access entered in reset rejected");
        wait(reset_done); reset_values();
        // Two callers share the target; lock must release after cancellation.
        worker_complete = false; start_worker.notify(SC_ZERO_TIME);
        wr(0x18, 16);
        if (!worker_complete) wait(worker_done);
        check(worker_status == tlm::TLM_OK_RESPONSE && rd(8) == 31 && rd(0x18) == 16, "concurrent serialized accesses recover");
    }
    void test_bus() {
#ifdef FU3_ADC_BUS_TEST
        input = {0x100, 0x200, 0x300, 0x400}; start(4, 12); done(); results(4);
        unsigned char bytes[16]{};
        tlm::tlm_generic_payload tx;
        tx.set_command(tlm::TLM_READ_COMMAND); tx.set_address(base + 0x1c);
        tx.set_data_ptr(bytes); tx.set_data_length(16); tx.set_streaming_width(16);
        fu3_bus::ScopedAccess metadata(tx, 4, fu3_bus::Burst::Increment);
        sc_time delay = SC_ZERO_TIME; cpu->b_transport(tx, delay);
        check(tx.is_response_ok(), "APB bridge splits four-word raw-sample burst");
        for (unsigned i = 0; i < 4; ++i)
            check(bytes[4*i] == 0 && bytes[4*i+1] == i+1 && bytes[4*i+2] == 0 && bytes[4*i+3] == 0, "burst byte order");
        check(tx.get_address() == base + 0x1c && tx.get_data_length() == 16 && tx.get_data_ptr() == bytes, "bus restores burst metadata");
        std::uint32_t v = 0;
        check(access(tlm::TLM_READ_COMMAND, 0x5c, v) == tlm::TLM_ADDRESS_ERROR_RESPONSE, "unimplemented offset through bus");
        check(access(tlm::TLM_WRITE_COMMAND, 8, v, 1, 1) == tlm::TLM_BURST_ERROR_RESPONSE, "subword rejected through APB path");
        check(access(tlm::TLM_READ_COMMAND, 0x10000, v) == tlm::TLM_ADDRESS_ERROR_RESPONSE, "outside ADC region");
#endif
    }
    void run() {
        wait(250, SC_NS);
        try {
            if (scenario == "registers") test_registers();
            else if (scenario == "channels") test_channels();
            else if (scenario == "counts") test_counts();
            else if (scenario == "timing_min") { timing_case(0,0,0); timing_case(31,3,11); timing_case(32,4,12); }
            else if (scenario == "timing_programmed") { timing_case(33,5,13); timing_case(64,16,32); }
            else if (scenario == "timing_max") timing_case(255,255,255);
            else if (scenario == "averaging") test_averaging();
            else if (scenario == "eoc_wait") test_eoc_wait();
            else if (scenario == "eoc_policy") test_eoc_policy();
            else if (scenario == "macro_phase") test_macro_phase();
            else if (scenario == "lifecycle") test_lifecycle();
            else if (scenario == "status") test_status();
            else if (scenario == "snapshot") test_snapshot();
            else if (scenario == "reset") test_reset();
            else if (scenario == "protocol") test_protocol();
            else if (scenario == "debug") test_debug();
            else if (scenario == "randomized") test_randomized();
            else if (scenario == "clock_pause") test_clock_pause();
            else if (scenario == "reset_phases") test_phases(true);
            else if (scenario == "disable_phases") test_phases(false);
            else if (scenario == "reset_transport") test_reset_transport();
            else if (scenario == "bus") test_bus();
            else throw std::runtime_error("unknown scenario");
            passed = true;
            std::cout << "PASS " << scenario << " SPEC_CHECKS=" << spec_checks << " POLICY_CHECKS=" << policy_checks << '\n';
        } catch (const std::exception& e) {
            std::cerr << scenario << " at " << sc_time_stamp() << ": " << e.what() << '\n';
        }
        sc_stop();
    }
};
int sc_main(int argc, char** argv) {
    Bench b("bench", argc > 1 ? argv[1] : "registers");
    sc_start(100, SC_MS);
    if (!b.passed) std::cerr << "FAIL or simulation watchdog expired\n";
    return b.passed ? 0 : 1;
}
