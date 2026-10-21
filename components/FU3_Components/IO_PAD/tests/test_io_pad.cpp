#include "fu3/io_pad/io_pad.h"
#include <array>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>

using namespace sc_core;
using namespace fu3::io_pad;

// Standalone IO_PAD fixture: no GPIO module, clock, reset or bus dependency.
struct PadBench : sc_module {
    IoPad pad{"pad"};
    sc_vector<sc_signal<sc_dt::sc_uint<2>>> select{"select", 80};
    sc_vector<sc_signal<bool>> pins{"pins", 80}, po{"po", 80}, ao{"ao", 80}, pe{"pe", 80},
        ae{"ae", 80}, data{"data", 80}, enable{"enable", 80}, pi{"pi", 80}, ai{"ai", 80};
    sc_signal<bool> test_mode{"test_mode"}, test_select{"test_select"};
    unsigned checks = 0, errors = 0;
    std::string scenario;
    PadBench* peer = nullptr;
    SC_HAS_PROCESS(PadBench);
    PadBench(sc_module_name name, const std::string& which, bool active = true)
        : sc_module(name), scenario(which) {
        for (unsigned i = 0; i < 80; ++i) {
            pad.select[i](select[i]);
            pad.pad_in[i](pins[i]);
            pad.primary_out[i](po[i]);
            pad.alternate_out[i](ao[i]);
            pad.primary_oe[i](pe[i]);
            pad.alternate_oe[i](ae[i]);
            pad.pad_data[i](data[i]);
            pad.requested_oe[i](enable[i]);
            pad.primary_in[i](pi[i]);
            pad.alternate_in[i](ai[i]);
        }
        pad.test_mode(test_mode);
        pad.test_select(test_select);
        if (which == "pad_delta") {
            // Nonzero values exist before simulation: initial evaluation must
            // decode test mode without relying on a later input transition.
            for (unsigned i = 0; i < 80; ++i) {
                select[i].write(i % 4);
                pins[i].write(true);
                po[i].write(true);
                ao[i].write(true);
                pe[i].write(true);
                ae[i].write(true);
            }
        }
        if (active) {
            SC_THREAD(run);
        }
    }
    void check(bool ok, const std::string& message) {
        ++checks;
        if (!ok) {
            ++errors;
            if (errors <= 30)
                std::cerr << "FAIL " << scenario << " @ " << sc_time_stamp() << ": " << message
                          << '\n';
        }
    }
    void settle() {
        wait(1, SC_NS);
    }
    // Independent expected membership from IO_PAD Table 7-2, PDF page 11.
    // No expected result is calculated by calling the implementation's pin_info.
    struct ExpectedPin {
        bool digital = false, input_only = false, mux = false, reserved = false;
    };
    std::array<ExpectedPin, 80> expected_pins() {
        std::array<ExpectedPin, 80> result{};
        for (unsigned p :
             {1u,  2u,  3u,  4u,  5u,  6u,  11u, 12u, 13u, 14u, 15u, 20u, 21u, 22u, 23u,
              24u, 25u, 26u, 27u, 28u, 29u, 30u, 44u, 45u, 46u, 47u, 54u, 55u, 56u, 57u,
              58u, 59u, 60u, 61u, 62u, 67u, 68u, 69u, 70u, 71u, 72u, 73u, 74u, 75u, 76u})
            result[p - 1].digital = true;
        for (unsigned p : {1u, 5u, 11u})
            result[p - 1].input_only = true;
        for (unsigned p :
             {6u, 12u, 13u, 14u, 15u, 20u, 21u, 22u, 25u, 26u, 27u, 28u, 29u, 46u, 47u})
            result[p - 1].mux = true;
        for (unsigned p : {23u, 24u})
            result[p - 1].reserved = true;
        return result;
    }
    void verify_pad(unsigned i, const ExpectedPin& info) {
        const bool mode = pins[4].read();
        const unsigned code = info.mux ? select[i].read().to_uint() : 0;
        const bool active = info.digital && !info.reserved && (!mode || i == 4 || i == 10);
        const bool primary = active && code == 0, alternate = active && code == 1;
        const bool out = primary ? po[i].read() : alternate ? ao[i].read() : false;
        const bool req = primary ? pe[i].read() : alternate ? ae[i].read() : false;
        const std::string at = " package " + std::to_string(i + 1);
        check(data[i].read() == (out && !info.input_only), "independent mux data" + at);
        check(enable[i].read() == (req && !info.input_only),
              "requested enable separate from data" + at);
        check(pi[i].read() == (primary && pins[i].read()), "selected primary receiver" + at);
        check(ai[i].read() == (alternate && pins[i].read()), "selected alternate receiver" + at);
    }
    void pad_truth() {
        const auto expected = expected_pins();
        for (unsigned mode = 0; mode < 2; ++mode) {
            pins[4].write(mode);
            settle();
            for (unsigned i = 0; i < 80; ++i) {
                const auto info = expected[i],
                           actual =
                               ExpectedPin{pin_info(i + 1).digital, pin_info(i + 1).input_only,
                                           pin_info(i + 1).configurable, pin_info(i + 1).reserved};
                check(info.digital == actual.digital && info.input_only == actual.input_only &&
                          info.mux == actual.mux && info.reserved == actual.reserved,
                      "independent package membership");
                for (unsigned code = 0; code < 4; ++code)
                    for (unsigned bits = 0; bits < 32; ++bits) {
                        select[i].write(code);
                        po[i].write(bits & 1);
                        ao[i].write(bits & 2);
                        pe[i].write(bits & 4);
                        ae[i].write(bits & 8);
                        if (i != 4)
                            pins[i].write(bits & 16);
                        settle();
                        verify_pad(i, info);
                        check(test_mode.read() == bool(mode) &&
                                  test_select.read() == bool(mode && pins[10].read()),
                              "test decoder remains live during routing sweep");
                    }
                if (i != 4)
                    pins[i].write(false);
            }
        }
        // Explicit return from test mode with nonzero sources in both partitions.
        for (unsigned i = 0; i < 80; ++i) {
            select[i].write(0);
            po[i].write(true);
            pe[i].write(true);
            if (i != 4)
                pins[i].write(true);
        }
        pins[4].write(true);
        settle();
        for (unsigned i = 0; i < 80; ++i)
            verify_pad(i, expected[i]);
        pins[4].write(false);
        settle();
        for (unsigned i = 0; i < 80; ++i)
            verify_pad(i, expected[i]);
        check(pi[0].read() && pi[1].read() && pi[2].read() && pi[3].read() && pi[75].read(),
              "POR, boot/mode straps and oscillator recover without clock");
    }
    void pad_random() {
        const auto expected = expected_pins();
        for (unsigned seed : {0x12345u, 0x6701u, 20261017u}) {
            std::mt19937 rng(seed);
            for (unsigned cycle = 0; cycle < 600; ++cycle) {
                for (unsigned i = 0; i < 80; ++i) {
                    const auto value = rng();
                    select[i].write(value & 3);
                    pins[i].write(value & 4);
                    po[i].write(value & 8);
                    ao[i].write(value & 16);
                    pe[i].write(value & 32);
                    ae[i].write(value & 64);
                }
                settle();
                for (unsigned i = 0; i < 80; ++i)
                    verify_pad(i, expected[i]);
                check(test_mode.read() == pins[4].read(), "random mode decoder");
                check(test_select.read() == (pins[4].read() && pins[10].read()),
                      "random select decoder");
            }
        }
    }
    void metadata() {
        // IO_PAD HDS Table 7-2, independent of the implementation's ranges.
        const unsigned mapping[3][8] = {{6, 12, 13, 14, 15, 20, 21, 22},
                                        {25, 26, 27, 28, 29, 30, 44, 45},
                                        {46, 47, 54, 55, 56, 59, 60, 61}};
        const auto expected = expected_pins();
        unsigned digital = 0, inputs = 0, configurable = 0, bonded = 0;
        for (unsigned p = 1; p <= 80; ++p) {
            int bank = -1, bit = -1;
            unsigned select_code = 0;
            for (unsigned b = 0; b < 3; ++b)
                for (unsigned pin = 0; pin < 8; ++pin)
                    if (mapping[b][pin] == p) {
                        bank = int(b);
                        bit = int(pin);
                        select_code = (b == 0 && pin < 5) || (b == 2 && pin < 2);
                    }
            const auto actual = pin_info(p);
            check(actual.gpio_bank == bank && actual.gpio_bit == bit &&
                      actual.gpio_select == select_code,
                  "GPIO bank/bit/select metadata package " + std::to_string(p));
            check(actual.digital == expected[p - 1].digital &&
                      actual.input_only == expected[p - 1].input_only &&
                      actual.configurable == expected[p - 1].mux &&
                      actual.reserved == expected[p - 1].reserved,
                  "digital/input-only/configurable/reserved metadata");
            digital += actual.digital;
            inputs += actual.input_only;
            configurable += actual.configurable;
            bonded += actual.gpio_bank >= 0;
        }
        check(digital == 45 && inputs == 3 && configurable == 15 && bonded == 24,
              "HDS package cardinalities");
        for (unsigned p : {0u, 81u, 100u, std::numeric_limits<unsigned>::max()}) {
            const auto info = pin_info(p);
            check(!info.digital && !info.input_only && !info.configurable && !info.reserved &&
                      info.gpio_bank == -1 && info.gpio_bit == -1 && info.gpio_select == 0,
                  "out-of-package metadata has no digital or GPIO route");
        }
    }
    void delta() {
        const auto expected = expected_pins();
        const auto start = sc_time_stamp();
        auto verify = [&]() {
            // This is a settling bound, not an exact hardware propagation delay.
            // Any timed wait in routing must fail while simulation time stays zero.
            for (unsigned d = 0; d < 8; ++d)
                wait(SC_ZERO_TIME);
            check(sc_time_stamp() == start, "routing requires no elapsed simulation time");
            for (unsigned i = 0; i < 80; ++i) {
                verify_pad(i, expected[i]);
                peer->verify_pad(i, expected[i]);
            }
            for (auto* bench : {this, peer}) {
                check(bench->test_mode.read() == bench->pins[4].read(),
                      "independent instance test mode decoder");
                check(bench->test_select.read() ==
                          (bench->pins[4].read() && bench->pins[10].read()),
                      "independent instance test select decoder");
            }
        };
        verify();
        check(test_mode.read() && test_select.read(), "nonzero test inputs at startup");
        // Cover every old/new select pair and both decoder input orders.
        // One instance changes while its peer retains all its sources.
        for (auto* changed : {this, peer})
            for (unsigned previous = 0; previous < 4; ++previous)
                for (unsigned next = 0; next < 4; ++next)
                    for (unsigned straps : {0u, 1u, 3u, 2u, 3u, 1u, 0u}) {
                        for (unsigned i = 0; i < 80; ++i) {
                            changed->select[i].write(previous);
                            changed->pins[i].write(i % 2);
                            changed->po[i].write(true);
                            changed->ao[i].write(false);
                            changed->pe[i].write(false);
                            changed->ae[i].write(true);
                        }
                        changed->pins[4].write(straps & 1);
                        changed->pins[10].write(straps & 2);
                        verify();
                        for (unsigned i = 0; i < 80; ++i)
                            changed->select[i].write(next);
                        verify();
                        // Toggle each source kind separately, with the selected
                        // function and all other source kinds held constant.
                        for (unsigned source = 0; source < 5; ++source) {
                            auto& signals = source == 0 ? changed->po
                                            : source == 1 ? changed->ao
                                            : source == 2 ? changed->pe
                                            : source == 3 ? changed->ae
                                                          : changed->pins;
                            for (unsigned i = 0; i < 80; ++i)
                                if (source != 4 || (i != 4 && i != 10))
                                    signals[i].write(!signals[i].read());
                            verify();
                        }
                    }
        checks += peer->checks;
        errors += peer->errors;
    }
    void isolation() {
        const auto expected = expected_pins();
        for (unsigned i = 0; i < 80; ++i) {
            select[i].write(i % 4);
            po[i].write(i % 2);
            ao[i].write(!(i % 2));
            pe[i].write(i % 3 == 0);
            ae[i].write(i % 3 != 0);
            pins[i].write(i % 4 < 2);
        }
        pins[4].write(false);
        settle();
        for (unsigned changed = 0; changed < 80; ++changed) {
            if (changed == 4) // Test mode intentionally controls both partitions.
                continue;
            for (unsigned code = 0; code < 4; ++code)
                for (unsigned source = 0; source < 6; ++source) {
                    select[changed].write(code);
                    po[changed].write(bool(changed % 2) ^ (source == 0));
                    ao[changed].write(bool(!(changed % 2)) ^ (source == 1));
                    pe[changed].write(bool(changed % 3 == 0) ^ (source == 2));
                    ae[changed].write(bool(changed % 3 != 0) ^ (source == 3));
                    pins[changed].write(bool(changed % 4 < 2) ^ (source == 4));
                    settle();
                    for (unsigned i = 0; i < 80; ++i)
                        verify_pad(i, expected[i]);
                    check(!test_mode.read() && !test_select.read(),
                          "non-test-mode pin changes cannot assert test decoder");
                }
        }
        for (unsigned transition = 0; transition < 8; ++transition) {
            pins[4].write(transition % 2);
            settle();
            for (unsigned i = 0; i < 80; ++i)
                verify_pad(i, expected[i]);
            check(test_mode.read() == bool(transition % 2) &&
                      test_select.read() == bool((transition % 2) && pins[10].read()),
                  "test-mode entry/exit restores both independent partitions");
        }
    }
    void run() {
        if (scenario == "pad_truth")
            pad_truth();
        else if (scenario == "pad_random")
            pad_random();
        else if (scenario == "pad_metadata")
            metadata();
        else if (scenario == "pad_isolation")
            isolation();
        else if (scenario == "pad_delta")
            delta();
        else
            check(false, "unknown scenario");
        std::cout << scenario << ": " << checks << " checks, " << errors << " failures\n";
        sc_stop();
    }
};
int sc_main(int argc, char** argv) {
    const std::string scenario = argc > 1 ? argv[1] : "pad_truth";
    if (scenario == "pad_invalid_partition") {
        try {
            IoSel invalid("invalid", 2);
        } catch (const std::invalid_argument&) {
            std::cout << "invalid IOSEL partition rejected\n";
            return 0;
        }
        std::cerr << "FAIL invalid IOSEL partition accepted\n";
        return 1;
    }
    PadBench bench("pad_spec", scenario);
    std::unique_ptr<PadBench> peer;
    if (scenario == "pad_delta") {
        peer = std::make_unique<PadBench>("pad_peer", scenario, false);
        bench.peer = peer.get();
    }
    sc_start();
    return bench.errors ? 1 : 0;
}
