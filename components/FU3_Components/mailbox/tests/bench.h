#pragma once
// Test reset, FIFO hai chiều, đầy/rỗng, threshold, IRQ sticky/W1C/enable, truy cập lỗi, debug và reset giữa chừng
// Dùng chung cho behavior.cpp (nối thẳng) và integration.cpp (qua bus FU3, định nghĩa FU3_MAILBOX_WITH_BUS)
#include <fu3_mailbox/mailbox.h>

#include <iostream>
#include <string>
#include <tlm_utils/simple_initiator_socket.h>

#ifdef FU3_MAILBOX_WITH_BUS
    #include <fu3_bus/access.h>
    #include <fu3_bus/bus_system.h>
#endif

using namespace fu3_mailbox;

struct Bench : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<Bench> cva6{"cva6"};
    tlm_utils::simple_initiator_socket<Bench> ibex{"ibex"};
    Mailbox dut{"dut"};
    sc_core::sc_signal<bool> reset_n{"reset_n", true};
    sc_core::sc_signal<bool> irq_cva6{"irq_cva6"}, irq_ibex{"irq_ibex"};
    std::uint64_t base[2] = {0, 0};
    unsigned checks = 0, errors = 0;

#ifdef FU3_MAILBOX_WITH_BUS
    fu3_bus::BusSystem* fabric = nullptr;
