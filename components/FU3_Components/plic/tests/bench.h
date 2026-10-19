#pragma once
// Test thanh ghi, gateway level/edge, claim/complete, priority/threshold/IE, reset và lỗi truy cập
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
    unsigned claim() { return rd(FU3_PLIC_ID_0_OFFSET); }
    void complete(unsigned id) { wr(FU3_PLIC_ID_0_OFFSET, id); settle(); }
    void set_priority(unsigned id, unsigned prio) {
        const std::uint32_t off = FU3_PLIC_PRIORITY_OFFSET(id);
        const std::uint32_t shift = FU3_PLIC_PRIORITY_SHIFT(id);
        wr(off, (rd(off) & ~(0xFu << shift)) | (prio << shift));
    }
    void raise(unsigned id, bool level) { src[id - 1] = level; settle(); }
    void pulse(unsigned id) { raise(id, true); raise(id, false); }
    void clear_all() {
        for (auto& s : src) 
            s = false;
        settle();

        for (unsigned id = 1; id <= kSources; ++id)
            complete(id);

        // Xả pending level đã chốt từ test trước: bật hết, claim + complete tới khi trống
        for (std::uint32_t off = FU3_PLIC_PRIORITY_0_OFFSET; off <= FU3_PLIC_PRIORITY_3_OFFSET; off += 4)
            wr(off, 0x1111'1111);
        wr(FU3_PLIC_IE_0_OFFSET, ~0u);
        wr(FU3_PLIC_THRESHOLD_0_OFFSET, 0);
        settle();

        for (unsigned id = claim(); id != 0; id = claim())
            complete(id);

        for (std::uint32_t off = FU3_PLIC_EL_OFFSET; off <= FU3_PLIC_THRESHOLD_0_OFFSET; off += 4) 
            wr(off, 0);
        settle();

        while (claim() != 0) {}

        for (unsigned id = 1; id <= kSources; ++id) 
            complete(id);
    }

    void run() {
        settle();

        // Reset value
        check(rd(FU3_PLIC_CONFIG_0_OFFSET) == 0x0001'0020, "CONFIG_0 = 32 sources, 1 target");
        check(rd(FU3_PLIC_CONFIG_1_OFFSET) == 0x0108'0008, "CONFIG_1 = 8 priorities, 8 pending, threshold");
        for (std::uint32_t off = FU3_PLIC_EL_OFFSET; off <= FU3_PLIC_THRESHOLD_0_OFFSET; off += 4)
            check(rd(off) == 0, "Reset Value");
        check(claim() == 0, "Claim with nothing pending returns 0");
        check(!irq.read(), "irq low after reset");

        // Mask rw/ro
        struct { std::uint32_t off, wmask; } regs[] = {
            {FU3_PLIC_EL_OFFSET, 0xFFFF'FFFF},
            {FU3_PLIC_PRIORITY_0_OFFSET, 0x7777'7777}, {FU3_PLIC_PRIORITY_1_OFFSET, 0x7777'7777},
            {FU3_PLIC_PRIORITY_2_OFFSET, 0x7777'7777}, {FU3_PLIC_PRIORITY_3_OFFSET, 0x7777'7777},
            {FU3_PLIC_IE_0_OFFSET, 0xFFFF'FFFF}, {FU3_PLIC_THRESHOLD_0_OFFSET, 0x7},
        };
        for (auto& r : regs) {
            wr(r.off, ~0u);
            check(rd(r.off) == r.wmask, "Write ones");
            wr(r.off, 0);
            check(rd(r.off) == 0, "Write zeros");
        }
        wr(FU3_PLIC_CONFIG_0_OFFSET, 0);
        wr(FU3_PLIC_CONFIG_1_OFFSET, 0);
        check(rd(FU3_PLIC_CONFIG_0_OFFSET) == 0x0001'0020 && rd(FU3_PLIC_CONFIG_1_OFFSET) == 0x0108'0008, "CONFIG RO");
        wr(0x28, ~0u);
        check(rd(0x28) == 0, "Unmapped RAZ/WI");

        // Level: claim, in-service, complete
        set_priority(FU3_PLIC_ID_TIMER1, 3);
        wr(FU3_PLIC_IE_0_OFFSET, FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_TIMER1));
        raise(FU3_PLIC_ID_TIMER1, true);
        check(irq.read(), "Level source raises irq");
        check(rd(FU3_PLIC_ID_0_OFFSET) == FU3_PLIC_ID_TIMER1, "Claim returns TIMER1 ID");
        settle();
        check(!irq.read(), "irq drops after claim");
        check(claim() == 0, "In-service source is not claimed again");
        complete(FU3_PLIC_ID_TIMER1);
        check(irq.read(), "Still-high level source pends again after complete");
        check(claim() == FU3_PLIC_ID_TIMER1, "Second claim");
        raise(FU3_PLIC_ID_TIMER1, false);
        complete(FU3_PLIC_ID_TIMER1);
        check(!irq.read() && claim() == 0, "Source low after complete: nothing pending");

        // Level pulse ngắn (như TIMER FU3: IRQ chỉ lên 1 tick) được chốt tới khi claim
        raise(FU3_PLIC_ID_TIMER1, true);
        raise(FU3_PLIC_ID_TIMER1, false);
        
        // check(!irq.read() && claim() == 0, "Level pulse dropped before claim is not kept");
        // Không giữ xung thì CPU mất IRQ 1 tick của TIMER FU3 (test end-to-end CVA6 + PLIC + TIMER1)
        check(irq.read(), "Level pulse is latched until claim");
        check(claim() == FU3_PLIC_ID_TIMER1, "Latched level source is claimed after the source fell");
        complete(FU3_PLIC_ID_TIMER1);
        check(!irq.read() && claim() == 0, "Source low at complete: nothing pending");

        // Priority arbitration, ties to lowest ID
        clear_all();
        set_priority(FU3_PLIC_ID_UART0, 2);
        set_priority(FU3_PLIC_ID_SPI0, 5);
        set_priority(FU3_PLIC_ID_GPIOA, 5);
        wr(FU3_PLIC_IE_0_OFFSET, FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_UART0) | FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_SPI0) |
                                 FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_GPIOA));
        raise(FU3_PLIC_ID_UART0, true);
        raise(FU3_PLIC_ID_SPI0, true);
        raise(FU3_PLIC_ID_GPIOA, true);
        check(claim() == FU3_PLIC_ID_SPI0, "Highest priority, lowest ID first");
        check(claim() == FU3_PLIC_ID_GPIOA, "Same priority, next ID");
        check(claim() == FU3_PLIC_ID_UART0, "Lowest priority last");
        check(claim() == 0, "All in service");

        // Threshold, 7. IE, 8. priority 0
        clear_all();
        set_priority(FU3_PLIC_ID_DMA0, 3);
        wr(FU3_PLIC_THRESHOLD_0_OFFSET, 3);
        wr(FU3_PLIC_IE_0_OFFSET, FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_DMA0));
        raise(FU3_PLIC_ID_DMA0, true);
        check(!irq.read() && claim() == 0, "priority == threshold is masked");
        wr(FU3_PLIC_THRESHOLD_0_OFFSET, 2);
        settle();
        check(irq.read(), "priority > threshold interrupts");
        wr(FU3_PLIC_IE_0_OFFSET, 0);
        settle();
        check(!irq.read() && claim() == 0, "IE=0 masks the source");
        check(dut.pending() & FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_DMA0), "Masked source is still pending");
        wr(FU3_PLIC_IE_0_OFFSET, FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_DMA0));
        wr(FU3_PLIC_THRESHOLD_0_OFFSET, 0);
        set_priority(FU3_PLIC_ID_DMA0, 0);
        settle();
        check(!irq.read() && claim() == 0, "Priority 0 never interrupts");

        // Edge: queue and saturation at MAX_PENDING_COUNT
        clear_all();
        set_priority(FU3_PLIC_ID_AES, 1);
        wr(FU3_PLIC_EL_OFFSET, FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_AES));
        wr(FU3_PLIC_IE_0_OFFSET, FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_AES));
        for (int i = 0; i < 3; ++i) pulse(FU3_PLIC_ID_AES);
        check(irq.read(), "Edge pulses raise irq after the source returned low");
        unsigned served = 0;
        while (claim() == FU3_PLIC_ID_AES) { ++served; complete(FU3_PLIC_ID_AES); }
        check(served == 3, "Three edges give three claims");
        for (int i = 0; i < 10; ++i) pulse(FU3_PLIC_ID_AES);
        served = 0;
        while (claim() == FU3_PLIC_ID_AES) { ++served; complete(FU3_PLIC_ID_AES); }
        check(served == kMaxPending, "Edge count saturates at MAX_PENDING_COUNT");
        
        // Edge during in-service is queued
        pulse(FU3_PLIC_ID_AES);
        check(claim() == FU3_PLIC_ID_AES, "Edge claim");
        pulse(FU3_PLIC_ID_AES);
        check(claim() == 0, "No second claim while in service");
        complete(FU3_PLIC_ID_AES);
        check(claim() == FU3_PLIC_ID_AES, "Edge during service is served after complete");
        complete(FU3_PLIC_ID_AES);

        // Complete wrong/unclaimed ID is ignored
        clear_all();
        set_priority(FU3_PLIC_ID_QSPI, 4);
        wr(FU3_PLIC_IE_0_OFFSET, FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_QSPI));
        raise(FU3_PLIC_ID_QSPI, true);
        check(claim() == FU3_PLIC_ID_QSPI, "Claim QSPI");
        complete(FU3_PLIC_ID_DMA0);
        complete(0);
        complete(33);
        check(!irq.read() && claim() == 0, "Wrong-ID complete does not release QSPI");
        complete(FU3_PLIC_ID_QSPI);
        check(irq.read(), "Correct complete releases QSPI");

        // Byte lane / PSTRB
        clear_all();
        std::uint32_t v = 0x5;
        check(xfer(true, FU3_PLIC_PRIORITY_1_OFFSET + 2, v, 1) == tlm::TLM_OK_RESPONSE, "Byte Write");
        check(rd(FU3_PLIC_PRIORITY_1_OFFSET) == 0x0005'0000, "Byte lane 2 = ID 13");
        const unsigned char strb[4] = {0xFF, 0x00, 0x00, 0xFF};
        v = 0x7777'7777;
        check(xfer(true, FU3_PLIC_PRIORITY_1_OFFSET, v, 4, strb) == tlm::TLM_OK_RESPONSE, "PSTRB Write");
        check(rd(FU3_PLIC_PRIORITY_1_OFFSET) == 0x7705'0077, "PSTRB lanes 0,3");

        // Debug read of ID_0 does not claim
        clear_all();
        set_priority(FU3_PLIC_ID_RNG, 6);
        wr(FU3_PLIC_IE_0_OFFSET, FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_RNG));
        raise(FU3_PLIC_ID_RNG, true);
        v = 0;
        xfer(false, FU3_PLIC_ID_0_OFFSET, v, 4, nullptr, true);
        check(v == FU3_PLIC_ID_RNG && irq.read(), "Debug read peeks without claiming");
        check(claim() == FU3_PLIC_ID_RNG, "Functional read claims");

        // reset_n
        reset_n = false;
        settle();
        check(!irq.read(), "Reset deasserts irq");
        check(rd(FU3_PLIC_IE_0_OFFSET) == 0 && rd(FU3_PLIC_PRIORITY_2_OFFSET) == 0, "Reset clears registers");
        wr(FU3_PLIC_IE_0_OFFSET, ~0u);
        check(rd(FU3_PLIC_IE_0_OFFSET) == 0, "Write ignored in reset");
        check(claim() == 0, "No claim in reset");
        reset_n = true;
        settle();
        set_priority(FU3_PLIC_ID_RNG, 6);
        wr(FU3_PLIC_IE_0_OFFSET, FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_RNG));
        settle();
        check(claim() == FU3_PLIC_ID_RNG, "Reset clears in-service state");

        // Lỗi truy cập
        if (base == 0) {   // Bus FU3 tự chặn các lỗi này trước khi tới PLIC
            check(xfer(false, 0x11, v, 2) == tlm::TLM_ADDRESS_ERROR_RESPONSE, "Misaligned Half");
            check(xfer(false, 0x10, v, 3) == tlm::TLM_BURST_ERROR_RESPONSE, "Length: 3");
            check(xfer(false, 0x10000, v) == tlm::TLM_ADDRESS_ERROR_RESPONSE, "Outside 64 KiB");
        }
        sc_core::sc_stop();
    }
};
