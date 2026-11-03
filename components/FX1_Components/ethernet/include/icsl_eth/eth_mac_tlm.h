// =============================================================================
// Integrated Circuits and Systems laboratory
// Author: Hung Nguyen <ngthung@hcmut.edu.vn>
// Module: virtual_platform.eth_mac_tlm.eth_mac_tlm
// Description: Loosely-timed SystemC/TLM-2.0 model of the 10/100 Ethernet MAC.
// Changelog:
//   2026-09-29: Initial version.
// =============================================================================
//
// Loosely timed Ethernet MAC model. Bind csr to the register bus, axi to
// memory, phy_tx/phy_rx to frame transport, mdio to a PHY, irq to an
// interrupt controller, and rst_n/rmii_rst_n to active-low resets.
// See README.md for interface behavior, timing and model limitations.
#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "icsl_eth/csr_file.h"
#include "icsl_eth/eth_core.h"
#include "icsl_eth/eth_frame.h"

namespace icsl_eth {

struct EthMacParams {
    sc_core::sc_time clk_period{10, sc_core::SC_NS};  
    unsigned csr_latency_cycles = 2;                    // AXI4-Lite access
    unsigned axi_data_bytes = 4;                        // PARA_AXI_DATA_WIDTH / 8: 4 or 8
    unsigned axi_max_burst = 16;                        // PARA_AXI_MAX_BURST_LEN, 1..16
    unsigned rx_fifo_bytes = 8192;                      // PARA_RX_FIFO_DEPTH
    unsigned tx_fifo_bytes = 4096;                      // PARA_TX_FIFO_DEPTH
    unsigned max_frame_len = 1518;                      // PARA_MAX_FRAME_LEN
    unsigned rx_unicast_num = 4;                        // PARA_RX_UNICAST_NUM, 1..4
    unsigned mdio_clk_div = 40;                         // PARA_MDIO_CLK_DIV
    unsigned poll_cycles = 1024;                        // PARA_RX_POLL_CYCLES
    unsigned tx_meta_depth = 64;                        // PARA_TX_META_DEPTH
    unsigned tx_meta_reserve = 5;                       // metadata FIFO space_ok threshold
    unsigned rx_fifo_latency_cycles = 8;                // RX FIFO commit -> DMA
    uint64_t dram_base = 0;                             // integration adder, 4 KiB multiple
    uint64_t csr_window = 0x10000;                      // 2^PARA_AXIL_ADDR_WIDTH, 16..32 bits

    // Empty when the set is legal (MAS Table 5-1), else what is wrong with it.
    // The constructor of EthMacTlm reports a non-empty result as SC_FATAL.
    std::string validate() const;
};

class EthMacTlm : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<EthMacTlm> csr;
    tlm_utils::simple_initiator_socket<EthMacTlm> axi;
    tlm_utils::simple_initiator_socket<EthMacTlm> phy_tx;
    tlm_utils::simple_target_socket<EthMacTlm> phy_rx;
    tlm_utils::simple_initiator_socket<EthMacTlm> mdio;
    sc_core::sc_in<bool> rst_n;
    sc_core::sc_in<bool> rmii_rst_n;
    sc_core::sc_out<bool> irq;

    SC_HAS_PROCESS(EthMacTlm);
    EthMacTlm(sc_core::sc_module_name name, const EthMacParams& p = EthMacParams());

