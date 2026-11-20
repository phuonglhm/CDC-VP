// has_cpu_direct.h -- HAS_CPU_DIRECT=1: the CPU programs the SAURIA, GVU and DMA blocks of neo_top directly (no data flow controller), as the
// configuration port of neo_lite RTL revision 2 allows: an AXI-Lite demux selects the block, and in front of each block an AXI-Lite mux
// arbitrates between the CPU and the data flow controller (no select bit; software must not run both at the same time).
//
// Address map of the model (system wrapper offset = model address; the wrapper passes these offsets through unchanged). One 4 KiB page per
// block; the offset inside a page is the offset inside the block of the RTL, so firmware changes only the base:
//   model address = 0x10000 + (RTL block offset >> 4) + offset            (FPGA: cfg_base + RTL block offset + offset)
//   0x11000  SAURIA block   0x000 CONTROL, 0x004 global interrupt enable, 0x008 interrupt enable, 0x00C interrupt status (write 1 to clear),
//                           0x010..0x01C status / counters (read), 0x200 CON, 0x400 ACT, 0x600 WEI, 0x800 OUT (packed words of config_regs.sv)
//   0x12000  GVU block      0x00..0x34 the 14 registers of gvu_config_regs.sv (no interrupt to the CPU: poll CONTROL bit 1)
//   0x13000  DMA block      0x00..0x3F one-dimensional DMA (conf_regs.sv), 0x80..0xC4 tensor operations (has/has_opgen.h, HAS_DMA_OPGEN)
//   0x14000  SYNC (model)   0x0 SYNC_ADDR, 0x4 SYNC_SIZE, 0x8 SYNC_CMD (1 = system RAM -> model DRAM, 2 = model DRAM -> system RAM), 0xC SYNC_STATUS
//                           (reads 0). The DRAM of the model is a vector of the wrapper; a wrapper that keeps it separate from system RAM acts on
//                           the write of SYNC_CMD. The model stores the values and does nothing else.
// The controller block stays where it is (0x40000800, has/has_host_neo_port.h). The one-dimensional DMA at 0x40000A00 + 0x00..0x3F is the same
// register file as 0x13000 + 0x00..0x3F while this knob is on.
//
// One-dimensional DMA (conf_regs.sv, dma_top.sv): CONTROL bit 0 starts the reader, bit 1 the writer, bit 8 selects write-zero; 0x04 interrupt
// enable (bit 0 reader, bit 1 writer), 0x0C interrupt status (write 1 to clear), 0x10 read start address, 0x20 write start address, 0x30 bytes to
// transfer. A bus address selects the memory as dma_top.sv does: (address & 0xFFF00000) == 0 a local memory of SAURIA (SRAM A 0x0, B 0x40000,
// C 0x80000), == 0x00100000 the GVU scratchpad (vbank0 +0x0, vbank1 +0x20000, vbank2 +0x40000, sbank +0x80000), any other value system memory
// (DRAM offset = address - dram_base). Local memories are reached on the side the accelerator does not use (the host half of the SRAM selected by
// the SAURIA select bits, the scratchpad buffer not selected by the GVU buffer-select flop), byte-linear inside a bank.
// Model of the sequence, not of the clocks: a command whose writer starts moves btt bytes at its end; it costs, in core cycles, the measured
// one-dimensional command of has/dma_dfc_1d.h (first 6 + ceil(btt / 16) + 1 data + tail 3). The reader only sets its status when the writer
// takes the data (the RTL reader fills a FIFO; the model has none). Write-zero writes btt zero bytes without the reader.
// Interrupts (neo_interface.sv): the reader / writer interrupt of the DMA reaches the CPU only while the data flow controller is idle; the model
// ORs them into o_irq (the RTL has separate wires). Writes to a block while the data flow controller runs an instruction are applied (the RTL
// does not block them) and counted; the first one is reported on stderr.
// The revision is checked at the first access, not at construction: a run profile sets HAS_NEO_REV after the members of HasNpuTop are built
// and has::neo_rev() keeps the first value it reads.
// SAURIA block, further (config_regs.sv): CONTROL reads back start / done / idle / ready / auto restart / mem switch / keep / soft reset and 0xAC in
// [31:24]; 0x10 status, 0x14 cycle counter, 0x18 stall counter, 0x1C version. The auto restart bit is stored and read back only (nothing in the RTL acts on
// it). A soft reset write (bit 23) aborts a run of the core and drains the PSM stream. START with the output-stream enable bit set waits for ready
// (done and the stream empty), as the RTL gates its start edge; the three stream bits of the packed words (psum_stream_en, cin_tile_last, oc_tile_last)
// are handed to the owner of the core with the START request.
// Env: HAS_CPU_DIRECT (0/1, default 0; acts with HAS_NEO_REV >= 2), HAS_CPU_DIRECT_DRAM_BASE (system address of DRAM offset 0, default 0x80000000).
// Pure C++17, no SystemC.
#ifndef HAS_CPU_DIRECT_H
#define HAS_CPU_DIRECT_H

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <vector>
#include "sauria_cfg_layout.h"
#include "driver/libneo_cfg.h"
#include "has/neo_rev.h"

