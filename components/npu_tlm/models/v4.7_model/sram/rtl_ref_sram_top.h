// Near-verbatim port of sauria_model's sram/sram_top.h in namespace sauria_rtl, used by the RtlRefLaneACoreA /
// rtl_ref_npu_top path. v4.5's own sram/sram_top.h is unchanged (NativeLaneACoreA and other testbenches use it).
//
// Why a separate port instead of v4.5's sauria::Sram: that one is two-lane (_a/_b ports per SRAM), while this core is
// a single Lane A, as in sauria_model. More importantly, the rtl_ref feeders (rtl_ref_ifmap_feeder_rtl.h,
// rtl_ref_wei_feeder_rtl.h) need the FX1_A3_SRAMA/SRAMB/SRAMC_RDEN_PHASE pair; without it the SRAM data would be
// gated twice (once by i_srama_rden / i_sramb_rden in the SRAM, once by the feeder's delayed rden), dropping one
// element per FIFO fill and repeating the next one (visible with the RTL FIFO depths act 5 / wei 4).
//
// FX1_A3_SRAM_CAP_BYTES / FX1_A3_SRAM_CAP_C_ROWS (from the reference): SRAMA_CAP / SRAMB_CAP are BYTES, the unit the
// feeders use (`ACT_SRAM_DEPTH = SRAMA_CAP/(sizeof(T_ACT)*Y_DIM)`); SRAM-C stays in rows (SRAMC_CAP = 1536).
//
// Logic unchanged from the reference: only the header guard (SAURIA_RTL_SRAM_TOP_H) and the namespace
// (sauria_rtl, with `using namespace sauria;` for the shared vector / mask types act_vector_t, wei_vector_t,
// psum_vector_t, sramc_mask_t, host_data_t, host_mask_t). read_bank_data() / write_bank_data() / the host-side DMA
// access below are testbench helpers (see their comments).

#ifndef SAURIA_RTL_SRAM_TOP_H
#define SAURIA_RTL_SRAM_TOP_H

#include "sauria_types.h"
#include "debug.h"
#ifndef FX1_NO_PERF
#include "instrumentation/rtl_ref_perf_counters.h"
#endif

#ifdef FX1_SRAM_CAP_CHECK
#include <cstdio>
#include <stdexcept>
#include <string>
#endif

