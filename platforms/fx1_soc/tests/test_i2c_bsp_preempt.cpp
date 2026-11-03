// Preemption inside the BSP I2C driver's completion wait (fw/fx1_soc/drivers/
// fx1_i2c.c, built for the host), against the I2C0 controller model
// (components/i2c_tlm, functional host mode, 400 kHz). Ethernet gap G6: under
// Ethernet load a FreeRTOS task was preempted for 3.1 ms between a status
// read (transaction not complete yet) and the deadline check, and aborted a
// transaction that had completed meanwhile (camera start EDEV). Deterministic
// regression of that sequence, after the reviewer's probe
// (plan/evidence/gaps-review/i2c-probe).
//
// Every register access goes through TLM to the real model and costs 1 us of
// simulated time; mtime is sc_time_stamp at 20 MHz. A case arms a preemption:
// right after the first INTR_STATE read that shows no CMD_COMPLETE, the bench
// stalls the caller for `preempt` us before the driver sees the value.
//   (a) preempt 3100 us, budget 2000 us: the transaction completes during the
//       preemption; the call returns OK, the device took it, no abort;
//   (b) control, no preemption, budget shorter than the transaction: the
//       driver still aborts (ETIMEOUT, nothing more queued);
//   (c) control, preempt 20 us, budget 30 us: still not complete after the
//       preemption, past the deadline: ETIMEOUT.
// Each for fx1_i2c_write and fx1_i2c_write_read.
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <cstdio>

#include "fx1_fw.h"  // tests/hostshim
#include "fx1_i2c.h"
#include "i2c.h"

using namespace sc_core;

namespace {

constexpr uint32_t kCmdComplete = 1u << 9;
constexpr uint32_t kFmtReset = 1u << 1;  // FIFO_CTRL
constexpr double kMmioUs = 1.0;  // bench MMIO latency
constexpr uint8_t kAddr = 0x11;

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
    Device dev;
    fx1_i2c_t bus{};
    double preempt_us = 0;  // armed: stall after the next incomplete INTR_STATE read
    bool preempted = false;
    unsigned fifo_resets = 0;
    int failures = 0;
    bool completed = false;

    SC_HAS_PROCESS(Bench);
    explicit Bench(sc_module_name n) : sc_module(n), dut("dut", i2c_config{true, 400e3, false}) {
        socket.bind(dut.socket);
        dut.irq(irq);
        dut.attach(kAddr, &dev);
        SC_THREAD(run);
    }

    static double now_us() { return sc_time_stamp().to_seconds() * 1e6; }

    uint32_t access(bool write, uint32_t off, uint32_t value = 0) {
        if (write && off == I2C_FIFO_CTRL && (value & kFmtReset)) ++fifo_resets;  // the driver's abort
        tlm::tlm_generic_payload gp;
        gp.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
        gp.set_address(off);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        sc_time d = SC_ZERO_TIME;
        socket->b_transport(gp, d);
        if (!gp.is_response_ok()) {
            std::printf("FAIL register access 0x%x not OK\n", off);
            ++failures;
        }
        wait(sc_time(kMmioUs, SC_US));
        if (!write && off == I2C_INTR_STATE && preempt_us > 0 && !preempted && !(value & kCmdComplete)) {
            preempted = true;  // the task loses the CPU with this value in hand
            wait(sc_time(preempt_us, SC_US));
        }
        return value;
    }

    void one(int api, const char* name, double preempt, unsigned budget, int want_rc, bool want_taken) {
        wait(sc_time(200, SC_US));  // quiet bus
        access(true, I2C_INTR_STATE, 0xFFFFFFFFu);
        const unsigned starts = dev.starts, writes = dev.writes, resets = fifo_resets;
        preempt_us = preempt;
        preempted = false;
        const double t0 = now_us();
        uint8_t w = 0xA5, r = 0;
        const int rc = api ? fx1_i2c_write_read(&bus, kAddr, &w, 1, &r, 1, budget)
                           : fx1_i2c_write(&bus, kAddr, &w, 1, budget);
        preempt_us = 0;
        const double el = now_us() - t0;
        const bool taken = dev.starts > starts &&
                           (api ? r == 0x5A : (dev.writes == writes + 1 && dev.last == 0xA5));
        const unsigned aborts = fifo_resets - resets;
        bool ok = rc == want_rc && (preempt <= 0 || preempted);
        if (want_taken) ok = ok && taken && aborts == 0;
        else ok = ok && aborts == 1;
        std::printf("%s %s %s: preempt %.0f us budget %u us rc %d elapsed %.1f us aborts %u%s\n", ok ? "PASS" : "FAIL",
                    api ? "write_read" : "write", name, preempt, budget, rc, el, aborts,
                    want_taken ? (taken ? " device took it" : " device did NOT take it") : "");
        if (!ok) ++failures;
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
    for (int api = 0; api < 2; ++api) {
        one(api, "(a) preempted after a status read, completed meanwhile", 3100, 2000, FX1_I2C_OK, true);
        one(api, "(b) control: no preemption, budget shorter than the transaction", 0, 20, FX1_I2C_ETIMEOUT, false);
        one(api, "(c) control: preempted, still incomplete past the deadline", 20, 30, FX1_I2C_ETIMEOUT, false);
    }
    completed = true;
    std::printf("i2c BSP preemption: %s\n", failures ? "FAIL" : "PASS");
    sc_stop();
}

int sc_main(int, char**) {
    Bench tb("tb");
    g_bench = &tb;
    sc_start(sc_time(1, SC_SEC));
    if (!tb.completed) std::printf("FAIL did not complete (stopped at %.3f us)\n", sc_time_stamp().to_seconds() * 1e6);
    return tb.completed && !tb.failures ? 0 : 1;
}