namespace has
{
    namespace cpu_direct
    {
        // ---- SAURIA block (config_regs.sv): region 0 and the 29 packed configuration words (CON 2, ACT 17, WEI 5, OUT 5) ----
        constexpr unsigned N_CORE_WORDS = neo::addr::CORE_WORDS_CON + neo::addr::CORE_WORDS_ACT + neo::addr::CORE_WORDS_WEI + neo::addr::CORE_WORDS_OUT;
        static_assert(N_CORE_WORDS == neo::N_CORE, "the 29 core words of the controller image");
        // Word index of a byte offset inside the SAURIA block (region base + 4 * index), -1 = not a configuration word.
        inline int core_word_index(uint32_t off)
        {
            using namespace neo::addr;
            const uint32_t reg = off & 0xF00u, i = (off & 0xFFu) / 4u;
            if (off % 4u) return -1;
            if (reg == CFG_CON_OFFSET) return i < CORE_WORDS_CON ? static_cast<int>(i) : -1;
            if (reg == CFG_ACT_OFFSET) return i < CORE_WORDS_ACT ? static_cast<int>(CORE_WORDS_CON + i) : -1;
            if (reg == CFG_WEI_OFFSET) return i < CORE_WORDS_WEI ? static_cast<int>(CORE_WORDS_CON + CORE_WORDS_ACT + i) : -1;
            if (reg == CFG_OUT_OFFSET) return i < CORE_WORDS_OUT ? static_cast<int>(CORE_WORDS_CON + CORE_WORDS_ACT + CORE_WORDS_WEI + i) : -1;
            return -1;
        }
        // The packed words as config_regs.sv reads them: the fields of sauria::cfg_decode plus the Y per-row offsets of the ifmap feeder
        // (o_loc_woffs, "local word offset array, encodes strides": row y reads at y * stride when it is active). The core of the model takes
        // one stride register instead of the array, so the offsets must be y * s for the active rows (s >= 0); stride = s. false = not
        // expressible in the model core (why says it).
        inline bool decode_core(const std::vector<uint32_t> &w, const sauria::CfgWidths &cw, uint64_t f[sauria::F_CFG_COUNT], uint32_t &stride,
                                const char *&why)
        {
            using namespace sauria;
            for (int i = 0; i < F_CFG_COUNT; i++) f[i] = 0;
            std::vector<uint64_t> per_row(cw.Y, 0);
            uint64_t row_mask = 0;
            size_t bitpos = 0;
            for (int i = 0; i < SAURIA_CFG_LAYOUT_N; i++)
            {
                const CfgFieldSpec &s = SAURIA_CFG_LAYOUT[i];
                const uint32_t width = cfg_resolve_width(s.wkind, cw);
                if (s.special == SP_SKIP_PER_ROW)
                {
                    row_mask = width >= 64 ? ~0ull : ((1ull << width) - 1ull);
                    for (uint32_t y = 0; y < cw.Y; y++) per_row[y] = cfg_read_bits(w, 0, bitpos, width);
                }
                else if (s.special == SP_ROWS_ACTIVE_MSB) f[s.field] = cfg_bitrev(cfg_read_bits(w, 0, bitpos, width), width);
                else f[s.field] = cfg_read_bits(w, 0, bitpos, width);
                if (s.align_after) cfg_align(bitpos);
            }
            const uint64_t act = f[F_CFG_ROWS_ACTIVE];
            int first = -1, second = -1;
            for (uint32_t y = 0; y < cw.Y; y++) if ((act >> y) & 1u) { if (first < 0) first = static_cast<int>(y); else if (second < 0) second = static_cast<int>(y); }
            uint64_t s = 0;
            if (first >= 0 && second >= 0 && per_row[static_cast<size_t>(second)] >= per_row[static_cast<size_t>(first)])
                s = (per_row[static_cast<size_t>(second)] - per_row[static_cast<size_t>(first)]) / static_cast<uint64_t>(second - first);
            for (uint32_t y = 0; y < cw.Y; y++)
                if (((act >> y) & 1u) && per_row[y] != ((y * s) & row_mask)) { why = "the per-row offsets of the ifmap feeder are not y * stride"; return false; }
            stride = static_cast<uint32_t>(s);
            why = nullptr;
            return true;
        }
        inline sauria::CfgWidths core_widths()
        {
            const sauria::SauriaTarget &t = neo::target_rev(neo_rev());
            sauria::CfgWidths w; w.idx_a = t.idx_a; w.idx_w = t.idx_w; w.idx_o = t.idx_o; w.X = t.X; w.Y = t.Y; return w;
        }

