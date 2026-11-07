// has_host_neo_port.h -- HAS_HOST=neo: the host port of HasNpuTop also takes the configuration register file of neo_lite
// (NEO_REGISTER_FILE.md).
//
// A window of the host address space (model address 0x40000800..0x400009FF; VP offset 0x10800 of the CDC wrapper) is the image of the controller window of
// df_controller_top: +0x000 CONTROL (bit 0 START, bit 1 DONE), +0x008 IEN, +0x00C ISTATUS, +0x010 + 4 i the 82 words. The sequence of the CPU is that of the
// hardware (82 words, IEN, START) plus, because the system wrapper stages DRAM only at a push: a write of 0 to 0x400004E4 (marks the push as an extended
// one for the wrapper; ignored here) and a push of opcode 0x17 to PUSH_A (the doorbell). START only arms: the instruction is built at the doorbell, after the
// wrapper has loaded the DRAM image.
//
// The doorbell decodes the words (host_neo::decode_regs) into one LayerInstr and puts it in the queue of the v4.5 host interface (MmioCompat::inject), so the
// DFC, STATUS / RETIRED and the interrupt are those of the v4.5 path. An image the hardware cannot run as given (host_neo::plan reports an A-* or B-* code)
// is REFUSED, never run by another path: one line on stderr, the sticky error of MmioCompat (STATUS bit 2), no instruction.
//   HAS_HOST=neo          strict: only what neo_lite RTL revision 1 runs as given (today ADD as RESIDUAL, MAX_POOL)
//   HAS_HOST=neo_patched  also accepts the hardware-gap codes listed in HAS_HOST_NEO_PATCHES (defaults in the code), each reported once on stderr; never a model-extension code
// Not set (default): no window, no doorbell; HasNpuTop behaves exactly as before. Pure C++ (no SystemC).
#ifndef HAS_HOST_NEO_PORT_H
#define HAS_HOST_NEO_PORT_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <vector>
#include "has/has_host_neo.h"
#include "has/has_mmio_compat.h"

namespace has
{
    class NeoPort
    {
    public:
        static constexpr uint32_t BASE = 0x40000800u, SIZE = 0x200u, MARKER = 0x400004E4u, BELL = 0x17u;
        enum Mode { OFF = 0, STRICT = 1, PATCHED = 2 };

        NeoPort()
        {
            if (const char *e = std::getenv("HAS_HOST"))
            {
                const std::string s = e;
                mode_ = s == "neo" ? STRICT : s == "neo_patched" ? PATCHED : OFF;
            }
            patches_ = {"A-H1", "A-H4"};
            if (const char *e = std::getenv("HAS_HOST_NEO_PATCHES"))
            {
                patches_.clear();
                std::string t = e, c;
                for (char ch : t + ",") { if (ch == ',') { if (!c.empty()) patches_.insert(c); c.clear(); } else c += ch; }
            }
            if (const char *e = std::getenv("HAS_HOST_NEO_QUIET")) quiet_ = std::atoi(e) != 0;
        }
        bool on() const { return mode_ != OFF; }
        Mode mode() const { return mode_; }
        bool owns(uint32_t a) const { return mode_ != OFF && a >= BASE && a < BASE + SIZE; }
        uint64_t accepted() const { return accepted_; }
        uint64_t refused() const { return refused_; }
        const std::string &last_reason() const { return last_reason_; }

        void soft_reset()
        {
            img_ = neo::Image();
            ien_ = 0; armed_ = false; accepted_ = refused_ = cleared_control_ = cleared_istatus_ = 0;
        }

        // True when the write belongs to the neo path and must not reach MmioCompat.
        bool handle_write(MmioCompat &m, uint32_t a, uint32_t v)
        {
            if (mode_ == OFF) return false;
            if (owns(a)) { window_write(m, a - BASE, v); return true; }
            if (a == MARKER && v == 0) return true;                       // staging flag for the wrapper: no effect on the model (X_MASK_ADDR 0 = unset)
            if (a == mmio::PUSH_A && (v & 0xFFu) == BELL) { doorbell(m); return true; }
            return false;
        }

        uint32_t read(const MmioCompat &m, uint32_t a) const
        {
            const uint32_t off = a - BASE;
            const bool done = accepted_ > cleared_control_ && !m.busy(), ist = accepted_ > cleared_istatus_ && !m.busy();
            if (off == neo::CTRL) return (armed_ ? 1u : 0u) | (done ? 2u : 0u) | 0xC0000000u;
            if (off == neo::IEN) return ien_;
            if (off == neo::ISTATUS) return ist ? 1u : 0u;
            if (off >= neo::REG_BASE && off < neo::reg_byte(neo::N_REGS) && off % 4u == 0) return img_.reg[(off - neo::REG_BASE) / 4u];
            return 0x1BADADD2u;
        }

    private:
        Mode mode_{STRICT};   // default on; HAS_HOST set to anything else than neo / neo_patched (for example v45) turns the window off
        std::set<std::string> patches_, reported_;
        bool quiet_{false};
        neo::Image img_;
        uint32_t ien_{0};
        bool armed_{false};
        uint64_t accepted_{0}, refused_{0}, cleared_control_{0}, cleared_istatus_{0};
        std::string last_reason_;

