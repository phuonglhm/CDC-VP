// =============================================================================
// Integrated Circuits and Systems laboratory
// Author: Hung Nguyen <ngthung@hcmut.edu.vn>
// Module: virtual_platform.eth_mac_tlm.eth_mac_dma
// Description: DMA Controller: transmit and receive descriptor engines.
// Changelog:
//   2026-09-29: Initial version.
// =============================================================================
//
// Descriptor and burst-level DMA engines. Commands are accepted through
// register writes. Each transaction waits for its annotated delay before
// processing the response. Reset advances a generation counter so stale
// transactions cannot update current state.
#include <algorithm>
#include <cstring>

#include "eth_regs.h"
#include "icsl_eth/eth_mac_tlm.h"

using namespace sc_core;

namespace icsl_eth {

namespace {
constexpr uint32_t kDes0Own = 1u << 31;
constexpr uint32_t kDes0Bit16 = 1u << 16;   // RX_TRUNC / TX_TOO_LONG / CFG_*
constexpr uint32_t kDes0Axi = 1u << 17;     // *_AXI_ERR
constexpr uint32_t kDes0Dec = 1u << 18;     // *_DECERR
constexpr uint32_t kRxTrunc = 4;            // ERRINFO class of RX_TRUNC
}  // namespace

// =============================================================================
// common
// =============================================================================
void EthMacTlm::dma_reset() {
    tx_ = TxDma();
    rx_ = RxDma();
    tx_open_.clear();
    ++dma_gen_;
    dma_reset_ev_.notify(SC_ZERO_TIME);
    tx_fifo_ev_.notify(SC_ZERO_TIME);
    update_irq();
}

bool EthMacTlm::wait_dma(const sc_time& t, uint64_t gen) {
    if (gen != dma_gen_) return false;
    const sc_time end = sc_time_stamp() + t;
    while (sc_time_stamp() < end) {
        wait(end - sc_time_stamp(), dma_reset_ev_);
        if (gen != dma_gen_) return false;      // woken by a reset, not the timeout
    }
    return true;
}

uint32_t EthMacTlm::desc_len(bool tx) const {
    return regs_.value(tx ? ETH_DMA_TX_DESC_LEN : ETH_DMA_RX_DESC_LEN) & 0xFFFF;
}

uint32_t EthMacTlm::idx_inc(uint32_t idx, bool tx) const {
    const uint32_t n = (idx + 1) & 0xFFFF;
    return (n == desc_len(tx)) ? 0 : n;
}

bool EthMacTlm::axi_read(uint64_t off, unsigned bytes, uint8_t* dst, bool& err, bool& dec,
                         uint64_t gen) {
    tlm::tlm_generic_payload t;
    t.set_command(tlm::TLM_READ_COMMAND);
    t.set_address(p_.dram_base + off);
    t.set_data_ptr(dst);
    t.set_data_length(bytes);
    t.set_streaming_width(bytes);
    t.set_byte_enable_ptr(nullptr);
    t.set_dmi_allowed(false);
    t.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    sc_time d = SC_ZERO_TIME;
    axi->b_transport(t, d);
    if (!wait_dma(std::max(d, p_.clk_period), gen)) return false;
    err = t.is_response_error();
    dec = t.get_response_status() == tlm::TLM_ADDRESS_ERROR_RESPONSE;   // DECERR
    return true;
}

bool EthMacTlm::axi_write(uint64_t off, const uint8_t* src, const uint8_t* be, unsigned bytes,
                          bool& err, bool& dec, uint64_t gen) {
    tlm::tlm_generic_payload t;
    std::vector<uint8_t> buf(src, src + bytes);
    std::vector<uint8_t> ben;
    t.set_command(tlm::TLM_WRITE_COMMAND);
    t.set_address(p_.dram_base + off);
    t.set_data_ptr(buf.data());
    t.set_data_length(bytes);
    t.set_streaming_width(bytes);
    if (be) {
        ben.assign(be, be + bytes);
        t.set_byte_enable_ptr(ben.data());
        t.set_byte_enable_length(bytes);
    } else {
        t.set_byte_enable_ptr(nullptr);
    }
    t.set_dmi_allowed(false);
    t.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    sc_time d = SC_ZERO_TIME;
    axi->b_transport(t, d);
    if (!wait_dma(std::max(d, p_.clk_period), gen)) return false;
    err = t.is_response_error();
    dec = t.get_response_status() == tlm::TLM_ADDRESS_ERROR_RESPONSE;
    return true;
}

// One read burst of the four descriptor words from BASE + idx * 16.
bool EthMacTlm::fetch_desc(uint32_t base_reg, uint32_t idx, Desc& d, bool& err, bool& dec,
                           uint64_t gen) {
    uint8_t b[16] = {};
    const uint64_t a = uint64_t(regs_.value(base_reg)) + uint64_t(idx) * 16;
    if (!axi_read(a, 16, b, err, dec, gen)) return false;
    for (int i = 0; i < 4; ++i) std::memcpy(&d.w[i], b + 4 * i, 4);   // little-endian
    return true;
}

// DES0 in one beat: bytes 0..3 of the descriptor's bus word, strobes 0..3.
bool EthMacTlm::write_des0(uint32_t base_reg, uint32_t idx, uint32_t des0, bool& err, bool& dec,
                           uint64_t gen) {
    const unsigned B = p_.axi_data_bytes;
    std::vector<uint8_t> beat(B, 0), be(B, 0);
    std::memcpy(beat.data(), &des0, 4);
    std::fill(be.begin(), be.begin() + 4, 0xFF);
    const uint64_t a = uint64_t(regs_.value(base_reg)) + uint64_t(idx) * 16;
    return axi_write(a, beat.data(), be.data(), B, err, dec, gen);
}

// ERRINFO latches the first error of an episode.  An error while ERR is set
// is kept for a W1C of ERR on the same edge, which then loads it.
void EthMacTlm::dma_err(bool tx, uint32_t cls, bool dec) {
    const uint32_t stat = tx ? ETH_DMA_TX_STAT : ETH_DMA_RX_STAT;
    const uint32_t info = tx ? ETH_DMA_TX_ERRINFO : ETH_DMA_RX_ERRINFO;
    const uint32_t v = (cls & 0xF) | (dec ? 0x10 : 0);
    ErrinfoPend& pend = errinfo_pend_[tx ? 1 : 0];
    if (!(regs_.value(stat) & ETH_DMA_TX_STAT_ERR_MASK))
        regs_.hw_store(info, 0x1F, v);
    else if (!(pend.valid && pend.at == sc_time_stamp()))
        pend = ErrinfoPend{sc_time_stamp(), true, v};
    regs_.hw_set(stat, ETH_DMA_TX_STAT_ERR_MASK);
    update_irq();
}

// count reaches max(COAL_CNT, 1) or the timer expires with frames counted.
void EthMacTlm::coal_frame(bool tx) {
    Coal& c = tx ? tx_.coal : rx_.coal;
    const uint32_t stat = tx ? ETH_DMA_TX_STAT : ETH_DMA_RX_STAT;
    const uint32_t reg = regs_.value(tx ? ETH_DMA_TX_COAL : ETH_DMA_RX_COAL);
    const uint32_t cnt = reg & 0xFF, timer = (reg >> 8) & 0xFFFF;
    // A frame completed on the edge the timer expires: that expiry does not
    // pulse; the count advances and the timer restarts (Table 9-3 Collisions).
    // Its DONE is retracted with the W1C of this edge applied (review MR01).
    if (c.expired && c.expired_at == sc_time_stamp()) {
        c.count = c.expired_count;
        regs_.hw_retract(stat, ETH_DMA_TX_STAT_DONE_MASK);
        update_irq();
    }
    c.expired = false;
    if (c.count + 1 >= std::max<uint32_t>(cnt, 1)) {
        regs_.hw_set(stat, ETH_DMA_TX_STAT_DONE_MASK);
        c.count = 0;
        c.run = false;
        update_irq();
    } else {
        ++c.count;
        c.run = timer != 0;
        c.deadline = sc_time_stamp() + p_.clk_period * double(uint64_t(timer) * 128);
    }
    (tx ? tx_coal_ev_ : rx_coal_ev_).notify(SC_ZERO_TIME);
}

void EthMacTlm::tx_coal_thread() {
    while (true) {
        Coal& c = tx_.coal;
        if (!c.run) {
            wait(tx_coal_ev_);
            continue;
        }
        if (sc_time_stamp() < c.deadline) {
            wait(c.deadline - sc_time_stamp(), tx_coal_ev_);    // the deadline, not the wait
            continue;
        }
        coal_expire(true);
    }
}

void EthMacTlm::rx_coal_thread() {
    while (true) {
        Coal& c = rx_.coal;
        if (!c.run) {
            wait(rx_coal_ev_);
            continue;
        }
        if (sc_time_stamp() < c.deadline) {
            wait(c.deadline - sc_time_stamp(), rx_coal_ev_);
            continue;
        }
        coal_expire(false);
    }
}

// The timer expired with frames counted: one pulse, remembered so that a
// completion on the same edge can take it back.
void EthMacTlm::coal_expire(bool tx) {
    Coal& c = tx ? tx_.coal : rx_.coal;
    const uint32_t stat = tx ? ETH_DMA_TX_STAT : ETH_DMA_RX_STAT;
    c.run = false;
    if (c.count > 0 && ((regs_.value(tx ? ETH_DMA_TX_COAL : ETH_DMA_RX_COAL) >> 8) & 0xFFFF) != 0) {
        c.expired = true;
        c.expired_at = sc_time_stamp();
        c.expired_count = c.count;
        regs_.hw_set(stat, ETH_DMA_TX_STAT_DONE_MASK);
        c.count = 0;
        update_irq();
    }
}

// =============================================================================

// =============================================================================
void EthMacTlm::tx_command(uint32_t pulse) {
    const bool start = pulse & ETH_DMA_TX_CTRL_START_MASK;
    const bool hreq = pulse & ETH_DMA_TX_CTRL_HALT_REQ_MASK;
    const bool srst = pulse & ETH_DMA_TX_CTRL_SRST_MASK;
    switch (tx_.st) {
    case TxSt::IDLE:
        if (start) {                                    // (a)
            tx_.st = TxSt::FETCH;
            tx_.halted = false;
            tx_.cause = C_NONE;
            tx_.dec = false;
            tx_.in_frame = tx_.frame_open = false;
            tx_.acc_len = 0;
            tx_.halt_pend = hreq;                       
            tx_.force = false;
        } else if (hreq) {                              // (b) -> HALT -> (w), nothing to drain
            tx_.halted = true;
            tx_.halt_pend = false;
        } else if (srst && tx_.halted) {                
            tx_.idx = 0;
            tx_.db_pend = false;
            tx_.coal.reset();
        }
        break;
    case TxSt::HALT:
        break;                                          
    default:
        if (hreq) {
            if (tx_.halt_pend) tx_.force = true;        // a second HALT_REQ aborts at once
            tx_.halt_pend = true;
        }
        break;
    }
    tx_ev_.notify(SC_ZERO_TIME);
    update_irq();
}

void EthMacTlm::tx_dma_thread() {
    while (true) {
        while (tx_.st == TxSt::IDLE) wait(tx_ev_);
        tx_run(dma_gen_);
    }
}

void EthMacTlm::tx_run(uint64_t gen) {
    const unsigned B = p_.axi_data_bytes, N = p_.axi_max_burst;
    Desc d;
    auto to_abort_or_halt = [&]() {                     // (y) (g) (h)
        tx_.cause = C_NONE;
        tx_.st = tx_.frame_open ? TxSt::ABORT : TxSt::HALT;
    };
    while (gen == dma_gen_) {
        switch (tx_.st) {
        case TxSt::FETCH: {
            bool err = false, dec = false;
            tx_.db_pend = false;                        // the fetch services the doorbell
            if (!fetch_desc(ETH_DMA_TX_DESC_BASE, tx_.idx, d, err, dec, gen)) return;
            if (err) {                                  // (e)
                tx_.cause = C_DESC_RD;
                tx_.dec = dec;
                tx_.st = TxSt::ABORT;
            } else if (tx_.halt_pend) {                 // (y)
                to_abort_or_halt();
            } else {
                tx_.st = d.own() ? TxSt::CFG_CHECK : TxSt::SUSPEND;   // (c) / (d)
            }
            break;
        }
        case TxSt::SUSPEND: {
            update_irq();                               // TX_BUF_UNAVAIL = SUSPENDED
            const sc_time tick = sc_time_stamp() + p_.clk_period * double(p_.poll_cycles);
            while (true) {
                if (tx_.halt_pend) {                    // (g) / (h)
                    to_abort_or_halt();
                    break;
                }
                if (tx_.db_pend || sc_time_stamp() >= tick) {   // (f) doorbell or poll tick
                    tx_.st = TxSt::FETCH;
                    break;
                }
                wait(tick - sc_time_stamp(), tx_ev_ | dma_reset_ev_);
                if (gen != dma_gen_) return;
            }
            update_irq();
            break;
        }
        case TxSt::CFG_CHECK: {
            const uint32_t max_frame = regs_.value(ETH_TX_MAX_FRAME) & 0xFFFF;
            const uint32_t acc = tx_.in_frame ? tx_.acc_len : 0;
            if (tx_.halt_pend) {                                          // (y)
                to_abort_or_halt();
            } else if (!tx_.in_frame && !d.fs()) {                        
                tx_.cause = C_NO_FS;
                tx_.dec = false;
                tx_.st = TxSt::ABORT;
            } else if (d.buflen() == 0) {                                 // (k)
                tx_.cause = C_LEN_ZERO;
                tx_.dec = false;
                tx_.st = TxSt::ABORT;
            } else if (acc + d.buflen() + 4 > max_frame) {                // (j) counts the FCS
                tx_.cause = C_TOO_LONG;
                tx_.dec = false;
                tx_.st = TxSt::ABORT;
            } else {                                                      
                if (!tx_.in_frame) tx_open_.clear();
                tx_.in_frame = true;
                tx_.acc_len = acc + d.buflen();
                tx_.st = TxSt::READ;
            }
            break;
        }
        case TxSt::READ: {
            // Burst generator: aligned-down reads of at most
            // N beats, never across 4 KB, never across the buffer.
            std::vector<uint8_t> data;
            data.reserve(d.buflen());
            uint64_t addr = d.off();
            uint32_t left = d.buflen();
            bool aborted = false;
            while (left) {
                if (tx_.force) {                        // (n) second HALT_REQ
                    tx_.force = false;
                    tx_.cause = C_NONE;
                    tx_.st = TxSt::ABORT;
                    aborted = true;
                    break;
                }
                const unsigned lead = unsigned(addr % B);
                const uint64_t aligned = addr - lead;
                const uint64_t need = (lead + uint64_t(left) + B - 1) / B;
                const uint64_t to4k = (4096 - (aligned % 4096)) / B;
                const unsigned beats = unsigned(std::min<uint64_t>({N, to4k, need}));
                std::vector<uint8_t> buf(size_t(beats) * B);
                bool err = false, dec = false;
                if (!axi_read(aligned, unsigned(buf.size()), buf.data(), err, dec, gen)) return;
                if (err) {                              // (m) payload read error
                    tx_.cause = C_DATA;
                    tx_.dec = dec;
                    tx_.st = TxSt::ABORT;
                    rmon_.tx_drop();                    
                    sync_rmon();
                    aborted = true;
                    break;
                }
                const uint32_t take = std::min<uint32_t>(uint32_t(buf.size() - lead), left);
                data.insert(data.end(), buf.begin() + lead, buf.begin() + lead + take);
                addr += take;
                left -= take;
            }
            if (aborted) break;
            // Forward into the Tx Checksum block and the TX FIFO: the stream

            while (!(tx_stream_ok() && tx_fifo_space(unsigned(data.size())) &&
                     (tx_.frame_open || tx_meta_space()))) {
                if (tx_.force) break;
                wait(tx_fifo_ev_ | tx_ev_ | dma_reset_ev_);
                if (gen != dma_gen_) return;
            }
            if (tx_.force) {                            // stalled + second HALT_REQ: abort level
                tx_.force = false;
                tx_.cause = C_NONE;
                tx_.st = TxSt::ABORT;
                break;
            }
            // one word per clock into the stream
            if (!wait_dma(p_.clk_period * double(words_of(data.size())), gen)) return;
            tx_open_.insert(tx_open_.end(), data.begin(), data.end());
            tx_.frame_open = true;
            if (d.ls()) {                               // tlast: the FIFO commits the frame
                TxFrame f;
                f.bytes = std::move(tx_open_);
                tx_open_.clear();
                const uint32_t cs = regs_.value(ETH_CSUM_TX_CFG);
                core::tx_checksum_insert(f.bytes, cs & ETH_CSUM_TX_CFG_CSUM_TX_EN_MASK,
                                         cs & ETH_CSUM_TX_CFG_CSUM_TX_L4_EN_MASK);
                tx_fifo_used_ += words_of(f.bytes.size()) * B;
                tx_fifo_.push_back(std::move(f));
                tx_.frame_open = false;
                tx_fifo_ev_.notify(SC_ZERO_TIME);
            }
            tx_.st = TxSt::DESC_WB;                     // (l)
            break;
        }
        case TxSt::DESC_WB: {
            // DES0 = OWN 0, status 0, as soon as this descriptor's data is forwarded
            bool err = false, dec = false;
            const uint32_t idx = tx_.idx;
            tx_.idx = idx_inc(idx, true);               // advances at the write grant
            if (d.ls()) {
                coal_frame(true);                       
                tx_.in_frame = false;
                tx_.acc_len = 0;
            }
            if (!write_des0(ETH_DMA_TX_DESC_BASE, idx, 0, err, dec, gen)) return;
            if (err) {                                  // (x): the index is already past it
                tx_.cause = C_DESC_WR;
                tx_.dec = dec;
                tx_.st = TxSt::ABORT;
            } else if (d.ls()) {
                tx_.cause = C_NONE;
                tx_.st = tx_.halt_pend ? TxSt::HALT : TxSt::FETCH;       // (q) / (p)
            } else {
                tx_.cause = C_NONE;
                tx_.st = tx_.halt_pend ? TxSt::ABORT : TxSt::FETCH;      // (r) / (o)
            }
            break;
        }
        case TxSt::ABORT:
            // the abort beat: the Tx Checksum block and the TX FIFO rewind the
            // open frame, which never reaches the line
            if (tx_.frame_open) {
                tx_open_.clear();
                tx_.frame_open = false;
                tx_fifo_ev_.notify(SC_ZERO_TIME);
            }
            tx_.force = false;
            tx_.in_frame = false;
            tx_.acc_len = 0;
            tx_.st = (tx_.cause == C_TOO_LONG && !tx_.halt_pend) ? TxSt::SKIP : TxSt::HALT;  // (s)/(t)
            break;
        case TxSt::SKIP: {
            // the failed descriptor first, then the rest of the frame through
            // LS, each written back OWN = 0 with DES0[16]; no data is read
            dma_err(true, C_TOO_LONG, false);
            rmon_.tx_oversize();                        
            sync_rmon();
            while (true) {
                if (tx_.force) {                        // (z)
                    tx_.cause = C_NONE;
                    tx_.st = TxSt::HALT;
                    break;
                }
                bool err = false, dec = false;
                const uint32_t idx = tx_.idx;
                tx_.idx = idx_inc(idx, true);
                if (!write_des0(ETH_DMA_TX_DESC_BASE, idx, kDes0Bit16, err, dec, gen)) return;
                if (err) {                              // (v) write-back error
                    tx_.cause = C_DESC_WR;
                    tx_.dec = dec;
                    tx_.st = TxSt::HALT;
                    break;
                }
                if (d.ls()) {                           // (u)
                    coal_frame(true);
                    tx_.cause = C_NONE;
                    tx_.st = tx_.halt_pend ? TxSt::HALT : TxSt::FETCH;
                    break;
                }
                if (!fetch_desc(ETH_DMA_TX_DESC_BASE, tx_.idx, d, err, dec, gen)) return;
                if (err) {                              // (v)
                    tx_.cause = C_DESC_RD;
                    tx_.dec = dec;
                    tx_.st = TxSt::HALT;
                    break;
                }
                if (!d.own()) {                         // (v) TX_SKIP_OWN
                    tx_.cause = C_SKIP_OWN;
                    tx_.dec = false;
                    tx_.st = TxSt::HALT;
                    break;
                }
            }
            break;
        }
        case TxSt::HALT:
            tx_halt_entry(gen);
            return;
        case TxSt::IDLE:
            return;
        }
        update_irq();
    }
}

// HALT: the entry write-back of the faulting descriptor (index not advanced),
// the error pulse, the drain; then IDLE with HALTED (w).
void EthMacTlm::tx_halt_entry(uint64_t gen) {
    tx_.halt_pend = tx_.force = false;
    tx_.in_frame = tx_.frame_open = false;
    tx_.acc_len = 0;
    const uint32_t cause = tx_.cause;
    if (cause != C_NONE) dma_err(true, cause, tx_.dec);
    if (cause == C_TOO_LONG) {
        rmon_.tx_oversize();
        sync_rmon();
    }
    if (cause == C_DATA || cause == C_TOO_LONG || cause == C_LEN_ZERO || cause == C_NO_FS) {
        const uint32_t des0 = cause == C_DATA ? (kDes0Axi | (tx_.dec ? kDes0Dec : 0)) : kDes0Bit16;
        bool err = false, dec = false;
        if (!write_des0(ETH_DMA_TX_DESC_BASE, tx_.idx, des0, err, dec, gen)) return;
        // an error on this write-back is drained without a second pulse
    }
    tx_.st = TxSt::IDLE;
    tx_.halted = true;
    update_irq();
}

// =============================================================================

// =============================================================================
void EthMacTlm::rx_command(uint32_t pulse) {
    const bool start = pulse & ETH_DMA_RX_CTRL_START_MASK;
    const bool hreq = pulse & ETH_DMA_RX_CTRL_HALT_REQ_MASK;
    const bool srst = pulse & ETH_DMA_RX_CTRL_SRST_MASK;
    switch (rx_.st) {
    case RxSt::IDLE:
        if (start) {                                    // (a): fetch the descriptor at idx
            rx_.st = RxSt::ARMED;
            rx_.halted = false;
            rx_.cause = C_NONE;
            rx_.dec = false;
            rx_.hd_valid = false;
            rx_.fetch_need = true;
            rx_.halt_pend = hreq;
            rx_.force = false;
        } else if (hreq) {                              // (b)
            rx_.halted = true;
            rx_.halt_pend = false;
        } else if (srst && rx_.halted) {
            rx_.idx = 0;
            rx_.db_pend = false;
            rx_.coal.reset();
        }
        break;
    case RxSt::HALT:
        break;
    default:
        if (hreq) {
            if (rx_.halt_pend) rx_.force = true;
            rx_.halt_pend = true;
        }
        break;
    }
    rx_ev_.notify(SC_ZERO_TIME);
    update_irq();
}

void EthMacTlm::rx_dma_thread() {
    while (true) {
        while (rx_.st == RxSt::IDLE) wait(rx_ev_);
        rx_run(dma_gen_);
    }
}

void EthMacTlm::rx_run(uint64_t gen) {
    const unsigned B = p_.axi_data_bytes, N = p_.axi_max_burst;
    uint32_t vrd = 0, wcnt = 0;
    bool trunc = false;
    uint64_t fgen = rx_fifo_gen_;                       // the frame at the head belongs to
    // The stream takes one word per clock; the frame leaves the FIFO once
    // taken.  A FIFO flush meanwhile voided it (branch r): nothing to pop.
    auto consume_head = [&](size_t words) -> bool {
        if (!wait_dma(p_.clk_period * double(words), gen)) return false;
        if (fgen == rx_fifo_gen_ && !rx_fifo_.empty()) {
            rx_fifo_used_ -= std::min(rx_fifo_used_, rx_fifo_.front().words);
            rx_fifo_.pop_front();
        }
        rx_fifo_ev_.notify(SC_ZERO_TIME);
        return true;
    };
    auto flushed = [&]() { return fgen != rx_fifo_gen_; };
    auto head_ready = [&]() {
        return rx_stream_ok() && !rx_fifo_.empty() && rx_fifo_.front().ready <= sc_time_stamp();
    };
    while (gen == dma_gen_) {
        switch (rx_.st) {
        case RxSt::ARMED: {
            if (rx_.fetch_need) {
                bool err = false, dec = false;
                Desc d;
                rx_.db_pend = false;
                rx_.fetch_need = false;
                if (!fetch_desc(ETH_DMA_RX_DESC_BASE, rx_.idx, d, err, dec, gen)) return;
                if (err) {                              // (f)
                    rx_.cause = C_DESC_RD;
                    rx_.dec = dec;
                    rx_.st = RxSt::HALT;
                    break;
                }
                rx_.hd_valid = true;
                rx_.hd_own = d.own();
                rx_.hd_off = d.off();
                rx_.poll_base = sc_time_stamp();
                update_irq();
            }
            if (rx_.halt_pend) {                        // (e)
                rx_.cause = C_NONE;
                rx_.st = RxSt::HALT;
                break;
            }
            if (head_ready()) {                         // store or discard, decided once
                fgen = rx_fifo_gen_;
                rx_.st = rx_.hd_own ? RxSt::WRITE : RxSt::DISCARD;       // (c) / (d)
                break;
            }
            // Held descriptor not owned: the doorbell, or the poll interval that
            // runs from the fetch (not from this wake-up), re-fetches it.
            const bool not_owned = rx_.hd_valid && !rx_.hd_own;
            const sc_time tick = rx_.poll_base + p_.clk_period * double(p_.poll_cycles);
            if (not_owned && (rx_.db_pend || sc_time_stamp() >= tick)) {
                rx_.fetch_need = true;
                break;
            }
            sc_time until = not_owned ? tick : sc_max_time();
            if (rx_stream_ok() && !rx_fifo_.empty())
                until = std::min(until, rx_fifo_.front().ready);
            if (until == sc_max_time())
                wait(rx_ev_ | rx_fifo_ev_ | dma_reset_ev_);
            else if (sc_time_stamp() < until)
                wait(until - sc_time_stamp(), rx_ev_ | rx_fifo_ev_ | dma_reset_ev_);
            if (gen != dma_gen_) return;
            break;                                      // re-evaluate
        }
        case RxSt::DISCARD: {
            // accepted and discarded: one drop pulse, RX_BUF_UNAVAIL meanwhile
            update_irq();
            rmon_.rx_drop();                            
            sync_rmon();
            regs_.hw_add(ETH_DMA_RX_DROP_CNT, 1);
            if (!consume_head(rx_fifo_.empty() ? 1 : rx_fifo_.front().words)) return;
            rx_.cause = C_NONE;
            rx_.st = rx_.halt_pend ? RxSt::HALT : RxSt::ARMED;          // (n) / (o)
            rx_.force = false;
            break;
        }
        case RxSt::WRITE: {
            if (flushed() || rx_fifo_.empty()) {        // (r)
                rx_.st = RxSt::ARMED;
                break;
            }
            const RxFrame& f = rx_fifo_.front();
            const size_t n = f.bytes.size();
            const uint32_t bsz = regs_.value(ETH_DMA_RX_BUF_SIZE) & 0xFFFF;
            const uint32_t cs = regs_.value(ETH_CSUM_RX_CFG);
            // the verdict record, paired with the frame on its tlast
            vrd = core::rx_checksum(f.bytes, cs & ETH_CSUM_RX_CFG_CSUM_RX_EN_MASK,
                                    cs & ETH_CSUM_RX_CFG_CSUM_RX_L4_EN_MASK);
            wcnt = uint32_t(std::min<size_t>(n, bsz));
            trunc = n > bsz;
            const std::vector<uint8_t> bytes(f.bytes.begin(), f.bytes.begin() + wcnt);
            const unsigned fwords = f.words;
            // Write bursts: the first word carries the DES2
            // lead as empty lanes, bursts of at most N words and never across
            // 4 KB; strobes mask the lead and the tail.
            const unsigned lead = rx_.hd_off % B;
            uint64_t word_addr = rx_.hd_off - lead;
            const size_t total = lead + bytes.size();
            size_t words = (total + B - 1) / B, pos = 0;          // pos: byte index in lead+data space
            bool aborted = false;
            const sc_time t0 = sc_time_stamp();
            while (words) {
                if (rx_.force) {                        
                    rx_.force = false;
                    rx_.cause = C_NONE;
                    rx_.st = RxSt::HALT;
                    aborted = true;
                    if (!consume_head(fwords)) return;
                    break;
                }
                const uint64_t to4k = (4096 - (word_addr % 4096)) / B;
                const unsigned beats = unsigned(std::min<uint64_t>({N, to4k, uint64_t(words)}));
                std::vector<uint8_t> buf(size_t(beats) * B, 0), be(size_t(beats) * B, 0);
                for (size_t i = 0; i < buf.size(); ++i, ++pos) {
                    if (pos >= lead && pos < total) {
                        buf[i] = bytes[pos - lead];
                        be[i] = 0xFF;
                    }
                }
                bool err = false, dec = false;
                if (!axi_write(word_addr, buf.data(), be.data(), unsigned(buf.size()), err, dec, gen))
                    return;
                if (err) {                              // (h) data write error
                    rx_.cause = C_DATA;
                    rx_.dec = dec;
                    rx_.st = RxSt::HALT;
                    aborted = true;
                    if (!consume_head(fwords)) return;  // HALT consumes the stream to tlast
                    rx_halt_entry(gen, kDes0Axi | (dec ? kDes0Dec : 0) | (trunc ? kDes0Bit16 : 0) |
                                           wcnt, true);
                    return;
                }
                if (flushed()) {                        // (r): descriptor kept, no write-back
                    rx_.st = RxSt::ARMED;
                    aborted = true;
                    break;
                }
                word_addr += uint64_t(beats) * B;
                words -= beats;
            }
            if (aborted) break;
            // the input stage takes one word per clock at most
            const sc_time floor = t0 + p_.clk_period * double(fwords);
            if (sc_time_stamp() < floor && !wait_dma(floor - sc_time_stamp(), gen)) return;
            if (!consume_head(0)) return;
            if (flushed()) {
                rx_.st = RxSt::ARMED;
                break;
            }
            rx_.st = RxSt::DESC_WB;                     // (g) then WAIT_VRD (i)
            break;
        }
        case RxSt::DESC_WB: {
            bool err = false, dec = false;
            const uint32_t des0 = (vrd & 0x3F) << 20 | (trunc ? kDes0Bit16 : 0) | (wcnt & 0xFFFF);
            const uint32_t idx = rx_.idx;
            rx_.idx = idx_inc(idx, false);              // at the write grant
            rx_.hd_valid = false;
            coal_frame(false);
            ++rx_frames_;
            if (trunc) dma_err(false, kRxTrunc, false); // RX_TRUNC: ERR, engine continues
            if (!write_des0(ETH_DMA_RX_DESC_BASE, idx, des0, err, dec, gen)) return;
            if (err) {                                  // (q)
                rx_.cause = C_DESC_WR;
                rx_.dec = dec;
                rx_.st = RxSt::HALT;
                break;
            }
            Desc d;
            if (!fetch_desc(ETH_DMA_RX_DESC_BASE, rx_.idx, d, err, dec, gen)) return;
            rx_.db_pend = false;
            if (err) {                                  // (m)
                rx_.cause = C_DESC_RD;
                rx_.dec = dec;
                rx_.st = RxSt::HALT;
                break;
            }
            rx_.hd_valid = true;
            rx_.hd_own = d.own();
            rx_.hd_off = d.off();
            rx_.poll_base = sc_time_stamp();
            rx_.cause = C_NONE;
            rx_.st = rx_.halt_pend ? RxSt::HALT : RxSt::ARMED;          // (l) / (k)
            break;
        }
        case RxSt::HALT:
            rx_halt_entry(gen, 0, false);
            return;
        case RxSt::WAIT_VRD:
        case RxSt::IDLE:
            return;
        }
        update_irq();
    }
}

void EthMacTlm::rx_halt_entry(uint64_t gen, uint32_t wb_des0, bool wb) {
    rx_.halt_pend = rx_.force = false;
    rx_.st = RxSt::HALT;
    if (rx_.cause != C_NONE) dma_err(false, rx_.cause, rx_.dec);
    if (wb) {                                           // RX_DATA_WR_ERR: the held descriptor
        bool err = false, dec = false;
        if (!write_des0(ETH_DMA_RX_DESC_BASE, rx_.idx, wb_des0, err, dec, gen)) return;
    }
    rx_.hd_valid = false;
    rx_.st = RxSt::IDLE;
    rx_.halted = true;
    update_irq();
}

}  // namespace icsl_eth