        constexpr uint32_t PAGE = 0x1000u;
        constexpr uint32_t SAURIA_BASE = 0x00011000u, GVU_BASE = 0x00012000u, DMA_BASE = 0x00013000u, SYNC_BASE = 0x00014000u;
        constexpr uint32_t LEGACY_DMA_BASE = 0x40000A00u;   // the DMA register files of has/has_opgen.h
        constexpr uint32_t model_addr(uint32_t rtl_block_offset, uint32_t off) { return 0x10000u + (rtl_block_offset >> 4) + off; }
        static_assert(model_addr(0x00010000u, 0) == SAURIA_BASE && model_addr(0x00020000u, 0) == GVU_BASE && model_addr(0x00030000u, 0) == DMA_BASE,
                      "one page per block, offset inside the block as in the RTL");

        // conf_regs.sv
        constexpr uint32_t CR_CONTROL = 0x00u, CR_IEN = 0x04u, CR_ISTATUS = 0x0Cu, CR_RADDR = 0x10u, CR_WADDR = 0x20u, CR_BTT = 0x30u;
        constexpr uint32_t CR_ID = 0x2Du, CR_BAD_READ = 0x3BADADD2u;
        // dma_top.sv / neo_addr_pkg.sv
        constexpr uint32_t BUS_SPACE_MASK = 0xFFF00000u, BUS_SAURIA = 0x00000000u, BUS_GVU = 0x00100000u;
        constexpr uint32_t SRAMB_OFF = 0x00040000u, SRAMC_OFF = 0x00080000u;
        // Bits of the packed core words: the output stream metadata of config_regs.sv (o_psum_stream_en, o_cin_tile_last, o_oc_tile_last).
        constexpr unsigned BIT_STREAM_EN = neo::addr::rev2::CORE_BIT_OUT_EXTRA + 1, BIT_CIN_LAST = neo::addr::rev2::CORE_BIT_OUT_EXTRA + 2,
                           BIT_OC_TILE_LAST = neo::addr::rev2::CORE_BIT_OUT_EXTRA + 3;
        inline bool core_bit(const std::vector<uint32_t> &w, unsigned bit) { return (bit / 32u) < w.size() && ((w[bit / 32u] >> (bit % 32u)) & 1u) != 0; }
        // version word of config_regs.sv: IA_W - 1, IB_W - 1, Y, X (ARITHMETIC 0)
        constexpr uint32_t SAURIA_VERSION = (7u << 20) | (7u << 16) | (32u << 8) | 32u;
        constexpr uint32_t GVU_MEM_MASK = 0x000F0000u, VBANK1_OFF = 0x00020000u, VBANK2_OFF = 0x00040000u, SBANK_SEL = 0x00080000u;

