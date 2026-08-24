#ifndef SAURIA_DMA_H
#define SAURIA_DMA_H

#include <systemc.h>
#include <vector>
#include <cstdint>
#include <algorithm>
#include "sauria_types.h"
#include "sram/sram_top.h"

#ifndef FX1_NO_PERF
#include "instrumentation/perf_counters.h"
#endif

namespace sauria
{
    // A simple, Cycle-Approximate / Transaction-Level model of the Sauria 4-Channel AXI DMA.
    // Spec:
    // - 4 Channels: CH0, CH1, CH2, CH3
    // - CH0 (Weight A Prefetch): Read, High Priority, Target Bank 0
    // - CH1 (Weight B Prefetch): Read, High Priority, Target Bank 1
    // - CH2 (IFmap A Prefetch): Read, High Priority, Target Bank 2
    // - CH3 (IFmap B Prefetch): Read, Medium Priority, Target Bank 3
    // - Write Master: Write, Medium Priority, Target Bank 4/5 -> DRAM
    //
    // AXI Bus:
    // - 256-bit wide bus (32 bytes per cycle).
    // - Burst length = 8 transfers/burst (256 bytes per burst).
    // - Each burst takes 8 cycles.
    // - Read channels CH0-CH3 are round-robin scheduled on a single AXI Read Port.
    // - Write channel uses a dedicated AXI Write Port (no contention with read channels).
    
    template <
        int X_DIM = 64,
        int Y_DIM = 64,
        typename T_ACT = float,
        typename T_WEI = float,
        typename T_PSUM = float,
        int SRAMA_CAP = 1024,
        int SRAMB_CAP = 1024,
        int SRAMC_CAP = 2048>
    class SauriaDma : public sc_module
    {
    public:
        // Clock & Reset
        sc_in<bool> i_clk{"i_clk"};
        sc_in<bool> i_rstn{"i_rstn"};

#ifndef FX1_NO_PERF
        fx1::PerfCounters *perf{nullptr};
#endif

        // Pointer to SRAM module to copy data into/out of SRAM banks directly
        Sram<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM, SRAMA_CAP, SRAMB_CAP, SRAMC_CAP>* m_sram{nullptr};
        // Pointer to external DRAM
        std::vector<uint8_t>* m_dram{nullptr};

        struct ChannelState
        {
            bool active{false};
            uint32_t dram_addr{0};
            int bank_id{0};
            uint32_t bank_offset{0};
            uint32_t size_bytes{0};
            uint32_t bytes_transferred{0};
        };

        ChannelState m_channels[4];
        ChannelState m_write_channel;

        // Round-robin scheduling state for read channels
        int m_rr_ptr{0};
        int m_read_burst_cycles_left{0};
        int m_current_read_ch{-1};

        // Write channel burst state
        int m_write_burst_cycles_left{0};

        SC_CTOR(SauriaDma)
        {
            SC_METHOD(dma_process);
            sensitive << i_clk.pos();
        }

        void set_sram(Sram<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM, SRAMA_CAP, SRAMB_CAP, SRAMC_CAP>* sram)
        {
            m_sram = sram;
        }

        void set_dram(std::vector<uint8_t>* dram)
        {
            m_dram = dram;
        }

        // Methods to start a DMA transfer (triggered by the instruction executor or controller)
        void start_read(int ch_id, uint32_t dram_addr, int bank_id, uint32_t bank_offset, uint32_t size_bytes)
        {
            if (ch_id < 0 || ch_id > 3) return;
            m_channels[ch_id].active = true;
            m_channels[ch_id].dram_addr = dram_addr;
            m_channels[ch_id].bank_id = bank_id;
            m_channels[ch_id].bank_offset = bank_offset;
            m_channels[ch_id].size_bytes = size_bytes;
            m_channels[ch_id].bytes_transferred = 0;
#ifndef FX1_NO_PERF
            if (perf)
            {
                perf->ddr_read_bytes += size_bytes;
            }
#endif
        }

        void start_write(uint32_t dram_addr, int bank_id, uint32_t bank_offset, uint32_t size_bytes)
        {
            m_write_channel.active = true;
            m_write_channel.dram_addr = dram_addr;
            m_write_channel.bank_id = bank_id;
            m_write_channel.bank_offset = bank_offset;
            m_write_channel.size_bytes = size_bytes;
            m_write_channel.bytes_transferred = 0;
#ifndef FX1_NO_PERF
            if (perf)
            {
                perf->ddr_write_bytes += size_bytes;
            }
#endif
        }

        bool is_read_active(int ch_id) const
        {
            if (ch_id < 0 || ch_id > 3) return false;
            return m_channels[ch_id].active;
        }

        bool is_write_active() const
        {
            return m_write_channel.active;
        }

        bool is_any_read_active() const
        {
            return m_channels[0].active || m_channels[1].active || m_channels[2].active || m_channels[3].active;
        }

