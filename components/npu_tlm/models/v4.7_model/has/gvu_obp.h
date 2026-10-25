// gvu_obp.h -- GEMM_FUSED epilogue per the HW drawing: SA -> PSM -> [Requant -> LUT_act] -> write PSUM SRAM.
// Spec: has/HAS_IFACE.md §6.1. Port names/types match sauria::Obp (psm/obp_top.h) so a testbench can swap it in, but the
// behaviour follows the drawing, not Obp:
//   - no bias stage (bias arrives as PSUM preload), no residual stage (residual is ELEM_WISE ADD);
//     i_bias_en / i_residual_en must stay 0, i_requant_en must stay 1 -- anything else is reported as a config error;
//   - requant = has::requant() (knobs: rounding, int16 narrowing, scale format), zero point from ZP_OUT;
//   - one LUT int8[256] shared by all lanes (the host may still write one copy per lane; lane copies must agree);
//   - channel of a vector = i_addr % NCH (explicit register) instead of Obp's implicit vec_channel counter;
//   - per-channel Out_Scale / Out_Shift (HAS: held in Scratchpad; counted as Scratchpad reads) or, with PERCH = 0, the
//     per-tensor broadcast values on i_requant_scale / i_requant_shift ("Instr Decoder" input of the drawing mux).
// Timing: 1 vector per cycle, fixed latency LAT cycles (default 6 = drawing stages; an ESTIMATE until the MAS fixes it).
#ifndef HAS_OBP_H
#define HAS_OBP_H

#include <systemc.h>
#include <array>
#include <deque>
#include <functional>
#include <vector>
#include "sauria_types.h"
#include "has/gvu_quant.h"
#include "has/gvu_rtl_obp.h"
#include "has/gvu_top_bridge.h"

namespace has
{
    using sauria::act_vector_t;
    using sauria::host_data_t;
    using sauria::host_mask_t;
    using sauria::psum_vector_t;
    using sauria::sramc_mask_t;

    template <int W, uint32_t LUT_BASE = 0x00140000, uint32_t SCALE_BASE = 0x00180000, uint32_t SHIFT_BASE = 0x00190000,
              uint32_t CFG_BASE = 0x001A0000, int MAX_CH = 4096>
    class HasObp : public sc_module
    {
    public:
        // Register offsets inside CFG_BASE (HAS_IFACE §6.1)
        static constexpr uint32_t REG_ZP_OUT = 0x0, REG_NCH = 0x4, REG_PERCH = 0x8;

        sc_in<bool> i_clk{"i_clk"};
        sc_in<bool> i_rstn{"i_rstn"};
        sc_in<psum_vector_t<W, int32_t>> i_data{"i_data"};
        sc_in<uint32_t> i_addr{"i_addr"};
        sc_in<sramc_mask_t<W>> i_wmask{"i_wmask"};
        sc_in<bool> i_valid{"i_valid"};
        sc_in<act_vector_t<W, int8_t>> i_residual{"i_residual"}; // kept for pin compatibility, ignored
        sc_out<psum_vector_t<W, int32_t>> o_sramc_wdata{"o_sramc_wdata"};
        sc_out<uint32_t> o_sramc_addr{"o_sramc_addr"};
        sc_out<bool> o_sramc_wren{"o_sramc_wren"};
        sc_out<sramc_mask_t<W>> o_sramc_wmask{"o_sramc_wmask"};
        sc_out<bool> o_valid{"o_valid"};
        sc_in<bool> i_bias_en{"i_bias_en"};
        sc_in<bool> i_requant_en{"i_requant_en"};
        sc_in<bool> i_lut_en{"i_lut_en"};
        sc_in<bool> i_residual_en{"i_residual_en"};
        sc_in<bool> i_vec_channel_mode{"i_vec_channel_mode"}; // ignored (channel comes from i_addr % NCH)
        sc_in<uint32_t> i_requant_scale{"i_requant_scale"};    // broadcast Out_Scale (PERCH = 0)
        sc_in<uint32_t> i_requant_shift{"i_requant_shift"};    // broadcast Out_Shift (PERCH = 0)
        sc_in<uint32_t> i_host_addr{"i_host_addr"};
        sc_in<bool> i_host_wren{"i_host_wren"};
        sc_in<bool> i_host_rden{"i_host_rden"};
        sc_in<host_data_t> i_host_wdata{"i_host_wdata"};
        sc_in<host_mask_t> i_host_wmask{"i_host_wmask"};
        sc_out<host_data_t> o_host_rdata{"o_host_rdata"};

