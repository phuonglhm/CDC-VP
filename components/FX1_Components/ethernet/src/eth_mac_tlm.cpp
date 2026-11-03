// =============================================================================
// Integrated Circuits and Systems laboratory
// Author: Hung Nguyen <ngthung@hcmut.edu.vn>
// Module: virtual_platform.eth_mac_tlm.eth_mac_tlm
// Description: Register access, interrupt, resets, RMON and MDIO of the MAC.
// Changelog:
//   2026-09-29: Initial version.
// =============================================================================
#include "icsl_eth/eth_mac_tlm.h"

#include <algorithm>
#include <cstring>

#include "eth_regs.h"

using namespace sc_core;

namespace icsl_eth {

namespace {
unsigned log2u(unsigned v) {
    unsigned r = 0;
    while ((1u << r) < v) ++r;
    return r;
}
bool is_rmon_counter(uint32_t off) {
    return (off >= RMON_TX_PKTS && off <= RMON_TX_PKTS_1024_1518) ||
           (off >= RMON_RX_DROP_EVENTS && off <= RMON_RX_FILTER_DROP);
}
// RMON rd_addr of a counter offset: RX 0x7800 + 4i -> i, TX 0x7010 + 4j -> 18 + j
unsigned rmon_index(uint32_t off) {
    return off >= RMON_RX_DROP_EVENTS ? (off - RMON_RX_DROP_EVENTS) / 4
                                      : core::kRmonRx + (off - RMON_TX_PKTS) / 4;
}
// The register a bus address selects: bits [1:0] and, above a 16-bit
// PARA_AXIL_ADDR_WIDTH, bits [31:16] are ignored (MAS Table 6-47 rows 9, 10).
uint32_t csr_offset(uint64_t addr) { return uint32_t(addr) & 0xFFFCu; }
bool is_uc_entry(uint32_t off) { return off >= ETH_UC_ADDR_L0 && off <= ETH_UC_CTRL3; }
// Fail fast before any SystemC object of the module exists.
const sc_module_name& checked(const sc_module_name& name, const EthMacParams& p) {
    const std::string why = p.validate();
    if (!why.empty()) SC_REPORT_FATAL("icsl_eth/EthMacTlm", ("illegal EthMacParams: " + why).c_str());
    return name;
}
uint64_t now_stamp() { return sc_time_stamp().value(); }
}  // namespace

std::string EthMacParams::validate() const {
    auto pow2 = [](uint64_t v) { return v != 0 && (v & (v - 1)) == 0; };
    if (clk_period <= SC_ZERO_TIME) return "clk_period must be positive";
    if (axi_data_bytes != 4 && axi_data_bytes != 8)
        return "axi_data_bytes must be 4 or 8 (PARA_AXI_DATA_WIDTH 32 or 64)";
    if (axi_max_burst < 1 || axi_max_burst > 16 || !pow2(axi_max_burst))
        return "axi_max_burst must be a power of two in 1..16 (PARA_AXI_MAX_BURST_LEN; "
               "ETH_DMA_BUILDCFG reports its log2)";
    if (max_frame_len < 64 || max_frame_len > 0xFFFF)
        return "max_frame_len must be in 64..65535 (PARA_MAX_FRAME_LEN, 16-bit MAX_FRAME fields)";
    if (!pow2(rx_fifo_bytes) || !pow2(tx_fifo_bytes))
        return "rx_fifo_bytes and tx_fifo_bytes must be powers of two (PARA_RX/TX_FIFO_DEPTH)";
    if (2 * (max_frame_len - 4) > tx_fifo_bytes || 2 * (max_frame_len - 4) > rx_fifo_bytes)
        return "tx_fifo_bytes and rx_fifo_bytes must be at least 2 x (max_frame_len - 4)";
    if (rx_unicast_num < 1 || rx_unicast_num > 4) return "rx_unicast_num must be in 1..4 (PARA_RX_UNICAST_NUM)";
    if (mdio_clk_div < 40 || mdio_clk_div > 128) return "mdio_clk_div must be in 40..128 (PARA_MDIO_CLK_DIV)";
    if (poll_cycles < 256 || poll_cycles > 8192 || !pow2(poll_cycles))
        return "poll_cycles must be a power of two in 256..8192 (PARA_RX_POLL_CYCLES)";
    if (!pow2(tx_meta_depth) || tx_meta_depth <= tx_meta_reserve)
        return "tx_meta_depth must be a power of two above tx_meta_reserve (PARA_TX_META_DEPTH)";
    if (dram_base % 4096) return "dram_base must be a multiple of 4 KiB";
    if (csr_window < 0x10000 || csr_window > (uint64_t(1) << 32) || !pow2(csr_window))
        return "csr_window must be 2^PARA_AXIL_ADDR_WIDTH with a width of 16..32 bits";
    return {};
}

EthMacTlm::EthMacTlm(sc_module_name name, const EthMacParams& p)
    : sc_module(checked(name, p)), csr("csr"), axi("axi"), phy_tx("phy_tx"), phy_rx("phy_rx"),
      mdio("mdio"), rst_n("rst_n"), rmii_rst_n("rmii_rst_n"), irq("irq"), p_(p) {
    csr.register_b_transport(this, &EthMacTlm::b_transport);
    csr.register_transport_dbg(this, &EthMacTlm::transport_dbg);
    csr.register_get_direct_mem_ptr(this, &EthMacTlm::get_direct_mem_ptr);
    phy_rx.register_b_transport(this, &EthMacTlm::phy_rx_b_transport);
    regs_.on_write = [this](const CsrFile::Write& w) { on_write(w); };
    regs_.hw_read = [this](uint32_t off, uint32_t stored) { return hw_read(off, stored); };
    regs_.accept_write = [this](uint32_t off) { return accept_write(off); };
    regs_.now = now_stamp;
    rmon_.now = now_stamp;
    // Reset values that follow the parameters (MAS Table 5-1, 8.2); the
    // workbook constants are those of the default parameter set.
    regs_.set_reset(ETH_RX_MAX_FRAME, ETH_RX_MAX_FRAME_MAX_FRAME_MASK, p_.max_frame_len);
    regs_.set_reset(ETH_TX_MAX_FRAME, ETH_TX_MAX_FRAME_MAX_FRAME_MASK, p_.max_frame_len);
    regs_.set_reset(ETH_DMA_RX_BUF_SIZE, ETH_DMA_RX_BUF_SIZE_BUF_SIZE_MASK, p_.max_frame_len - 4);
    regs_.set_reset(ETH_MDIO_CTRL, ETH_MDIO_CTRL_CLK_DIV_MASK, p_.mdio_clk_div << ETH_MDIO_CTRL_CLK_DIV_SHIFT);
    // Registers that store the value in use (MAS Table 6-47 row 14).
    regs_.set_range(ETH_TX_MAX_FRAME, 0, p_.max_frame_len);
    regs_.set_range(ETH_RX_MAX_FRAME, 0, p_.max_frame_len);
    regs_.set_range(ETH_IPG, core::kMinIpg, 0xFF);
    regs_.set_range(ETH_TX_MIN_FRAME, core::kMinFrame, 0xFFFF);
    hard_reset();

    SC_THREAD(reset_thread);
    SC_THREAD(rmii_reset_thread);
    SC_THREAD(tx_dma_thread);
    SC_THREAD(rx_dma_thread);
    SC_THREAD(tx_coal_thread);
    SC_THREAD(rx_coal_thread);
    SC_THREAD(tx_mac_thread);
    SC_THREAD(rx_mac_thread);
    SC_THREAD(mdio_thread);
    SC_METHOD(drive_irq);
    sensitive << irq_ev_;
}

uint32_t EthMacTlm::peek(uint32_t offset) const {
    if (is_rmon_counter(offset)) return rmon_live() ? rmon_.peek(rmon_index(offset)) : 0;
    return regs_.read(offset);
}

// =============================================================================
// register access
// =============================================================================
void EthMacTlm::b_transport(tlm::tlm_generic_payload& tx, sc_time& delay) {
    // annotate, no wait(); an RMON counter read waits for the counter bank
    unsigned cyc = p_.csr_latency_cycles;
    if (is_rmon_counter(csr_offset(tx.get_address())) && rmon_live()) cyc += 3;
    delay += p_.clk_period * double(cyc);
    tx.set_dmi_allowed(false);
    tx.set_response_status(access(tx, false));
}

unsigned EthMacTlm::transport_dbg(tlm::tlm_generic_payload& tx) {
    // Debug reads only: writes have side effects (W1C, W1P commands, WC), and
    // a debug read of an RMON counter never clears it.
    if (tx.get_command() != tlm::TLM_READ_COMMAND) return 0;
    return access(tx, true) == tlm::TLM_OK_RESPONSE ? tx.get_data_length() : 0;
}

bool EthMacTlm::get_direct_mem_ptr(tlm::tlm_generic_payload&, tlm::tlm_dmi&) {
    return false;                       // a raw pointer would bypass every side effect
}

tlm::tlm_response_status EthMacTlm::access(tlm::tlm_generic_payload& tx, bool debug) {
    unsigned char* data = tx.get_data_ptr();
    const unsigned len = tx.get_data_length();
    const uint64_t addr = tx.get_address();
    if (data == nullptr || len != 4) return tlm::TLM_GENERIC_ERROR_RESPONSE;
    // Beyond 2^PARA_AXIL_ADDR_WIDTH the address never reaches the IP: the
    // interconnect's decode miss, not a CSR response.
    if (addr >= p_.csr_window) return tlm::TLM_ADDRESS_ERROR_RESPONSE;
    if (tx.get_streaming_width() != 0 && tx.get_streaming_width() < len)
        return tlm::TLM_BURST_ERROR_RESPONSE;
    const unsigned char* be = tx.get_byte_enable_ptr();
    const unsigned be_len = tx.get_byte_enable_length();
    if (be && be_len == 0) return tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE;
    // The data lanes are those of the 32-bit word that contains the address.
    const uint32_t off = csr_offset(addr);

    uint32_t mask = 0xFFFFFFFFu;
    if (be) {
        mask = 0;
        for (unsigned i = 0; i < 4; ++i)
            if (be[i % be_len]) mask |= 0xFFu << (8 * i);
    }
    auto put = [&](uint32_t v) {                // little-endian lanes
        for (unsigned i = 0; i < 4; ++i)
            if (mask & (0xFFu << (8 * i))) data[i] = uint8_t(v >> (8 * i));
    };
    // ADDR_DEC miss: read data 0 and SLVERR, a write changes nothing (rows 1, 2)
    if (!regs_.mapped(off)) {
        if (tx.get_command() == tlm::TLM_READ_COMMAND) put(0);
        return tlm::TLM_GENERIC_ERROR_RESPONSE;
    }
    // a unicast entry at or above PARA_RX_UNICAST_NUM reads 0 and ignores
    // writes, OKAY, whatever the RX_EN window (row 4)
    const bool absent = is_uc_entry(off) && (off - ETH_UC_ADDR_L0) / 16 >= p_.rx_unicast_num;
    switch (tx.get_command()) {
    case tlm::TLM_READ_COMMAND: {
        uint32_t v;
        if (absent) {
            v = 0;
        } else if (is_rmon_counter(off)) {
            // held in reset: not forwarded, reads 0; otherwise the counter
            // bank read, which clears the counter under READ_CLEAR_EN
            const bool rc = !debug && regs_.value(RMON_CTRL) & RMON_CTRL_READ_CLEAR_EN_MASK;
            v = rmon_live() ? (debug ? rmon_.peek(rmon_index(off)) : rmon_.read(rmon_index(off), rc)) : 0;
            if (rc) sync_rmon();
        } else {
            v = regs_.read(off);
        }
        put(v);
        return tlm::TLM_OK_RESPONSE;
    }
    case tlm::TLM_WRITE_COMMAND: {
        if (debug) return tlm::TLM_COMMAND_ERROR_RESPONSE;
        if (absent) return tlm::TLM_OK_RESPONSE;
        uint32_t v = 0;
        std::memcpy(&v, data, 4);               // the payload buffer may be unaligned
        // outside its write window: SLVERR (row 6); no byte enable changes
        // nothing and is answered OKAY (row 5)
        if (mask && !rst_asserted_ && !accept_write(off)) return tlm::TLM_GENERIC_ERROR_RESPONSE;
        if (!rst_asserted_) regs_.write(off, v, mask);
        update_irq();
        return tlm::TLM_OK_RESPONSE;
    }
    default:
        return tlm::TLM_COMMAND_ERROR_RESPONSE;
    }
}

bool EthMacTlm::accept_write(uint32_t off) const {
    // MAC configuration windows: a field the Rx MAC or Tx MAC uses changes only
    // while that MAC is disabled and idle (MAS 8.2).
    const bool rxmac_stopped = !(regs_.value(ETH_RX_MAC_CFG) & ETH_RX_MAC_CFG_RX_EN_MASK) && rx_line_.empty();
    const bool txmac_stopped = !(regs_.value(ETH_TX_MAC_CFG) & ETH_TX_MAC_CFG_TX_EN_MASK) && !tx_busy_;
    if (is_uc_entry(off)) return rxmac_stopped;          // absent entries never get here
    switch (off) {
    case ETH_RMII_CFG:
        return rxmac_stopped && txmac_stopped;
    case ETH_RX_MAX_FRAME:
    case ETH_RX_MIN_FRAME:
    case ETH_FILT_CTRL:
    case ETH_HASH_L:
    case ETH_HASH_H:
        return rxmac_stopped;
    case ETH_IPG:
    case ETH_TX_MIN_FRAME:
        return txmac_stopped;
    case ETH_TX_MAX_FRAME:
    case ETH_CSUM_TX_CFG:
        return tx_.st == TxSt::IDLE;
    case ETH_CSUM_RX_CFG:
        return rx_.st == RxSt::IDLE;
    case ETH_DMA_TX_DESC_BASE:
    case ETH_DMA_TX_DESC_LEN:
        return tx_.st == TxSt::IDLE;
    case ETH_DMA_RX_DESC_BASE:
    case ETH_DMA_RX_DESC_LEN:
    case ETH_DMA_RX_BUF_SIZE:
        return rx_.st == RxSt::IDLE;
    case ETH_DMA_AXI_CFG:
        return tx_.st == TxSt::IDLE && rx_.st == RxSt::IDLE;
    default:
        return true;
    }
}

void EthMacTlm::on_write(const CsrFile::Write& w) {
    const uint32_t d = w.data & w.mask;
    switch (w.offset) {
    case ETH_CTRL:
        apply_block_resets(w.before, w.after);
        break;
    case ETH_RX_MAC_CFG:
        // clearing RX_EN aborts the frame in flight: no RMON event, no commit
        if ((w.before & ETH_RX_MAC_CFG_RX_EN_MASK) && !(w.after & ETH_RX_MAC_CFG_RX_EN_MASK)) {
            rx_line_.clear();
            ++rxmac_gen_;
            rx_line_ev_.notify(SC_ZERO_TIME);
        }
        break;
    case ETH_TX_MAC_CFG:
        tx_fifo_ev_.notify(SC_ZERO_TIME);       // TX_EN gates the Tx MAC claim
        break;
    case ETH_DMA_TX_CTRL:
        if (!in_rst(4)) tx_command(w.pulse);
        break;
    case ETH_DMA_TX_POLL:
        if (!in_rst(4) && (w.pulse & ETH_DMA_TX_POLL_POLL_MASK)) {
            tx_.db_pend = true;
            tx_ev_.notify(SC_ZERO_TIME);
        }
        break;
    case ETH_DMA_TX_STAT:
        errinfo_on_clear(true, w);
        break;
    case ETH_DMA_RX_CTRL:
        if (!in_rst(4)) rx_command(w.pulse);
        break;
    case ETH_DMA_RX_POLL:
        if (!in_rst(4) && (w.pulse & ETH_DMA_RX_POLL_POLL_MASK)) {
            rx_.db_pend = true;
            rx_ev_.notify(SC_ZERO_TIME);
        }
        break;
    case ETH_DMA_RX_STAT:
        errinfo_on_clear(false, w);
        break;
    case ETH_DMA_TX_COAL:
    case ETH_DMA_RX_COAL: {
        // COAL_TIMER written 0 stops a running timer; the frames counted are
        // reported at COAL_CNT, and only a completion restarts it (Table 9-3)
        const bool tx = w.offset == ETH_DMA_TX_COAL;
        if (!((w.after >> 8) & 0xFFFF)) {
            (tx ? tx_.coal : rx_.coal).run = false;
            (tx ? tx_coal_ev_ : rx_coal_ev_).notify(SC_ZERO_TIME);
        }
        break;
    }
    case RMON_CTRL:
        rmon_.en = w.after & RMON_CTRL_RMON_EN_MASK;
        rmon_.freeze = w.after & RMON_CTRL_FREEZE_MASK;
        if ((w.pulse & RMON_CTRL_CLEAR_ALL_MASK) && rmon_live()) rmon_.clear_all();
        sync_rmon();
        break;
    case RMON_OVF_STAT_TX:
        rmon_.clear_ovf(0, d);
        sync_rmon();
        break;
    case RMON_OVF_STAT_RX:
        rmon_.clear_ovf(d, 0);
        sync_rmon();
        break;
    case ETH_MDIO_CTRL:
        if (w.pulse & ETH_MDIO_CTRL_START_MASK) mdio_start();
        break;
    default:
        break;
    }
}

// W1C of ERR clears ERRINFO (MAS Table 9-3).  A W1C on the edge of a new
// error leaves ERR set (CsrFile) and ERRINFO loads that error (Table 6-47 row 11).
void EthMacTlm::errinfo_on_clear(bool tx, const CsrFile::Write& w) {
    const uint32_t info = tx ? ETH_DMA_TX_ERRINFO : ETH_DMA_RX_ERRINFO;
    const ErrinfoPend& pend = errinfo_pend_[tx ? 1 : 0];
    if (w.cleared & ETH_DMA_TX_STAT_ERR_MASK)
        regs_.hw_store(info, ~0u, 0);
    else if ((w.data & w.mask & ETH_DMA_TX_STAT_ERR_MASK) && pend.valid && pend.at == sc_time_stamp())
        regs_.hw_store(info, ~0u, pend.value);
}

uint32_t EthMacTlm::hw_read(uint32_t off, uint32_t stored) const {
    switch (off) {
    case ETH_STATUS: {
        uint32_t v = 0;
        if (tx_busy_) v |= ETH_STATUS_TX_BUSY_MASK;
        if (!rx_line_.empty()) v |= ETH_STATUS_RX_BUSY_MASK;
        if (tx_.st != TxSt::IDLE) v |= ETH_STATUS_DMA_TX_BUSY_MASK;
        if (rx_.st != RxSt::IDLE) v |= ETH_STATUS_DMA_RX_BUSY_MASK;
        if ((regs_.value(ETH_DMA_TX_STAT) | regs_.value(ETH_DMA_RX_STAT)) & ETH_DMA_TX_STAT_ERR_MASK)
            v |= ETH_STATUS_AXI_ERR_STICKY_MASK;
        return v;
    }
    case ETH_CAP:
        return ETH_CAP_FULL_DUPLEX_MASK |
               ((log2u(p_.axi_data_bytes * 8) - 5) << ETH_CAP_AXI_DATA_WIDTH_SHIFT);
    case ETH_DMA_BUILDCFG:
        return (log2u(p_.axi_data_bytes * 8) << ETH_DMA_BUILDCFG_DATA_WIDTH_LOG2_SHIFT) |
               (32u << ETH_DMA_BUILDCFG_ADDR_WIDTH_SHIFT) |
               (log2u(p_.axi_max_burst) << ETH_DMA_BUILDCFG_MAX_BURST_LOG2_SHIFT) |
               (log2u(p_.tx_fifo_bytes) << ETH_DMA_BUILDCFG_TX_FIFO_DEPTH_LOG2_SHIFT) |
               (log2u(p_.rx_fifo_bytes) << ETH_DMA_BUILDCFG_RX_FIFO_DEPTH_LOG2_SHIFT) |
               (log2u(p_.poll_cycles) << ETH_DMA_BUILDCFG_POLL_CYCLES_LOG2_SHIFT);
    case ETH_INT_RAW:
    case ETH_INT_STATUS: {
        const uint32_t ts = regs_.value(ETH_DMA_TX_STAT), rs = regs_.value(ETH_DMA_RX_STAT);
        uint32_t raw = 0;
        if (ts & ETH_DMA_TX_STAT_DONE_MASK) raw |= ETH_INT_RAW_TX_DONE_MASK;
        if (rs & ETH_DMA_RX_STAT_DONE_MASK) raw |= ETH_INT_RAW_RX_DONE_MASK;
        if (ts & ETH_DMA_TX_STAT_ERR_MASK) raw |= ETH_INT_RAW_TX_ERR_MASK;
        if (rs & ETH_DMA_RX_STAT_ERR_MASK) raw |= ETH_INT_RAW_RX_ERR_MASK;
        if (tx_.st == TxSt::SUSPEND) raw |= ETH_INT_RAW_TX_BUF_UNAVAIL_MASK;
        // RX_BUF_UNAVAIL: ARMED with the held descriptor not owned, or DISCARD
        if ((rx_.st == RxSt::ARMED && rx_.hd_valid && !rx_.hd_own) || rx_.st == RxSt::DISCARD)
            raw |= ETH_INT_RAW_RX_BUF_UNAVAIL_MASK;
        if (regs_.value(ETH_MDIO_STAT) & ETH_MDIO_STAT_DONE_MASK) raw |= ETH_INT_RAW_MDIO_DONE_MASK;
        if (off == ETH_INT_RAW) return raw;
        return (raw | regs_.value(ETH_INT_INJ)) & regs_.value(ETH_INT_EN);
    }
    case ETH_DMA_TX_STAT: {
        uint32_t v = 0;
        if (tx_.st != TxSt::IDLE) v |= ETH_DMA_TX_STAT_RUNNING_MASK;
        if (tx_.halted) v |= ETH_DMA_TX_STAT_HALTED_MASK;
        if (tx_.st == TxSt::IDLE) v |= ETH_DMA_TX_STAT_IDLE_MASK;
        if (tx_.st == TxSt::SUSPEND) v |= ETH_DMA_TX_STAT_SUSPENDED_MASK;
        return v;
    }
    case ETH_DMA_RX_STAT: {
        uint32_t v = 0;
        if (rx_.st != RxSt::IDLE) v |= ETH_DMA_RX_STAT_RUNNING_MASK;
        if (rx_.halted) v |= ETH_DMA_RX_STAT_HALTED_MASK;
        if (rx_.st == RxSt::IDLE) v |= ETH_DMA_RX_STAT_IDLE_MASK;
        if ((rx_.st == RxSt::ARMED || rx_.st == RxSt::DISCARD) && rx_.hd_valid && !rx_.hd_own)
            v |= ETH_DMA_RX_STAT_SUSPENDED_MASK;
        return v;
    }
    case ETH_DMA_TX_CUR_DESC:
        return tx_.idx;
    case ETH_DMA_RX_CUR_DESC:
        return rx_.idx;
    case ETH_MDIO_STAT:
        return mdio_.busy ? ETH_MDIO_STAT_BUSY_MASK : 0;
    case ETH_UC_ADDR_L1: case ETH_UC_ADDR_H1: case ETH_UC_CTRL1:
    case ETH_UC_ADDR_L2: case ETH_UC_ADDR_H2: case ETH_UC_CTRL2:
    case ETH_UC_ADDR_L3: case ETH_UC_ADDR_H3: case ETH_UC_CTRL3:
        return stored;
    default:
        return stored;                  // constants and hardware-stored fields
    }
}

// =============================================================================

// =============================================================================
bool EthMacTlm::irq_level() const { return hw_read(ETH_INT_STATUS, 0) != 0; }

void EthMacTlm::update_irq() { irq_ev_.notify(p_.clk_period); }

void EthMacTlm::drive_irq() { irq.write(irq_level()); }

// =============================================================================

// =============================================================================
bool EthMacTlm::in_rst(unsigned bit) const {
    const uint32_t c = regs_.value(ETH_CTRL);
    return rst_asserted_ || !(c & 1u) || !(c & (1u << bit));
}

bool EthMacTlm::rmii_in_rst(unsigned bit) const { return rmii_rst_asserted_ || in_rst(bit); }

// A FIFO takes both of its ETH_CTRL bits and both domain resets: either side
// in reset holds the whole FIFO.  TX path group (MAS 6.2): the Tx Checksum and
// both TX FIFOs are reset together by ETH_CTRL[2], [5] or [12].
bool EthMacTlm::tx_stream_ok() const { return !in_rst(2) && !in_rst(5) && !rmii_in_rst(12); }
bool EthMacTlm::tx_line_ok() const {
    return !rmii_in_rst(10) && !rmii_in_rst(8) && !rmii_in_rst(12) && !in_rst(5) && !in_rst(2);
}
// The RX FIFO resets (ETH_CTRL[6], [13]) hold only the FIFO: the Rx MAC keeps
// receiving and classifying (MAS Table 6-2 row 8).
bool EthMacTlm::rx_line_ok() const { return !rmii_in_rst(7) && !rmii_in_rst(9); }
bool EthMacTlm::rx_fifo_writable() const { return !rmii_in_rst(13) && !in_rst(6); }
bool EthMacTlm::rx_stream_ok() const { return !in_rst(1) && !in_rst(6) && !rmii_in_rst(13); }

void EthMacTlm::hard_reset() {
    regs_.reset();
    errinfo_pend_[0] = errinfo_pend_[1] = ErrinfoPend();
    dma_reset();
    rmon_.reset();
    rmon_.en = rmon_.freeze = false;
    mdio_.busy = mdio_.pending = false;
    mdio_.first = true;
    ++mdio_.gen;
    tx_fifo_flush();
    rx_fifo_flush();
    rx_line_.clear();
    ++rxmac_gen_;
    ++txmac_gen_;
    tx_busy_ = false;
    mdio_ev_.notify(SC_ZERO_TIME);
    rx_line_ev_.notify(SC_ZERO_TIME);
    txmac_reset_ev_.notify(SC_ZERO_TIME);
    update_irq();
}

void EthMacTlm::apply_block_resets(uint32_t before, uint32_t after) {
    auto held = [](uint32_t c, unsigned bit) { return !(c & 1u) || !(c & (1u << bit)); };
    auto enters = [&](unsigned bit) { return !held(before, bit) && held(after, bit); };
    if (enters(4)) dma_reset();
    if (enters(3)) {
        mdio_.busy = mdio_.pending = false;     
        mdio_.first = true;
        ++mdio_.gen;
        mdio_ev_.notify(SC_ZERO_TIME);
    }
    if (enters(11)) rmon_.reset();
    if (enters(2) || enters(5) || enters(12)) tx_fifo_flush();   // TX path group
    if (enters(6) || enters(13)) rx_fifo_flush();
    if (enters(7) || enters(9)) {
        rx_line_.clear();
        ++rxmac_gen_;
        rx_line_ev_.notify(SC_ZERO_TIME);
    }
    // A path-group reset flushes even a claimed frame. Cancel the worker's
    // timed wait before it can read/pop the cleared FIFO or count completion.
    if (enters(2) || enters(5) || enters(12) || enters(8) || enters(10)) {
        ++txmac_gen_;
        tx_busy_ = false;
        txmac_reset_ev_.notify(SC_ZERO_TIME);
    }
    sync_rmon();
    // released blocks: anything waiting on a path re-checks it
    tx_ev_.notify(SC_ZERO_TIME);
    rx_ev_.notify(SC_ZERO_TIME);
    tx_fifo_ev_.notify(SC_ZERO_TIME);
    rx_fifo_ev_.notify(SC_ZERO_TIME);
}

void EthMacTlm::reset_thread() {
    while (true) {
        if (!rst_n.read()) {
            rst_asserted_ = true;
            hard_reset();
            wait(rst_n.posedge_event());
        }
        rst_asserted_ = false;
        tx_ev_.notify(SC_ZERO_TIME);
        rx_ev_.notify(SC_ZERO_TIME);
        wait(rst_n.negedge_event());
    }
}

void EthMacTlm::rmii_reset_thread() {
    while (true) {
        if (!rmii_rst_n.read()) {
            rmii_rst_asserted_ = true;
            // the RMII side of both FIFOs and the MACs; either-side rule
            tx_fifo_flush();
            rx_fifo_flush();
            rx_line_.clear();
            ++rxmac_gen_;
            ++txmac_gen_;
            tx_busy_ = false;
            rx_line_ev_.notify(SC_ZERO_TIME);
            txmac_reset_ev_.notify(SC_ZERO_TIME);
            wait(rmii_rst_n.posedge_event());
        }
        rmii_rst_asserted_ = false;
        tx_fifo_ev_.notify(SC_ZERO_TIME);
        wait(rmii_rst_n.negedge_event());
    }
}

// =============================================================================
// CSR fields the datapath samples
// =============================================================================
bool EthMacTlm::speed100() const { return regs_.value(ETH_RMII_CFG) & ETH_RMII_CFG_SPEED_MASK; }

core::RxMacConfig EthMacTlm::rx_mac_config() const {
    core::RxMacConfig c;
    c.min_frame = uint16_t(regs_.value(ETH_RX_MIN_FRAME));
    c.max_frame = uint16_t(regs_.value(ETH_RX_MAX_FRAME));
    const uint32_t fc = regs_.value(ETH_FILT_CTRL);
    c.filt.bcast_accept = fc & ETH_FILT_CTRL_BCAST_ACCEPT_MASK;
    c.filt.pass_all_mcast = fc & ETH_FILT_CTRL_PASS_ALL_MCAST_MASK;
    c.filt.hash_en = fc & ETH_FILT_CTRL_HASH_EN_MASK;
    c.filt.hash_table = uint64_t(regs_.value(ETH_HASH_H)) << 32 | regs_.value(ETH_HASH_L);
    c.filt.uc_num = std::min(p_.rx_unicast_num, 4u);
    for (unsigned n = 0; n < c.filt.uc_num; ++n) {
        const uint32_t lo = regs_.value(ETH_UC_ADDR_L0 + 0x10 * n);
        const uint32_t hi = regs_.value(ETH_UC_ADDR_H0 + 0x10 * n);
        for (unsigned k = 0; k < 4; ++k) c.filt.uc[n][k] = uint8_t(lo >> (8 * k));
        c.filt.uc[n][4] = uint8_t(hi);
        c.filt.uc[n][5] = uint8_t(hi >> 8);
        c.filt.uc_en[n] = regs_.value(ETH_UC_CTRL0 + 0x10 * n) & 1;
    }
    return c;
}

// =============================================================================
// RMON: counters in the core, overflow flags mirrored in RMON_OVF_STAT_*
// =============================================================================
bool EthMacTlm::rmon_live() const { return !in_rst(11); }

void EthMacTlm::sync_rmon() {
    regs_.hw_store(RMON_OVF_STAT_RX, ~0u, rmon_.ovf_rx());
    regs_.hw_store(RMON_OVF_STAT_TX, ~0u, rmon_.ovf_tx());
}

// =============================================================================

// =============================================================================
void EthMacTlm::mdio_start() {
    if (in_rst(3)) return;                      // START pulse lands on a block in reset
    if (mdio_.busy) {
        regs_.hw_set(ETH_MDIO_STAT, ETH_MDIO_STAT_START_WHILE_BUSY_MASK);
        return;
    }
    const uint32_t c = regs_.value(ETH_MDIO_CTRL);
    mdio_.op_write = c & ETH_MDIO_CTRL_OP_MASK;
    mdio_.regad = (c & ETH_MDIO_CTRL_REGAD_MASK) >> ETH_MDIO_CTRL_REGAD_SHIFT;
    mdio_.phyad = (c & ETH_MDIO_CTRL_PHYAD_MASK) >> ETH_MDIO_CTRL_PHYAD_SHIFT;
    mdio_.clk_div = std::max(40u, (c & ETH_MDIO_CTRL_CLK_DIV_MASK) >> ETH_MDIO_CTRL_CLK_DIV_SHIFT);
    // the first frame after reset always sends the full 32-bit preamble
    mdio_.pre_suppress = !mdio_.first && (c & ETH_MDIO_CTRL_PRE_SUPPRESS_MASK);
    mdio_.first = false;
    mdio_.wdata = regs_.value(ETH_MDIO_DATA) & 0xFFFF;
    mdio_.busy = mdio_.pending = true;
    mdio_ev_.notify(SC_ZERO_TIME);
}

void EthMacTlm::mdio_thread() {
    while (true) {
        wait(mdio_ev_);
        if (!mdio_.pending) continue;
        mdio_.pending = false;
        const uint64_t gen = mdio_.gen;
        // 65 bit times (bit 0..64), or 34 from bit 31 with a 1-bit preamble
        const unsigned bits = mdio_.pre_suppress ? 34 : 65;
        const sc_time end = sc_time_stamp() + p_.clk_period * double(bits * mdio_.clk_div);
        while (sc_time_stamp() < end && gen == mdio_.gen) wait(end - sc_time_stamp(), mdio_ev_);
        if (gen != mdio_.gen) continue;         // reset during the frame: no DONE
        uint16_t rdata;
        uint8_t buf[2] = {uint8_t(mdio_.wdata), uint8_t(mdio_.wdata >> 8)};
        tlm::tlm_generic_payload t;
        t.set_command(mdio_.op_write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
        t.set_address((mdio_.phyad << 5) | mdio_.regad);
        t.set_data_ptr(buf);
        t.set_data_length(2);
        t.set_streaming_width(2);
        t.set_byte_enable_ptr(nullptr);
        t.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_time d = SC_ZERO_TIME;
        mdio->b_transport(t, d);
        if (gen != mdio_.gen) continue;
        if (mdio_.op_write) {
            rdata = uint16_t(mdio_.wdata);      // DATA reloads the written value
        } else if (t.get_response_status() == tlm::TLM_OK_RESPONSE) {
            rdata = uint16_t(buf[0] | buf[1] << 8);
        } else {
            rdata = 0xFFFF;                     // turnaround sampled 1: nobody answered
            regs_.hw_set(ETH_MDIO_STAT, ETH_MDIO_STAT_NO_RESPONSE_MASK);
        }
        regs_.hw_store(ETH_MDIO_DATA, 0xFFFF, rdata);
        regs_.hw_set(ETH_MDIO_STAT, ETH_MDIO_STAT_DONE_MASK);
        mdio_.busy = false;
        update_irq();
    }
}

}  // namespace icsl_eth