namespace sauria_rtl
{
    using namespace sauria; // shared vector/mask types + host_data_t/host_mask_t/SRAMx_OFFSET/SAURIA_MEM_ADDR_MASK

#ifdef FX1_SRAM_CAP_CHECK
    // Capacity check, OFF by default -> no behaviour or cost change for any existing build.
    // The arrays below keep their size and every address keeps its `% ROWS` (changing ROWS_C would change the PSM index
    // field width); instead each REAL access (rden/wren asserted, host access, backdoor) is checked
    // against the HAS per-buffer capacity (A 79 KB, B 81 KB, C 96 KB; override with FX1_SRAM_REAL_{A,B,C}_BYTES) and
    // against the modelled array (wrap-around). Counters are process-wide (one core SRAM per testbench) and printed at
    // exit as "[SRAM_CAP_CHECK] ..."; with FX1_SRAM_CAP_FATAL the first violation throws.
#ifndef FX1_SRAM_REAL_A_BYTES
#define FX1_SRAM_REAL_A_BYTES (79 * 1024)
#endif
#ifndef FX1_SRAM_REAL_B_BYTES
#define FX1_SRAM_REAL_B_BYTES (81 * 1024)
#endif
#ifndef FX1_SRAM_REAL_C_BYTES
#define FX1_SRAM_REAL_C_BYTES (96 * 1024)
#endif
    struct SramCapCheck
    {
        // index 0 = A, 1 = B, 2 = C
        uint64_t accesses[3]{}, over_real[3]{}, wrapped[3]{}, backdoor_over_real[3]{}, backdoor_dropped[3]{};
        uint32_t max_row[3]{}, real_rows[3]{}, model_rows[3]{};
        void hit(int m, uint32_t raw_row, bool backdoor)
        {
            accesses[m]++;
            if (raw_row >= model_rows[m])
            {
                if (backdoor) backdoor_dropped[m]++;
                else wrapped[m]++;
                violation(m, raw_row, backdoor ? "backdoor access beyond modelled array (silently dropped)"
                                               : "address wraps around the modelled array");
            }
            const uint32_t row = model_rows[m] ? raw_row % model_rows[m] : raw_row;
            if (row > max_row[m]) max_row[m] = row;
            if (row >= real_rows[m])
            {
                if (backdoor) backdoor_over_real[m]++;
                else over_real[m]++;
                violation(m, row, "row beyond the HAS per-buffer capacity");
            }
        }
        uint64_t n_violations{0};
        void violation(int m, uint32_t row, const char *what)
        {
            // first 20 violations are printed immediately (they land in the log just before the job line that owns them)
            if (++n_violations <= 20)
            {
                std::printf("[SRAM_CAP_CHECK] VIOLATION #%llu SRAM %c: %s, row %u\n", (unsigned long long)n_violations, "ABC"[m], what, row);
                std::fflush(stdout);
            }
#ifdef FX1_SRAM_CAP_FATAL
            throw std::runtime_error(std::string("[SRAM_CAP_CHECK] SRAM ") + "ABC"[m] + ": " + what + ", row " + std::to_string(row));
#else
            (void)m; (void)row; (void)what;
#endif
        }
        ~SramCapCheck()
        {
            for (int m = 0; m < 3; m++)
                std::printf("[SRAM_CAP_CHECK] SRAM %c: accesses %llu, max row %u (HAS rows/buffer %u, modelled %u), over HAS %llu, "
                            "wrapped %llu, backdoor over HAS %llu, backdoor dropped %llu\n",
                            "ABC"[m], (unsigned long long)accesses[m], max_row[m], real_rows[m], model_rows[m],
                            (unsigned long long)over_real[m], (unsigned long long)wrapped[m],
                            (unsigned long long)backdoor_over_real[m], (unsigned long long)backdoor_dropped[m]);
            std::fflush(stdout);
        }
    };
    inline SramCapCheck g_sram_capchk;
#define FX1_CAPCHK(m, raw_row, backdoor) ::sauria_rtl::g_sram_capchk.hit((m), static_cast<uint32_t>(raw_row), (backdoor))
#else
#define FX1_CAPCHK(m, raw_row, backdoor) ((void)0)
#endif

    template <
        int X_DIM = 32,
        int Y_DIM = 32,
        typename T_ACT = float,
        typename T_WEI = float,
        typename T_PSUM = float,
        int SRAMA_CAP = 1024,
        int SRAMB_CAP = 1024,
        int SRAMC_CAP = 2048>
    class Sram : public sc_module
    {
    public:
        // Clock & Reset
        sc_in<bool> i_clk{"i_clk"};
        sc_in<bool> i_rstn{"i_rstn"};

        // Power Gating / Deep Sleep (Global)
        sc_in<bool> i_deepsleep{"i_deepsleep"};
        sc_in<bool> i_powergate{"i_powergate"};

        // Buffer Selection (Double Buffering)
        // bit 0: SRAM A, bit 1: SRAM B, bit 2: SRAM C
        sc_in<sc_bv<3>> i_select{"i_select"};

        // Host-side Interface
        sc_in<uint32_t> i_host_addr{"i_host_addr"};
        sc_in<bool> i_host_wren{"i_host_wren"};
        sc_in<bool> i_host_rden{"i_host_rden"};
        sc_in<host_data_t> i_host_wdata{"i_host_wdata"}; // 128-bit host bus (4 floats)
        sc_in<host_mask_t> i_host_wmask{"i_host_wmask"}; // Write byte/word mask
        sc_out<host_data_t> o_host_rdata{"o_host_rdata"};