        // Statistics (validation evidence)
        QuantCounters qc;
        uint64_t vectors_in{0}, vectors_out{0}, scratch_param_reads{0}, cfg_errors{0}, lut_lane_mismatch{0};

        HasObp(sc_module_name nm, const Knobs &knobs, int latency = 6)
            : sc_module(nm), k(knobs), lat(latency < 1 ? 1 : latency), scale_bits(MAX_CH, 0), shift(MAX_CH, 0)
        {
            lut.fill(0);
            lut_written.fill(false);
            SC_HAS_PROCESS(HasObp);
            SC_METHOD(tick);
            sensitive << i_clk.pos();
            dont_initialize();
        }

        const Knobs &knobs() const { return k; }
        int latency() const { return lat; }
        bool idle() const { return pipe.empty(); }

        // Inline mode -- OBP on the PSM -> SRAM-C write path (vector-unit drawing, figure 5). The per-channel scale/shift sit in the
        // double-buffered Scratchpad sbank, loaded by DMA behind the previous tile (GVU §4), so the DFC sets them here
        // without host-bus cycles. apply_inline() = the same arithmetic as the streamed path (compute), one channel.
        uint64_t inline_vectors{0};
        // HAS_GVU_TOP=3 (has/gvu_top_stream.h): the beat is not replaced here; it goes into the clocked stream through this sink
        // (psum vector, lane mask, channel, Out_Scale, Out_Shift, "GvuTop cannot express these parameters", model result).
        std::function<void(const psum_vector_t<W, int32_t> &, uint32_t, uint32_t, int64_t, int, bool, const psum_vector_t<W, int32_t> &)> stream_sink;
        uint32_t inline_word_addr{0};   // word address of the SRAM-C write of the beat being applied (set by the write hook)
        void set_channel_params(uint32_t ch, uint32_t scale_word, int shift_amount)
        {
            if (ch >= MAX_CH) { cfg_errors++; return; }
            scale_bits[ch] = scale_word;
            shift[ch] = shift_amount;
        }
        void set_nch(uint32_t n) { nch = n >= 1 ? n : 1u; }
        psum_vector_t<W, int32_t> apply_inline(const psum_vector_t<W, int32_t> &in, uint32_t ch, const sramc_mask_t<W> &m)
        {
            inline_vectors++;
            psum_vector_t<W, int32_t> r = compute(in, ch, m);
            if (stream_sink) stream_beat(in, ch, m, r);
            else if (GvuTopBridge::on()) top_beat(in, ch, m, r);
            else if (RtlObp::on()) rtl_beat(in, ch, m, r);
            return r;
        }
        // Tail behind the last write of a core pass: the depth of the cycle-level pipeline with HAS_GVU_TOP (GvuTop, stream
        // mode) or HAS_OBP_RTL, else LAT.
        // HAS_GVU_TOP=3: no fixed tail; the DFC waits until the stream is empty instead.
        int inline_latency() const { return stream_sink ? 0 : GvuTopBridge::on() ? GvuTopBridge::kDepth : RtlObp::on() ? RtlObp::kLatency : lat; }
        // HAS_GVU_TOP=2: the layer's table set without host-bus cycles (the instruction path charges the cycles of the
        // Scratchpad path instead, GvuTopBridge::table_cycles()). The same table as 32 copies over the host port.
        void load_lut(const uint8_t *table)
        {
            for (int e = 0; e < 256; e++) { lut[e] = static_cast<int8_t>(table[e]); lut_written[e] = true; }
            rtl_lut_stale = top_lut_stale = true;
        }

    private:
        struct Slot
        {
            psum_vector_t<W, int32_t> data;
            uint32_t addr;
            sramc_mask_t<W> mask;
            int age;
        };

