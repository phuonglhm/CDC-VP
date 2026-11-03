// =============================================================================
// Integrated Circuits and Systems laboratory
// Author: Hung Nguyen <ngthung@hcmut.edu.vn>
// Module: vp.common.memory_tlm
// Description: Sparse LT memory target (DDR model) with byte enables.
// Changelog:
//   2026-10-19: Write error window with an extra response delay (a slow B).
//   2026-09-29: Write-only error window (a descriptor readable but not
//               writable); taken into the Ethernet MAC VP.
//   2026-09-28: Initial version.
// =============================================================================
//
// A byte-addressed memory of any size, allocated in 64 KiB pages on first
// write (unwritten bytes read zero), so a 40-bit DDR window costs only what is
// touched.  b_transport honours byte enables and streaming width, and
// annotates `latency + beats * beat_time`.  transport_dbg is the untimed
// loader/inspection path (ELF loading, host DMA, test oracles).  Optional
// error windows (any access, or writes only) return TLM_GENERIC_ERROR_RESPONSE,
// standing in for an AXI SLVERR so DMA error handling can be exercised.
//
// DMI is not granted: pages are not contiguous.  The DMA masters move whole
// bursts per transaction, so there is nothing for DMI to save here.
#pragma once

#include <cstdint>
#include <cstring>
#include <memory>
#include <unordered_map>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

namespace vp {

class MemoryTlm : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<MemoryTlm> socket;

    MemoryTlm(sc_core::sc_module_name name, uint64_t size,
              sc_core::sc_time latency = sc_core::sc_time(50, sc_core::SC_NS),
              sc_core::sc_time beat_time = sc_core::sc_time(10, sc_core::SC_NS),
              unsigned beat_bytes = 16)
        : sc_module(name), socket("socket"), size_(size), latency_(latency),
          beat_time_(beat_time), beat_bytes_(beat_bytes) {
        socket.register_b_transport(this, &MemoryTlm::b_transport);
        socket.register_transport_dbg(this, &MemoryTlm::transport_dbg);
    }

    uint64_t size() const { return size_; }

    // Untimed host access (also used by transport_dbg).
    void read(uint64_t addr, void* dst, size_t n) const {
        auto* d = static_cast<uint8_t*>(dst);
        while (n) {
            const uint64_t pg = addr >> kPageBits, off = addr & (kPage - 1);
            const size_t k = size_t(std::min<uint64_t>(n, kPage - off));
            auto it = pages_.find(pg);
            if (it == pages_.end()) std::memset(d, 0, k);
            else std::memcpy(d, it->second.get() + off, k);
            addr += k;
            d += k;
            n -= k;
        }
    }
    void write(uint64_t addr, const void* src, size_t n) {
        auto* s = static_cast<const uint8_t*>(src);
        while (n) {
            const uint64_t pg = addr >> kPageBits, off = addr & (kPage - 1);
            const size_t k = size_t(std::min<uint64_t>(n, kPage - off));
            std::memcpy(page(pg) + off, s, k);
            addr += k;
            s += k;
            n -= k;
        }
    }
    // Accesses touching [base, base+size) answer with an error response.
    void set_error_window(uint64_t base, uint64_t size) { err_base_ = base; err_size_ = size; }
    // Writes touching [base, base+size) answer with an error; reads succeed.
    // `extra` holds the error response back that much longer (a slow B).
    void set_write_error_window(uint64_t base, uint64_t size,
                                sc_core::sc_time extra = sc_core::SC_ZERO_TIME) {
        werr_base_ = base;
        werr_size_ = size;
        werr_delay_ = extra;
    }
    uint64_t transactions() const { return transactions_; }

private:
    static constexpr unsigned kPageBits = 16;
    static constexpr uint64_t kPage = uint64_t(1) << kPageBits;

    uint8_t* page(uint64_t pg) {
        auto& p = pages_[pg];
        if (!p) {
            p.reset(new uint8_t[kPage]);
            std::memset(p.get(), 0, kPage);
        }
        return p.get();
    }

    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
        ++transactions_;
        const uint64_t a = tx.get_address();
        const unsigned len = tx.get_data_length();
        unsigned char* data = tx.get_data_ptr();
        const unsigned sw = tx.get_streaming_width() ? tx.get_streaming_width() : len;
        tx.set_dmi_allowed(false);
        if (!data || len == 0) {
            tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }
        if (a >= size_ || size_ - a < std::min(len, sw)) {
            tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        const bool werr = tx.is_write() && werr_size_ && a < werr_base_ + werr_size_ && werr_base_ < a + len;
        if (werr || (err_size_ && a < err_base_ + err_size_ && err_base_ < a + len)) {
            tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);   // SLVERR
            delay += latency_ + (werr ? werr_delay_ : sc_core::SC_ZERO_TIME);
            return;
        }
        const unsigned char* be = tx.get_byte_enable_ptr();
        const unsigned be_len = tx.get_byte_enable_length();
        if (be && be_len == 0) {
            tx.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
            return;
        }
        for (unsigned i = 0; i < len; ++i) {
            const uint64_t ai = a + (i % sw);
            const bool en = !be || be[i % be_len] != TLM_BYTE_DISABLED;
            if (tx.is_read()) {
                if (en) read(ai, data + i, 1);
            } else if (tx.is_write()) {
                if (en) write(ai, data + i, 1);
            } else {
                tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
                return;
            }
        }
        delay += latency_ + beat_time_ * double((len + beat_bytes_ - 1) / beat_bytes_);
        tx.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    unsigned transport_dbg(tlm::tlm_generic_payload& tx) {
        const uint64_t a = tx.get_address();
        if (a >= size_) return 0;
        const unsigned n = unsigned(std::min<uint64_t>(tx.get_data_length(), size_ - a));
        if (tx.is_read()) read(a, tx.get_data_ptr(), n);
        else if (tx.is_write()) write(a, tx.get_data_ptr(), n);
        else return 0;
        return n;
    }

    uint64_t size_;
    sc_core::sc_time latency_, beat_time_, werr_delay_ = sc_core::SC_ZERO_TIME;
    unsigned beat_bytes_;
    std::unordered_map<uint64_t, std::unique_ptr<uint8_t[]>> pages_;
    uint64_t err_base_ = 0, err_size_ = 0, werr_base_ = 0, werr_size_ = 0;
    uint64_t transactions_ = 0;
};

}  // namespace vp
