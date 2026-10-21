#include "fu3/gpio/apb.h"
#include <array>
#include <iostream>
#include <random>
#include <string>

using namespace sc_core;
using U32 = std::uint32_t;

struct Bus {
    sc_signal<bool> clk{"clk"}, reset_n{"reset_n"}, sel{"sel"}, enable{"enable"}, write{"write"};
    sc_signal<sc_dt::sc_uint<32>> data{"data"}, input{"input"};
};

template <unsigned Width> struct Lane : sc_module {
    fu3::gpio::ApbGpio<Width> dut{"dut"};
    sc_signal<sc_dt::sc_biguint<Width>> address{"address"};
    sc_signal<sc_dt::sc_uint<32>> read{"read"}, output{"output"}, direction{"direction"},
        sync{"sync"};
    sc_signal<bool> ready{"ready"}, error{"error"}, irq{"irq"}, wake{"wake"};
    sc_vector<sc_signal<sc_dt::sc_uint<6>>> config{"config", 32};
    Lane(sc_module_name name, Bus& bus) : sc_module(name) {
        dut.HCLK(bus.clk);
        dut.HRESETn(bus.reset_n);
        dut.PSEL(bus.sel);
        dut.PENABLE(bus.enable);
        dut.PWRITE(bus.write);
        dut.PADDR(address);
        dut.PWDATA(bus.data);
        dut.PRDATA(read);
        dut.PREADY(ready);
        dut.PSLVERR(error);
        dut.gpio_in(bus.input);
        dut.gpio_out(output);
        dut.gpio_dir(direction);
        dut.gpio_in_sync(sync);
        dut.interrupt(irq);
        dut.power_event(wake);
        for (unsigned i = 0; i < 32; ++i)
            dut.gpio_padcfg[i](config[i]);
    }
};