        // Accelerator-side Interface: SRAM A (Activations) -- single Lane A, no _b variant
        sc_in<uint32_t> i_srama_addr{"i_srama_addr"};
        sc_in<bool> i_srama_rden{"i_srama_rden"};
        sc_out<act_vector_t<Y_DIM, T_ACT>> o_srama_data{"o_srama_data"};

        // Accelerator-side Interface: SRAM B (Weights)
        sc_in<uint32_t> i_sramb_addr{"i_sramb_addr"};
        sc_in<bool> i_sramb_rden{"i_sramb_rden"};
        sc_out<wei_vector_t<X_DIM, T_WEI>> o_sramb_data{"o_sramb_data"};

        // Accelerator-side Interface: SRAM C (Partial Sums / Outputs)
        sc_in<psum_vector_t<Y_DIM, T_PSUM>> i_sramc_wdata{"i_sramc_wdata"};
        sc_in<uint32_t> i_sramc_addr{"i_sramc_addr"};
        sc_in<bool> i_sramc_wren{"i_sramc_wren"};
        sc_in<bool> i_sramc_rden{"i_sramc_rden"};
        sc_in<sramc_mask_t<Y_DIM>> i_sramc_wmask{"i_sramc_wmask"}; // Element-wise write mask (Y bits)
        sc_out<psum_vector_t<Y_DIM, T_PSUM>> o_sramc_rdata{"o_sramc_rdata"};

#ifndef FX1_NO_PERF
        sauria_rtl::PerfCounters *perf{nullptr};
#endif

        SC_CTOR(Sram)
        {
#ifdef FX1_SRAM_CAP_CHECK
            g_sram_capchk.model_rows[0] = ROWS_A; g_sram_capchk.model_rows[1] = ROWS_B; g_sram_capchk.model_rows[2] = ROWS_C;
            g_sram_capchk.real_rows[0] = FX1_SRAM_REAL_A_BYTES / (Y_DIM * sizeof(T_ACT));
            g_sram_capchk.real_rows[1] = FX1_SRAM_REAL_B_BYTES / (X_DIM * sizeof(T_WEI));
            g_sram_capchk.real_rows[2] = FX1_SRAM_REAL_C_BYTES / (Y_DIM * sizeof(T_PSUM));
#endif
            SC_METHOD(beh_process);
            sensitive << i_clk.pos();
        }

        const psum_vector_t<Y_DIM, T_PSUM> &peek_sramc_npu(uint32_t addr) const
        {
            sc_bv<3> sel = i_select.read();
            int npu_c_idx = sel[2].to_bool() ? 0 : 1;
            return mem_c[npu_c_idx][addr % ROWS_C];
        }