        enum Mem : int { M_SRAMA = 0, M_SRAMB = 1, M_SRAMC = 2, M_VBANK0 = 3, M_VBANK1 = 4, M_VBANK2 = 5, M_SBANK = 6, M_DRAM = 7, M_BAD = 8 };

        // HAS_CPU_DIRECT set (no revision check: safe to call while HasNpuTop is being built)
        inline bool env_wanted()
        {
            const char *e = std::getenv("HAS_CPU_DIRECT");
            return e && std::atoi(e) != 0;
        }

        struct Stats
        {
            uint64_t writes{0}, reads{0}, bad_access{0}, dfc_overlap{0};
            uint64_t dma_cmds{0}, dma_bytes{0}, dma_cycles{0}, dma_bad{0}, dma_zero_cmds{0}, sync_cmds{0};
            uint64_t sauria_starts{0}, sauria_failed{0}, sauria_start_ignored{0}, sauria_write_busy{0}, sauria_soft_reset{0}, sauria_cycles{0};
            uint64_t selfcheck_calls{0}, selfcheck_bad{0}, selfcheck_wide{0};
            uint64_t status_reads{0}, start_without_soft_reset{0};
            uint32_t last_status{0}, last_cycles{0}, last_stalls{0}, last_version{0};   // the last values read from 0x10 .. 0x1C
        };

        // Byte access to a local memory, given by HasNpuTop: (memory, byte offset inside the bank, data, bytes, write) -> false = outside the bank.
        typedef std::function<bool(Mem, uint32_t, uint8_t *, uint32_t, bool)> LocalAccess;

        class Port
        {
        public:
            Port() : want_(env_wanted())
            {
                if (const char *e = std::getenv("HAS_CPU_DIRECT_DRAM_BASE")) dram_base_ = static_cast<uint32_t>(std::strtoul(e, nullptr, 0));
            }
            bool wanted() const { return want_; }   // the knob is set (wiring at construction)
            bool on() const                            // the knob is set and the revision is 2 or later (at run time)
            {
                if (!checked_) { checked_ = true; on_ = want_ && neo_rev() >= NEO_REV_2; }
                return on_;
            }
            const Stats &stats() const { return st_; }
            void set_dram(std::vector<uint8_t> *d) { dram_ = d; }
            void set_dram_base(uint32_t b) { dram_base_ = b; }
            uint32_t dram_base() const { return dram_base_; }
            void set_local(LocalAccess f) { local_ = std::move(f); }
            // The GVU block: the register file of the vector unit (has/gvu_top_stream.h cpu_write / cpu_read); unset = writes counted as bad.
            void set_gvu(std::function<void(uint32_t, uint32_t)> w, std::function<uint32_t(uint32_t)> r) { gvu_w_ = std::move(w); gvu_r_ = std::move(r); }

            // The pages of this knob, plus the one-dimensional DMA at its earlier address (shared register file).
            bool owns(uint32_t a) const
            {
                if (!on()) return false;
                if (a >= SAURIA_BASE && a < SAURIA_BASE + PAGE) return true;
                if (a >= GVU_BASE && a < GVU_BASE + PAGE) return true;
                if (a >= DMA_BASE && a < DMA_BASE + 0x40u) return true;
                if (a >= LEGACY_DMA_BASE && a < LEGACY_DMA_BASE + 0x40u) return true;
                return a >= SYNC_BASE && a < SYNC_BASE + 0x10u;
            }
            // Offsets 0x80.. of the DMA page are the tensor-operation register file of has/has_opgen.h: model address of it at 0x40000A00.
            static bool is_opgen_page(uint32_t a) { return a >= DMA_BASE + 0x80u && a < DMA_BASE + PAGE; }
            static uint32_t opgen_alias(uint32_t a) { return LEGACY_DMA_BASE + (a - DMA_BASE); }

            void soft_reset()
            {
                cr_ = Conf(); run_ = false; remaining_ = 0;
                sync_addr_ = sync_size_ = sync_cmd_ = 0;
                sa_ = Sauria();
            }

