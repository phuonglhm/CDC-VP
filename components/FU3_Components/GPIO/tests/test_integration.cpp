#include "fu3/gpio/subsystem.h"
#include <array>
#include <iostream>
#include <random>
#include <string>
#include <tlm_utils/simple_initiator_socket.h>

using namespace fu3::gpio;
using namespace sc_core;
using Word = sc_dt::sc_uint<32>;

struct Bench : sc_module {
    Subsystem dut{"dut"};
    sc_clock clock{"clock", 10, SC_NS, 0.5, 5, SC_NS};
    sc_signal<bool> reset_n{"reset_n"}, wake{"wake"}, test_mode{"test_mode"},
        test_select{"test_select"};
    sc_vector<sc_signal<bool>> irqs{"irqs", 3}, wakes{"wakes", 3};
    sc_vector<sc_signal<sc_dt::sc_uint<2>>> select{"select", 80};
    sc_vector<sc_signal<bool>> pads{"pads", 80}, data{"data", 80}, oe{"oe", 80};
    sc_vector<tlm_utils::simple_initiator_socket<Bench>> cpu{"cpu", 3};
    std::string scenario;
    unsigned checks = 0, errors = 0;
    sc_event jobs[2], done[2];
    tlm::tlm_response_status worker_status[2] = {tlm::TLM_OK_RESPONSE, tlm::TLM_OK_RESPONSE};
    unsigned worker_bank[2] = {0, 0};
    SC_HAS_PROCESS(Bench);
    Bench(sc_module_name name, const std::string& which) : sc_module(name), scenario(which) {
        dut.clk(clock);
        dut.reset_n(reset_n);
        dut.wake(wake);
        dut.io_pad.test_mode(test_mode);
        dut.io_pad.test_select(test_select);
        cpu[0].bind(dut.gpioa.socket);
        cpu[1].bind(dut.gpiob.socket);
        cpu[2].bind(dut.gpioc.socket);
        for (unsigned b = 0; b < 3; ++b) {
            dut.irq[b](irqs[b]);
            dut.wake_bank[b](wakes[b]);
        }
        for (unsigned i = 0; i < 80; ++i) {
            dut.io_pad.select[i](select[i]);
            dut.io_pad.pad_in[i](pads[i]);
            dut.io_pad.pad_data[i](data[i]);
            dut.io_pad.requested_oe[i](oe[i]);
        }
        SC_THREAD(run);
        SC_THREAD(worker0);
        SC_THREAD(worker1);
    }
    void check(bool ok, const std::string& message) {
        ++checks;
        if (!ok) {
            ++errors;
            if (errors < 30)
                std::cerr << "FAIL " << scenario << " @ " << sc_time_stamp() << ": " << message
                          << '\n';
        }
    }
    void tick(unsigned n = 1) {
        for (unsigned i = 0; i < n; ++i) {
            wait(clock.posedge_event());
            wait(1, SC_NS);
        }
    }
    void settle() {
        wait(1, SC_NS);
    }
    void gpio_mux() {
        for (unsigned p = 1; p <= 80; ++p) {
            const auto info = pin_info(p);
            if (info.gpio_bank >= 0)
                select[p - 1].write(info.gpio_select);
        }
    }
    void raw(unsigned b, std::uint32_t value) {
        dut.unbonded_in[b].write(value);
        for (unsigned p = 1; p <= 80; ++p) {
            const auto info = pin_info(p);
            if (info.gpio_bank == int(b))
                pads[p - 1].write((value >> info.gpio_bit) & 1);
        }
    }
    void reset() {
        reset_n.write(false);
        for (unsigned i = 0; i < 80; ++i) {
            pads[i].write(false);
            select[i].write(0);
        }
        for (unsigned b = 0; b < 3; ++b)
            dut.unbonded_in[b].write(0);
        gpio_mux();
        settle();
        reset_n.write(true);
        settle();
    }
    std::uint32_t access(unsigned bank, bool write, std::uint64_t address, std::uint32_t value = 0,
                         tlm::tlm_response_status expected = tlm::TLM_OK_RESPONSE,
                         sc_time delay = SC_ZERO_TIME) {
        unsigned char bytes[4];
        for (unsigned i = 0; i < 4; ++i)
            bytes[i] = (value >> (8 * i)) & 255;
        tlm::tlm_generic_payload tx;
        tx.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
        tx.set_address(address);
        tx.set_data_ptr(bytes);
        tx.set_data_length(4);
        tx.set_streaming_width(4);
        cpu[bank]->b_transport(tx, delay);
        check(tx.get_response_status() == expected,
              "transport response at " + std::to_string(address));
        check(!tx.is_dmi_allowed(), "no DMI");
        check(delay == SC_ZERO_TIME, "incoming delay consumed");
        std::uint32_t result = 0;
        for (unsigned i = 0; i < 4; ++i)
            result |= std::uint32_t(bytes[i]) << (8 * i);
        return result;
    }
    void wr(unsigned b, unsigned a, std::uint32_t v) {
        access(b, true, a, v);
    }
    std::uint32_t rd(unsigned b, unsigned a) {
        return access(b, false, a);
    }
    std::uint32_t peek(unsigned b, unsigned a) {
        unsigned char bytes[4] = {};
        tlm::tlm_generic_payload tx;
        tx.set_command(tlm::TLM_READ_COMMAND);
        tx.set_address(a);
        tx.set_data_ptr(bytes);
        tx.set_data_length(4);
        tx.set_streaming_width(4);
        check(cpu[b]->transport_dbg(tx) == 4 && tx.is_response_ok(), "debug read");
        std::uint32_t v = 0;
        for (unsigned i = 0; i < 4; ++i)
            v |= std::uint32_t(bytes[i]) << (8 * i);
        return v;
    }
    void registers() {
        for (unsigned b = 0; b < 3; ++b) {
            for (unsigned i = 0; i < 16; ++i)
                check(rd(b, i * 4) == (i >= 8 ? reg::PADCFG_RESET : 0), "reset register");
            for (unsigned i = 0; i < 16; ++i) {
                wr(b, i * 4, 0xffffffff);
                if (i == 1 || i == 6)
                    check(peek(b, i * 4) == 0, "RO/status write ignored");
                else
                    check(peek(b, i * 4) == (i >= 8 ? reg::PADCFG_MASK : 0xffffffff), "write mask");
                wr(b, i * 4, 0);
            }
            for (unsigned pin = 0; pin < 32; ++pin) {
                const auto bit = std::uint32_t{1} << pin;
                wr(b, reg::PADOUT, bit);
                check(rd(b, reg::PADOUT) == bit, "walking output bit");
                wr(b, reg::PADDIR, bit);
                check(rd(b, reg::PADDIR) == bit, "walking direction bit");
                wr(b, reg::PADCFG0 + 4 * (pin / 4), std::uint32_t{63} << (8 * (pin % 4)));
                check(dut.pad_config[b * 32 + pin].read() == 63, "per-pin pad configuration");
            }
            wr(b, reg::PADDIR, 0);
            wr(b, reg::PADOUT, 0xa5a5a5a5);
            check(peek(b, reg::PADOUT) == 0xa5a5a5a5, "output independent of direction");
        }
        check(peek(0, reg::PADOUT) == 0xa5a5a5a5, "bank isolation A");
        wr(1, reg::PADOUT, 123);
        check(peek(0, reg::PADOUT) == 0xa5a5a5a5 && peek(2, reg::PADOUT) == 0xa5a5a5a5,
              "bank isolation B/C");
    }
    void sampling() {
        for (unsigned b = 0; b < 3; ++b) {
            reset();
            const auto value = 0x9182a5c3u;
            raw(b, value);
            settle();
            tick();
            check(dut.input_sync[b].read() == 0 && peek(b, reg::PADIN) == 0,
                  "first synchronization stage");
            tick();
            check(dut.input_sync[b].read() == value && peek(b, reg::PADIN) == 0,
                  "second synchronization stage");
            tick();
            check(peek(b, reg::PADIN) == value, "third input sample stage");
            wr(b, reg::PADOUT, ~value);
            wr(b, reg::PADDIR, 0xffffffff);
            check(peek(b, reg::PADIN) == value, "input independent of output/direction");
            raw(b, 0);
            settle();
            tick(3);
            check(peek(b, reg::PADIN) == 0, "falling input pipeline");
        }
    }
    void interrupt_matrix(unsigned bank) {
        for (unsigned mode = 0; mode < 4; ++mode)
            for (unsigned pin = 0; pin < 32; ++pin) {
                reset();
                const auto bit = std::uint32_t{1} << pin;
                const bool baseline = mode == 1 || mode == 3;
                raw(bank, baseline ? bit : 0);
                settle();
                tick(4);
                wr(bank, reg::INTTYPE0, (mode & 1) ? bit : 0);
                wr(bank, reg::INTTYPE1, (mode & 2) ? bit : 0);
                wr(bank, reg::INTEN, bit);
                tick();
                check(!irqs[bank].read(), "inactive baseline mode " + std::to_string(mode));
                raw(bank, baseline ? 0 : bit);
                settle();
                tick(2);
                check(!irqs[bank].read(), "no IRQ before synchronized event");
                tick();
                check(irqs[bank].read() == (mode >= 2), "edge IRQ on third edge; level on fourth");
                if (mode < 2)
                    tick();
                check(irqs[bank].read() && peek(bank, reg::INTSTATUS) == bit,
                      "all pins / all four interrupt modes");
                check(peek(bank, reg::INTSTATUS) == bit && irqs[bank].read(),
                      "debug status read has no clear effect");
                check(rd(bank, reg::INTSTATUS) == bit, "RC returns original status");
                settle();
                check(!irqs[bank].read() && peek(bank, reg::INTSTATUS) == 0,
                      "RC clears status and IRQ");
                tick();
                check(irqs[bank].read() == (mode < 2), "level reasserts; edge does not repeat");
            }
    }
    void interrupts() {
        for (unsigned bank = 0; bank < 3; ++bank)
            interrupt_matrix(bank);
        reset();
        wr(0, reg::INTEN, 0xffffffff);
        raw(0, 0x81240008);
        settle();
        tick(4);
        check(peek(0, reg::INTSTATUS) == 0x81240008, "complete simultaneous event vector");
        raw(0, 0xffffffff);
        settle();
        tick(5);
        check(peek(0, reg::INTSTATUS) == 0x81240008, "later events do not replace pending status");
        // A new edge coincident with clear is lost, as in RTL.
        reset();
        wr(0, reg::INTTYPE1, 3);
        wr(0, reg::INTEN, 3);
        raw(0, 1);
        settle();
        tick(3);
        check(peek(0, reg::INTSTATUS) == 1, "first edge latched");
        raw(0, 3);
        settle();
        tick(2);
        check(rd(0, reg::INTSTATUS) == 1, "clear coincides with new edge");
        tick(3);
        check(!irqs[0].read() && peek(0, reg::INTSTATUS) == 0, "coincident event not queued");
        reset();
        raw(2, 0xffffffff);
        settle();
        tick(5);
        check(!irqs[2].read(), "INTEN masks all events");
    }
    void wake_checks() {
        for (unsigned b = 0; b < 3; ++b)
            for (unsigned pin = 0; pin < 32; ++pin) {
                const auto bit = std::uint32_t{1} << pin;
                raw(b, 0);
                wr(b, reg::POWEREVENT, bit);
                raw(b, bit);
                settle();
                check(wakes[b].read() && wake.read(), "asynchronous per-pin wake");
                raw(b, 0);
                settle();
                check(!wakes[b].read(), "wake not latched");
                wr(b, reg::POWEREVENT, 0);
            }
        wr(0, reg::POWEREVENT, 0x80000001);
        raw(0, 1);
        settle();
        check(!wake.read(), "highest selected pin wins, not OR");
        raw(0, 0x80000000);
        settle();
        check(wake.read(), "pin31 priority");
        wr(1, reg::POWEREVENT, 0x100);
        raw(1, 0x100);
        raw(0, 0);
        settle();
        check(wake.read() && !wakes[0].read() && wakes[1].read(), "board OR of bank events");
        check(!irqs[0].read() && !irqs[1].read(), "wake independent of IRQ enable");
    }
    void protocol() {
        wr(0, reg::PADOUT, 0x12345678);
        for (auto a : {0x8ull, 0x9ull, 0x3c000aull, 0x10000000bull})
            check(rd(0, a) == 0x12345678, "PADDR[5:2] alias");
        unsigned char bytes[8] = {}, enables[4] = {255, 255, 255, 255};
        for (unsigned kind = 0; kind < 6; ++kind) {
            tlm::tlm_generic_payload tx;
            tx.set_command(tlm::TLM_READ_COMMAND);
            tx.set_address(reg::INTSTATUS);
            tx.set_data_ptr(bytes);
            tx.set_data_length(4);
            tx.set_streaming_width(4);
            auto expected = tlm::TLM_BURST_ERROR_RESPONSE;
            if (kind == 0) {
                tx.set_command(tlm::TLM_IGNORE_COMMAND);
                expected = tlm::TLM_COMMAND_ERROR_RESPONSE;
            }
            if (kind == 1) {
                tx.set_byte_enable_ptr(enables);
                expected = tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE;
            }
            if (kind == 2)
                tx.set_data_length(0);
            if (kind == 3)
                tx.set_data_length(8);
            if (kind == 4)
                tx.set_streaming_width(2);
            if (kind == 5) {
                tx.set_data_ptr(nullptr);
                expected = tlm::TLM_GENERIC_ERROR_RESPONSE;
            }
            auto delay = SC_ZERO_TIME;
            const auto before = sc_time_stamp();
            cpu[0]->b_transport(tx, delay);
            check(tx.get_response_status() == expected && sc_time_stamp() == before,
                  "invalid TLM rejected without timing/state effects");
            check(cpu[0]->transport_dbg(tx) == 0, "invalid debug transaction");
        }
        tlm::tlm_generic_payload tx;
        tx.set_command(tlm::TLM_WRITE_COMMAND);
        tx.set_data_ptr(bytes);
        tx.set_data_length(4);
        tx.set_streaming_width(4);
        tx.set_address(reg::PADOUT);
        check(cpu[0]->transport_dbg(tx) == 0 &&
                  tx.get_response_status() == tlm::TLM_COMMAND_ERROR_RESPONSE,
              "debug writes rejected");
        check(peek(0, reg::PADOUT) == 0x12345678, "debug write did not modify state");
        const auto before = sc_time_stamp();
        access(0, true, reg::PADOUT, 0x89abcdef, tlm::TLM_OK_RESPONSE, sc_time(23, SC_NS));
        check(sc_time_stamp() - before >= sc_time(23, SC_NS) && peek(0, reg::PADOUT) == 0x89abcdef,
              "incoming annotated delay respected");
    }
    void reset_checks() {
        wr(0, reg::PADDIR, 0xffffffff);
        wr(0, reg::PADOUT, 0xffffffff);
        wr(0, reg::PADCFG0, 0);
        wr(0, reg::INTEN, 1);
        raw(0, 1);
        settle();
        tick(4);
        wr(0, reg::POWEREVENT, 1);
        check(irqs[0].read(), "pending before reset");
        reset_n.write(false);
        settle();
        check(!irqs[0].read() && !wake.read(), "asynchronous reset clears IRQ/wake");
        for (unsigned b = 0; b < 3; ++b)
            for (unsigned i = 0; i < 16; ++i)
                check(peek(b, i * 4) == (i >= 8 ? reg::PADCFG_RESET : 0),
                      "asynchronous reset restores complete register bank");
        access(0, true, reg::PADOUT, 99, tlm::TLM_GENERIC_ERROR_RESPONSE);
        check(peek(0, reg::PADOUT) == 0, "no writes during reset");
        reset_n.write(true);
        settle();
        tick(3);
        check(peek(0, reg::PADIN) == 1, "input resamples after reset release");
    }
    void worker(unsigned id) {
        while (true) {
            wait(jobs[id]);
            access(worker_bank[id], true, id ? reg::PADDIR : reg::PADOUT, 0x13579bdf + id,
                   worker_status[id]);
            done[id].notify(SC_ZERO_TIME);
        }
    }
    void worker0() {
        worker(0);
    }
    void worker1() {
        worker(1);
    }
    void concurrency() {
        for (unsigned n = 0; n < 16; ++n) {
            reset();
            const auto before = sc_time_stamp();
            jobs[0].notify(SC_ZERO_TIME);
            jobs[1].notify(SC_ZERO_TIME);
            wait(done[0] & done[1]);
            settle();
            check(peek(0, reg::PADOUT) == 0x13579bdf && peek(0, reg::PADDIR) == 0x13579be0,
                  "concurrent callers serialized without lost writes");
            check(sc_time_stamp() - before >= sc_time(10, SC_NS),
                  "separate clock edges for concurrent APB accesses");
        }
        // Reset aborts an access waiting for its clock edge and wakes the caller.
        tick();
        worker_status[0] = tlm::TLM_GENERIC_ERROR_RESPONSE;
        jobs[0].notify(SC_ZERO_TIME);
        wait(1, SC_NS);
        reset_n.write(false);
        wait(done[0]);
        settle();
        check(peek(0, reg::PADOUT) == 0 && !irqs[0].read(),
              "pending access canceled by asynchronous reset");
        reset_n.write(true);
        settle();
        worker_status[0] = tlm::TLM_OK_RESPONSE;
        jobs[0].notify(SC_ZERO_TIME);
        wait(done[0]);
        settle();
        check(peek(0, reg::PADOUT) == 0x13579bdf, "socket usable after canceled access");

        reset();
        tick();
        worker_bank[1] = 1;
        const auto before = sc_time_stamp();
        jobs[0].notify(SC_ZERO_TIME);
        jobs[1].notify(SC_ZERO_TIME);
        wait(done[0] & done[1]);
        settle();
        check(peek(0, reg::PADOUT) == 0x13579bdf && peek(1, reg::PADDIR) == 0x13579be0,
              "different banks complete independently");
        check(sc_time_stamp() - before < sc_time(15, SC_NS),
              "different bank sockets use the same access edge");
        worker_bank[1] = 0;
        reset();
        tick();
        worker_status[0] = worker_status[1] = tlm::TLM_GENERIC_ERROR_RESPONSE;
        jobs[0].notify(SC_ZERO_TIME);
        jobs[1].notify(SC_ZERO_TIME);
        wait(1, SC_NS);
        reset_n.write(false);
        wait(done[0] & done[1]);
        settle();
        check(peek(0, reg::PADOUT) == 0 && peek(0, reg::PADDIR) == 0,
              "reset releases pending and mutex-queued callers without stale writes");
    }
    void iosel_checks() {
        unsigned digital = 0, configurable = 0, inputs = 0;
        for (unsigned p = 1; p <= 80; ++p) {
            const auto info = pin_info(p);
            digital += info.digital;
            configurable += info.configurable;
            inputs += info.input_only;
        }
        check(digital == 45 && configurable == 15 && inputs == 3,
              "package digital / mux / input-only counts");
        // Test raw IOSEL paths only on peripheral-owned legs; GPIO legs are tested below.
        for (unsigned p = 1; p <= 80; ++p) {
            if (p == 5)
                continue;
            const auto info = pin_info(p);
            for (unsigned s = 0; s < 4; ++s)
                for (unsigned level = 0; level < 2; ++level) {
                    select[p - 1].write(s);
                    pads[p - 1].write(level);
                    dut.peripheral_primary_out[p - 1].write(true);
                    dut.peripheral_alternate_out[p - 1].write(true);
                    dut.peripheral_primary_oe[p - 1].write(true);
                    dut.peripheral_alternate_oe[p - 1].write(true);
                    settle();
                    const auto effective = info.configurable ? s : 0;
                    const bool active = info.digital && !info.reserved && effective < 2;
                    check(dut.primary_in[p - 1].read() == (active && effective == 0 && level),
                          "primary input route position " + std::to_string(p));
                    check(dut.alternate_in[p - 1].read() == (active && effective == 1 && level),
                          "alternate input route");
                    const bool gpio = info.gpio_bank >= 0 && effective == info.gpio_select;
                    check(data[p - 1].read() == (active && !info.input_only && !gpio),
                          "selected data / reserved code");
                    check(oe[p - 1].read() == (active && !info.input_only && !gpio),
                          "requested enable / input-only");
                }
            pads[p - 1].write(false);
        }
        for (unsigned mode = 0; mode < 4; ++mode) {
            pads[4].write(mode & 1);
            pads[10].write(mode & 2);
            settle();
            check(test_mode.read() == bool(mode & 1) &&
                      test_select.read() == bool((mode & 1) && (mode & 2)),
                  "test decode truth table");
            if (mode & 1)
                for (unsigned p = 1; p <= 80; ++p) {
                    check(!data[p - 1].read() && !oe[p - 1].read(),
                          "normal output routing zero in test mode");
                    if (p != 5 && p != 11)
                        check(!dut.primary_in[p - 1].read() && !dut.alternate_in[p - 1].read(),
                              "normal input routing zero in test mode");
                }
            check(dut.primary_in[4].read() == bool(mode & 1) &&
                      dut.primary_in[10].read() == bool(mode & 2),
                  "test receiver paths stay available");
        }
    }
    void reset_pulse() {
        tick();
        worker_status[0] = worker_status[1] = tlm::TLM_GENERIC_ERROR_RESPONSE;
        jobs[0].notify(SC_ZERO_TIME);
        jobs[1].notify(SC_ZERO_TIME);
        wait(1, SC_NS);
        reset_n.write(false);
        // Release before the aborted caller has returned and unlocked the bus.
        wait(SC_ZERO_TIME);
        reset_n.write(true);
        wait(done[0] & done[1]);
        settle();
        check(peek(0, reg::PADOUT) == 0 && peek(0, reg::PADDIR) == 0,
              "short reset pulse invalidates both pending and mutex-queued writes");
        worker_status[0] = worker_status[1] = tlm::TLM_OK_RESPONSE;
        jobs[0].notify(SC_ZERO_TIME);
        jobs[1].notify(SC_ZERO_TIME);
        wait(done[0] & done[1]);
        settle();
        check(peek(0, reg::PADOUT) == 0x13579bdf && peek(0, reg::PADDIR) == 0x13579be0,
              "new callers recover after a short reset pulse");
    }
    void integration() {
        // Independently specified GPIO package mapping, not derived from pin_info.
        const unsigned mapping[3][8] = {{6, 12, 13, 14, 15, 20, 21, 22},
                                        {25, 26, 27, 28, 29, 30, 44, 45},
                                        {46, 47, 54, 55, 56, 59, 60, 61}};
        for (unsigned b = 0; b < 3; ++b)
            for (unsigned bit = 0; bit < 8; ++bit) {
                reset();
                const auto p = mapping[b][bit];
                wr(b, reg::PADOUT, 1u << bit);
                wr(b, reg::PADDIR, 1u << bit);
                settle();
                for (unsigned other = 0; other < 8; ++other)
                    check(data[mapping[b][other] - 1].read() == (other == bit),
                          "GPIO output package mapping");
                check(oe[p - 1].read(), "GPIO requested direction reaches selected pin");
                pads[p - 1].write(true);
                settle();
                tick(3);
                check(peek(b, reg::PADIN) == (1u << bit),
                      "package input reaches matching bank bit");
                wr(b, reg::POWEREVENT, 1u << bit);
                settle();
                check(wakes[b].read(), "package wake route");
                wr(b, reg::INTEN, 1u << bit);
                tick();
                check(irqs[b].read(), "package IRQ route");
                const auto info = pin_info(p);
                if (info.configurable) {
                    select[p - 1].write(1 - info.gpio_select);
                    settle();
                    tick(3);
                    check(peek(b, reg::PADIN) == 0 && !wakes[b].read(),
                          "mux disconnects GPIO input and raw wake");
                }
            }
    }
    void wake_iosel() {
        // Independent package map from IO_PAD HDS Table 7-2. Exercise the
        // complete package -> IOSEL -> GPIO raw wake path after separation.
        const unsigned mapping[3][8] = {{6, 12, 13, 14, 15, 20, 21, 22},
                                        {25, 26, 27, 28, 29, 30, 44, 45},
                                        {46, 47, 54, 55, 56, 59, 60, 61}};
        for (unsigned bank = 0; bank < 3; ++bank)
            for (unsigned bit = 0; bit < 8; ++bit) {
                reset();
                const auto position = mapping[bank][bit];
                const bool configurable = bank == 0 || (bank == 1 && bit < 5) ||
                                          (bank == 2 && bit < 2);
                const unsigned gpio_select = (bank == 0 && bit < 5) || (bank == 2 && bit < 2);
                wr(bank, reg::POWEREVENT, std::uint32_t{1} << bit);
                pads[position - 1].write(true);
                for (unsigned code = 0; code < 4; ++code)
                    for (unsigned mode = 0; mode < 2; ++mode) {
                        select[position - 1].write(code);
                        pads[4].write(mode);
                        settle();
                        const bool connected = !mode && (!configurable || code == gpio_select);
                        check(wakes[bank].read() == connected && wake.read() == connected,
                              "IOSEL/test mode gates raw bank wake and aggregate wake");
                        for (unsigned other = 0; other < 3; ++other)
                            if (other != bank)
                                check(!wakes[other].read(), "IOSEL wake bank isolation");
                        tick(3);
                        check(peek(bank, reg::PADIN) ==
                                  (connected ? std::uint32_t{1} << bit : 0),
                              "IOSEL/test mode controls synchronized GPIO input");
                    }
            }
        reset();
        wr(0, reg::POWEREVENT, 1);
        wr(1, reg::POWEREVENT, 1);
        pads[5].write(true);  // Package 6: GPIO A0 on alternate leg.
        pads[24].write(true); // Package 25: GPIO B0 on primary leg.
        settle();
        check(wakes[0].read() && wakes[1].read() && wake.read(), "two selected banks wake");
        select[5].write(0);
        settle();
        check(!wakes[0].read() && wakes[1].read() && wake.read(),
              "disconnecting one bank preserves the other bank's wake");
        select[24].write(2);
        settle();
        check(!wakes[0].read() && !wakes[1].read() && !wake.read(),
              "reserved IOSEL code disconnects the last wake source");
    }
    void random_checks() {
        struct Reference {
            std::array<std::uint32_t, 16> w{};
            std::uint32_t q0 = 0, q1 = 0, q2 = 0;
            bool irq = false;
        };
        for (unsigned seed : {61443u, 24301u, 20261017u}) {
            reset();
            Reference ref[3];
            for (auto& r : ref)
                for (unsigned i = 8; i < 16; ++i)
                    r.w[i] = reg::PADCFG_RESET;
            std::mt19937 rng(seed);
            for (unsigned iteration = 0; iteration < 1800; ++iteration) {
                std::uint32_t stimulus[3];
                for (unsigned b = 0; b < 3; ++b) {
                    stimulus[b] = rng();
                    raw(b, stimulus[b]);
                }
                settle();
                const unsigned bank = rng() % 3, index = rng() % 16;
                const bool write = rng() & 1;
                const std::uint32_t value = rng();
                const auto expected_read = ref[bank].w[index];
                for (unsigned b = 0; b < 3; ++b) {
                    auto& r = ref[b];
                    std::uint32_t active = 0;
                    for (unsigned bit = 0; bit < 32; ++bit) {
                        const unsigned mode = ((r.w[4] >> bit) & 1) | (((r.w[5] >> bit) & 1) << 1);
                        const bool old = (r.q2 >> bit) & 1, synced = (r.q1 >> bit) & 1;
                        const bool event = mode == 0   ? old
                                           : mode == 1 ? !old
                                           : mode == 2 ? (synced && !old)
                                                       : (!synced && old);
                        if (event && ((r.w[3] >> bit) & 1))
                            active |= std::uint32_t{1} << bit;
                    }
                    if (r.irq) {
                        if (b == bank && !write && index == 6) {
                            r.irq = false;
                            r.w[6] = 0;
                        }
                    } else if (active) {
                        r.irq = true;
                        r.w[6] = active;
                    }
                    if (b == bank && write && index != 1 && index != 6)
                        r.w[index] = value & (index >= 8 ? reg::PADCFG_MASK : 0xffffffffu);
                    r.q2 = r.q1;
                    r.q1 = r.q0;
                    r.q0 = stimulus[b];
                    r.w[1] = r.q2;
                }
                const auto got = access(bank, write, index * 4, value);
                if (!write)
                    check(got == expected_read,
                          "random pre-edge read seed " + std::to_string(seed));
                settle();
                for (unsigned b = 0; b < 3; ++b) {
                    const auto& r = ref[b];
                    for (unsigned i = 0; i < 16; ++i)
                        check(peek(b, i * 4) == r.w[i],
                              "random reference register seed " + std::to_string(seed));
                    check(irqs[b].read() == r.irq, "random reference IRQ");
                    check(dut.input_sync[b].read() == r.q1, "random reference synchronizer");
                    bool expected_wake = false;
                    for (unsigned pin = 0; pin < 32; ++pin)
                        if (r.w[7] & (std::uint32_t{1} << pin))
                            expected_wake = (stimulus[b] >> pin) & 1;
                    check(wakes[b].read() == expected_wake, "random reference raw wake priority");
                }
            }
        }
    }
    void run() {
        reset();
        if (scenario == "registers")
            registers();
        else if (scenario == "sampling")
            sampling();
        else if (scenario == "interrupts")
            interrupts();
        else if (scenario == "wake")
            wake_checks();
        else if (scenario == "protocol")
            protocol();
        else if (scenario == "reset")
            reset_checks();
        else if (scenario == "concurrency")
            concurrency();
        else if (scenario == "reset_pulse")
            reset_pulse();
        else if (scenario == "iosel")
            iosel_checks();
        else if (scenario == "integration")
            integration();
        else if (scenario == "wake_iosel")
            wake_iosel();
        else if (scenario == "random")
            random_checks();
        else
            check(false, "unknown scenario");
        std::cout << scenario << ": " << checks << " checks, " << errors << " failures\n";
        sc_stop();
    }
};
int sc_main(int argc, char** argv) {
    Bench bench("bench", argc > 1 ? argv[1] : "registers");
    sc_start();
    return bench.errors ? 1 : 0;
}
