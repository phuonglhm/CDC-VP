// Test memory target for the CSI-2 DMA: a DDR window filled with a sentinel,
// a fixed latency per write, an optional hook to delay or fail a write (M4b),
// and a log of every write with its AXI attributes (Csi2AxiExtension).
#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include "fx1_csi2/csi2_rx_controller.h"

struct TlmMem : sc_core::sc_module {
    tlm_utils::simple_target_socket<TlmMem> socket{"socket"};
    struct Write {
        uint64_t addr;
        unsigned len;
        uint8_t awid, awlen, last_wstrb;
        sc_core::sc_time start, end;
    };
    uint64_t base;
    std::vector<uint8_t> mem;
    sc_core::sc_time latency;
    std::vector<Write> log;
    // Called in the target's b_transport before the write lands; may wait()
    // and may return a TLM error status instead of OK.
    std::function<tlm::tlm_response_status(tlm::tlm_generic_payload&)> hook;

    TlmMem(sc_core::sc_module_name nm, uint64_t base_, size_t size, sc_core::sc_time lat = sc_core::sc_time(20, sc_core::SC_NS))
        : sc_core::sc_module(nm), base(base_), mem(size, 0xA5), latency(lat) {
        socket.register_b_transport(this, &TlmMem::b_transport);
    }
    void b_transport(tlm::tlm_generic_payload& p, sc_core::sc_time& d) {
        const sc_core::sc_time t0 = sc_core::sc_time_stamp();
        wait(d + latency);
        d = sc_core::SC_ZERO_TIME;
        tlm::tlm_response_status st = tlm::TLM_OK_RESPONSE;
        if (hook) st = hook(p);
        const uint64_t a = p.get_address();
        if (st == tlm::TLM_OK_RESPONSE && (a < base || a + p.get_data_length() > base + mem.size()))
            st = tlm::TLM_ADDRESS_ERROR_RESPONSE;
        if (st == tlm::TLM_OK_RESPONSE && p.is_write())
            std::copy(p.get_data_ptr(), p.get_data_ptr() + p.get_data_length(), mem.begin() + (a - base));
        Write w{a, p.get_data_length(), 0, 0, 0xFF, t0, sc_core::sc_time_stamp()};
        if (auto* e = p.get_extension<fx1::csi2::Csi2AxiExtension>()) {
            w.awid = e->awid;
            w.awlen = e->awlen;
            w.last_wstrb = e->last_wstrb;
        }
        log.push_back(w);
        p.set_response_status(st);
    }
    std::vector<uint8_t> read(uint64_t a, size_t n) const {
        return std::vector<uint8_t>(mem.begin() + (a - base), mem.begin() + (a - base) + n);
    }
    bool untouched(uint64_t a, size_t n) const {
        for (size_t i = 0; i < n; ++i)
            if (mem[a - base + i] != 0xA5) return false;
        return true;
    }
};