struct ApbBench : sc_module {
    Bus bus;
    Lane<6> narrow{"narrow", bus};
    Lane<12> normal{"normal", bus};
    Lane<128> wide{"wide", bus};
    std::string scenario;
    unsigned checks = 0, errors = 0;
    SC_HAS_PROCESS(ApbBench);
    ApbBench(sc_module_name name, const std::string& which) : sc_module(name), scenario(which) {
        SC_THREAD(run);
    }
    void check(bool ok, const std::string& message) {
        ++checks;
        if (!ok) {
            ++errors;
            if (errors <= 30)
                std::cerr << "FAIL APB " << scenario << " @ " << sc_time_stamp() << ": "
                          << message << '\n';
        }
    }
    void settle() {
        wait(1, SC_NS);
    }
    template <unsigned Width> void compare(const Lane<Width>& lane) {
        check(lane.ready.read() && !lane.error.read(), "PREADY=1 and PSLVERR=0, including reset");
        check(lane.read.read() == normal.read.read(), "address-width aliases preserve PRDATA");
        check(lane.output.read() == normal.output.read() &&
                  lane.direction.read() == normal.direction.read() &&
                  lane.sync.read() == normal.sync.read() && lane.irq.read() == normal.irq.read() &&
                  lane.wake.read() == normal.wake.read(),
              "6/12/128-bit interfaces have identical digital behavior");
        for (unsigned i = 0; i < 32; ++i)
            check(lane.config[i].read() == normal.config[i].read(), "address-width pad outputs");
    }
    void compare() {
        compare(narrow);
        check(normal.ready.read() && !normal.error.read(), "default-width APB response constants");
        compare(wide);
    }
    void address(U32 value) {
        narrow.address.write(value);
        normal.address.write(0xa00u | value);
        sc_dt::sc_biguint<128> extended = 1;
        extended <<= 127;
        extended[64] = true;
        extended |= value;
        wide.address.write(extended);
    }
    void edge(unsigned n = 1) {
        for (unsigned i = 0; i < n; ++i) {
            bus.clk.write(true);
            settle();
            compare();
            bus.clk.write(false);
            settle();
        }
    }
    void idle(unsigned n = 1) {
        bus.sel.write(false);
        bus.enable.write(false);
        settle();
        edge(n);
    }
    void reset() {
        bus.reset_n.write(false);
        bus.sel.write(false);
        bus.enable.write(false);
        bus.write.write(false);
        bus.input.write(0);
        address(0);
        settle();
        compare();
        bus.reset_n.write(true);
        settle();
    }
    U32 peek(unsigned offset) {
        bus.sel.write(false);
        bus.enable.write(false);
        address(offset);
        settle();
        compare();
        return normal.read.read().to_uint();
    }
    U32 access(bool write, unsigned offset, U32 value = 0) {
        bus.sel.write(true);
        bus.enable.write(false);
        bus.write.write(write);
        bus.data.write(value);
        address(offset);
        settle();
        edge(); // Setup phase must not perform a register access.
        bus.enable.write(true);
        settle();
        const U32 result = normal.read.read().to_uint();
        edge(); // Access phase; read samples pre-edge PRDATA.
        bus.sel.write(false);
        bus.enable.write(false);
        settle();
        return result;
    }
    void wr(unsigned offset, U32 value) {
        access(true, offset, value);
    }
    U32 rd(unsigned offset) {
        return access(false, offset);
    }
    void registers() {
        for (unsigned word = 0; word < 16; ++word) {
            reset();
            for (unsigned i = 0; i < 16; ++i)
                check(peek(i * 4) == (i >= 8 ? 0x02020202u : 0), "complete HDS reset map");
            wr(word * 4 + 3, 0xffffffff);
            for (unsigned i = 0; i < 16; ++i) {
                U32 expected = i >= 8 ? 0x02020202u : 0;
                if (i == word && i != 1 && i != 6)
                    expected = i >= 8 ? 0x3f3f3f3fu : 0xffffffffu;
                check(peek(i * 4) == expected, "all word writes, RO behavior and reserved bits");
            }
        }
        reset();
        bus.input.write(0x91a5c382);
        idle(3);
        for (unsigned bit = 0; bit < 32; ++bit) {
            wr(0x08, U32{1} << bit);
            wr(0x00, ~(U32{1} << bit));
            check(normal.output.read() == (U32{1} << bit), "walking output independent of direction");
            check(normal.direction.read() == U32(~(U32{1} << bit)), "walking direction output");
            check(peek(0x04) == 0x91a5c382, "sampling independent of direction and output");
        }
    }
    void phases() {
        wr(0x08, 0x12345678);
        for (unsigned controls = 0; controls < 4; ++controls) {
            bus.sel.write(controls & 1);
            bus.enable.write(controls & 2);
            bus.write.write(true);
            bus.data.write(0xabcdef01);
            address(0x08);
            settle();
            check(normal.output.read() == 0x12345678, "no combinational write side effect");
            edge();
            check(normal.output.read() == (controls == 3 ? 0xabcdef01u : 0x12345678u),
                  "only selected access phase writes");
        }
        bus.sel.write(false);
        bus.enable.write(false);
        settle();
        wr(0x00, 0x76543210);
        for (unsigned controls = 0; controls < 4; ++controls) {
            bus.sel.write(controls & 1);
            bus.enable.write(controls & 2);
            address(0x08);
            settle();
            check(normal.read.read() == 0xabcdef01, "PRDATA independent of select/enable");
            address(0x00);
            settle();
            check(normal.read.read() == 0x76543210, "PRDATA follows address without a clock");
        }
        // Abort a setup phase, then hold access over successive edges.
        bus.sel.write(true);
        bus.enable.write(false);
        bus.write.write(true);
        bus.data.write(99);
        address(0x08);
        settle();
        edge();
        idle();
        check(normal.output.read() == 0xabcdef01, "aborted setup does not write");
        bus.sel.write(true);
        bus.enable.write(true);
        bus.data.write(1);
        settle();
        edge();
        check(normal.output.read() == 1, "first held access edge writes");
        bus.data.write(2);
        settle();
        edge();
        check(normal.output.read() == 2, "next held access edge also writes without extra latency");
        bus.enable.write(false);
        address(0x00);
        bus.data.write(3);
        settle();
        edge();
        check(normal.direction.read() == 0x76543210, "back-to-back setup has no write effect");
        bus.enable.write(true);
        settle();
        edge();
        check(normal.direction.read() == 3 && normal.output.read() == 2,
              "back-to-back access with PSEL held changes only the addressed word");
    }
    void interrupts() {
        for (unsigned mode = 0; mode < 4; ++mode)
            for (unsigned pin = 0; pin < 32; ++pin) {
                reset();
                const U32 bit = U32{1} << pin;
                const bool baseline = mode == 1 || mode == 3;
                bus.input.write(baseline ? bit : 0);
                idle(4);
                wr(0x10, mode & 1 ? bit : 0);
                wr(0x14, mode & 2 ? bit : 0);
                wr(0x0c, bit);
                check(!normal.irq.read(), "inactive IRQ baseline");
                bus.input.write(baseline ? 0 : bit);
                idle(2);
                check(!normal.irq.read(), "no premature IRQ");
                idle();
                check(normal.irq.read() == (mode >= 2), "edge IRQ at stage three");
                if (mode < 2)
                    idle();
                check(peek(0x18) == bit && normal.irq.read(), "IRQ and full pin status");
                wr(0x18, 0xffffffff);
                check(peek(0x18) == bit && normal.irq.read(), "status write is ignored, not W1C");
                bus.sel.write(false);
                bus.enable.write(true);
                bus.write.write(false);
                settle();
                edge();
                check(normal.irq.read(), "unselected read cannot clear status");
                bus.sel.write(true);
                bus.enable.write(false);
                settle();
                edge();
                check(normal.irq.read(), "setup read cannot clear status");
                check(rd(0x18) == bit, "selected read returns latched pre-edge status");
                check(!normal.irq.read() && peek(0x18) == 0, "selected read clears IRQ/status");
                idle();
                check(normal.irq.read() == (mode < 2), "level reasserts; edge is not replayed");
            }
        reset();
        wr(0x14, 3);
        wr(0x0c, 3);
        bus.input.write(1);
        idle();
        check(rd(0x18) == 0 && normal.irq.read() && peek(0x18) == 1,
              "new event at empty RC read is latched");
        bus.input.write(3);
        idle();
        check(rd(0x18) == 1 && !normal.irq.read(), "pending clear wins over concurrent new edge");
        idle(3);
        check(peek(0x18) == 0, "concurrent edge is lost, not queued");
    }
    void sampling_wake() {
        bus.input.write(0x9182a5c3);
        idle();
        check(normal.sync.read() == 0 && peek(0x04) == 0, "first sample stage only");
        idle();
        check(normal.sync.read() == 0x9182a5c3 && peek(0x04) == 0, "second sample output");
        idle();
        check(peek(0x04) == 0x9182a5c3, "PADIN at third sample stage");
        for (unsigned high = 0; high < 32; ++high) {
            const U32 bit = U32{1} << high, lower = bit - 1;
            wr(0x1c, bit | lower);
            const U32 held = peek(0x04);
            bus.input.write(lower);
            settle();
            check(!normal.wake.read(), "higher selected low pin overrides lower highs without clock");
            bus.input.write(bit);
            settle();
            check(normal.wake.read(), "highest selected high drives asynchronous wake");
            check(peek(0x04) == held, "raw wake does not advance PADIN with clock stopped");
        }
        wr(0x1c, 0);
        bus.input.write(0xffffffff);
        settle();
        check(!normal.wake.read(), "no wake selection gives zero");
    }
    void reset_access() {
        wr(0x08, 0xffffffff);
        wr(0x00, 0xffffffff);
        for (unsigned word = 8; word < 16; ++word)
            wr(word * 4, 0xffffffff);
        wr(0x0c, 0xffffffff);
        wr(0x1c, 0xffffffff);
        bus.input.write(0xffffffff);
        idle(4);
        check(normal.irq.read() && normal.wake.read(), "dirty IRQ/wake before reset");
        bus.sel.write(true);
        bus.enable.write(true);
        bus.write.write(true);
        bus.data.write(0xffffffff);
        address(0x08);
        bus.reset_n.write(false);
        settle();
        check(normal.output.read() == 0 && normal.direction.read() == 0 &&
                  normal.sync.read() == 0 && !normal.irq.read() && !normal.wake.read(),
              "asynchronous reset clears ports without any clock");
        edge(3);
        check(normal.output.read() == 0, "held reset takes priority over selected writes");
        for (unsigned word = 0; word < 16; ++word)
            check(peek(word * 4) == (word >= 8 ? 0x02020202u : 0), "all registers reset");
        for (unsigned pin = 0; pin < 32; ++pin)
            check(normal.config[pin].read() == 2, "all pad configuration outputs reset");
        bus.reset_n.write(true);
        settle();
        check(peek(0x04) == 0, "release alone does not sample high raw input");
        idle();
        check(normal.sync.read() == 0 && peek(0x04) == 0, "reset pipeline stage one");
        idle();
        check(normal.sync.read() == 0xffffffff && peek(0x04) == 0, "reset pipeline stage two");
        idle();
        check(peek(0x04) == 0xffffffff && !normal.irq.read(), "stage three; INTEN remains reset");
    }
    void random() {
        constexpr bool event[4][4] = {{false, true, false, true}, {true, false, true, false},
                                      {false, false, true, false}, {false, true, false, false}};
        for (unsigned seed : {0x314159u, 0x6701u, 20261021u}) {
            reset();
            std::array<U32, 16> expected{};
            for (unsigned i = 8; i < 16; ++i)
                expected[i] = 0x02020202;
            std::array<U32, 512> history{};
            std::mt19937 rng(seed);
            for (unsigned cycle = 0; cycle < history.size(); ++cycle) {
                history[cycle] = rng();
                const unsigned index = rng() % 16, controls = rng() % 8;
                const bool selected = (controls & 3) == 3, write = controls & 4;
                const U32 value = rng();
                bus.sel.write(controls & 1);
                bus.enable.write(controls & 2);
                bus.write.write(write);
                bus.data.write(value);
                bus.input.write(history[cycle]);
                address((rng() & 0xffffffc3u) | (index << 2));
                settle();
                check(normal.read.read() == expected[index], "random combinational pre-edge PRDATA");
                U32 active = 0;
                const U32 synced = cycle >= 2 ? history[cycle - 2] : 0;
                const U32 previous = cycle >= 3 ? history[cycle - 3] : 0;
                for (unsigned pin = 0; pin < 32; ++pin) {
                    const unsigned mode = ((expected[5] >> pin) & 1) * 2 +
                                          ((expected[4] >> pin) & 1);
                    const unsigned pair = ((synced >> pin) & 1) * 2 + ((previous >> pin) & 1);
                    if (((expected[3] >> pin) & 1) && event[mode][pair])
                        active |= U32{1} << pin;
                }
                if (expected[6]) {
                    if (selected && !write && index == 6)
                        expected[6] = 0;
                } else
                    expected[6] = active;
                if (selected && write && index != 1 && index != 6)
                    expected[index] = value & (index >= 8 ? 0x3f3f3f3fu : 0xffffffffu);
                expected[1] = synced;
                edge();
                check(normal.read.read() == expected[index], "random post-edge PRDATA");
                check(normal.output.read() == expected[2] && normal.direction.read() == expected[0],
                      "random data/direction outputs");
                check(normal.sync.read() == (cycle ? history[cycle - 1] : 0), "random sync history");
                check(normal.irq.read() == bool(expected[6]), "random frozen/cleared status");
                bool wake = false;
                for (int pin = 31; pin >= 0; --pin)
                    if (expected[7] & (U32{1} << pin)) {
                        wake = (history[cycle] >> pin) & 1;
                        break;
                    }
                check(normal.wake.read() == wake, "random highest-selected wake");
                for (unsigned pin = 0; pin < 32; ++pin)
                    check(normal.config[pin].read() ==
                              ((expected[8 + pin / 4] >> (8 * (pin % 4))) & 63),
                          "random six-bit pad outputs");
            }
        }
    }
    void run() {
        reset();
        if (scenario == "registers")
            registers();
        else if (scenario == "phases")
            phases();
        else if (scenario == "interrupts")
            interrupts();
        else if (scenario == "sampling_wake")
            sampling_wake();
        else if (scenario == "reset")
            reset_access();
        else if (scenario == "random")
            random();
        else
            check(false, "unknown scenario");
        std::cout << "apb_" << scenario << ": " << checks << " checks, " << errors << " failures\n";
        sc_stop();
    }
};
int sc_main(int argc, char** argv) {
    ApbBench bench("apb", argc > 1 ? argv[1] : "phases");
    sc_start();
    return bench.errors ? 1 : 0;
}