    private:
        void dma_process()
        {
            if (!i_rstn.read())
            {
                for (int i = 0; i < 4; i++)
                {
                    m_channels[i] = ChannelState();
                }
                m_write_channel = ChannelState();
                m_rr_ptr = 0;
                m_read_burst_cycles_left = 0;
                m_current_read_ch = -1;
                m_write_burst_cycles_left = 0;
                return;
            }

#ifndef FX1_NO_PERF
            if (perf)
            {
                if (m_read_burst_cycles_left > 0 || m_write_burst_cycles_left > 0 || is_any_read_active() || is_write_active())
                {
                    perf->transfer_cycles++;
                    perf->dma_engine_cycles++;
                }
                if (m_read_burst_cycles_left > 0)
                {
                    perf->dma_read_cycles_raw++;
                }
                if (m_write_burst_cycles_left > 0)
                {
                    perf->dma_write_cycles_raw++;
                }
            }
#endif

            // ----------------------------------------------------------------
            // 1. AXI Read Port Process (Round-Robin between CH0-CH3)
            // ----------------------------------------------------------------
            if (m_read_burst_cycles_left > 0)
            {
                m_read_burst_cycles_left--;
                if (m_read_burst_cycles_left == 0)
                {
                    // Burst complete! Let's actually copy the data for this burst.
                    int ch = m_current_read_ch;
                    if (ch >= 0 && ch < 4 && m_channels[ch].active)
                    {
                        uint32_t dram_src = m_channels[ch].dram_addr + m_channels[ch].bytes_transferred;
                        uint32_t bank_dst = m_channels[ch].bank_offset + m_channels[ch].bytes_transferred;
                        uint32_t remaining = m_channels[ch].size_bytes - m_channels[ch].bytes_transferred;
                        uint32_t burst_size = std::min(remaining, (uint32_t)256);
                        
                        if (m_dram && m_sram)
                        {
                            // Bounds check DRAM
                            if (dram_src + burst_size <= m_dram->size())
                            {
                                m_sram->write_bank_data(m_channels[ch].bank_id, bank_dst, &(*m_dram)[dram_src], burst_size);
                            }
                            else if (dram_src < m_dram->size())
                            {
                                uint32_t safe_size = m_dram->size() - dram_src;
                                m_sram->write_bank_data(m_channels[ch].bank_id, bank_dst, &(*m_dram)[dram_src], safe_size);
                            }
                        }
                        
                        m_channels[ch].bytes_transferred += burst_size;
                        if (m_channels[ch].bytes_transferred >= m_channels[ch].size_bytes)
                        {
                            m_channels[ch].active = false;
                        }
                    }
                    m_current_read_ch = -1;
                }
            }

            if (m_read_burst_cycles_left == 0)
            {
                // Find next active read channel using round-robin starting from m_rr_ptr
                int selected_ch = -1;
                for (int i = 0; i < 4; i++)
                {
                    int ch = (m_rr_ptr + i) % 4;
                    if (m_channels[ch].active)
                    {
                        selected_ch = ch;
                        m_rr_ptr = (ch + 1) % 4;
                        break;
                    }
                }
                
                if (selected_ch != -1)
                {
                    m_current_read_ch = selected_ch;
                    m_read_burst_cycles_left = 8; // 8 cycles per burst
                }
            }

            // ----------------------------------------------------------------
            // 2. AXI Write Port Process (Dedicated write master)
            // ----------------------------------------------------------------
            if (m_write_burst_cycles_left > 0)
            {
                m_write_burst_cycles_left--;
                if (m_write_burst_cycles_left == 0)
                {
                    // Burst complete! Let's copy from SRAM to DRAM.
                    if (m_write_channel.active)
                    {
                        uint32_t bank_src = m_write_channel.bank_offset + m_write_channel.bytes_transferred;
                        uint32_t dram_dst = m_write_channel.dram_addr + m_write_channel.bytes_transferred;
                        uint32_t remaining = m_write_channel.size_bytes - m_write_channel.bytes_transferred;
                        uint32_t burst_size = std::min(remaining, (uint32_t)256);

                        if (m_dram && m_sram)
                        {
                            // Ensure DRAM has enough space
                            if (dram_dst + burst_size > m_dram->size())
                            {
                                m_dram->resize(dram_dst + burst_size, 0);
                            }
                            m_sram->read_bank_data(m_write_channel.bank_id, bank_src, &(*m_dram)[dram_dst], burst_size);
                        }

                        m_write_channel.bytes_transferred += burst_size;
                        if (m_write_channel.bytes_transferred >= m_write_channel.size_bytes)
                        {
                            m_write_channel.active = false;
                        }
                    }
                }
            }

            if (m_write_burst_cycles_left == 0 && m_write_channel.active)
            {
                m_write_burst_cycles_left = 8; // 8 cycles per burst
            }
        }
    };
} // namespace sauria

#endif // SAURIA_DMA_H
