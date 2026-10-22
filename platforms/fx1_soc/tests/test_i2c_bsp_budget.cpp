// Budget contract of the BSP I2C driver (fw/fx1_soc/drivers/fx1_i2c.c, built
// for the host) against the I2C0 controller model (components/i2c_tlm,
// functional host mode, 400 kHz), CSI-2 M6a follow-up review R2. Design after
// the reviewer's harness (plan/evidence/m6a_followup_20261022).
//
// Every register access goes through TLM to the real model and costs 1 us of
// simulated time (CPU/MMIO latency of the bench, not a property of the IP);
// mtime is sc_time_stamp at 20 MHz. Before each driver call an earlier
// transaction is left aborting: its address byte is put on the bus, then FMT
// reset is requested `phase` us later, so the host turns idle somewhere
// around the call's deadline (the byte and STOP drain in the model). Swept
// over phases and budgets, for fx1_i2c_write and fx1_i2c_write_read:
//   (a) no START of the call is queued after its deadline;
//   (b) the call returns within budget + FX1_I2C_ABORT_US + kMmio;
//   (c) OK: the device took the transaction (byte written / byte read);
//       nothing queued: the device saw no START;
//   (d) a call that queued nothing returns within budget + kMmio (the abort
//       allowance is for a transaction that was started; review M6a R3);
// and each API must have hit the gate (nothing queued) and completed OK.
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <cstdio>

#include "fx1_fw.h"  // tests/hostshim
#include "fx1_i2c.h"
#include "i2c.h"

using namespace sc_core;

namespace {

constexpr uint32_t kFdataStart = 1u << 8;
constexpr uint32_t kFifoReset = 3u;            // FIFO_CTRL RX + FMT reset
constexpr double kMmioUs = 1.0;                // bench MMIO latency
constexpr double kMmio = 10.0;                 // bench allowance: polls and the abort's register accesses
constexpr uint8_t kOld = 0x10, kNew = 0x11;
constexpr int kMaxFailures = 10;

struct Device : i2c_device {
    unsigned starts = 0, writes = 0;
    uint8_t last = 0;
    bool i2c_start(bool) override { ++starts; return true; }
    bool i2c_write(uint8_t b) override { ++writes; last = b; return true; }
    uint8_t i2c_read(bool) override { return 0x5A; }
    void i2c_stop() override {}
};

struct Bench : sc_module {
    tlm_utils::simple_initiator_socket<Bench> socket{"socket"};
    sc_signal<bool> irq{"irq"};
    i2c dut;
    Device old_dev, dev;
    fx1_i2c_t bus{};
    bool recording = false;
    double first_start_us = -1;
    int failures = 0;
    bool completed = false;  // set only at the end of the sweep (M6a R2 review R3)

    SC_HAS_PROCESS(Bench);
    explicit Bench(sc_module_name n) : sc_module(n), dut("dut", i2c_config{true, 400e3, false}) {
        socket.bind(dut.socket);
        dut.irq(irq);
        dut.attach(kOld, &old_dev);
        dut.attach(kNew, &dev);
        SC_THREAD(run);
    }

    static double now_us() { return sc_time_stamp().to_seconds() * 1e6; }

    uint32_t access(bool write, uint32_t off, uint32_t value = 0) {
        if (recording && write && off == I2C_FDATA && (value & kFdataStart) && first_start_us < 0) first_start_us = now_us();
        tlm::tlm_generic_payload gp;
        gp.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
        gp.set_address(off);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        sc_time d = SC_ZERO_TIME;
        socket->b_transport(gp, d);
        if (!gp.is_response_ok()) {  // not sc_assert: compiled out with NDEBUG
            std::printf("FAIL register access 0x%x not OK\n", off);
            ++failures;
        }
        wait(sc_time(kMmioUs, SC_US));
        return value;
    }

    void fail(const char* what, int api, unsigned phase, unsigned budget, int rc, double el) {
        if (++failures <= kMaxFailures)
            std::printf("FAIL %s: %s phase %u us budget %u us rc %d elapsed %.3f us start %.3f\n", what,
                        api ? "write_read" : "write", phase, budget, rc, el, first_start_us);
    }

    void run();
};

Bench* g_bench = nullptr;

}  // namespace