        // Testbench helpers (not in sauria_model's Sram): write_bank_data / read_bank_data, API-compatible with v4.5's
        // sram/sram_top.h, used by tools/fe/sysc/tb_fe_core_tile.cpp and tools/test_sauria_ref_layer_runner.cpp to load
        // stimulus directly from C++, bypassing the host port. No change to beh_process().
        //
        // This Sram picks the NPU-side buffer from the LIVE i_select (`npu_a_idx = sel_a ? 0 : 1`), not from a fixed
        // bank id, so write_bank_data writes BOTH copies (buffer 0 and 1) identically: correct for whatever i_select is
        // when the core reads. The bank category (A/B/C) still comes from bank_id.
        void write_bank_data(int bank_id, uint32_t offset_bytes, const uint8_t *src_data, uint32_t size_bytes)
        {
            uint32_t written = 0;
            while (written < size_bytes)
            {
                uint32_t curr_offset = offset_bytes + written;
                uint8_t byte_val = src_data[written];
                if (bank_id == 0 || bank_id == 1)
                {
                    uint32_t row_size = X_DIM * sizeof(T_WEI);
                    uint32_t row_idx = curr_offset / row_size;
                    uint32_t byte_in_row = curr_offset % row_size;
                    if (byte_in_row == 0 || written == 0) FX1_CAPCHK(1, row_idx, true);
                    if (row_idx < (uint32_t)ROWS_B)
                    {
                        uint32_t elem_idx = byte_in_row / sizeof(T_WEI);
                        uint32_t byte_in_elem = byte_in_row % sizeof(T_WEI);
                        for (int buf = 0; buf < 2; buf++)
                        {
                            uint8_t *ptr = reinterpret_cast<uint8_t *>(&mem_b[buf][row_idx][elem_idx]);
                            ptr[byte_in_elem] = byte_val;
                        }
                    }
                }
                else if (bank_id == 2 || bank_id == 3)
                {
                    uint32_t row_size = Y_DIM * sizeof(T_ACT);
                    uint32_t row_idx = curr_offset / row_size;
                    uint32_t byte_in_row = curr_offset % row_size;
                    if (byte_in_row == 0 || written == 0) FX1_CAPCHK(0, row_idx, true);
                    if (row_idx < (uint32_t)ROWS_A)
                    {
                        uint32_t elem_idx = byte_in_row / sizeof(T_ACT);
                        uint32_t byte_in_elem = byte_in_row % sizeof(T_ACT);
                        for (int buf = 0; buf < 2; buf++)
                        {
                            uint8_t *ptr = reinterpret_cast<uint8_t *>(&mem_a[buf][row_idx][elem_idx]);
                            ptr[byte_in_elem] = byte_val;
                        }
                    }
                }
                else if (bank_id == 4 || bank_id == 5)
                {
                    uint32_t row_size = Y_DIM * sizeof(T_PSUM);
                    uint32_t row_idx = curr_offset / row_size;
                    uint32_t byte_in_row = curr_offset % row_size;
                    if (byte_in_row == 0 || written == 0) FX1_CAPCHK(2, row_idx, true);
                    if (row_idx < (uint32_t)ROWS_C)
                    {
                        uint32_t elem_idx = byte_in_row / sizeof(T_PSUM);
                        uint32_t byte_in_elem = byte_in_row % sizeof(T_PSUM);
                        for (int buf = 0; buf < 2; buf++)
                        {
                            uint8_t *ptr = reinterpret_cast<uint8_t *>(&mem_c[buf][row_idx][elem_idx]);
                            ptr[byte_in_elem] = byte_val;
                        }
                    }
                }
                written++;
            }
        }

