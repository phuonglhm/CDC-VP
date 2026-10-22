// Functional host mode of the i2c model (i2c_config::functional_host): bus
// order and timing against an attached device, ACK / NACK, repeated START,
// multi-byte reads with RCONT, RX FIFO stretching, NACK halt and recovery,
// disable / re-enable between and inside transactions, abort by FMT reset.
// The legacy behaviour is covered by test_i2c_tlm.
//
//   test_i2c_functional
#include <cstdio>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include "i2c.h"

using namespace sc_core;

namespace {
int g_fail = 0;
bool g_completed = false;  // set only after the last case
#define CHECK(c)                                                              \
    do {                                                                      \
        if (!(c)) {                                                           \
            std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #c);  \
            ++g_fail;                                                         \
        }                                                                     \
    } while (0)

// A device with 16-bit register addressing, auto-increment, as camera sensors use.
struct RegDevice : i2c_device {
    std::vector<uint8_t> regs = std::vector<uint8_t>(0x10000, 0);
    std::vector<std::string> log;
    unsigned ptr = 0, nbytes = 0;
    bool reading = false;
    bool nack_data_at = false;  // NACK the 3rd written byte
    bool i2c_start(bool read) override {
        log.push_back(std::string("S") + (read ? "r" : "w") + "@" + std::to_string(sc_time_stamp().to_seconds() * 1e9));
        reading = read;
        nbytes = 0;
        return true;
    }
    bool i2c_write(uint8_t b) override {
        log.push_back("W" + std::to_string(b));
        if (nack_data_at && nbytes == 2) return false;
        if (nbytes == 0) ptr = unsigned(b) << 8;
        else if (nbytes == 1) ptr |= b;
        else regs[ptr++ & 0xFFFF] = b;
        ++nbytes;
        return true;
    }
    uint8_t i2c_read(bool ack) override {
        log.push_back(std::string("R") + (ack ? "a" : "n"));
        return regs[ptr++ & 0xFFFF];
    }
    void i2c_stop() override { log.push_back("P"); }
};

SC_MODULE(Tb) {
    tlm_utils::simple_initiator_socket<Tb> socket{"socket"};
    sc_signal<bool> irq{"irq"};
    i2c dut;
    RegDevice dev;

    SC_HAS_PROCESS(Tb);
    explicit Tb(sc_module_name nm) : sc_module(nm), dut("i2c", make_cfg()) {
        socket.bind(dut.socket);
        dut.irq(irq);
        dut.attach(0x10, &dev);
        SC_THREAD(run);
    }
    static i2c_config make_cfg() {
        i2c_config c;
        c.functional_host = true;
        c.scl_hz = 400e3;
        return c;
    }
    uint32_t xfer(tlm::tlm_command cmd, uint32_t off, uint32_t v) {
        tlm::tlm_generic_payload p;
        sc_time d = SC_ZERO_TIME;
        p.set_command(cmd);
        p.set_address(off);
        p.set_data_ptr(reinterpret_cast<unsigned char*>(&v));
        p.set_data_length(4);
        p.set_streaming_width(4);
        socket->b_transport(p, d);
        return v;
    }
    uint32_t rd(uint32_t off) { return xfer(tlm::TLM_READ_COMMAND, off, 0); }
    void wr(uint32_t off, uint32_t v) { xfer(tlm::TLM_WRITE_COMMAND, off, v); }
    bool wait_idle(unsigned max_us = 2000) {
        for (unsigned t = 0; t < max_us && !(rd(I2C_STATUS) & STATUS_HOSTIDLE); ++t) wait(1, SC_US);
        return rd(I2C_STATUS) & STATUS_HOSTIDLE;
    }
    // Register write: START+addr(W), reg hi, reg lo, data..., last with STOP.
    void reg_write(uint8_t addr7, uint16_t reg, const std::vector<uint8_t>& data) {
        wr(I2C_FDATA, FDATA_START | (addr7 << 1));
        wr(I2C_FDATA, reg >> 8);
        wr(I2C_FDATA, reg & 0xFF);
        for (size_t i = 0; i < data.size(); ++i) wr(I2C_FDATA, data[i] | (i + 1 == data.size() ? FDATA_STOP : 0));
    }
    // Register read: write the pointer, repeated START+addr(R), read n with STOP.
    std::vector<uint8_t> reg_read(uint8_t addr7, uint16_t reg, unsigned n) {
        wr(I2C_FDATA, FDATA_START | (addr7 << 1));
        wr(I2C_FDATA, reg >> 8);
        wr(I2C_FDATA, reg & 0xFF);
        wr(I2C_FDATA, FDATA_START | (addr7 << 1) | 1);
        wr(I2C_FDATA, FDATA_READB | FDATA_STOP | (n & 0xFF));
        CHECK(wait_idle());
        std::vector<uint8_t> v;
        for (unsigned i = 0; i < n; ++i) v.push_back(static_cast<uint8_t>(rd(I2C_RDATA)));
        return v;
    }

    void run() {
        wr(I2C_CTRL, CTRL_ENABLEHOST);
        wr(I2C_INTR_ENABLE, INTR_CMD_COMPLETE | INTR_NAK);
        // 1. Write 3 bytes at 0x0340: bus order, timing, completion.
        const sc_time t0 = sc_time_stamp();
        reg_write(0x10, 0x0340, {0x11, 0x22, 0x33});
        CHECK(!(rd(I2C_STATUS) & STATUS_HOSTIDLE));  // the transaction is running
        CHECK(wait_idle());
        const double us = (sc_time_stamp() - t0).to_seconds() * 1e6;
        // START (1) + 6 bytes x 9 + STOP (1) = 56 bits at 400 kHz = 140 us
        std::printf("write of 3 bytes: %.1f us\n", us);
        CHECK(us >= 140.0 && us < 142.0);
        CHECK(dev.regs[0x340] == 0x11 && dev.regs[0x341] == 0x22 && dev.regs[0x342] == 0x33);
        CHECK(rd(I2C_INTR_STATE) & INTR_CMD_COMPLETE);
        wait(SC_ZERO_TIME);
        wait(SC_ZERO_TIME);
        CHECK(irq.read());
        wr(I2C_INTR_STATE, INTR_CMD_COMPLETE);
        wait(SC_ZERO_TIME);
        wait(SC_ZERO_TIME);
        CHECK(!irq.read());
        CHECK(dev.log.size() == 7 && dev.log.front().rfind("Sw", 0) == 0 && dev.log.back() == "P");  // Sw W W W W W P
        // 2. Read back with a repeated START: last byte NACKed by the controller.
        dev.log.clear();
        const auto v = reg_read(0x10, 0x0340, 3);
        CHECK((v == std::vector<uint8_t>{0x11, 0x22, 0x33}));
        CHECK(dev.log.size() == 8);  // Sw W W Sr Ra Ra Rn P
        if (dev.log.size() == 8) CHECK(dev.log[3].rfind("Sr", 0) == 0 && dev.log[5] == "Ra" && dev.log[6] == "Rn");
        CHECK(rd(I2C_STATUS) & STATUS_RXEMPTY);
        // 3. No device at the address: NACK, STOP at once, rest discarded.
        wr(I2C_INTR_STATE, 0xFFFFFFFF);
        dev.log.clear();
        reg_write(0x22, 0x0100, {0x01});
        CHECK(wait_idle());
        CHECK(rd(I2C_INTR_STATE) & INTR_NAK);
        CHECK(rd(I2C_INTR_STATE) & INTR_CMD_COMPLETE);
        CHECK(dev.log.empty());
        // The next transaction runs normally.
        wr(I2C_INTR_STATE, 0xFFFFFFFF);
        reg_write(0x10, 0x0100, {0x01});
        CHECK(wait_idle());
        CHECK(dev.regs[0x100] == 0x01);
        CHECK(!(rd(I2C_INTR_STATE) & INTR_NAK));
        // 4. The device NACKs a data byte: halt; NAKOK lets it continue.
        dev.nack_data_at = true;
        wr(I2C_INTR_STATE, 0xFFFFFFFF);
        reg_write(0x10, 0x0200, {0x55, 0x66});
        CHECK(wait_idle());
        CHECK(rd(I2C_INTR_STATE) & INTR_NAK);
        CHECK(dev.regs[0x200] == 0 && dev.regs[0x201] == 0);
        wr(I2C_INTR_STATE, 0xFFFFFFFF);
        wr(I2C_FDATA, FDATA_START | (0x10 << 1));
        wr(I2C_FDATA, 0x02);
        wr(I2C_FDATA, 0x10);
        wr(I2C_FDATA, FDATA_NAKOK | FDATA_STOP | 0x77);
        CHECK(wait_idle());
        CHECK(!(rd(I2C_INTR_STATE) & INTR_NAK));
        dev.nack_data_at = false;
        // 5. Read of 100 bytes: the RX FIFO (64) fills, the clock stretches,
        //    nothing is lost once software drains it.
        for (unsigned i = 0; i < 100; ++i) dev.regs[0x1000 + i] = static_cast<uint8_t>(i * 3);
        wr(I2C_FDATA, FDATA_START | (0x10 << 1));
        wr(I2C_FDATA, 0x10);
        wr(I2C_FDATA, 0x00);
        wr(I2C_FDATA, FDATA_START | (0x10 << 1) | 1);
        wr(I2C_FDATA, FDATA_READB | FDATA_STOP | 100);
        wait(3, SC_MS);
        CHECK(!(rd(I2C_STATUS) & STATUS_HOSTIDLE));  // stretched on the full FIFO
        CHECK((rd(I2C_HOST_FIFO_STATUS) >> 8) == 64);
        bool ok = true;
        for (unsigned i = 0; i < 100; ++i) {
            while (rd(I2C_STATUS) & STATUS_RXEMPTY) wait(5, SC_US);
            ok &= rd(I2C_RDATA) == uint32_t(uint8_t(i * 3));
        }
        CHECK(ok);
        CHECK(wait_idle());
        // 7. Disable before the worker takes the queue (review F2): the queue
        //    waits unchanged, no bus event; enable resumes it.
        wr(I2C_INTR_STATE, 0xFFFFFFFF);
        dev.log.clear();
        reg_write(0x10, 0x0500, {0x5A});
        wr(I2C_CTRL, 0);  // same time stamp: nothing taken yet
        wait(200, SC_US);
        CHECK((rd(I2C_HOST_FIFO_STATUS) & 0xFF) == 4);
        CHECK(!(rd(I2C_STATUS) & STATUS_HOSTIDLE));
        CHECK(dev.log.empty() && dev.regs[0x500] == 0);
        CHECK(!(rd(I2C_INTR_STATE) & INTR_CMD_COMPLETE));
        wr(I2C_CTRL, CTRL_ENABLEHOST);
        CHECK(wait_idle());
        CHECK(dev.regs[0x500] == 0x5A);
        CHECK(rd(I2C_INTR_STATE) & INTR_CMD_COMPLETE);
        CHECK(!(rd(I2C_INTR_STATE) & INTR_NAK));
        // 8. Disable inside a transaction: it runs to its STOP, the next one
        //    waits for the enable.
        wr(I2C_INTR_STATE, 0xFFFFFFFF);
        dev.log.clear();
        reg_write(0x10, 0x0600, {0x01, 0x02, 0x03});
        reg_write(0x10, 0x0610, {0x44});
        wait(30, SC_US);  // START + address done, the register bytes in flight
        CHECK(!dev.log.empty() && dev.log.front().rfind("Sw", 0) == 0);
        wr(I2C_CTRL, 0);
        wait(500, SC_US);
        CHECK(dev.regs[0x600] == 0x01 && dev.regs[0x601] == 0x02 && dev.regs[0x602] == 0x03);
        CHECK(dev.log.size() == 7 && dev.log.back() == "P");  // first transaction only, complete
        CHECK(dev.regs[0x610] == 0);
        CHECK((rd(I2C_HOST_FIFO_STATUS) & 0xFF) == 4);
        CHECK(rd(I2C_INTR_STATE) & INTR_CMD_COMPLETE);
        wr(I2C_INTR_STATE, 0xFFFFFFFF);
        wr(I2C_CTRL, CTRL_ENABLEHOST);
        CHECK(wait_idle());
        CHECK(dev.regs[0x610] == 0x44);
        CHECK(rd(I2C_INTR_STATE) & INTR_CMD_COMPLETE);
        // 9. FMT reset inside a write (M5b review F2): the byte on the bus
        //    completes, STOP at once, host idle, no NAK / CMD_COMPLETE; the
        //    next transaction runs.
        wr(I2C_INTR_STATE, 0xFFFFFFFF);
        dev.log.clear();
        reg_write(0x10, 0x0700, {0x01, 0x02, 0x03, 0x04});
        wait(30, SC_US);                   // START + address done, a register byte in flight
        wr(I2C_FIFO_CTRL, 1u << 1);        // FMT reset
        CHECK((rd(I2C_HOST_FIFO_STATUS) & 0xFF) == 0);
        CHECK(!(rd(I2C_STATUS) & STATUS_HOSTIDLE));  // still ending
        wait(25, SC_US);                   // at most one byte (22.5 us) + STOP (2.5 us) at 400 kHz
        CHECK(rd(I2C_STATUS) & STATUS_HOSTIDLE);
        CHECK(!dev.log.empty() && dev.log.back() == "P");
        CHECK(dev.regs[0x702] == 0 && dev.regs[0x703] == 0);
        CHECK(!(rd(I2C_INTR_STATE) & (INTR_NAK | INTR_CMD_COMPLETE)));
        reg_write(0x10, 0x0710, {0x66});
        CHECK(wait_idle());
        CHECK(dev.regs[0x710] == 0x66 && (rd(I2C_INTR_STATE) & INTR_CMD_COMPLETE));
        // 10. FMT + RX reset inside a stretched read (RX FIFO full): STOP after
        //     the byte in progress, no further byte read from the device.
        for (unsigned i = 0; i < 100; ++i) dev.regs[0x1100 + i] = static_cast<uint8_t>(i);
        wr(I2C_INTR_STATE, 0xFFFFFFFF);
        dev.log.clear();
        wr(I2C_FDATA, FDATA_START | (0x10 << 1));
        wr(I2C_FDATA, 0x11);
        wr(I2C_FDATA, 0x00);
        wr(I2C_FDATA, FDATA_START | (0x10 << 1) | 1);
        wr(I2C_FDATA, FDATA_READB | FDATA_STOP | 100);
        wait(3, SC_MS);                    // 64 bytes in the RX FIFO, clock stretched
        CHECK(!(rd(I2C_STATUS) & STATUS_HOSTIDLE));
        wr(I2C_FIFO_CTRL, (1u << 1) | (1u << 0));
        wait(25, SC_US);
        CHECK(rd(I2C_STATUS) & STATUS_HOSTIDLE);
        size_t reads = 0;
        for (const auto& e : dev.log) reads += e[0] == 'R';
        CHECK(reads <= 65 && dev.log.back() == "P");
        CHECK(!(rd(I2C_INTR_STATE) & (INTR_NAK | INTR_CMD_COMPLETE)));
        CHECK(reg_read(0x10, 0x0710, 1) == std::vector<uint8_t>{0x66});
        // 11. Abort during the STOP that follows an address NACK (M5b
        //     re-review F2): START + address 25 us, STOP 25..27.5 us; FMT + RX
        //     reset at 26 us. The next transaction must run, not be discarded.
        wr(I2C_INTR_STATE, 0xFFFFFFFF);
        reg_write(0x22, 0x0100, {0x01});
        wait(26, SC_US);
        wr(I2C_FIFO_CTRL, (1u << 1) | (1u << 0));
        wait(4, SC_US);
        CHECK(rd(I2C_STATUS) & STATUS_HOSTIDLE);
        CHECK(!(rd(I2C_INTR_STATE) & INTR_CMD_COMPLETE));  // the aborted transaction does not complete
        wr(I2C_INTR_STATE, 0xFFFFFFFF);
        reg_write(0x10, 0x0720, {0x77});
        CHECK(wait_idle());
        CHECK(dev.regs[0x720] == 0x77);
        CHECK((rd(I2C_INTR_STATE) & (INTR_CMD_COMPLETE | INTR_NAK)) == INTR_CMD_COMPLETE);
        // 12. Abort during the STOP of a normal write: START + address + 3
        //     bytes = 92.5 us, STOP 92.5..95 us; reset at 93.5 us. The bytes are
        //     written, no CMD_COMPLETE is reported, the host is idle.
        wr(I2C_INTR_STATE, 0xFFFFFFFF);
        reg_write(0x10, 0x0730, {0x55});
        wait(93500, SC_NS);
        wr(I2C_FIFO_CTRL, 1u << 1);
        wait(5, SC_US);
        CHECK(rd(I2C_STATUS) & STATUS_HOSTIDLE);
        CHECK(dev.regs[0x730] == 0x55);
        CHECK(!(rd(I2C_INTR_STATE) & INTR_CMD_COMPLETE));
        // 13. Abort during a read byte: write pointer + repeated START = 95 us,
        //     first byte 95..117.5 us; reset at 100 us: that byte completes on
        //     the bus but is not kept, STOP, RX FIFO empty.
        wr(I2C_INTR_STATE, 0xFFFFFFFF);
        dev.log.clear();
        wr(I2C_FDATA, FDATA_START | (0x10 << 1));
        wr(I2C_FDATA, 0x07);
        wr(I2C_FDATA, 0x20);
        wr(I2C_FDATA, FDATA_START | (0x10 << 1) | 1);
        wr(I2C_FDATA, FDATA_READB | FDATA_STOP | 4);
        wait(100, SC_US);
        wr(I2C_FIFO_CTRL, (1u << 1) | (1u << 0));
        wait(25, SC_US);
        CHECK(rd(I2C_STATUS) & STATUS_HOSTIDLE);
        CHECK(rd(I2C_STATUS) & STATUS_RXEMPTY);
        size_t r13 = 0;
        for (const auto& e : dev.log) r13 += e[0] == 'R';
        CHECK(r13 == 1 && dev.log.back() == "P");
        CHECK(!(rd(I2C_INTR_STATE) & (INTR_NAK | INTR_CMD_COMPLETE)));
        // 6. Host disabled: commands wait in the FIFO.
        wr(I2C_CTRL, 0);
        wr(I2C_FDATA, FDATA_START | (0x10 << 1));
        CHECK((rd(I2C_HOST_FIFO_STATUS) & 0xFF) == 0);  // FDATA ignored while the host is disabled
        g_completed = true;
        std::printf("test_i2c_functional: %s\n", g_fail ? "FAIL" : "PASS");
        sc_stop();
    }
};
}  // namespace

int sc_main(int, char**) {
    Tb tb("tb");
    sc_start(1, SC_SEC);
    // The time cap reached before the last case is a failure, not a pass.
    if (!g_completed) std::printf("FAIL test_i2c_functional did not complete\n");
    return g_completed && !g_fail ? 0 : 1;
}