extern "C" uint64_t fx1_mtime(void) { return static_cast<uint64_t>(sc_time_stamp().to_seconds() * FX1_MTIME_HZ + 1e-6); }
extern "C" uint32_t fx1_read32(uintptr_t a) { return g_bench->access(false, static_cast<uint32_t>(a)); }
extern "C" void fx1_write32(uintptr_t a, uint32_t v) { g_bench->access(true, static_cast<uint32_t>(a), v); }

void Bench::run() {
    if (fx1_i2c_open(&bus, 0, 0, 0, 0) != FX1_I2C_OK) {
        std::printf("FAIL fx1_i2c_open\n");
        ++failures;
        sc_stop();
        return;
    }
    static const unsigned budgets[] = {5, 10, 15, 20, 30, 60, 200};
    for (int api = 0; api < 2 && failures < kMaxFailures; ++api) {
        unsigned gated = 0, ok = 0, started_timeout = 0;
        for (unsigned phase = 0; phase <= 30; phase += 2) {
            for (unsigned budget : budgets) {
                // Quiet bus, then an earlier transaction left aborting.
                access(true, I2C_FIFO_CTRL, kFifoReset);
                wait(sc_time(60, SC_US));
                access(true, I2C_INTR_STATE, 0xFFFFFFFFu);
                access(true, I2C_FDATA, kFdataStart | (kOld << 1));
                wait(sc_time(phase, SC_US));
                access(true, I2C_FIFO_CTRL, kFifoReset);

                const unsigned starts = dev.starts, writes = dev.writes;
                first_start_us = -1;
                recording = true;
                const double t0 = now_us(), deadline = t0 + budget;
                uint8_t w = 0xA5, r = 0;
                const int rc = api ? fx1_i2c_write_read(&bus, kNew, &w, 1, &r, 1, budget)
                                   : fx1_i2c_write(&bus, kNew, &w, 1, budget);
                recording = false;
                const double el = now_us() - t0;
                const bool queued = first_start_us >= 0;
                if (queued && first_start_us > deadline + 1e-9) fail("(a) START queued after the deadline", api, phase, budget, rc, el);
                if (el > budget + FX1_I2C_ABORT_US + kMmio) fail("(b) returned too late", api, phase, budget, rc, el);
                if (rc == FX1_I2C_OK) {
                    ++ok;
                    const bool took = dev.starts > starts && (api ? r == 0x5A : (dev.writes > writes && dev.last == 0xA5));
                    if (!took) fail("(c) OK without the transaction", api, phase, budget, rc, el);
                } else if (!queued) {
                    ++gated;
                    if (dev.starts != starts) fail("(c) nothing queued but the device saw a START", api, phase, budget, rc, el);
                    if (rc != FX1_I2C_ETIMEOUT && rc != FX1_I2C_EBUSY) fail("(c) unexpected result", api, phase, budget, rc, el);
                    if (el > budget + kMmio) fail("(d) nothing sent, returned after the budget", api, phase, budget, rc, el);
                } else {
                    ++started_timeout;
                }
                wait(sc_time(60, SC_US));
                if (failures >= kMaxFailures) break;
            }
            if (failures >= kMaxFailures) break;
        }
        std::printf("%s: %u OK, %u gated (nothing sent), %u started and timed out\n", api ? "write_read" : "write", ok,
                    gated, started_timeout);
        if (!gated || !ok) {
            ++failures;
            std::printf("FAIL the sweep must reach the gate and complete OK\n");
        }
    }
    completed = true;
    std::printf("i2c BSP budget contract: %s\n", failures ? "FAIL" : "PASS");
    sc_stop();
}

int sc_main(int, char**) {
    Bench tb("tb");
    g_bench = &tb;
    sc_start(sc_time(1, SC_SEC));  // bounded: the sweep takes well under 100 ms
    // A time cap reached before the end is a failure, not a pass: nothing
    // after the stall was checked. Not sc_assert (compiled out with NDEBUG).
    if (!tb.completed) std::printf("FAIL sweep did not complete (stopped at %.3f us)\n", sc_time_stamp().to_seconds() * 1e6);
    return tb.completed && !tb.failures ? 0 : 1;
}