        // Reads the NPU-side buffer, derived from i_select.read() exactly as beh_process(): the NPU writes its results into
        // `mem_x[npu_idx]` with `npu_idx = sel ? 0 : 1` from the LIVE i_select. (With sram_select = 0, sel_c = 0 ->
        // npu_c_idx = 1; buffer [0] would still hold the preload copy written by write_bank_data.)
        void read_bank_data(int bank_id, uint32_t offset_bytes, uint8_t *dest_data, uint32_t size_bytes)
        {
            sc_bv<3> sel = i_select.read();
            int npu_a_idx = sel[0].to_bool() ? 0 : 1;
            int npu_b_idx = sel[1].to_bool() ? 0 : 1;
            int npu_c_idx = sel[2].to_bool() ? 0 : 1;
            uint32_t nread = 0;
            while (nread < size_bytes)
            {
                uint32_t curr_offset = offset_bytes + nread;
                uint8_t byte_val = 0;
                if (bank_id == 0 || bank_id == 1)
                {
                    uint32_t row_size = X_DIM * sizeof(T_WEI);
                    uint32_t row_idx = curr_offset / row_size;
                    uint32_t byte_in_row = curr_offset % row_size;
                    if (byte_in_row == 0 || nread == 0) FX1_CAPCHK(1, row_idx, true);
                    if (row_idx < (uint32_t)ROWS_B)
                    {
                        uint32_t elem_idx = byte_in_row / sizeof(T_WEI);
                        uint32_t byte_in_elem = byte_in_row % sizeof(T_WEI);
                        const uint8_t *ptr = reinterpret_cast<const uint8_t *>(&mem_b[npu_b_idx][row_idx][elem_idx]);
                        byte_val = ptr[byte_in_elem];
                    }
                }
                else if (bank_id == 2 || bank_id == 3)
                {
                    uint32_t row_size = Y_DIM * sizeof(T_ACT);
                    uint32_t row_idx = curr_offset / row_size;
                    uint32_t byte_in_row = curr_offset % row_size;
                    if (byte_in_row == 0 || nread == 0) FX1_CAPCHK(0, row_idx, true);
                    if (row_idx < (uint32_t)ROWS_A)
                    {
                        uint32_t elem_idx = byte_in_row / sizeof(T_ACT);
                        uint32_t byte_in_elem = byte_in_row % sizeof(T_ACT);
                        const uint8_t *ptr = reinterpret_cast<const uint8_t *>(&mem_a[npu_a_idx][row_idx][elem_idx]);
                        byte_val = ptr[byte_in_elem];
                    }
                }
                else if (bank_id == 4 || bank_id == 5)
                {
                    uint32_t row_size = Y_DIM * sizeof(T_PSUM);
                    uint32_t row_idx = curr_offset / row_size;
                    uint32_t byte_in_row = curr_offset % row_size;
                    if (byte_in_row == 0 || nread == 0) FX1_CAPCHK(2, row_idx, true);
                    if (row_idx < (uint32_t)ROWS_C)
                    {
                        uint32_t elem_idx = byte_in_row / sizeof(T_PSUM);
                        uint32_t byte_in_elem = byte_in_row % sizeof(T_PSUM);
                        const uint8_t *ptr = reinterpret_cast<const uint8_t *>(&mem_c[npu_c_idx][row_idx][elem_idx]);
                        byte_val = ptr[byte_in_elem];
                    }
                }
                dest_data[nread] = byte_val;
                nread++;
            }
        }

        // DMA path of HasNpuTop onto ONE half of the ping-pong
        // pair -- the HOST half (host_x_idx = sel_x ? 1 : 0, exactly as beh_process()), i.e. the buffer the core is not
        // using. Lets the DMA preload tile i+1 / drain tile i-1 while the core computes tile i (HAS §6.12, §9.6).
        // write_bank_data()/read_bank_data() above are unchanged (both halves / NPU half) so every existing testbench
        // behaves as before. Same bank ids (0/1 = B, 2/3 = A, 4/5 = C), same capacity check.
        void write_host_side(int bank_id, uint32_t offset_bytes, const uint8_t *src, uint32_t size_bytes)
        {
            host_side_access(bank_id, offset_bytes, const_cast<uint8_t *>(src), size_bytes, true);
        }
        void read_host_side(int bank_id, uint32_t offset_bytes, uint8_t *dst, uint32_t size_bytes)
        {
            host_side_access(bank_id, offset_bytes, dst, size_bytes, false);
        }

