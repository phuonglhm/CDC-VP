// has_dma.h -- 4-channel DMA of the HAS (section 6.11) for HasNpuTop.
//
// Same scheduling as control/sauria_dma.h (one read port shared by CH0..CH3, one independent write port, write
// processed before read in a cycle, a burst's data lands when its last beat completes) with what the v4.5 model
// hard-codes turned into parameters:
//   bytes_per_beat   32 = v4.5 model (256-bit bus)          16 = HAS AXI 128-bit
//   burst_beats      8 (HAS: "8-beat burst")
//   dram_latency     extra cycles before the FIRST burst of every transfer (read and write); ESTIMATE, 0 = v4.5
//   ch3_low_priority false = v4.5 round robin over CH0..CH3; true = HAS: CH3 (LUT / bias / scale) only when CH0..CH2 idle
// Compat parameters (32, 8, 0, false) reproduce SauriaDma cycle for cycle (tools/has/tb_has_dma checks it), so numbers
// measured with the v4.5 DMA stay comparable. The SRAM side goes through a port object, so the DMA can target the
// host half of the core's ping-pong SRAM instead of the separate v4.5 Sram:
//   struct Port { void write(int bank, uint32_t off, const uint8_t *src, uint32_t n);
//                 void read (int bank, uint32_t off, uint8_t *dst, uint32_t n); };
#ifndef HAS_DMA_H
#define HAS_DMA_H

#include <systemc.h>
#include <algorithm>
#include <cstdint>
#include <vector>

namespace has
{
    struct DmaParams
    {
        uint32_t bytes_per_beat{32};
        uint32_t burst_beats{8};
        uint32_t dram_latency{0};
        bool ch3_low_priority{false};
        static DmaParams v45() { return DmaParams(); }
        static DmaParams has_axi128(uint32_t latency = 0)
        {
            DmaParams p;
            p.bytes_per_beat = 16;
            p.dram_latency = latency;
            p.ch3_low_priority = true;
            return p;
        }
        uint32_t burst_bytes() const { return bytes_per_beat * burst_beats; }
    };

    struct DmaStats
    {
        uint64_t read_bytes{0}, write_bytes{0}, read_bursts{0}, write_bursts{0}, busy_cycles{0}, read_port_cycles{0}, write_port_cycles{0};
    };

    template <class Port>
    class HasDma : public sc_module
    {
    public:
        sc_in<bool> i_clk{"i_clk"};
        sc_in<bool> i_rstn{"i_rstn"};

        DmaParams prm;
        DmaStats st;

        SC_HAS_PROCESS(HasDma);
        HasDma(sc_module_name nm, const DmaParams &p = DmaParams()) : sc_module(nm), prm(p)
        {
            SC_METHOD(process);
            sensitive << i_clk.pos();
        }

        void set_port(Port *p) { port_ = p; }
        void set_dram(std::vector<uint8_t> *d) { dram_ = d; }

        // `timed` (3-D descriptors): bytes that cross the AXI bus when a 3-D descriptor moves only part of `size`
        // (padding filled by the DMA, int8 packed on write-back). 0 = size. The data copy is always `size` bytes.
        void start_read(int ch, uint32_t dram_addr, int bank, uint32_t bank_offset, uint32_t size, uint32_t timed = 0)
        {
            if (ch < 0 || ch > 3) return;
            rd_[ch] = Chan{true, dram_addr, bank, bank_offset, size, 0, true, clamp_timed(size, timed), 0};
            st.read_bytes += rd_[ch].timed;
        }
        void start_write(uint32_t dram_addr, int bank, uint32_t bank_offset, uint32_t size, uint32_t timed = 0)
        {
            wr_ = Chan{true, dram_addr, bank, bank_offset, size, 0, true, clamp_timed(size, timed), 0};
            st.write_bytes += wr_.timed;
        }
        bool is_read_active(int ch) const { return ch >= 0 && ch < 4 && rd_[ch].active; }
        bool is_write_active() const { return wr_.active; }
        bool is_any_read_active() const { return rd_[0].active || rd_[1].active || rd_[2].active || rd_[3].active; }

