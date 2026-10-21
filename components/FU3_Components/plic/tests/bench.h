#pragma once
// Test thanh ghi, gateway level, claim/complete, priority/threshold/enable, reset và lỗi truy cập
// Dùng chung cho behavior.cpp (nối thẳng) và integration.cpp (qua bus FU3, định nghĩa FU3_PLIC_WITH_BUS)
#include <fu3_plic/plic.h>

#include <iostream>
#include <tlm_utils/simple_initiator_socket.h>

#ifdef FU3_PLIC_WITH_BUS
    #include <fu3_bus/access.h>
    #include <fu3_bus/bus_system.h>
#endif

using namespace fu3_plic;

struct Bench : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<Bench> cpu{"cpu"};
    Plic dut{"dut"};
    sc_core::sc_signal<bool> reset_n{"reset_n", true};
    sc_core::sc_vector<sc_core::sc_signal<bool>> src{"src", kSources};
    sc_core::sc_signal<bool> irq{"irq"};
    std::uint64_t base = 0;
    unsigned checks = 0, errors = 0;

#ifdef FU3_PLIC_WITH_BUS
    fu3_bus::BusSystem* fabric = nullptr;
#endif

    SC_HAS_PROCESS(Bench);
    explicit Bench(sc_core::sc_module_name n, bool via_bus) : sc_module(n) {
        dut.reset_n(reset_n);
        for (unsigned k = 0; k < kSources; ++k) dut.irq_sources[k](src[k]);
        dut.irq(irq);

#ifdef FU3_PLIC_WITH_BUS
        if (via_bus) {
            auto cfg = fu3_bus::BusConfig::fu3();

            for (auto& t : cfg.targets)
                t.enabled = t.name == "AP_PLIC";

            fabric = new fu3_bus::BusSystem("fabric", cfg);
            cpu.bind(fabric->initiator("CVA6"));
            fabric->target("AP_PLIC").bind(dut.socket);
            base = FU3_PLIC_BASE;
        } else
#endif

        {
            (void)via_bus;
            cpu.bind(dut.socket);
        }
        SC_THREAD(run);
    }

    void check(bool ok, const char* what) {
        ++checks;
        if (!ok) {
            ++errors;
            std::cerr << "FAIL: " << what << '\n';
        }
    }

    tlm::tlm_response_status xfer(bool write, std::uint32_t offset, std::uint32_t& value,
                                  unsigned len = 4, const unsigned char* be = nullptr, bool debug = false) {
        unsigned char data[4] = {};
        for (unsigned i = 0; i < len; ++i) data[i] = (value >> (8 * i)) & 0xFF;
        tlm::tlm_generic_payload tx;

        tx.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
        tx.set_address(base + offset);
        tx.set_data_ptr(data);
        tx.set_data_length(len);
        tx.set_streaming_width(len);

        if (be) {
            tx.set_byte_enable_ptr(const_cast<unsigned char*>(be));
            tx.set_byte_enable_length(len);
        }

        tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

#ifdef FU3_PLIC_WITH_BUS
        fu3_bus::ScopedAccess access(tx, len);
#endif

        if (debug) {
            cpu->transport_dbg(tx);
        } else {
            cpu->b_transport(tx, delay);
        }
        if (!write) {
            value = 0;
            for (unsigned i = 0; i < len; ++i) value |= std::uint32_t(data[i]) << (8 * i);
        }
        return tx.get_response_status();
    }
    std::uint32_t rd(std::uint32_t offset) {
        std::uint32_t v = 0;
        check(xfer(false, offset, v) == tlm::TLM_OK_RESPONSE, "Read OK");
        return v;
    }
    void wr(std::uint32_t offset, std::uint32_t v) {
        check(xfer(true, offset, v) == tlm::TLM_OK_RESPONSE, "Write OK");
    }
    void settle() { sc_core::wait(1, sc_core::SC_NS); }
    unsigned claim() { return rd(FU3_PLIC_CLAIM_OFFSET); }
    void complete(unsigned id) { wr(FU3_PLIC_CLAIM_OFFSET, id); settle(); }
    void set_priority(unsigned id, unsigned prio) { wr(FU3_PLIC_PRIORITY_OFFSET(id), prio); }
    void raise(unsigned id, bool level) { src[id - 1] = level; settle(); }
    void clear_all() {
        for (auto& s : src) s = false;
        settle();
        for (unsigned id = 1; id <= kSources; ++id) complete(id);

        // Xả pending đã chốt từ test trước: bật hết, claim + complete tới khi trống
        for (unsigned id = 1; id <= kSources; ++id) set_priority(id, 1);
        wr(FU3_PLIC_ENABLE_OFFSET, FU3_PLIC_ENABLE_MASK);
        wr(FU3_PLIC_THRESHOLD_OFFSET, 0);
        settle();
        for (unsigned id = claim(); id != 0; id = claim()) complete(id);

        for (unsigned id = 1; id <= kSources; ++id) set_priority(id, 0);
        wr(FU3_PLIC_ENABLE_OFFSET, 0);
        settle();
    }

    void run() {
        settle();

        // Reset value
        for (unsigned id = 0; id <= kSources; ++id)
            check(rd(FU3_PLIC_PRIORITY_OFFSET(id)) == 0, "Priority reset 0");
        check(rd(FU3_PLIC_PENDING_OFFSET) == 0, "Pending reset 0");
        check(rd(FU3_PLIC_ENABLE_OFFSET) == 0, "Enable reset 0");
        check(rd(FU3_PLIC_THRESHOLD_OFFSET) == 0, "Threshold reset 0");
        check(claim() == 0, "Claim with nothing pending returns 0");
        check(!irq.read(), "irq low after reset");

        // Mask rw/ro
        for (unsigned id = 1; id <= kSources; ++id) {
            wr(FU3_PLIC_PRIORITY_OFFSET(id), ~0u);
            check(rd(FU3_PLIC_PRIORITY_OFFSET(id)) == FU3_PLIC_MAX_PRIORITY, "Priority 3 bit");
            wr(FU3_PLIC_PRIORITY_OFFSET(id), 0);
        }
        wr(FU3_PLIC_PRIORITY_OFFSET(0), ~0u);
        check(rd(FU3_PLIC_PRIORITY_OFFSET(0)) == 0, "ID 0 priority reads zero");
        wr(FU3_PLIC_ENABLE_OFFSET, ~0u);
        check(rd(FU3_PLIC_ENABLE_OFFSET) == FU3_PLIC_ENABLE_MASK, "Enable bits 1-23, bit 0 fixed 0");
        wr(FU3_PLIC_ENABLE_OFFSET, 0);
        wr(FU3_PLIC_THRESHOLD_OFFSET, ~0u);
        check(rd(FU3_PLIC_THRESHOLD_OFFSET) == 0x7, "Threshold 3 bit");
        wr(FU3_PLIC_THRESHOLD_OFFSET, 0);
        wr(FU3_PLIC_PENDING_OFFSET, ~0u);
        check(rd(FU3_PLIC_PENDING_OFFSET) == 0, "Pending RO");

        // Level: Pending bitmap, Claim, In-service, Complete
        set_priority(FU3_PLIC_ID_TIMER1, 3);
        wr(FU3_PLIC_ENABLE_OFFSET, FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_TIMER1));
        raise(FU3_PLIC_ID_TIMER1, true);
        check(rd(FU3_PLIC_PENDING_OFFSET) == FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_TIMER1), "Pending bit n = ID n");
        check(irq.read(), "Level source raises irq");
        check(claim() == FU3_PLIC_ID_TIMER1, "Claim returns TIMER1 ID");
        settle();
        check(!irq.read(), "irq drops after claim");
        check(rd(FU3_PLIC_PENDING_OFFSET) == 0, "Claim clears pending");
        check(claim() == 0, "In-service source is not claimed again");
        complete(FU3_PLIC_ID_TIMER1);
        check(irq.read(), "Still-high level source pends again after complete");
        check(claim() == FU3_PLIC_ID_TIMER1, "Second claim");
        raise(FU3_PLIC_ID_TIMER1, false);
        complete(FU3_PLIC_ID_TIMER1);
        check(!irq.read() && claim() == 0, "Source low after complete: nothing pending");

        // Xung level ngắn (TIMER FU3: IRQ chỉ lên 1 tick) được chốt tới khi claim
        raise(FU3_PLIC_ID_TIMER1, true);
        raise(FU3_PLIC_ID_TIMER1, false);
        check(irq.read(), "Level pulse is latched until claim");
        check(claim() == FU3_PLIC_ID_TIMER1, "Latched level source is claimed after the source fell");
        complete(FU3_PLIC_ID_TIMER1);
        check(!irq.read() && claim() == 0, "Source low at complete: nothing pending");

        // Priority arbitration, ties to lowest ID
        clear_all();
        set_priority(FU3_PLIC_ID_UART0, 2);
        set_priority(FU3_PLIC_ID_SPI0, 5);
        set_priority(FU3_PLIC_ID_GPIOA, 5);
        wr(FU3_PLIC_ENABLE_OFFSET, FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_UART0) | FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_SPI0) |
                                   FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_GPIOA));
        raise(FU3_PLIC_ID_UART0, true);
        raise(FU3_PLIC_ID_SPI0, true);
        raise(FU3_PLIC_ID_GPIOA, true);
        check(claim() == FU3_PLIC_ID_SPI0, "Highest priority, lowest ID first");
        check(claim() == FU3_PLIC_ID_GPIOA, "Same priority, next ID");
        check(claim() == FU3_PLIC_ID_UART0, "Lowest priority last");
        check(claim() == 0, "All in service");

        // Threshold, Enable, Priority 0
        clear_all();
        set_priority(FU3_PLIC_ID_DMA0, 3);
        wr(FU3_PLIC_THRESHOLD_OFFSET, 3);
        wr(FU3_PLIC_ENABLE_OFFSET, FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_DMA0));
        raise(FU3_PLIC_ID_DMA0, true);
        check(!irq.read() && claim() == 0, "priority == threshold is masked");
        wr(FU3_PLIC_THRESHOLD_OFFSET, 2);
        settle();
        check(irq.read(), "priority > threshold interrupts");
        set_priority(FU3_PLIC_ID_DMA0, 7);
        wr(FU3_PLIC_THRESHOLD_OFFSET, 7);
        settle();
        check(!irq.read() && claim() == 0, "Threshold 7 masks priority 7");
        wr(FU3_PLIC_THRESHOLD_OFFSET, 0);
        wr(FU3_PLIC_ENABLE_OFFSET, 0);
        settle();
        check(!irq.read() && claim() == 0, "Enable 0 masks the source");
        check(rd(FU3_PLIC_PENDING_OFFSET) & FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_DMA0), "Masked source is still pending");
        wr(FU3_PLIC_ENABLE_OFFSET, FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_DMA0));
        set_priority(FU3_PLIC_ID_DMA0, 0);
        settle();
        check(!irq.read() && claim() == 0, "Priority 0 never interrupts");

        // Complete sai ID / chưa claim / khác 1-23 bị bỏ qua
        clear_all();
        set_priority(FU3_PLIC_ID_QSPI, 4);
        wr(FU3_PLIC_ENABLE_OFFSET, FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_QSPI));
        raise(FU3_PLIC_ID_QSPI, true);
        check(claim() == FU3_PLIC_ID_QSPI, "Claim QSPI");
        complete(FU3_PLIC_ID_DMA0);
        complete(0);
        complete(24);
        complete(31);
        check(!irq.read() && claim() == 0, "Wrong-ID complete does not release QSPI");
        complete(FU3_PLIC_ID_QSPI);
        check(irq.read(), "Correct complete releases QSPI");

        // Debug read of CLAIM does not claim
        clear_all();
        set_priority(FU3_PLIC_ID_RNG, 6);
        wr(FU3_PLIC_ENABLE_OFFSET, FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_RNG));
        raise(FU3_PLIC_ID_RNG, true);
        std::uint32_t v = 0;
        xfer(false, FU3_PLIC_CLAIM_OFFSET, v, 4, nullptr, true);
        check(v == FU3_PLIC_ID_RNG && irq.read(), "Debug read peeks without claiming");
        check(claim() == FU3_PLIC_ID_RNG, "Functional read claims");

        // reset_n
        reset_n = false;
        settle();
        check(!irq.read(), "Reset deasserts irq");
        check(rd(FU3_PLIC_ENABLE_OFFSET) == 0 && rd(FU3_PLIC_PRIORITY_OFFSET(FU3_PLIC_ID_RNG)) == 0,
              "Reset clears registers");
        wr(FU3_PLIC_ENABLE_OFFSET, ~0u);
        check(rd(FU3_PLIC_ENABLE_OFFSET) == 0, "Write ignored in reset");
        check(claim() == 0, "No claim in reset");
        reset_n = true;
        settle();
        set_priority(FU3_PLIC_ID_RNG, 6);
        wr(FU3_PLIC_ENABLE_OFFSET, FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_RNG));
        settle();
        check(claim() == FU3_PLIC_ID_RNG, "Reset clears in-service state");

        // Lỗi truy cập: offset không map và ghi thiếu PSTRB => PSLVERR
        v = 0;
        check(xfer(false, FU3_PLIC_PRIORITY_OFFSET(24), v) == tlm::TLM_GENERIC_ERROR_RESPONSE, "Priority ID 24 unmapped");
        check(xfer(true, 0x1004, v) == tlm::TLM_GENERIC_ERROR_RESPONSE, "0x1004 unmapped");
        check(xfer(false, 0x3008, v) == tlm::TLM_GENERIC_ERROR_RESPONSE, "0x3008 unmapped");
        const unsigned char strb[4] = {0xFF, 0x00, 0x00, 0x00};
        v = 0x7;
        check(xfer(true, FU3_PLIC_THRESHOLD_OFFSET, v, 4, strb) == tlm::TLM_GENERIC_ERROR_RESPONSE, "Partial PSTRB rejected");
        check(rd(FU3_PLIC_THRESHOLD_OFFSET) == 0, "Partial write has no effect");
        if (base == 0) {   // Bus FU3 tự chặn các lỗi này trước khi tới PLIC
            check(xfer(false, 0x6, v, 2) == tlm::TLM_BURST_ERROR_RESPONSE, "Halfword rejected");
            check(xfer(false, 0x2, v) == tlm::TLM_ADDRESS_ERROR_RESPONSE, "Misaligned word");
            check(xfer(false, 0x10000, v) == tlm::TLM_ADDRESS_ERROR_RESPONSE, "Outside 64 KiB");
        }
        sc_core::sc_stop();
    }
};