    const EthMacParams& params() const { return p_; }
    // Introspection for tests and platforms (never used by software).
    uint32_t peek(uint32_t offset) const;
    uint64_t frames_sent() const { return tx_frames_; }
    uint64_t frames_received() const { return rx_frames_; }

private:
    // ---- register access ----------------------------------------------------
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay);
    unsigned transport_dbg(tlm::tlm_generic_payload& tx);
    bool get_direct_mem_ptr(tlm::tlm_generic_payload& tx, tlm::tlm_dmi& dmi);
    tlm::tlm_response_status access(tlm::tlm_generic_payload& tx, bool debug);
    void on_write(const CsrFile::Write& w);
    void errinfo_on_clear(bool tx, const CsrFile::Write& w);
    uint32_t hw_read(uint32_t offset, uint32_t stored) const;
    bool accept_write(uint32_t offset) const;

    // ---- interrupt: update_irq() only schedules, drive_irq() is the one writer
    bool irq_level() const;
    void update_irq();
    void drive_irq();
    sc_core::sc_event irq_ev_;

    // ---- resets --------------------------------------------------------------
    void reset_thread();
    void rmii_reset_thread();
    void hard_reset();
    void apply_block_resets(uint32_t before, uint32_t after);
    bool in_rst(unsigned bit) const;                    // block of ETH_CTRL[bit] held
    bool rmii_in_rst(unsigned bit) const;
    bool tx_stream_ok() const;                          // DMA -> Tx csum -> TX FIFO
    bool tx_line_ok() const;                            // TX FIFO -> Tx MAC -> RMII
    bool rx_line_ok() const;                            // RMII -> Rx MAC
    bool rx_fifo_writable() const;                      // Rx MAC -> RX FIFO
    bool rx_stream_ok() const;                          // RX FIFO -> Rx csum -> DMA
    bool rst_asserted_ = true;                          // rst_n low

    // ---- CSR fields the datapath samples ----------------------------------------
    bool speed100() const;
    core::RxMacConfig rx_mac_config() const;

    // ---- DMA (eth_mac_dma.cpp) ----------------------------------------------------
    struct Coal {
        uint32_t count = 0;
        bool run = false;
        sc_core::sc_time deadline;
        // the last timer expiry, undone by a completion on the same edge (Table 9-3)
        bool expired = false;
        uint32_t expired_count = 0;
        sc_core::sc_time expired_at;
        void reset() { count = 0; run = false; expired = false; }
    };
    enum class TxSt { IDLE, FETCH, SUSPEND, CFG_CHECK, READ, DESC_WB, ABORT, SKIP, HALT };
    enum class RxSt { IDLE, ARMED, WRITE, WAIT_VRD, DESC_WB, DISCARD, HALT };
    enum Cause : uint32_t { C_NONE = 0, C_DESC_RD, C_DESC_WR, C_DATA, C_TOO_LONG, C_LEN_ZERO,
                            C_NO_FS, C_SKIP_OWN };
    struct Desc {
        uint32_t w[4] = {0, 0, 0, 0};
        bool own() const { return w[0] >> 31; }
        bool ls() const { return (w[1] >> 31) & 1; }
        bool fs() const { return (w[1] >> 30) & 1; }
        uint32_t buflen() const { return w[1] & 0xFFFF; }
        uint32_t off() const { return w[2]; }
    };
    struct TxDma {
        TxSt st = TxSt::IDLE;
        uint32_t idx = 0;
        bool halted = false, halt_pend = false, force = false, db_pend = false;
        bool in_frame = false, frame_open = false;
        uint32_t acc_len = 0;
        uint32_t cause = C_NONE;
        bool dec = false;
        Coal coal;
    } tx_;
    struct RxDma {
        RxSt st = RxSt::IDLE;
        uint32_t idx = 0;
        bool halted = false, halt_pend = false, force = false, db_pend = false;
        bool hd_valid = false, hd_own = false, fetch_need = false;
        uint32_t hd_off = 0;
        sc_core::sc_time poll_base;                     // poll interval runs from here
        uint32_t cause = C_NONE;
        bool dec = false;
        Coal coal;
    } rx_;
    void tx_dma_thread();
    void rx_dma_thread();
    void tx_coal_thread();
    void rx_coal_thread();
    void tx_run(uint64_t gen);
    void rx_run(uint64_t gen);
    void tx_command(uint32_t pulse);
    void rx_command(uint32_t pulse);
    void tx_halt_entry(uint64_t gen);
    void rx_halt_entry(uint64_t gen, uint32_t wb_des0, bool wb);
    void dma_err(bool tx, uint32_t cls, bool dec);
    void coal_frame(bool tx);
    void coal_expire(bool tx);
    struct ErrinfoPend {                                // an error while ERR was already set
        sc_core::sc_time at = sc_core::SC_ZERO_TIME;
        bool valid = false;
        uint32_t value = 0;
    } errinfo_pend_[2];                                 // [0] RX, [1] TX
    void dma_reset();
    // AXI helpers: false on an error response (dec = DECERR); every call waits
    // out its annotated delay and returns false early if `gen` is overtaken.
    bool axi_read(uint64_t off, unsigned bytes, uint8_t* dst, bool& err, bool& dec, uint64_t gen);
    bool axi_write(uint64_t off, const uint8_t* src, const uint8_t* be, unsigned bytes, bool& err,
                   bool& dec, uint64_t gen);
    bool fetch_desc(uint32_t base_reg, uint32_t idx, Desc& d, bool& err, bool& dec, uint64_t gen);
    bool write_des0(uint32_t base_reg, uint32_t idx, uint32_t des0, bool& err, bool& dec,
                    uint64_t gen);
    bool wait_dma(const sc_core::sc_time& t, uint64_t gen);
    uint32_t desc_len(bool tx) const;
    uint32_t idx_inc(uint32_t idx, bool tx) const;
    uint64_t dma_gen_ = 0;
    sc_core::sc_event tx_ev_, rx_ev_, dma_reset_ev_, tx_coal_ev_, rx_coal_ev_;

    // ---- frame FIFOs and MACs (eth_mac_datapath.cpp) -------------------------------
    struct TxFrame {
        core::Bytes bytes;                              // DA..payload, checksums inserted
    };
    struct RxFrame {
        core::Bytes bytes;                              // DA..payload (FCS stripped)
        unsigned words = 0;                             // RX FIFO occupancy
        sc_core::sc_time ready;                         // visible to the DMA from
    };
    struct LineFrame {
        core::Bytes bytes;                              // DA..FCS
        EthFrameExt ext;
        sc_core::sc_time end;
        uint64_t gen = 0;                               // Rx MAC epoch
        // RX FIFO epoch at the SOF; only a frame whose SOF found the FIFO out of
        // reset, with no flush since, is written into it (MAS Table 6-2 row 8)
        uint64_t fifo_gen = 0;
        bool fifo_open = false;
    };
    void phy_rx_b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay);
    void tx_mac_thread();
    void rx_mac_thread();
    unsigned words_of(size_t bytes) const;
    // TX FIFO and metadata FIFO (store and forward)
    std::deque<TxFrame> tx_fifo_;
    bool tx_claimed_ = false;                           // front frame is on the line
    unsigned tx_fifo_used_ = 0;                         // bytes, committed frames by word
    core::Bytes tx_open_;                               // the frame being forwarded
    bool tx_fifo_space(unsigned bytes) const;
    bool tx_meta_space() const;
    void tx_fifo_flush();
    sc_core::sc_event tx_fifo_ev_;
    // RX FIFO
    std::deque<RxFrame> rx_fifo_;
    unsigned rx_fifo_used_ = 0;                         // words
    uint64_t rx_fifo_gen_ = 0;                          // moves on every flush
    void rx_fifo_flush();
    sc_core::sc_event rx_fifo_ev_;
    // the line
    std::deque<LineFrame> rx_line_;
    uint64_t rxmac_gen_ = 0, txmac_gen_ = 0;
    bool tx_busy_ = false;
    sc_core::sc_time tx_last_end_;
    sc_core::sc_event rx_line_ev_, txmac_reset_ev_;
    uint64_t tx_frames_ = 0, rx_frames_ = 0;

    // ---- RMON -------------------------------------------------------------------
    core::Rmon rmon_;
    void sync_rmon();
    bool rmon_live() const;

    // ---- MDIO (eth_mac_tlm.cpp) -----------------------------------------------------
    void mdio_thread();
    void mdio_start();
    struct Mdio {
        bool busy = false, pending = false, first = true;
        bool op_write = false, pre_suppress = false;
        uint32_t phyad = 0, regad = 0, clk_div = 40, wdata = 0;
        uint64_t gen = 0;
    } mdio_;
    sc_core::sc_event mdio_ev_;

    EthMacParams p_;
    CsrFile regs_;
    bool rmii_rst_asserted_ = true;                     // rmii_rst_n low
};

}  // namespace icsl_eth