        Knobs k;
        int lat;
        std::array<int8_t, 256> lut;
        std::array<bool, 256> lut_written;
        std::vector<uint32_t> scale_bits;
        std::vector<int> shift;
        int zp_out{0};
        uint32_t nch{1};
        bool perch{true};
        std::deque<Slot> pipe;
        RtlObp::Unit rtl_ep;          // HAS_OBP_RTL: the cycle-level epilogue and whether its table follows `lut`
        bool rtl_lut_stale{true};
        bool top_lut_stale{true};     // HAS_GVU_TOP: the table changed since GvuTop loaded it

        // HAS_GVU_TOP=3: the beat goes into the clocked stream; r (the model result) stays what the hook may use for comparison.
        void stream_beat(const psum_vector_t<W, int32_t> &in, uint32_t x, const sramc_mask_t<W> &m, const psum_vector_t<W, int32_t> &r)
        {
            if (W != 32) return;
            const uint32_t ch = x % nch;
            const int64_t S = decode_scale(perch ? scale_bits[ch] : i_requant_scale.read(), k.scale_fmt);
            const int s = perch ? shift[ch] : static_cast<int>(i_requant_shift.read());
            uint32_t mask = 0;
            for (int i = 0; i < W; i++) if (m[i]) mask |= 1u << i;
            auto fit = [](int64_t v, int bits) { const int64_t h = static_cast<int64_t>(1) << (bits - 1); return v >= -h && v < h; };
            // the parameters GvuTop's stream cannot take (same conditions as GvuTopBridge::gemm_beat_narrow, zero point in the 8 bits
            // of the RTL register): the model result goes back instead
            const bool bypass = k.req_narrow == NARROW_NONE || !fit(S, 32) || s < 0 || s > 63 || !GvuTopBridge::zp_fits(zp_out) || k.round_mode < 0 || k.round_mode > 6;
            stream_sink(in, mask, ch, S, s, bypass, r);
        }

        // HAS_GVU_TOP: the beat of compute() on GvuTop (stream mode); r holds the model result and gets GvuTop's.
        void top_beat(const psum_vector_t<W, int32_t> &in, uint32_t addr, const sramc_mask_t<W> &m, psum_vector_t<W, int32_t> &r)
        {
            if (W != 32) return;
            const uint32_t ch = addr % nch;
            const int64_t S = decode_scale(perch ? scale_bits[ch] : i_requant_scale.read(), k.scale_fmt);
            const int s = perch ? shift[ch] : static_cast<int>(i_requant_shift.read());
            uint32_t mask = 0;
            for (int i = 0; i < W; i++) if (m[i]) mask |= 1u << i;
            if (GvuTopBridge::gemm_beat_narrow(in, r, mask, addr, S, s, zp_out, k.round_mode, k.req_narrow, i_lut_en.read(), lut.data(), top_lut_stale))
                top_lut_stale = false;
        }

        // HAS_OBP_RTL: the beat of compute() on the cycle-level pipeline; r holds the model result and gets the pipeline's.
        void rtl_beat(const psum_vector_t<W, int32_t> &in, uint32_t addr, const sramc_mask_t<W> &m, psum_vector_t<W, int32_t> &r)
        {
            if (W != 32) return;
            const uint32_t ch = addr % nch;
            const int64_t S = decode_scale(perch ? scale_bits[ch] : i_requant_scale.read(), k.scale_fmt);
            const int s = perch ? shift[ch] : static_cast<int>(i_requant_shift.read());
            if (rtl_lut_stale) { rtl_ep.load_act(lut.data()); rtl_lut_stale = false; RtlObp::table_loaded(lut.data()); }
            uint32_t mask = 0;
            for (int i = 0; i < W; i++) if (m[i]) mask |= 1u << i;
            RtlObp::beat(rtl_ep, in, r, mask, S, s, zp_out, k.round_mode, k.req_narrow == SAT16, i_lut_en.read());
        }