#endif

    SC_HAS_PROCESS(Bench);
    explicit Bench(sc_core::sc_module_name n, bool via_bus) : sc_module(n) {
        dut.reset_n(reset_n);
        dut.irq_cva6(irq_cva6);
        dut.irq_ibex(irq_ibex);

#ifdef FU3_MAILBOX_WITH_BUS
        if (via_bus) {
            auto cfg = fu3_bus::BusConfig::fu3();
            for (auto& t : cfg.targets)
                t.enabled = t.name == "MAILBOX_CVA6" || t.name == "MAILBOX_IBEX";

            fabric = new fu3_bus::BusSystem("fabric", cfg);
            cva6.bind(fabric->initiator("CVA6"));
            ibex.bind(fabric->initiator("IBEX"));
            fabric->target("MAILBOX_CVA6").bind(dut.cva6_socket);
            fabric->target("MAILBOX_IBEX").bind(dut.ibex_socket);
            base[kCva6] = FU3_MAILBOX_CVA6_BASE;
            base[kIbex] = FU3_MAILBOX_IBEX_BASE;
        } else
#endif

        {
            (void)via_bus;
            cva6.bind(dut.cva6_socket);
            ibex.bind(dut.ibex_socket);
        }
        SC_THREAD(run);
    }

    void check(bool ok, const std::string& what) {
        ++checks;
        if (!ok) {
            ++errors;
            std::cerr << "FAIL: " << what << '\n';
        }
    }

    tlm::tlm_response_status xfer(Port p, bool write, std::uint32_t offset, std::uint32_t& value,
                                  unsigned len = 4, const unsigned char* be = nullptr, bool debug = false) {
        unsigned char data[4] = {};
        for (unsigned i = 0; i < len; ++i) 
            data[i] = (value >> (8 * i)) & 0xFF;
        tlm::tlm_generic_payload tx;

        tx.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
        tx.set_address(base[p] + offset);
        tx.set_data_ptr(data);
        tx.set_data_length(len);
        tx.set_streaming_width(len);

        if (be) {
            tx.set_byte_enable_ptr(const_cast<unsigned char*>(be));
            tx.set_byte_enable_length(len);
        }

        tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

#ifdef FU3_MAILBOX_WITH_BUS
        fu3_bus::ScopedAccess access(tx, len);
#endif

        auto& socket = p == kCva6 ? cva6 : ibex;
        if (debug) {
            socket->transport_dbg(tx);
        } else {
            socket->b_transport(tx, delay);
        }
        if (!write) {
            value = 0;
            for (unsigned i = 0; i < len; ++i) value |= std::uint32_t(data[i]) << (8 * i);
        }
        return tx.get_response_status();
    }

    static const char* name_of(Port p) { return p == kCva6 ? "CVA6" : "IBEX"; }

    std::uint32_t rd(Port p, std::uint32_t offset) {
        std::uint32_t v = 0;
        check(xfer(p, false, offset, v) == tlm::TLM_OK_RESPONSE, std::string(name_of(p)) + " read OK");
        return v;
    }
    void wr(Port p, std::uint32_t offset, std::uint32_t v) {
        check(xfer(p, true, offset, v) == tlm::TLM_OK_RESPONSE, std::string(name_of(p)) + " write OK");
    }
    bool rd_fails(Port p, std::uint32_t offset) {
        std::uint32_t v = 0;
        return xfer(p, false, offset, v) == tlm::TLM_GENERIC_ERROR_RESPONSE;
    }
    bool wr_fails(Port p, std::uint32_t offset, std::uint32_t v) {
        return xfer(p, true, offset, v) == tlm::TLM_GENERIC_ERROR_RESPONSE;
    }
    void settle() { sc_core::wait(1, sc_core::SC_NS); }
    void send(Port p, std::uint32_t v) { wr(p, FU3_MAILBOX_FIFO_WR_OFFSET, v); }
    std::uint32_t recv(Port p) { return rd(p, FU3_MAILBOX_FIFO_RD_OFFSET); }
    std::uint32_t status(Port p) { return rd(p, FU3_MAILBOX_STATUS_OFFSET); }
    std::uint32_t irq_status(Port p) { return rd(p, FU3_MAILBOX_IRQ_STATUS_OFFSET); }
    bool irq(Port p) { settle(); return p == kCva6 ? irq_cva6.read() : irq_ibex.read(); }
    void clear_irqs() {
        wr(kCva6, FU3_MAILBOX_IRQ_STATUS_OFFSET, ~0u);
        wr(kIbex, FU3_MAILBOX_IRQ_STATUS_OFFSET, ~0u);
    }

    void do_reset() {
        reset_n = false;
        settle();
        reset_n = true;
        settle();
    }

    void check_reset_state(const std::string& when) {
        for (Port p : {kCva6, kIbex}) {
            const std::string w = when + " " + name_of(p);
            check(status(p) == (FU3_MAILBOX_STATUS_EMPTY | FU3_MAILBOX_STATUS_SEND_THRESHOLD), w + ": STATUS = EMPTY | SEND_THRESHOLD");
            check(rd(p, FU3_MAILBOX_WR_THRESHOLDS_OFFSET) == 0, w + ": WR_THRESHOLDS = 0");
            check(rd(p, FU3_MAILBOX_RD_THRESHOLDS_OFFSET) == 0, w + ": RD_THRESHOLDS = 0");
            check(irq_status(p) == 0, w + ": IRQ_STATUS = 0");
            check(rd(p, FU3_MAILBOX_IRQ_ENABLE_OFFSET) == 0, w + ": IRQ_ENABLE = 0");
            check(rd(p, FU3_MAILBOX_IRQ_PENDING_OFFSET) == 0, w + ": IRQ_PENDING = 0");
            check(!irq(p), w + ": irq low");
        }
    }

    void test_fifo_both_ways() {
        for (Port from : {kCva6, kIbex}) {
            const Port to = from == kCva6 ? kIbex : kCva6;
            const std::string w = std::string(name_of(from)) + " -> " + name_of(to);
            send(from, 0x1111'0000u + from);
            send(from, 0x2222'0000u + from);
            send(from, 0x3333'0000u + from);
            check(!(status(to) & FU3_MAILBOX_STATUS_EMPTY), w + ": receiver not empty");
            check(status(from) & FU3_MAILBOX_STATUS_EMPTY, w + ": sender's own receive FIFO still empty");
            check(recv(to) == 0x1111'0000u + from, w + ": word 1 in order");
            check(recv(to) == 0x2222'0000u + from, w + ": word 2 in order");
            check(recv(to) == 0x3333'0000u + from, w + ": word 3 in order");
            check(status(to) & FU3_MAILBOX_STATUS_EMPTY, w + ": receiver empty again");
        }
        // Threshold 0: word đầu đến là sườn lên receive, FIFO gửi rút về 0 là sườn lên send
        check(irq_status(kCva6) == (FU3_MAILBOX_IRQ_WR_THRESHOLD | FU3_MAILBOX_IRQ_RD_THRESHOLD), "CVA6: send and receive threshold edges");
        check(irq_status(kIbex) == (FU3_MAILBOX_IRQ_WR_THRESHOLD | FU3_MAILBOX_IRQ_RD_THRESHOLD), "IBEX: send and receive threshold edges");
        check(!irq(kCva6) && !irq(kIbex), "IRQ_ENABLE 0: no irq");
        clear_irqs();
    }

    void test_full_and_errors() {
        for (unsigned i = 0; i < FU3_MAILBOX_DEPTH; ++i) {
            check(!(status(kCva6) & FU3_MAILBOX_STATUS_FULL), "not full before word " + std::to_string(i));
            send(kCva6, 0xA000'0000u + i);
        }
        check(status(kCva6) & FU3_MAILBOX_STATUS_FULL, "FULL after DEPTH words");
        clear_irqs();
        check(wr_fails(kCva6, FU3_MAILBOX_FIFO_WR_OFFSET, 0xDEAD'BEEFu), "write when full -> PSLVERR");
        check(irq_status(kCva6) == FU3_MAILBOX_IRQ_FIFO_WR_ERROR, "write when full sets FIFO_WR_ERROR only on the writer");
        check(irq_status(kIbex) == 0, "receiver IRQ_STATUS untouched by the writer's error");
        for (unsigned i = 0; i < FU3_MAILBOX_DEPTH; ++i)
            check(recv(kIbex) == 0xA000'0000u + i, "full FIFO drains in order, word " + std::to_string(i));
        clear_irqs();

        std::uint32_t v = 0x1234;
        check(xfer(kIbex, false, FU3_MAILBOX_FIFO_RD_OFFSET, v) == tlm::TLM_GENERIC_ERROR_RESPONSE, "read when empty -> PSLVERR");
        check(v == 0, "read when empty returns 0");
        check(irq_status(kIbex) == FU3_MAILBOX_IRQ_FIFO_RD_ERROR, "read when empty sets FIFO_RD_ERROR");

        // W1C: ghi 0 không đổi, ghi 1 bit chỉ xoá bit đó
        wr(kIbex, FU3_MAILBOX_IRQ_STATUS_OFFSET, 0);
        check(irq_status(kIbex) == FU3_MAILBOX_IRQ_FIFO_RD_ERROR, "W1C: writing 0 keeps the bit");
        wr(kIbex, FU3_MAILBOX_IRQ_STATUS_OFFSET, FU3_MAILBOX_IRQ_FIFO_WR_ERROR);
        check(irq_status(kIbex) == FU3_MAILBOX_IRQ_FIFO_RD_ERROR, "W1C: other bit keeps it");
        wr(kIbex, FU3_MAILBOX_IRQ_STATUS_OFFSET, FU3_MAILBOX_IRQ_FIFO_RD_ERROR);
        check(irq_status(kIbex) == 0, "W1C clears");
    }

    void test_receive_threshold() {
        // Ibex: RD_THRESHOLDS = 2 -> active khi có > 2 word
        wr(kIbex, FU3_MAILBOX_RD_THRESHOLDS_OFFSET, 2);
        wr(kIbex, FU3_MAILBOX_IRQ_ENABLE_OFFSET, FU3_MAILBOX_IRQ_RD_THRESHOLD);
        send(kCva6, 1);
        send(kCva6, 2);
        check(!(status(kIbex) & FU3_MAILBOX_STATUS_RECV_THRESHOLD), "2 words: receive threshold inactive");
        check(!irq(kIbex), "2 words: irq_ibex low");
        send(kCva6, 3);
        check(status(kIbex) & FU3_MAILBOX_STATUS_RECV_THRESHOLD, "3 words: receive threshold active");
        check(irq_status(kIbex) == FU3_MAILBOX_IRQ_RD_THRESHOLD, "rising edge sets RD_THRESHOLD_IRQ");
        check(rd(kIbex, FU3_MAILBOX_IRQ_PENDING_OFFSET) == FU3_MAILBOX_IRQ_RD_THRESHOLD, "IRQ_PENDING = STATUS & ENABLE");
        check(irq(kIbex), "irq_ibex high");
        check(!irq(kCva6), "irq_cva6 stays low");

        // Sticky: còn active, xoá xong không set lại vì không có sườn mới
        wr(kIbex, FU3_MAILBOX_IRQ_STATUS_OFFSET, FU3_MAILBOX_IRQ_RD_THRESHOLD);
        send(kCva6, 4);
        check(irq_status(kIbex) == 0, "still above threshold: no new edge, no IRQ");
        check(!irq(kIbex), "irq_ibex low after W1C");

        // Xuống dưới rồi lên lại -> sườn mới
        recv(kIbex);
        recv(kIbex);
        check(!(status(kIbex) & FU3_MAILBOX_STATUS_RECV_THRESHOLD), "2 words left: inactive");
        send(kCva6, 5);
        check(irq_status(kIbex) == FU3_MAILBOX_IRQ_RD_THRESHOLD, "New rising edge sets RD_THRESHOLD_IRQ again");

        // Tắt enable: status còn, pending và irq về 0
        wr(kIbex, FU3_MAILBOX_IRQ_ENABLE_OFFSET, 0);
        check(irq_status(kIbex) == FU3_MAILBOX_IRQ_RD_THRESHOLD, "Status kept with enable 0");
        check(rd(kIbex, FU3_MAILBOX_IRQ_PENDING_OFFSET) == 0, "Pending 0 with enable 0");
        check(!irq(kIbex), "irq_ibex low with enable 0");
        while (!(status(kIbex) & FU3_MAILBOX_STATUS_EMPTY)) recv(kIbex);
        wr(kIbex, FU3_MAILBOX_RD_THRESHOLDS_OFFSET, 0);
        wr(kIbex, FU3_MAILBOX_IRQ_STATUS_OFFSET, ~0u);
    }

    void test_send_threshold() {
        // CVA6: WR_THRESHOLDS = 1 -> active khi FIFO gửi có <= 1 word
        wr(kCva6, FU3_MAILBOX_WR_THRESHOLDS_OFFSET, 1);
        wr(kCva6, FU3_MAILBOX_IRQ_ENABLE_OFFSET, FU3_MAILBOX_IRQ_WR_THRESHOLD);
        clear_irqs();
        check(status(kCva6) & FU3_MAILBOX_STATUS_SEND_THRESHOLD, "Empty send FIFO: send threshold active");
        check(irq_status(kCva6) == 0, "Threshold write keeps an already active level: no edge");
        send(kCva6, 1);
        send(kCva6, 2);
        send(kCva6, 3);
        check(!(status(kCva6) & FU3_MAILBOX_STATUS_SEND_THRESHOLD), "3 words: send threshold inactive");
        check(!irq(kCva6), "IRQ_CVA6  low");
        recv(kIbex);
        check(irq_status(kCva6) == 0, "2 words left: still inactive");
        recv(kIbex);
        check(irq_status(kCva6) == FU3_MAILBOX_IRQ_WR_THRESHOLD, "Receiver drain to 1 word sets WR_THRESHOLD_IRQ on the sender");
        check(irq(kCva6), "IRQ_CVA6 high");
        check(!irq(kIbex), "IRQ_IBEX low");
        recv(kIbex);
        wr(kCva6, FU3_MAILBOX_IRQ_STATUS_OFFSET, ~0u);
        check(!irq(kCva6), "irq_cva6 low after W1C");

        // Ghi threshold làm mức lên cũng là sườn lên
        wr(kCva6, FU3_MAILBOX_WR_THRESHOLDS_OFFSET, 0);
        send(kCva6, 9);
        check(!(status(kCva6) & FU3_MAILBOX_STATUS_SEND_THRESHOLD), "1 word, threshold 0: inactive");
        wr(kCva6, FU3_MAILBOX_WR_THRESHOLDS_OFFSET, 5);
        check(irq_status(kCva6) == FU3_MAILBOX_IRQ_WR_THRESHOLD, "Raising WR_THRESHOLDS above the count is a rising edge");
        recv(kIbex);
        wr(kCva6, FU3_MAILBOX_IRQ_ENABLE_OFFSET, 0);
        wr(kCva6, FU3_MAILBOX_WR_THRESHOLDS_OFFSET, 0);
        wr(kCva6, FU3_MAILBOX_IRQ_STATUS_OFFSET, ~0u);
    }

    void test_registers() {
        wr(kIbex, FU3_MAILBOX_WR_THRESHOLDS_OFFSET, 0xFFFF'FFFFu);
        check(rd(kIbex, FU3_MAILBOX_WR_THRESHOLDS_OFFSET) == 0xFFFF'FFFFu, "WR_THRESHOLDS is 32-bit RW");
        wr(kIbex, FU3_MAILBOX_RD_THRESHOLDS_OFFSET, 0x8000'0001u);
        check(rd(kIbex, FU3_MAILBOX_RD_THRESHOLDS_OFFSET) == 0x8000'0001u, "RD_THRESHOLDS is 32-bit RW");
        check(rd(kCva6, FU3_MAILBOX_RD_THRESHOLDS_OFFSET) == 0, "Thresholds are per port");
        wr(kIbex, FU3_MAILBOX_IRQ_ENABLE_OFFSET, ~0u);
        check(rd(kIbex, FU3_MAILBOX_IRQ_ENABLE_OFFSET) == FU3_MAILBOX_IRQ_MASK, "IRQ_ENABLE only [31:28]");
        wr(kIbex, FU3_MAILBOX_IRQ_ENABLE_OFFSET, 0);
        wr(kIbex, FU3_MAILBOX_WR_THRESHOLDS_OFFSET, 0);
        wr(kIbex, FU3_MAILBOX_RD_THRESHOLDS_OFFSET, 0);
        wr(kIbex, FU3_MAILBOX_IRQ_STATUS_OFFSET, ~0u);
    }

    void test_invalid_access() {
        for (Port p : {kCva6, kIbex}) {
            const std::string w = name_of(p);
            check(rd_fails(p, FU3_MAILBOX_FIFO_WR_OFFSET), w + ": Read of FIFO_WR (WO) -> PSLVERR");
            check(wr_fails(p, FU3_MAILBOX_FIFO_RD_OFFSET, 1), w + ": Write of FIFO_RD (RO) -> PSLVERR");
            check(wr_fails(p, FU3_MAILBOX_STATUS_OFFSET, 0), w + ": Write of STATUS (RO) -> PSLVERR");
            check(wr_fails(p, FU3_MAILBOX_IRQ_PENDING_OFFSET, 0), w + ": Write of IRQ_PENDING (RO) -> PSLVERR");
            for (std::uint32_t off : {0x004u, 0x00Cu, 0x014u, 0x02Cu, 0xFFFCu}) {
                check(rd_fails(p, off), w + ": Read of unmapped " + std::to_string(off) + " -> PSLVERR");
                check(wr_fails(p, off, 0), w + ": Write of unmapped " + std::to_string(off) + " -> PSLVERR");
            }
            std::uint32_t v = 0x55;
            check(xfer(p, true, FU3_MAILBOX_FIFO_WR_OFFSET, v, 1) == tlm::TLM_GENERIC_ERROR_RESPONSE, w + ": Byte write -> PSLVERR");
            check(xfer(p, false, FU3_MAILBOX_STATUS_OFFSET, v, 2) == tlm::TLM_GENERIC_ERROR_RESPONSE, w + ": Halfword read -> PSLVERR");
            const unsigned char partial[4] = {TLM_BYTE_ENABLED, TLM_BYTE_DISABLED, TLM_BYTE_ENABLED, TLM_BYTE_ENABLED};
            check(xfer(p, true, FU3_MAILBOX_FIFO_WR_OFFSET, v, 4, partial) == tlm::TLM_GENERIC_ERROR_RESPONSE, w + ": Partial PSTRB -> PSLVERR");
            check(irq_status(p) == 0, w + ": Access errors do not set FIFO error IRQs");
        }
        check(status(kIbex) & FU3_MAILBOX_STATUS_EMPTY, "Rejected FIFO_WR pushed nothing");
    }

    void test_debug() {
        clear_irqs();
        send(kCva6, 0xCAFE'0001u);
        std::uint32_t v = 0;
        xfer(kIbex, false, FU3_MAILBOX_FIFO_RD_OFFSET, v, 4, nullptr, true);
        check(v == 0xCAFE'0001u, "Debug read of FIFO_RD peeks the head");
        check(recv(kIbex) == 0xCAFE'0001u, "Debug read does not pop");
        v = 0;
        xfer(kIbex, false, FU3_MAILBOX_FIFO_RD_OFFSET, v, 4, nullptr, true);
        check(v == 0 && !(irq_status(kIbex) & FU3_MAILBOX_IRQ_FIFO_RD_ERROR), "Debug read of empty FIFO_RD returns 0, no error flag");
        v = 7;
        xfer(kCva6, true, FU3_MAILBOX_FIFO_WR_OFFSET, v, 4, nullptr, true);
        check(status(kIbex) & FU3_MAILBOX_STATUS_EMPTY, "Debug write is ignored");
    }

    void test_reset() {
        send(kCva6, 1);
        send(kIbex, 2);
        wr(kIbex, FU3_MAILBOX_RD_THRESHOLDS_OFFSET, 3);
        wr(kCva6, FU3_MAILBOX_IRQ_ENABLE_OFFSET, FU3_MAILBOX_IRQ_FIFO_RD_ERROR);
        check(rd_fails(kCva6, FU3_MAILBOX_FIFO_RD_OFFSET) == false, "CVA6 has the Ibex word");
        check(rd_fails(kCva6, FU3_MAILBOX_FIFO_RD_OFFSET), "CVA6 empty read");
        check(irq(kCva6), "IRQ_CVA6 high before reset");
        send(kCva6, 3);

        reset_n = false;
        settle();
        check(!irq_cva6.read(), "IRQ low while in reset");
        std::uint32_t v = 9;
        check(xfer(kCva6, true, FU3_MAILBOX_FIFO_WR_OFFSET, v) == tlm::TLM_OK_RESPONSE, "Write during reset: OK, ignored");
        check(xfer(kIbex, false, FU3_MAILBOX_FIFO_RD_OFFSET, v) == tlm::TLM_OK_RESPONSE && v == 0, "Read during reset: OK, 0");
        reset_n = true;
        settle();
        check_reset_state("After reset");
    }

    void run() {
        settle();
        check_reset_state("Power-on");
        test_fifo_both_ways();
        test_full_and_errors();
        test_receive_threshold();
        test_send_threshold();
        test_registers();
        test_invalid_access();
        test_debug();
        test_reset();
        sc_core::sc_stop();
    }
};