    private:
        void host_side_access(int bank_id, uint32_t offset_bytes, uint8_t *buf, uint32_t size_bytes, bool write)
        {
            const sc_bv<3> sel = i_select.read();
            const int cat = (bank_id == 0 || bank_id == 1) ? 1 : ((bank_id == 2 || bank_id == 3) ? 0 : 2);   // 0 A, 1 B, 2 C
            const int idx = sel[cat == 0 ? 0 : (cat == 1 ? 1 : 2)].to_bool() ? 1 : 0;                          // host half
            const uint32_t row_size = cat == 1 ? X_DIM * sizeof(T_WEI) : (cat == 0 ? Y_DIM * sizeof(T_ACT) : Y_DIM * sizeof(T_PSUM));
            const uint32_t rows = cat == 1 ? (uint32_t)ROWS_B : (cat == 0 ? (uint32_t)ROWS_A : (uint32_t)ROWS_C);
            for (uint32_t i = 0; i < size_bytes; i++)
            {
                const uint32_t off = offset_bytes + i, row = off / row_size, in_row = off % row_size;
                if (in_row == 0 || i == 0) FX1_CAPCHK(cat, row, true);
                uint8_t *p = nullptr;
                if (row < rows)
                {
                    if (cat == 0) p = reinterpret_cast<uint8_t *>(&mem_a[idx][row][0]) + in_row;
                    else if (cat == 1) p = reinterpret_cast<uint8_t *>(&mem_b[idx][row][0]) + in_row;
                    else p = reinterpret_cast<uint8_t *>(&mem_c[idx][row][0]) + in_row;
                }
                if (write) { if (p) *p = buf[i]; }
                else buf[i] = p ? *p : 0;
            }
        }
        static const int SUBWORDS_A = Y_DIM / 4;
        static const int SUBWORDS_B = X_DIM / 4;
        static const int SUBWORDS_C = Y_DIM / 4;

#ifdef FX1_A3_SRAM_CAP_BYTES
        static constexpr int ROWS_A =
            (SRAMA_CAP / (int)(sizeof(T_ACT) * Y_DIM)) > 0
                ? (SRAMA_CAP / (int)(sizeof(T_ACT) * Y_DIM)) : 1;
        static constexpr int ROWS_B =
            (SRAMB_CAP / (int)(sizeof(T_WEI) * X_DIM)) > 0
                ? (SRAMB_CAP / (int)(sizeof(T_WEI) * X_DIM)) : 1;
#ifdef FX1_A3_SRAM_CAP_C_ROWS
        static constexpr int ROWS_C = SRAMC_CAP;
#else
        static constexpr int ROWS_C =
            (SRAMC_CAP / (int)(sizeof(T_PSUM) * Y_DIM)) > 0
                ? (SRAMC_CAP / (int)(sizeof(T_PSUM) * Y_DIM)) : 1;
#endif
#else
        static constexpr int ROWS_A = SRAMA_CAP;
        static constexpr int ROWS_B = SRAMB_CAP;
        static constexpr int ROWS_C = SRAMC_CAP;
#endif

        act_vector_t<Y_DIM, T_ACT> mem_a[2][ROWS_A];
        wei_vector_t<X_DIM, T_WEI> mem_b[2][ROWS_B];
        psum_vector_t<Y_DIM, T_PSUM> mem_c[2][ROWS_C];