            void write(uint32_t a, uint32_t v, bool dfc_busy)
            {
                st_.writes++;
                note_overlap(dfc_busy);
                if (a >= SAURIA_BASE && a < SAURIA_BASE + PAGE) { sauria_write(a - SAURIA_BASE, v); return; }
                if (a >= GVU_BASE && a < GVU_BASE + PAGE) { if (gvu_w_) gvu_w_(a - GVU_BASE, v); else st_.bad_access++; return; }
                if (a >= SYNC_BASE && a < SYNC_BASE + 0x10u)
                {
                    const uint32_t o = a - SYNC_BASE;
                    if (o == 0x0u) sync_addr_ = v; else if (o == 0x4u) sync_size_ = v;
                    else if (o == 0x8u) { sync_cmd_ = v; st_.sync_cmds++; }
                    return;
                }
                const uint32_t o = (a >= LEGACY_DMA_BASE ? a - LEGACY_DMA_BASE : a - DMA_BASE) & 0x3Fu;
                if (o == CR_CONTROL)
                {
                    cr_.wz = (v >> 8) & 1u;
                    if (v & 1u) cr_.rd_pend = true;
                    if (v & 2u) { cr_.wr_pend = true; if (!run_) begin_cmd(); }
                }
                else if (o == CR_IEN) cr_.ien = v & 3u;
                else if (o == CR_ISTATUS) cr_.ist &= ~(v & 3u);
                else if (o == CR_RADDR) cr_.raddr = v;
                else if (o == CR_WADDR) cr_.waddr = v;
                else if (o == CR_BTT) cr_.btt = v;
                else st_.bad_access++;
            }

            uint32_t read(uint32_t a)
            {
                st_.reads++;
                if (a >= SAURIA_BASE && a < SAURIA_BASE + PAGE) return sauria_read(a - SAURIA_BASE);
                if (a >= GVU_BASE && a < GVU_BASE + PAGE) return gvu_r_ ? gvu_r_(a - GVU_BASE) : 0u;
                if (a >= SYNC_BASE && a < SYNC_BASE + 0x10u)
                {
                    const uint32_t o = a - SYNC_BASE;
                    return o == 0x0u ? sync_addr_ : o == 0x4u ? sync_size_ : o == 0x8u ? sync_cmd_ : 0u;
                }
                const uint32_t o = (a >= LEGACY_DMA_BASE ? a - LEGACY_DMA_BASE : a - DMA_BASE) & 0x3Fu;
                if (o == CR_CONTROL) return (CR_ID << 24) | (cr_.wz << 8);
                if (o == CR_IEN) return cr_.ien;
                if (o == CR_ISTATUS) return cr_.ist;
                if (o == CR_RADDR) return cr_.raddr;
                if (o == CR_WADDR) return cr_.waddr;
                if (o == CR_BTT) return cr_.btt;
                st_.bad_access++;
                return CR_BAD_READ;
            }

            // Interrupt lines of the DMA (conf_regs.sv: enable and status); neo_interface.sv forwards them while the controller is idle.
            bool reader_irq() const { return (cr_.ien & cr_.ist & 1u) != 0; }
            bool writer_irq() const { return (cr_.ien & cr_.ist & 2u) != 0; }
            bool sauria_irq() const { return sa_.done_intr && sa_.gien && sa_.ien; }   // config_regs.sv o_doneintr
            bool irq(bool dfc_idle) const { return on() && dfc_idle && (reader_irq() || writer_irq() || sauria_irq()); }