    private:
        struct Chan
        {
            bool active{false};
            uint32_t dram_addr{0};
            int bank{0};
            uint32_t bank_offset{0}, size{0}, done{0};
            bool first{true};   // first burst still to issue -> pays dram_latency
            uint32_t timed{0}, tdone{0};   // bus bytes (<= size) and bus bytes done
        };
        static uint32_t clamp_timed(uint32_t size, uint32_t timed) { return (timed == 0 || timed > size) ? size : (timed ? timed : 1); }
        Chan rd_[4], wr_;
        int rr_{0}, cur_rd_{-1};
        uint32_t rd_left_{0}, wr_left_{0};
        Port *port_{nullptr};
        std::vector<uint8_t> *dram_{nullptr};

        void process()
        {
            if (!i_rstn.read())
            {
                for (auto &c : rd_) c = Chan();
                wr_ = Chan();
                rr_ = 0; cur_rd_ = -1; rd_left_ = wr_left_ = 0;
                return;
            }
            if (rd_left_ || wr_left_ || is_any_read_active() || is_write_active()) st.busy_cycles++;
            if (rd_left_) st.read_port_cycles++;
            if (wr_left_) st.write_port_cycles++;

            // 1. write port (before read: DRAM RAW)
            if (wr_left_ > 0 && --wr_left_ == 0 && wr_.active)
            {
                const uint32_t nt = std::min(wr_.timed - wr_.tdone, prm.burst_bytes());
                wr_.tdone += nt;
                const uint32_t n = wr_.tdone >= wr_.timed ? wr_.size - wr_.done : std::min(wr_.size - wr_.done, nt);
                if (dram_ && port_ && n)
                {
                    const uint32_t dst = wr_.dram_addr + wr_.done;
                    if (dst + n > dram_->size()) dram_->resize(size_t(dst) + n, 0);
                    port_->read(wr_.bank, wr_.bank_offset + wr_.done, &(*dram_)[dst], n);
                }
                wr_.done += n;
                st.write_bursts++;
                if (wr_.tdone >= wr_.timed) wr_.active = false;
            }
            if (wr_left_ == 0 && wr_.active)
            {
                wr_left_ = prm.burst_beats + (wr_.first ? prm.dram_latency : 0);
                wr_.first = false;
            }

            // 2. read port
            if (rd_left_ > 0 && --rd_left_ == 0)
            {
                const int ch = cur_rd_;
                if (ch >= 0 && rd_[ch].active)
                {
                    Chan &c = rd_[ch];
                    const uint32_t nt = std::min(c.timed - c.tdone, prm.burst_bytes());
                    c.tdone += nt;
                    const uint32_t n = c.tdone >= c.timed ? c.size - c.done : std::min(c.size - c.done, nt);
                    if (dram_ && port_ && n)
                    {
                        const uint32_t src = c.dram_addr + c.done;
                        if (src + n <= dram_->size()) port_->write(c.bank, c.bank_offset + c.done, &(*dram_)[src], n);
                        else if (src < dram_->size())
                            port_->write(c.bank, c.bank_offset + c.done, &(*dram_)[src], uint32_t(dram_->size() - src));
                    }
                    c.done += n;
                    st.read_bursts++;
                    if (c.tdone >= c.timed) c.active = false;
                }
                cur_rd_ = -1;
            }
            if (rd_left_ == 0)
            {
                int sel = -1;
                const int n_hi = prm.ch3_low_priority ? 3 : 4;
                for (int i = 0; i < 4 && sel < 0; i++)
                {
                    const int ch = (rr_ + i) % 4;
                    if (ch < n_hi && rd_[ch].active) sel = ch;
                }
                if (sel < 0 && prm.ch3_low_priority && rd_[3].active) sel = 3;
                if (sel >= 0)
                {
                    if (sel < n_hi) rr_ = (sel + 1) % 4;
                    cur_rd_ = sel;
                    rd_left_ = prm.burst_beats + (rd_[sel].first ? prm.dram_latency : 0);
                    rd_[sel].first = false;
                }
            }
        }
    };
} // namespace has

#endif
