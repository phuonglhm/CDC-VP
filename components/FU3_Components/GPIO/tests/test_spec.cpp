#include "fu3/gpio/gpio.h"
#include <array>
#include <iostream>
#include <random>
#include <string>
#include <tlm_utils/simple_initiator_socket.h>

using namespace sc_core;
using namespace fu3::gpio;
using U32 = std::uint32_t;

// Core pins are tested directly, including the 24 unbonded bits of each bank.
struct SpecBench : sc_module {
    Gpio a{"a"}, b{"b"}, c{"c"};
    sc_clock oscillator{"oscillator", 10, SC_NS, 0.5, 5, SC_NS};
    sc_signal<bool> clock_enable{"clock_enable"}, clock{"clock"}, reset_n{"reset_n"};
    sc_vector<sc_signal<sc_dt::sc_uint<32>>> input{"input", 3}, output{"output", 3},
        direction{"direction", 3}, synced{"synced", 3};
    sc_vector<sc_signal<sc_dt::sc_uint<6>>> cfg{"cfg", 96};
    sc_vector<sc_signal<bool>> irq{"irq", 3}, wake{"wake", 3};
    sc_vector<tlm_utils::simple_initiator_socket<SpecBench>> cpu{"cpu", 3};
    sc_event job, done;
    tlm::tlm_response_status job_status = tlm::TLM_OK_RESPONSE;
    unsigned checks = 0, errors = 0;
    std::string scenario;
    SC_HAS_PROCESS(SpecBench);
    SpecBench(sc_module_name name, const std::string& which) : sc_module(name), scenario(which) {
        Gpio* banks[] = {&a, &b, &c};
        for (unsigned bank = 0; bank < 3; ++bank) {
            auto& g = *banks[bank];
            cpu[bank].bind(g.socket);
            g.clk(clock);
            g.reset_n(reset_n);
            g.gpio_in(input[bank]);
            g.gpio_out(output[bank]);
            g.gpio_dir(direction[bank]);
            g.gpio_in_sync(synced[bank]);
            g.interrupt(irq[bank]);
            g.power_event(wake[bank]);
            for (unsigned i = 0; i < 32; ++i)
                g.gpio_padcfg[i](cfg[32 * bank + i]);
        }
        SC_METHOD(gate_clock);
        sensitive << oscillator << clock_enable;
        SC_THREAD(writer);
        SC_THREAD(run);
    }
    void gate_clock() {
        clock.write(clock_enable.read() && oscillator.read());
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
    void tick(unsigned n = 1) {
        for (unsigned i = 0; i < n; ++i) {
            wait(clock.posedge_event());
            settle();
        }
    }
    void pause() {
        wait(clock.negedge_event());
        clock_enable.write(false);
        settle();
    }
    void resume() {
        clock_enable.write(true);
        settle();
    }
    void reset() {
        reset_n.write(false);
        for (unsigned bank = 0; bank < 3; ++bank)
            input[bank].write(0);
        settle();
        reset_n.write(true);
        settle();
    }
    U32 access(unsigned bank, bool write, std::uint64_t address, U32 value = 0,
               tlm::tlm_response_status expected = tlm::TLM_OK_RESPONSE, bool debug = false) {
        unsigned char bytes[4];
        for (unsigned i = 0; i < 4; ++i)
            bytes[i] = (value >> (8 * i)) & 255;
        tlm::tlm_generic_payload tx;
        tx.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
        tx.set_address(address);
        tx.set_data_ptr(bytes);
        tx.set_data_length(4);
        tx.set_streaming_width(4);
        tx.set_dmi_allowed(true);
        if (debug)
            check(cpu[bank]->transport_dbg(tx) == 4, "debug read transfers one word");
        else {
            sc_time delay = SC_ZERO_TIME;
            cpu[bank]->b_transport(tx, delay);
            check(delay == SC_ZERO_TIME, "transport delay consumed");
        }
        check(tx.get_response_status() == expected,
              "response bank " + std::to_string(bank) + " offset " + std::to_string(address));
        check(!tx.is_dmi_allowed(), "DMI disabled for MMIO");
        U32 result = 0;
        for (unsigned i = 0; i < 4; ++i)
            result |= U32(bytes[i]) << (8 * i);
        return result;
    }
    void wr(unsigned bank, std::uint64_t address, U32 value) {
        access(bank, true, address, value);
    }
    U32 rd(unsigned bank, std::uint64_t address) {
        return access(bank, false, address);
    }
    U32 peek(unsigned bank, std::uint64_t address) {
        return access(bank, false, address, 0, tlm::TLM_OK_RESPONSE, true);
    }
    void writer() {
        while (true) {
            wait(job);
            access(0, true, reg::PADOUT, 0x76543210, job_status);
            done.notify(SC_ZERO_TIME);
        }
    }
    void register_outputs() {
        for (unsigned bank = 0; bank < 3; ++bank) {
            for (unsigned pin = 0; pin < 32; ++pin)
                for (unsigned invert = 0; invert < 2; ++invert) {
                    const U32 value = (U32{1} << pin) ^ (invert ? 0xffffffffu : 0);
                    wr(bank, reg::PADOUT, value);
                    wr(bank, reg::PADDIR, ~value);
                    settle();
                    check(output[bank].read() == value && direction[bank].read() == U32(~value),
                          "all 32 output/direction ports, walking ones/zeros");
                    wr(bank, reg::PADDIR, value);
                    settle();
                    check(output[bank].read() == value && direction[bank].read() == value,
                          "direction does not gate output data");
                    for (unsigned other = 0; other < 3; ++other)
                        if (other != bank)
                            check(output[other].read() == 0 && direction[other].read() == 0,
                                  "direct port bank isolation");
                }
            wr(bank, reg::PADOUT, 0);
            wr(bank, reg::PADDIR, 0);
            std::array<unsigned, 32> expected;
            expected.fill(2);
            for (unsigned pin = 0; pin < 32; ++pin) {
                const unsigned word = pin / 4, shift = 8 * (pin % 4);
                for (unsigned value = 0; value < 64; ++value) {
                    U32 packed = 0;
                    for (unsigned j = 0; j < 4; ++j)
                        packed |= U32(expected[word * 4 + j]) << (8 * j);
                    packed = (packed & ~(U32{63} << shift)) | (U32(value) << shift);
                    wr(bank, reg::PADCFG0 + 4 * word, packed | 0xc0c0c0c0);
                    expected[pin] = value;
                    settle();
                    check(peek(bank, reg::PADCFG0 + 4 * word) == packed,
                          "six-bit packing and reserved bits ignored");
                    for (unsigned j = 0; j < 32; ++j)
                        check(cfg[bank * 32 + j].read() == expected[j],
                              "all 64 pad values; adjacent pins preserved");
                    for (unsigned other = 0; other < 3; ++other)
                        if (other != bank)
                            for (unsigned j = 0; j < 32; ++j)
                                check(cfg[other * 32 + j].read() == 2,
                                      "pad configuration bank isolation");
                }
            }
            // Restore this bank so subsequent isolation tests have a known baseline.
            for (unsigned word = 0; word < 8; ++word)
                wr(bank, reg::PADCFG0 + word * 4, reg::PADCFG_RESET);
        }
    }
    void input_stream() {
        U32 q0[3] = {}, q1[3] = {}, q2[3] = {};
        std::mt19937 rng(0x6701);
        for (unsigned cycle = 0; cycle < 256; ++cycle) {
            U32 next[3];
            for (unsigned bank = 0; bank < 3; ++bank) {
                next[bank] = cycle < 64 ? (U32{1} << (cycle % 32)) ^ (cycle >= 32 ? 0xffffffffu : 0)
                                        : U32(rng());
                input[bank].write(next[bank]);
            }
            settle();
            tick();
            for (unsigned bank = 0; bank < 3; ++bank) {
                q2[bank] = q1[bank];
                q1[bank] = q0[bank];
                q0[bank] = next[bank];
                check(synced[bank].read() == q1[bank], "back-to-back samples, second stage");
                check(peek(bank, reg::PADIN) == q2[bank], "back-to-back samples, third stage");
            }
        }
        reset();
        // A pulse entirely between rising edges is not captured by the synchronizer.
        tick();
        for (unsigned bank = 0; bank < 3; ++bank)
            input[bank].write(0xffffffff);
        settle();
        for (unsigned bank = 0; bank < 3; ++bank)
            input[bank].write(0);
        settle();
        tick(4);
        for (unsigned bank = 0; bank < 3; ++bank)
            check(peek(bank, reg::PADIN) == 0, "unsampled sub-cycle pulse not captured");
        // One sampled clock of pulse is preserved: no debounce/glitch filter.
        for (unsigned bank = 0; bank < 3; ++bank)
            input[bank].write(0xffffffff);
        settle();
        tick();
        for (unsigned bank = 0; bank < 3; ++bank)
            input[bank].write(0);
        settle();
        tick();
        for (unsigned bank = 0; bank < 3; ++bank)
            check(synced[bank].read() == 0xffffffff, "sampled one-cycle pulse reaches sync output");
        tick();
        for (unsigned bank = 0; bank < 3; ++bank)
            check(peek(bank, reg::PADIN) == 0xffffffff, "one-cycle pulse reaches PADIN");
        tick();
        for (unsigned bank = 0; bank < 3; ++bank)
            check(peek(bank, reg::PADIN) == 0, "one-cycle pulse falls after pipeline delay");
    }
    void irq_boundaries() {
        for (unsigned bank = 0; bank < 3; ++bank) {
            reset();
            wr(bank, reg::INTTYPE1, 1);
            wr(bank, reg::INTEN, 1);
            input[bank].write(1);
            settle();
            tick(2);
            check(rd(bank, reg::INTSTATUS) == 0, "empty RC read returns pre-edge status");
            settle();
            check(irq[bank].read() && peek(bank, reg::INTSTATUS) == 1,
                  "first edge coincident with empty read is latched");
            wr(bank, reg::INTSTATUS, 0xffffffff);
            check(irq[bank].read() && peek(bank, reg::INTSTATUS) == 1, "status write is not W1C");
            wr(bank, reg::INTEN, 0);
            wr(bank, reg::INTTYPE0, 0xffffffff);
            check(irq[bank].read() && peek(bank, reg::INTSTATUS) == 1,
                  "disabling/reprogramming does not clear pending status");
            check(rd(bank, reg::PADOUT) == 0 && irq[bank].read(),
                  "neighboring read does not clear status");
            // Invalid reads must not clear an IRQ or consume a clock edge.
            unsigned char bytes[4] = {};
            tlm::tlm_generic_payload tx;
            tx.set_command(tlm::TLM_READ_COMMAND);
            tx.set_address(reg::INTSTATUS);
            tx.set_data_ptr(bytes);
            tx.set_data_length(2);
            tx.set_streaming_width(2);
            sc_time delay = SC_ZERO_TIME;
            cpu[bank]->b_transport(tx, delay);
            check(tx.get_response_status() == tlm::TLM_BURST_ERROR_RESPONSE && irq[bank].read(),
                  "malformed RC read has no clear side effect");
            check(rd(bank, reg::INTSTATUS) == 1, "pending status survives configuration changes");
            tick(2);
            check(!irq[bank].read(), "disabled bank remains clear");

            reset();
            input[bank].write(1);
            settle();
            tick(4);
            wr(bank, reg::INTEN, 1);
            settle();
            check(!irq[bank].read(), "enable write uses old enable at access edge");
            wr(bank, reg::INTEN, 0);
            settle();
            check(irq[bank].read(), "disable write still observes old enable at access edge");
            check(rd(bank, reg::INTSTATUS) == 1, "disable-edge status is latched");
            tick();
            check(!irq[bank].read(), "new disabled configuration applies following edge");

            reset();
            wr(bank, reg::INTTYPE1, 1);
            input[bank].write(1);
            settle();
            tick(4);
            wr(bank, reg::INTEN, 1);
            tick(3);
            check(!irq[bank].read(), "edge occurring while disabled is not replayed on enable");
            input[bank].write(0);
            settle();
            tick(3);
            check(!irq[bank].read(), "rising mode ignores falling edge");
            input[bank].write(1);
            settle();
            tick(3);
            check(irq[bank].read() && peek(bank, reg::INTSTATUS) == 1,
                  "fresh enabled rising edge is accepted");

            reset();
            wr(bank, reg::INTTYPE0, 0xaaaaaaaa);
            wr(bank, reg::INTTYPE1, 0xcccccccc);
            input[bank].write(0xaaaaaaaa);
            settle();
            tick(4);
            wr(bank, reg::INTEN, 0xffffffff);
            input[bank].write(0x55555555);
            settle();
            tick(3);
            check(peek(bank, reg::INTSTATUS) == 0xcccccccc,
                  "mixed modes: edge conditions latch before level conditions");
            check(rd(bank, reg::INTSTATUS) == 0xcccccccc, "mixed-mode RC clears old edge vector");
            tick();
            check(peek(bank, reg::INTSTATUS) == 0x33333333,
                  "persistent high/low levels latch after edge vector is cleared");

            reset();
            input[bank].write(0xffffffff);
            settle();
            tick(4);
            wr(bank, reg::INTEN, 0x5a5aa5a5);
            tick();
            check(peek(bank, reg::INTSTATUS) == 0x5a5aa5a5,
                  "INTEN masks the captured event vector per pin");
            wr(bank, reg::INTEN, 0);
            rd(bank, reg::INTSTATUS);
            tick();
            check(!irq[bank].read(), "masked high inputs cannot reassert");

            reset();
            input[bank].write(0xffffffff);
            settle();
            tick(4);
            wr(bank, reg::INTTYPE0, 0xffffffff);
            wr(bank, reg::INTTYPE1, 0xffffffff);
            wr(bank, reg::INTEN, 0xffffffff);
            input[bank].write(0);
            settle();
            tick(3);
            check(peek(bank, reg::INTSTATUS) == 0xffffffff, "simultaneous falling edge vector");
            rd(bank, reg::INTSTATUS);
            input[bank].write(0xffffffff);
            settle();
            tick(4);
            check(!irq[bank].read(), "falling mode ignores opposite rising edges");
        }
    }
    void irq_histories() {
        // HDS 6.3/6.5, pp10-11: expected samples come directly from stimulus
        // history, not a copy of the implementation's three pipeline variables.
        // Columns are {sync1, previous_sample} = 00, 01, 10, 11.
        constexpr bool event[4][4] = {{false, true, false, true},
                                      {true, false, true, false},
                                      {false, false, true, false},
                                      {false, true, false, false}};
        constexpr U32 masks[] = {0, 0xffffffffu, 0xa5a55a5au, 0x80000001u};
        for (unsigned bank = 0; bank < 3; ++bank)
            for (unsigned pattern = 0; pattern < 64; ++pattern)
                for (unsigned mode_offset = 0; mode_offset < 4; ++mode_offset)
                    for (unsigned reads = 0; reads < 4; ++reads) {
                        reset();
                        U32 type0 = 0, type1 = 0;
                        std::array<U32, 6> history{};
                        for (unsigned pin = 0; pin < 32; ++pin) {
                            const unsigned mode = (pin + mode_offset) % 4;
                            type0 |= U32(mode & 1) << pin;
                            type1 |= U32(mode >> 1) << pin;
                            for (unsigned cycle = 0; cycle < history.size(); ++cycle)
                                if ((pattern >> ((cycle + pin) % 6)) & 1)
                                    history[cycle] |= U32{1} << pin;
                        }
                        const U32 enabled = masks[(pattern + mode_offset + reads) % 4];
                        wr(bank, reg::INTTYPE0, type0);
                        wr(bank, reg::INTTYPE1, type1);
                        wr(bank, reg::INTEN, enabled);
                        U32 status = 0;
                        for (unsigned cycle = 0; cycle < history.size(); ++cycle) {
                            input[bank].write(history[cycle]);
                            settle();
                            const U32 previous = cycle >= 3 ? history[cycle - 3] : 0;
                            const U32 sync = cycle >= 2 ? history[cycle - 2] : 0;
                            U32 conditions = 0;
                            for (unsigned pin = 0; pin < 32; ++pin) {
                                const unsigned mode = (pin + mode_offset) % 4;
                                const unsigned pair = (((sync >> pin) & 1) << 1) |
                                                      ((previous >> pin) & 1);
                                if (((enabled >> pin) & 1) && event[mode][pair])
                                    conditions |= U32{1} << pin;
                            }
                            const bool clear = reads == 1 || (reads == 2 && cycle % 2 == 1) ||
                                               (reads == 3 && cycle == history.size() - 1);
                            const std::string at = " bank " + std::to_string(bank) + " pattern " +
                                                   std::to_string(pattern) + " mode offset " +
                                                   std::to_string(mode_offset) + " reads " +
                                                   std::to_string(reads) + " cycle " +
                                                   std::to_string(cycle);
                            if (clear)
                                check(rd(bank, reg::INTSTATUS) == status,
                                      "RC returns pre-edge status" + at);
                            else
                                tick();
                            // A pending interrupt freezes its vector unless read. When
                            // empty, even a simultaneous read must retain a new event.
                            status = status ? (clear ? 0 : status) : conditions;
                            check(irq[bank].read() == bool(status), "IRQ lifecycle" + at);
                            check(peek(bank, reg::INTSTATUS) == status, "latched vector" + at);
                            check(synced[bank].read() == (cycle ? history[cycle - 1] : 0),
                                  "second-stage sample from input history" + at);
                            check(peek(bank, reg::PADIN) == sync,
                                  "third-stage sample from input history" + at);
                        }
                    }
    }
    void wake_priority() {
        for (unsigned bank = 0; bank < 3; ++bank) {
            for (unsigned high = 0; high < 32; ++high) {
                const U32 bit = U32{1} << high, lower = bit - 1;
                wr(bank, reg::POWEREVENT, bit | lower);
                pause();
                input[bank].write(lower);
                settle();
                check(!wake[bank].read(),
                      "all lower selected high pins lose to selected low highest pin");
                input[bank].write(bit);
                settle();
                check(wake[bank].read(), "highest selected pin drives wake with clock stopped");
                input[bank].write(0xffffffff);
                settle();
                check(wake[bank].read(), "unselected inputs do not affect selected high pin");
                input[bank].write(~(bit | lower));
                settle();
                check(!wake[bank].read(), "unselected high inputs cannot cause wake");
                check(!irq[bank].read(), "raw power event does not generate disabled IRQ");
                for (unsigned other = 0; other < 3; ++other)
                    if (other != bank)
                        check(!wake[other].read(), "raw wake bank isolation");
                resume();
            }
            wr(bank, reg::POWEREVENT, 0);
            input[bank].write(0xffffffff);
            settle();
            check(!wake[bank].read(), "zero select mask ignores every raw input");
        }
    }
    void clock_reset() {
        tick();
        // Accepted write cannot change output before its rising access edge.
        job.notify(SC_ZERO_TIME);
        wait(2, SC_NS);
        check(output[0].read() == 0 && peek(0, reg::PADOUT) == 0,
              "pending write has no pre-edge effect");
        wait(done);
        settle();
        check(output[0].read() == 0x76543210, "write published at rising edge");
        for (unsigned bank = 0; bank < 3; ++bank) {
            wr(bank, reg::PADOUT, 0xffffffff);
            wr(bank, reg::PADDIR, 0xffffffff);
            wr(bank, reg::INTEN, 0xffffffff);
            wr(bank, reg::POWEREVENT, 0xffffffff);
            for (unsigned word = 0; word < 8; ++word)
                wr(bank, reg::PADCFG0 + 4 * word, 0x3f3f3f3f);
            input[bank].write(0xffffffff);
        }
        settle();
        tick(4);
        pause();
        for (unsigned bank = 0; bank < 3; ++bank) {
            check(irq[bank].read() && wake[bank].read(),
                  "all banks pending before asynchronous reset");
            input[bank].write(0);
        }
        settle();
        wait(27, SC_NS);
        for (unsigned bank = 0; bank < 3; ++bank) {
            check(peek(bank, reg::PADIN) == 0xffffffff && synced[bank].read() == 0xffffffff,
                  "stopped clock holds sequential input state");
            check(irq[bank].read() && !wake[bank].read(),
                  "IRQ holds while raw wake changes without clock");
        }
        job_status = tlm::TLM_GENERIC_ERROR_RESPONSE;
        job.notify(SC_ZERO_TIME);
        settle();
        reset_n.write(false);
        wait(done);
        settle();
        for (unsigned bank = 0; bank < 3; ++bank) {
            check(output[bank].read() == 0 && direction[bank].read() == 0 &&
                      synced[bank].read() == 0,
                  "reset clears all core ports without clock");
            check(!irq[bank].read() && !wake[bank].read(),
                  "reset clears every IRQ and power mask without clock");
            for (unsigned word = 0; word < 16; ++word)
                check(peek(bank, word * 4) == (word >= 8 ? reg::PADCFG_RESET : 0),
                      "complete reset image without clock");
            for (unsigned pin = 0; pin < 32; ++pin)
                check(cfg[bank * 32 + pin].read() == 2, "all 96 reset pad fields");
        }
        for (unsigned bank = 0; bank < 3; ++bank)
            input[bank].write(0xffffffff);
        wait(oscillator.negedge_event());
        resume();
        tick(3);
        for (unsigned bank = 0; bank < 3; ++bank)
            check(synced[bank].read() == 0 && peek(bank, reg::PADIN) == 0 && !irq[bank].read(),
                  "asserted reset holds pipeline clear across clock edges and high inputs");
        pause();
        reset_n.write(true);
        settle();
        wait(17, SC_NS);
        for (unsigned bank = 0; bank < 3; ++bank)
            check(peek(bank, reg::PADIN) == 0, "reset release alone does not advance sampling");
        // Resume from an oscillator-low phase to give an unambiguous first edge.
        wait(oscillator.negedge_event());
        resume();
        tick();
        for (unsigned bank = 0; bank < 3; ++bank)
            check(synced[bank].read() == 0 && peek(bank, reg::PADIN) == 0,
                  "pipeline cleared stage1");
        tick();
        for (unsigned bank = 0; bank < 3; ++bank)
            check(synced[bank].read() == 0xffffffff && peek(bank, reg::PADIN) == 0,
                  "pipeline cleared stage2");
        tick();
        for (unsigned bank = 0; bank < 3; ++bank)
            check(peek(bank, reg::PADIN) == 0xffffffff && !irq[bank].read(),
                  "pipeline refills stage3; INTEN remains reset");
    }
    void address_decode() {
        for (unsigned bank = 0; bank < 3; ++bank) {
            for (unsigned index = 0; index < 16; ++index)
                if (index != 1 && index != 6) {
                    // Unique values keep INTEN disabled and isolate read-only side effects.
                    if (index == 3)
                        continue;
                    const U32 value =
                        index >= 8 ? (0x01020304u + index) & reg::PADCFG_MASK : 0x10203040u + index;
                    wr(bank, index * 4, value);
                    for (unsigned low = 0; low < 4; ++low)
                        for (unsigned high : {0u, 0x40u, 0xfc0u, 0xffff0000u})
                            check(rd(bank, high + index * 4 + low) == value,
                                  "all low-bit/high-bit aliases preserve word decoding");
                }
            // A normal read through an unaligned/high-address status alias must clear.
            reset();
            wr(bank, reg::INTEN, 1);
            input[bank].write(1);
            settle();
            tick(4);
            check(rd(bank, 0xffdb) == 1, "INTSTATUS alias returns pending state");
            settle();
            check(!irq[bank].read() && peek(bank, reg::INTSTATUS) == 0,
                  "INTSTATUS alias clears on accepted read");
        }
    }
    void address_writes() {
        // HDS 7.1/8.2, pp12-14: aliases apply equally to reads and writes.
        // Include 64-bit TLM addresses; these are VP aliases, not physical
        // APB address-width or interconnect-range requirements.
        constexpr std::uint64_t upper[] = {0, 0x40, 0xffff0000, 0x100000000ull,
                                           0xffffffffffffffc0ull};
        for (unsigned bank = 0; bank < 3; ++bank)
            for (unsigned index = 0; index < 16; ++index)
                for (auto high : upper)
                    for (unsigned low = 0; low < 4; ++low) {
                        reset();
                        input[bank].write(0x9182a5c3);
                        settle();
                        tick(3);
                        if (index == 6) {
                            wr(bank, reg::INTEN, 0xffffffff);
                            tick();
                            check(irq[bank].read(), "pending status before aliased RO write");
                        }
                        std::array<U32, 16> expected;
                        for (unsigned word = 0; word < expected.size(); ++word)
                            expected[word] = peek(bank, word * 4);
                        const U32 value = 0xc5a37ef0u ^ (U32{0x01010101} * index) ^ low;
                        wr(bank, high | (index * 4) | low, value);
                        if (index != 1 && index != 6)
                            expected[index] = value & (index >= 8 ? 0x3f3f3f3fu : 0xffffffffu);
                        const std::string at = " bank " + std::to_string(bank) + " index " +
                                               std::to_string(index) + " address " +
                                               std::to_string(high | (index * 4) | low);
                        for (unsigned word = 0; word < expected.size(); ++word)
                            check(peek(bank, word * 4) == expected[word],
                                  "aliased write changes only its writable word" + at);
                        check(output[bank].read() == expected[2] &&
                                  direction[bank].read() == expected[0],
                              "aliased writes publish output and direction" + at);
                        for (unsigned pin = 0; pin < 32; ++pin)
                            check(cfg[bank * 32 + pin].read() ==
                                      ((expected[8 + pin / 4] >> (8 * (pin % 4))) & 63),
                                  "aliased PADCFG write preserves six-bit outputs" + at);
                        check(irq[bank].read() == (index == 6),
                              "aliased status write cannot clear IRQ" + at);
                    }
    }
    void run() {
        clock_enable.write(true);
        reset();
        if (scenario == "register_outputs")
            register_outputs();
        else if (scenario == "input_stream")
            input_stream();
        else if (scenario == "irq_boundaries")
            irq_boundaries();
        else if (scenario == "irq_histories")
            irq_histories();
        else if (scenario == "wake_priority")
            wake_priority();
        else if (scenario == "clock_reset")
            clock_reset();
        else if (scenario == "address_decode")
            address_decode();
        else if (scenario == "address_writes")
            address_writes();
        else
            check(false, "unknown scenario");
        std::cout << scenario << ": " << checks << " checks, " << errors << " failures\n";
        sc_stop();
    }
};
int sc_main(int argc, char** argv) {
    SpecBench bench("spec", argc > 1 ? argv[1] : "irq_boundaries");
    sc_start();
    return bench.errors ? 1 : 0;
}