            // ---- SAURIA block, the side of the data flow controller thread of HasNpuTop ----
            // A start edge (CONTROL bit 0 rising while the core is ready) or a memory-switch edge (bit 16) waiting to be carried out.
            struct SauriaReq { bool start{false}, sw{false}, soft{false}, stream_en{false}, cin_last{false}, oc_tile_last{false}; bool keep[3]{false, false, false}; std::vector<uint32_t> words; };
            bool take_sauria(SauriaReq &r)
            {
                if (!sa_.start_req && !sa_.switch_req && !sa_.soft_req) return false;
                r = SauriaReq();
                r.start = sa_.start_req; r.sw = sa_.switch_req; r.soft = sa_.soft_req;
                for (int i = 0; i < 3; i++) r.keep[i] = sa_.keep[i];
                if (r.start)
                {
                    r.words = sa_.words; sa_.busy = true; sa_.done = false; st_.sauria_starts++;
                    r.stream_en = core_bit(r.words, BIT_STREAM_EN); r.cin_last = core_bit(r.words, BIT_CIN_LAST); r.oc_tile_last = core_bit(r.words, BIT_OC_TILE_LAST);
                }
                sa_.start_req = sa_.switch_req = sa_.soft_req = false;
                return true;
            }
            // The core reported done (ok) or the model gave up on the run (deadlock, image not expressible).
            void sauria_done(bool ok, uint64_t cycles)
            {
                sa_.busy = false;
                sa_.done = true; sa_.done_intr = true;   // config_regs.sv:588-590: set on the rising edge of the done of the core
                st_.sauria_cycles += cycles;
                if (!ok) st_.sauria_failed++;
            }
            bool soft_pending() const { return sa_.soft_req; }
            // A soft reset aborted the run in flight (if any): the FSMs go to idle, which raises done as a finished run does (config_regs.sv:586-590).
            void sauria_soft_reset_done(bool was_running)
            {
                sa_.busy = false;
                if (was_running) { sa_.done = true; sa_.done_intr = true; }
            }
            // Owner of the core: the PSM stream is empty (ready = done and the stream empty) and the status / counters the RTL reads back.
            void set_stream_idle(bool idle) { stream_idle_ = idle; }
            void set_sauria_counters(uint32_t status, uint32_t cycles, uint32_t stalls) { sa_.status = status; sa_.cycles = cycles; sa_.stalls = stalls; }
            void count_selfcheck(bool bad) { st_.selfcheck_calls++; if (bad) st_.selfcheck_bad++; }
            void count_wide() { st_.selfcheck_wide++; }
            bool busy() const { return run_; }

            // One core clock at which the data flow controller is idle. True = the engine used this clock.
            bool step()
            {
                if (!on() || !run_) return false;
                if (remaining_ > 0) remaining_--;
                if (remaining_ == 0) finish_cmd();
                return true;
            }

            // Where a bus address lands: memory and byte offset inside it (dma_top.sv, gvu_scratchpad.sv, neo_addr_pkg.sv).
            Mem classify(uint32_t a, uint32_t &off) const
            {
                if ((a & BUS_SPACE_MASK) == BUS_SAURIA)
                {
                    if (a >= SRAMC_OFF) { off = a - SRAMC_OFF; return M_SRAMC; }
                    if (a >= SRAMB_OFF) { off = a - SRAMB_OFF; return M_SRAMB; }
                    off = a; return M_SRAMA;
                }
                if ((a & BUS_SPACE_MASK) == BUS_GVU)
                {
                    const uint32_t g = a & 0x000FFFFFu;
                    if (g & SBANK_SEL) { off = g & 0x7FFFFu; return M_SBANK; }
                    const uint32_t r = g & GVU_MEM_MASK;
                    off = g & 0xFFFFu;
                    return r == 0u ? M_VBANK0 : r == VBANK1_OFF ? M_VBANK1 : r == VBANK2_OFF ? M_VBANK2 : M_BAD;
                }
                if (a < dram_base_) return M_BAD;
                off = a - dram_base_;
                return M_DRAM;
            }

        private:
            struct Conf { uint32_t wz{0}, ien{0}, ist{0}, raddr{0}, waddr{0}, btt{0}; bool rd_pend{false}, wr_pend{false}; };
            bool want_{false};
            mutable bool on_{false}, checked_{false};
            Conf cr_;
            Stats st_;
            std::vector<uint8_t> *dram_{nullptr};
            uint32_t dram_base_{0x80000000u};
            LocalAccess local_;
            std::function<void(uint32_t, uint32_t)> gvu_w_;
            std::function<uint32_t(uint32_t)> gvu_r_;
            bool run_{false}, cmd_zero_{false}, cmd_read_{false};
            uint64_t remaining_{0};
            bool stream_idle_{true}, auto_restart_reported_{false}, soft_since_start_{false}, start_without_soft_reported_{false};
            uint64_t starts_taken_{0};
            uint32_t cmd_r_{0}, cmd_w_{0}, cmd_n_{0};
            uint32_t sync_addr_{0}, sync_size_{0}, sync_cmd_{0};
            bool overlap_reported_{false};