        // A signed register value arrives either as the signed number itself or as the 32-bit two's-complement word the
        // host bus carries (a zero point of -128 written as 0xFFFFFF80 = 4294967168): both decode to the same int.
        static int host_i32(double v)
        {
            if (v >= 2147483648.0 && v < 4294967296.0) v -= 4294967296.0;
            return static_cast<int>(v);
        }
        void host_write(uint32_t a, const host_data_t &d, const host_mask_t &m)
        {
            if (a >= LUT_BASE && a < LUT_BASE + static_cast<uint32_t>(W) * 256u)
            {
                const uint32_t off = a - LUT_BASE, e0 = off % 256u;
                // A write to LUT_BASE + 0 starts loading a new table (next layer): forget the previous one, otherwise the
                // lane-consistency check below reports every changed entry as a mismatch (false alarm found by Frontend, 353).
                if (off == 0)
                    lut_written.fill(false);
                for (uint32_t q = 0; q < 4 && e0 + q < 256; q++)
                {
                    if (!m[q]) continue;
                    const int8_t v = static_cast<int8_t>(static_cast<int>(d[q]));
                    if (lut_written[e0 + q] && lut[e0 + q] != v) lut_lane_mismatch++;
                    lut[e0 + q] = v;
                    lut_written[e0 + q] = true;
                    rtl_lut_stale = top_lut_stale = true;
                }
            }
            else if (a >= SCALE_BASE && a < SCALE_BASE + 4u * MAX_CH)
                scale_bits[(a - SCALE_BASE) / 4] = static_cast<uint32_t>(static_cast<uint64_t>(d[0]));
            else if (a >= SHIFT_BASE && a < SHIFT_BASE + 4u * MAX_CH)
                shift[(a - SHIFT_BASE) / 4] = host_i32(d[0]);
            else if (a == CFG_BASE + REG_ZP_OUT)
                zp_out = host_i32(d[0]);
            else if (a == CFG_BASE + REG_NCH)
                nch = d[0] >= 1 ? static_cast<uint32_t>(d[0]) : 1u;
            else if (a == CFG_BASE + REG_PERCH)
                perch = d[0] != 0.0;
        }

        psum_vector_t<W, int32_t> compute(const psum_vector_t<W, int32_t> &in, uint32_t addr, const sramc_mask_t<W> &m)
        {
            const uint32_t ch = addr % nch;
            int64_t S;
            int s;
            if (perch)
            {
                S = decode_scale(scale_bits[ch], k.scale_fmt);
                s = shift[ch];
                scratch_param_reads += 2;
            }
            else
            {
                S = decode_scale(i_requant_scale.read(), k.scale_fmt);
                s = static_cast<int>(i_requant_shift.read());
            }
            const bool use_lut = i_lut_en.read();
            psum_vector_t<W, int32_t> out(0);
            for (int i = 0; i < W; i++)
            {
                if (!m[i]) continue;
                int8_t q = requant(in[i], S, s, zp_out, k, &qc);
                if (use_lut) q = lut_direct(lut.data(), q);
                out[i] = q;
            }
            return out;
        }

        void tick()
        {
            if (!i_rstn.read())
            {
                pipe.clear();
                o_sramc_wren.write(false);
                o_valid.write(false);
                return;
            }
            if (i_host_wren.read())
                host_write(i_host_addr.read(), i_host_wdata.read(), i_host_wmask.read());

            for (auto &sl : pipe) sl.age++;
            if (i_valid.read())
            {
                if (i_bias_en.read() || i_residual_en.read() || !i_requant_en.read())
                    cfg_errors++; // drawing: no bias / no residual in GEMM_FUSED, requant always on
                pipe.push_back({compute(i_data.read(), i_addr.read(), i_wmask.read()), i_addr.read(), i_wmask.read(), 0});
                vectors_in++;
            }
            if (!pipe.empty() && pipe.front().age >= lat)
            {
                const Slot &sl = pipe.front();
                o_sramc_wdata.write(sl.data);
                o_sramc_addr.write(sl.addr);
                o_sramc_wmask.write(sl.mask);
                o_sramc_wren.write(true);
                o_valid.write(true);
                pipe.pop_front();
                vectors_out++;
            }
            else
            {
                o_sramc_wren.write(false);
                o_valid.write(false);
            }
        }
    };
} // namespace has

#endif // HAS_OBP_H