        void window_write(const MmioCompat &m, uint32_t off, uint32_t v)
        {
            if (off == neo::CTRL)
            {
                armed_ = (v & 1u) != 0;
                if (v & 2u) cleared_control_ = accepted_;
            }
            else if (off == neo::IEN) ien_ = v & 1u;
            else if (off == neo::ISTATUS) { if (v & 1u) cleared_istatus_ = accepted_; }
            else if (off >= neo::REG_BASE && off < neo::reg_byte(neo::N_REGS) && off % 4u == 0) img_.reg[(off - neo::REG_BASE) / 4u] = v;
            (void)m;
        }

        void refuse(MmioCompat &m, const std::string &why)
        {
            last_reason_ = why;
            refused_++;
            armed_ = false;
            m.refuse(Err::UNSUPPORTED_OP);
            if (!quiet_) std::fprintf(stderr, "[neo] push refused: %s\n", why.c_str());
        }

        // The model's ADD from the words of RESIDUAL: out = clamp(((a sa + b sb + bias) >> sh) + z). The model's ADD has zero points, not a bias, so find
        // zpA, zpB in [-128, 127] with bias = half - zpA sa - zpB sb (round half up, half = 2^(sh-1)); the bias of the hardware test of tb_neo_top.sv is one such.
        static bool residual_to_add(const neo::Image &im, LayerInstr &li, std::string &why)
        {
            const uint32_t r0 = im.gvu(neo::G_RES0);
            const int32_t sa = static_cast<int32_t>(im.gvu(neo::G_RES1)), sb = static_cast<int32_t>(im.gvu(neo::G_RES2));
            const int64_t bias = static_cast<int64_t>((static_cast<uint64_t>(im.gvu(neo::G_RES4)) << 32) | im.gvu(neo::G_RES3));
            const int sh = static_cast<int>((r0 >> 24) & 0x3Fu), z = static_cast<int8_t>((r0 >> 16) & 0xFFu);
            if ((r0 >> 30) & 1u) { why = "RESIDUAL in subtract mode has no form in the model's ADD"; return false; }
            if (sa < 0 || sb < 0) { why = "RESIDUAL with a negative scale has no form in the model's ADD"; return false; }
            const __int128 half = sh > 0 ? (static_cast<__int128>(1) << (sh - 1)) : 0;
            for (int za = -128; za < 128; za++)
            {
                const __int128 rem = half - static_cast<__int128>(bias) - static_cast<__int128>(za) * sa;   // = zpB * sb
                int zb = 0;
                if (sb == 0) { if (rem != 0) continue; }
                else { if (rem % sb != 0) continue; const __int128 q = rem / sb; if (q < -128 || q > 127) continue; zb = static_cast<int>(q); }
                li.add = AddQ();
                li.add.zpA = za; li.add.zpB = zb; li.add.zpO = z;
                li.add.SA = static_cast<uint32_t>(sa); li.add.SB = static_cast<uint32_t>(sb); li.add.SO = 1;
                li.add.sA = li.add.sB = 0; li.add.sO = sh;
                return true;
            }
            why = "RESIDUAL bias is not a pair of 8-bit zero points of the model's ADD (needs the vector-unit path HAS_GVU_RESIDUAL_HW)";
            return false;
        }

        bool build(LayerInstr &li, std::string &why)
        {
            if (!armed_) { why = "START was not written to the neo window before the doorbell"; return false; }
            host_neo::Decoded d = host_neo::decode_regs(img_);
            if (!d.errors.empty()) { why = "inconsistent register image: " + d.errors.front(); return false; }
            li = d.li;
            const uint32_t op = img_.reg[neo::OPCODE];
            if (op == neo::RESIDUAL && !residual_to_add(img_, li, why)) return false;
            const host_neo::Plan p = host_neo::plan(li);
            if (!p.error.empty()) { why = p.error; return false; }
            std::string bad;
            for (const std::string &c : p.issues)
            {
                const bool ok = mode_ == PATCHED && c[0] == 'A' && patches_.count(c);
                if (ok) { if (reported_.insert(c).second && !quiet_) std::fprintf(stderr, "[neo] accepted under patch %s\n", c.c_str()); }
                else bad += (bad.empty() ? "" : ", ") + c;
            }
            if (!bad.empty()) { why = "the hardware cannot run this instruction as given: " + bad + " (COMPILER_GUIDE.md, support matrix)"; return false; }
            if (mode_ == PATCHED && op == neo::GEMM_FUSED)
            {
                // Conventions that stand in for a load of the sbank (the sbank is never loaded in neo_lite RTL revision 1): the requant block sits at the sbank start as
                // scale[cout] | shift[cout] | bias[cout], 32-bit words per output channel (the arrays the model reads), the activation table at the vbank1 start.
                if (li.scale_addr == 0) { why = "GEMM_FUSED under patch A-H4 needs the requant block at the sbank start (word 34); none given"; return false; }
                li.per_channel = true;
                li.shift_addr = li.scale_addr + 4u * static_cast<uint32_t>(li.geom.cout);
                li.bias_addr = li.scale_addr + 8u * static_cast<uint32_t>(li.geom.cout);
                if (li.act) li.lut_addr = img_.reg[neo::START_V1];
            }
            return true;
        }

        void doorbell(MmioCompat &m)
        {
            LayerInstr li;
            std::string why;
            if (!build(li, why)) { refuse(m, why); return; }
            armed_ = false;
            if (!m.inject(li)) { refused_++; last_reason_ = "instruction queue full"; return; }
            accepted_++;
        }
    };
}

#endif // HAS_HOST_NEO_PORT_H