            struct Sauria
            {
                bool done{false}, auto_restart{false}, gien{false}, ien{false}, done_intr{false};
                bool keep[3]{false, false, false};
                bool start_req{false}, switch_req{false}, soft_req{false}, busy{false};
                uint32_t status{0}, cycles{0}, stalls{0};
                std::vector<uint32_t> words = std::vector<uint32_t>(N_CORE_WORDS, 0u);
            } sa_;
            // config_regs.sv:492-537 (region 0) and :543-560 (the packed words). Start and mem_switch clear themselves one clock after the
            // write (:457-465), so every write with the bit set is an edge.
            void sauria_write(uint32_t off, uint32_t v)
            {
                if ((off & 0xF00u) == 0u)
                {
                    const uint32_t o = off & 0xFFu;
                    if (o == 0x0u)
                    {
                        const bool start = (v & 1u) != 0, sw = ((v >> 16) & 1u) != 0;
                        if (v & 2u) sa_.done = false;                            // write 1 to clear
                        sa_.auto_restart = ((v >> 7) & 1u) != 0;
                        if (sa_.auto_restart && !auto_restart_reported_)
                        {
                            auto_restart_reported_ = true;
                            std::fprintf(stderr, "[cpu-direct] the auto restart bit of the SAURIA CONTROL register is stored and read back; nothing in the hardware acts on it\n");
                        }
                        for (int i = 0; i < 3; i++) sa_.keep[i] = ((v >> (17 + i)) & 1u) != 0;
                        if ((v >> 23) & 1u) { sa_.soft_req = true; soft_since_start_ = true; st_.sauria_soft_reset++; }   // soft reset (config_regs.sv:984): one pulse
                        if (start)
                        {
                            // not ready: the RTL takes no start edge (the shadow copy of the words is taken at start & ready; with the output stream
                            // enabled the start pulse itself waits for ready = done and the stream empty)
                            const bool stream_en = core_bit(sa_.words, BIT_STREAM_EN);
                            if (sa_.busy || sa_.start_req || (stream_en && !stream_idle_)) st_.sauria_start_ignored++;
                            else
                            {
                                sa_.start_req = true;
                                // the RTL does not run a second invocation after a first one unless the core is soft reset in between (measured on the neo_top
                                // simulation); the model runs on, the program is wrong for the hardware
                                if (starts_taken_++ > 0 && !soft_since_start_)
                                {
                                    st_.start_without_soft_reset++;
                                    if (!start_without_soft_reported_)
                                    {
                                        start_without_soft_reported_ = true;
                                        std::fprintf(stderr, "[cpu-direct] SAURIA START without a soft reset (CONTROL bit 23) since the previous run: the hardware does not run it\n");
                                    }
                                }
                                soft_since_start_ = false;
                            }
                        }
                        else if (sw && !sa_.busy) sa_.switch_req = true;
                    }
                    else if (o == 0x4u) sa_.gien = (v & 1u) != 0;
                    else if (o == 0x8u) sa_.ien = (v & 1u) != 0;
                    else if (o == 0xCu) { if (v & 1u) sa_.done_intr = false; }
                    else st_.bad_access++;
                    return;
                }
                const int k = core_word_index(off);
                if (k < 0) { st_.bad_access++; return; }
                if (sa_.busy) st_.sauria_write_busy++;   // the core runs on the copy taken at the start edge (config_regs.sv q2)
                sa_.words[static_cast<size_t>(k)] = v;
            }
            uint32_t sauria_read(uint32_t off)
            {
                if ((off & 0xF00u) == 0u)
                {
                    const uint32_t o = off & 0xFFu;
                    // [1] done [2] idle [3] ready [7] auto restart [17..19] keep [31:24] 0xAC; start, mem switch and soft reset clear themselves
                    // one clock after the write, so a read never sees them
                    if (o == 0x0u) return (sa_.done ? 2u : 0u) | (sa_.busy ? 0u : 0x4u) | ((!sa_.busy && stream_idle_) ? 0x8u : 0u) | (sa_.auto_restart ? 0x80u : 0u) |
                                          (sa_.keep[0] ? 1u << 17 : 0u) | (sa_.keep[1] ? 1u << 18 : 0u) | (sa_.keep[2] ? 1u << 19 : 0u) | 0xAC000000u;
                    if (o == 0x4u) return sa_.gien ? 1u : 0u;
                    if (o == 0x8u) return sa_.ien ? 1u : 0u;
                    if (o == 0xCu) return sa_.done_intr ? 1u : 0u;
                    if (o >= 0x10u && o <= 0x1Cu)
                    {
                        st_.status_reads++;
                        if (o == 0x10u) return st_.last_status = sa_.status;
                        if (o == 0x14u) return st_.last_cycles = sa_.cycles;
                        if (o == 0x18u) return st_.last_stalls = sa_.stalls;
                        return st_.last_version = SAURIA_VERSION;
                    }
                    st_.bad_access++;
                    return 0u;
                }
                const int k = core_word_index(off);
                if (k < 0) { st_.bad_access++; return 0u; }
                return sa_.words[static_cast<size_t>(k)];
            }
            void note_overlap(bool dfc_busy)
            {
                if (!dfc_busy) return;
                st_.dfc_overlap++;
                if (!overlap_reported_)
                {
                    overlap_reported_ = true;
                    std::fprintf(stderr, "[cpu-direct] the CPU wrote a block while the data flow controller runs an instruction (the AXI-Lite arbiter of the "
                                         "hardware interleaves both masters; software must not do this)\n");
                }
            }
            static uint64_t env_u(const char *n, uint64_t d) { const char *e = std::getenv(n); return e ? std::strtoull(e, nullptr, 0) : d; }