        void beh_process()
        {
            if (!i_rstn.read())
            {
                o_host_rdata.write(host_data_t());
                o_srama_data.write(act_vector_t<Y_DIM, T_ACT>());
                o_sramb_data.write(wei_vector_t<X_DIM, T_WEI>());
                o_sramc_rdata.write(psum_vector_t<Y_DIM, T_PSUM>());
                return;
            }

            if (i_powergate.read())
            {
                for (int i = 0; i < 2; i++)
                {
                    for (int d = 0; d < ROWS_A; d++)
                        mem_a[i][d] = act_vector_t<Y_DIM, T_ACT>();
                    for (int d = 0; d < ROWS_B; d++)
                        mem_b[i][d] = wei_vector_t<X_DIM, T_WEI>();
                    for (int d = 0; d < ROWS_C; d++)
                        mem_c[i][d] = psum_vector_t<Y_DIM, T_PSUM>();
                }
                o_host_rdata.write(host_data_t());
                o_srama_data.write(act_vector_t<Y_DIM, T_ACT>());
                o_sramb_data.write(wei_vector_t<X_DIM, T_WEI>());
                o_sramc_rdata.write(psum_vector_t<Y_DIM, T_PSUM>());
                return;
            }

            if (i_deepsleep.read())
            {
                o_host_rdata.write(host_data_t());
                o_srama_data.write(act_vector_t<Y_DIM, T_ACT>());
                o_sramb_data.write(wei_vector_t<X_DIM, T_WEI>());
                o_sramc_rdata.write(psum_vector_t<Y_DIM, T_PSUM>());
                return;
            }

            sc_bv<3> sel = i_select.read();
            bool sel_a = sel[0].to_bool();
            bool sel_b = sel[1].to_bool();
            bool sel_c = sel[2].to_bool();

            int npu_a_idx = sel_a ? 0 : 1;
            int host_a_idx = sel_a ? 1 : 0;

            int npu_b_idx = sel_b ? 0 : 1;
            int host_b_idx = sel_b ? 1 : 0;

            int npu_c_idx = sel_c ? 0 : 1;
            int host_c_idx = sel_c ? 1 : 0;

            // ---- 1. Host-Side Accesses ----
            uint32_t addr = i_host_addr.read();
            uint32_t mem_region = addr & SAURIA_MEM_ADDR_MASK;
            uint32_t local_addr = addr & ~SAURIA_MEM_ADDR_MASK;

            if (i_host_wren.read())
            {
                host_data_t wdata = i_host_wdata.read();
                host_mask_t wmask = i_host_wmask.read();

                if (mem_region == SRAMA_OFFSET)
                {
                    uint32_t sub_word = local_addr % SUBWORDS_A;
                    FX1_CAPCHK(0, local_addr / SUBWORDS_A, false);
                    uint32_t phys_addr = (local_addr / SUBWORDS_A) % ROWS_A;
                    for (int i = 0; i < 4; i++)
                        if (wmask[i] && (sub_word * 4 + i < Y_DIM))
                            mem_a[host_a_idx][phys_addr][sub_word * 4 + i] = wdata[i];
                }
                else if (mem_region == SRAMB_OFFSET)
                {
                    uint32_t sub_word = local_addr % SUBWORDS_B;
                    FX1_CAPCHK(1, local_addr / SUBWORDS_B, false);
                    uint32_t phys_addr = (local_addr / SUBWORDS_B) % ROWS_B;
                    for (int i = 0; i < 4; i++)
                        if (wmask[i] && (sub_word * 4 + i < X_DIM))
                            mem_b[host_b_idx][phys_addr][sub_word * 4 + i] = wdata[i];
                }
                else if (mem_region == SRAMC_OFFSET)
                {
                    uint32_t sub_word = local_addr % SUBWORDS_C;
                    FX1_CAPCHK(2, local_addr / SUBWORDS_C, false);
                    uint32_t phys_addr = (local_addr / SUBWORDS_C) % ROWS_C;
                    for (int i = 0; i < 4; i++)
                        if (wmask[i] && (sub_word * 4 + i < Y_DIM))
                            mem_c[host_c_idx][phys_addr][sub_word * 4 + i] = wdata[i];
                }
            }

            if (i_host_rden.read())
            {
                host_data_t rdata;
                if (mem_region == SRAMA_OFFSET)
                {
                    uint32_t sub_word = local_addr % SUBWORDS_A;
                    FX1_CAPCHK(0, local_addr / SUBWORDS_A, false);
                    uint32_t phys_addr = (local_addr / SUBWORDS_A) % ROWS_A;
                    for (int i = 0; i < 4; i++)
                        if (sub_word * 4 + i < Y_DIM)
                            rdata[i] = mem_a[host_a_idx][phys_addr][sub_word * 4 + i];
                }
                else if (mem_region == SRAMB_OFFSET)
                {
                    uint32_t sub_word = local_addr % SUBWORDS_B;
                    FX1_CAPCHK(1, local_addr / SUBWORDS_B, false);
                    uint32_t phys_addr = (local_addr / SUBWORDS_B) % ROWS_B;
                    for (int i = 0; i < 4; i++)
                        if (sub_word * 4 + i < X_DIM)
                            rdata[i] = mem_b[host_b_idx][phys_addr][sub_word * 4 + i];
                }
                else if (mem_region == SRAMC_OFFSET)
                {
                    uint32_t sub_word = local_addr % SUBWORDS_C;
                    FX1_CAPCHK(2, local_addr / SUBWORDS_C, false);
                    uint32_t phys_addr = (local_addr / SUBWORDS_C) % ROWS_C;
                    for (int i = 0; i < 4; i++)
                        if (sub_word * 4 + i < Y_DIM)
                            rdata[i] = mem_c[host_c_idx][phys_addr][sub_word * 4 + i];
                }
                o_host_rdata.write(rdata);
            }

            // ---- 2. Accelerator-Side Accesses ----

            // SRAM A Reads
#ifdef FX1_A3_SRAMA_RDEN_PHASE
            {
                uint32_t addr_a_raw = i_srama_addr.read() % ROWS_A;
                o_srama_data.write(mem_a[npu_a_idx][addr_a_raw]);
            }
#endif
            if (i_srama_rden.read())
            {
                uint32_t addr_a = i_srama_addr.read() % ROWS_A;
                FX1_CAPCHK(0, i_srama_addr.read(), false);
                auto data_a = mem_a[npu_a_idx][addr_a];
                o_srama_data.write(data_a);
#ifndef FX1_NO_PERF
                if (perf)
                {
                    perf->srama_read_beats++;
                    perf->srama_read_bytes += (uint64_t)Y_DIM * sizeof(T_ACT);
                }
#endif
            }

            // SRAM B Reads
#ifdef FX1_A3_SRAMB_RDEN_PHASE
            {
                uint32_t addr_b_raw = i_sramb_addr.read() % ROWS_B;
                o_sramb_data.write(mem_b[npu_b_idx][addr_b_raw]);
            }
#endif
            if (i_sramb_rden.read())
            {
                uint32_t addr_b = i_sramb_addr.read() % ROWS_B;
                FX1_CAPCHK(1, i_sramb_addr.read(), false);
                auto data_b = mem_b[npu_b_idx][addr_b];
                o_sramb_data.write(data_b);
#ifndef FX1_NO_PERF
                if (perf)
                {
                    perf->sramb_read_beats++;
                    perf->sramb_read_bytes += (uint64_t)X_DIM * sizeof(T_WEI);
                }
#endif
            }

            // SRAM C Reads/Writes
            if (i_sramc_wren.read())
            {
                uint32_t addr_c = i_sramc_addr.read() % ROWS_C;
                FX1_CAPCHK(2, i_sramc_addr.read(), false);
                psum_vector_t<Y_DIM, T_PSUM> wdata_c = i_sramc_wdata.read();
                sramc_mask_t<Y_DIM> wmask_c = i_sramc_wmask.read();

                uint32_t n_written_c = 0;
                for (int i = 0; i < Y_DIM; i++)
                    if (wmask_c[i])
                    {
                        mem_c[npu_c_idx][addr_c][i] = wdata_c[i];
                        n_written_c++;
                    }
#ifndef FX1_NO_PERF
                if (perf)
                {
                    perf->sramc_write_beats++;
                    perf->sramc_write_bytes += (uint64_t)n_written_c * sizeof(T_PSUM);
                }
#endif
            }

#ifdef FX1_A3_SRAMC_RDEN_PHASE
            {
                uint32_t addr_c_raw = i_sramc_addr.read() % ROWS_C;
                o_sramc_rdata.write(mem_c[npu_c_idx][addr_c_raw]);
            }
#endif
            if (i_sramc_rden.read())
            {
                uint32_t addr_c = i_sramc_addr.read() % ROWS_C;
                FX1_CAPCHK(2, i_sramc_addr.read(), false);
                o_sramc_rdata.write(mem_c[npu_c_idx][addr_c]);
#ifndef FX1_NO_PERF
                if (perf)
                {
                    perf->sramc_read_beats++;
                    perf->sramc_read_bytes += (uint64_t)Y_DIM * sizeof(T_PSUM);
                }
#endif
            }
        }
    };

} // namespace sauria_rtl

#endif // SAURIA_RTL_SRAM_TOP_H