            // A command starts when its writer starts: the reader of the same or of an earlier write, or write-zero, gives the data.
            void begin_cmd()
            {
                cmd_zero_ = cr_.wz != 0;
                cmd_read_ = cr_.rd_pend;
                cmd_r_ = cr_.raddr; cmd_w_ = cr_.waddr; cmd_n_ = cr_.btt;
                cr_.rd_pend = cr_.wr_pend = false;
                const uint64_t first = env_u("HAS_DMA_DFC1D_FIRST", 6), tail = env_u("HAS_DMA_DFC1D_TAIL", 3);   // every CPU command is its own operation
                remaining_ = std::max<uint64_t>(1, first + (static_cast<uint64_t>(cmd_n_) + 15u) / 16u + 1u + tail);
                run_ = true;
                st_.dma_cmds++; st_.dma_cycles += remaining_;
                if (cmd_zero_) st_.dma_zero_cmds++;
            }
            bool access(uint32_t a, uint8_t *p, uint32_t n, bool write)
            {
                uint32_t off = 0;
                const Mem m = classify(a, off);
                if (m == M_BAD) return false;
                if (m == M_DRAM)
                {
                    if (!dram_ || static_cast<uint64_t>(off) + n > dram_->size()) return false;
                    if (write) std::memcpy(dram_->data() + off, p, n); else std::memcpy(p, dram_->data() + off, n);
                    return true;
                }
                return local_ && local_(m, off, p, n, write);
            }
            void finish_cmd()
            {
                run_ = false;
                std::vector<uint8_t> buf(cmd_n_, 0);
                bool ok = true;
                if (!cmd_zero_)
                {
                    if (cmd_read_) ok = access(cmd_r_, buf.data(), cmd_n_, false);
                    else ok = false;   // writer without data: the RTL writer waits for ever on an empty FIFO
                }
                if (ok) ok = access(cmd_w_, buf.data(), cmd_n_, true);
                if (!ok)
                {
                    if (st_.dma_bad++ == 0)
                        std::fprintf(stderr, "[cpu-direct] one-dimensional DMA command not executed: read 0x%08x write 0x%08x %u bytes%s (outside the memories of the model)\n",
                                     cmd_r_, cmd_w_, cmd_n_, (!cmd_zero_ && !cmd_read_) ? ", writer started without a reader" : "");
                }
                else st_.dma_bytes += cmd_n_;
                if (cmd_read_) cr_.ist |= 1u;
                cr_.ist |= 2u;
                if (cr_.wr_pend) begin_cmd();   // the writer of a later write was already requested
            }
        };
    } // namespace cpu_direct
} // namespace has

#endif // HAS_CPU_DIRECT_H
