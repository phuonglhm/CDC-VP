// Proof of concept: TLM-2.0 bus-master variant of the DMA of control/sauria_dma.h. That DMA reaches DRAM
// through a backdoor pointer (m_dram) while its scheduling and timing are modelled; only the two points
// where a burst completes (read and write) copy bytes, and these two points are replaced here by TLM
// transactions on a tlm_utils::simple_initiator_socket. The SRAM side keeps the backdoor (on-chip memory,
// not exposed on the SoC bus).
//
// This file is an independent copy: it does NOT replace control/sauria_dma.h, which npu_top.h uses. It is
// the starting point for connecting the model to an SoC bus model.
#ifndef SAURIA_DMA_TLM_H
#define SAURIA_DMA_TLM_H

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <cstring>
#include "sauria_types.h"
#include "sram/sram_top.h"

namespace sauria
{
    template <
        int X_DIM = 32,
        int Y_DIM = 32,
        typename T_ACT = int8_t,
        typename T_WEI = int8_t,
        typename T_PSUM = int32_t,
        int SRAMA_CAP = 5056,
        int SRAMB_CAP = 5184,
        int SRAMC_CAP = 1536>
    class SauriaDmaTlm : public sc_module
    {
    public:
        sc_in<bool> i_clk{"i_clk"};
        sc_in<bool> i_rstn{"i_rstn"};

        // Bus-master TLM initiator towards the external DRAM, in place of the m_dram pointer of the original.
        tlm_utils::simple_initiator_socket<SauriaDmaTlm> dram_socket{"dram_socket"};

        Sram<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM, SRAMA_CAP, SRAMB_CAP, SRAMC_CAP>* m_sram{nullptr};

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

        int m_rr_ptr{0};
        int m_read_burst_cycles_left{0};
        int m_current_read_ch{-1};
        int m_write_burst_cycles_left{0};

        SC_CTOR(SauriaDmaTlm)
        {
            SC_METHOD(dma_process);
            sensitive << i_clk.pos();
        }

        void set_sram(Sram<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM, SRAMA_CAP, SRAMB_CAP, SRAMC_CAP>* sram)
        {
            m_sram = sram;
        }

        void start_read(int ch_id, uint32_t dram_addr, int bank_id, uint32_t bank_offset, uint32_t size_bytes)
        {
            if (ch_id < 0 || ch_id > 3) return;
            m_channels[ch_id].active = true;
            m_channels[ch_id].dram_addr = dram_addr;
            m_channels[ch_id].bank_id = bank_id;
            m_channels[ch_id].bank_offset = bank_offset;
            m_channels[ch_id].size_bytes = size_bytes;
            m_channels[ch_id].bytes_transferred = 0;
        }

        void start_write(uint32_t dram_addr, int bank_id, uint32_t bank_offset, uint32_t size_bytes)
        {
            m_write_channel.active = true;
            m_write_channel.dram_addr = dram_addr;
            m_write_channel.bank_id = bank_id;
            m_write_channel.bank_offset = bank_offset;
            m_write_channel.size_bytes = size_bytes;
            m_write_channel.bytes_transferred = 0;
        }

        bool is_read_active(int ch_id) const { return (ch_id >= 0 && ch_id <= 3) ? m_channels[ch_id].active : false; }
        bool is_write_active() const { return m_write_channel.active; }
        bool is_any_read_active() const
        {
            return m_channels[0].active || m_channels[1].active || m_channels[2].active || m_channels[3].active;
        }

    private:
        // Simple TLM transaction (blocking, LT style) to dram_socket, replacing the pointer copy
        // into m_dram. is_write=true -> TLM_WRITE_COMMAND, otherwise TLM_READ_COMMAND.
        void tlm_transfer(uint64_t addr, unsigned char* data_ptr, unsigned len, bool is_write)
        {
            tlm::tlm_generic_payload trans;
            sc_time delay = SC_ZERO_TIME;
            trans.set_command(is_write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
            trans.set_address(addr);
            trans.set_data_ptr(data_ptr);
            trans.set_data_length(len);
            trans.set_streaming_width(len);
            trans.set_byte_enable_ptr(nullptr);
            trans.set_dmi_allowed(false);
            trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

            dram_socket->b_transport(trans, delay);

            if (trans.get_response_status() != tlm::TLM_OK_RESPONSE)
            {
                SC_REPORT_ERROR("SauriaDmaTlm", "TLM transfer to dram_socket did not return TLM_OK_RESPONSE");
            }
        }

        void dma_process()
        {
            if (!i_rstn.read())
            {
                for (int i = 0; i < 4; i++) m_channels[i] = ChannelState();
                m_write_channel = ChannelState();
                m_rr_ptr = 0;
                m_read_burst_cycles_left = 0;
                m_current_read_ch = -1;
                m_write_burst_cycles_left = 0;
                return;
            }

            // 1. AXI write port (same scheduling; only the byte copy at burst completion changes)
            if (m_write_burst_cycles_left > 0)
            {
                m_write_burst_cycles_left--;
                if (m_write_burst_cycles_left == 0)
                {
                    if (m_write_channel.active)
                    {
                        uint32_t bank_src = m_write_channel.bank_offset + m_write_channel.bytes_transferred;
                        uint32_t dram_dst = m_write_channel.dram_addr + m_write_channel.bytes_transferred;
                        uint32_t remaining = m_write_channel.size_bytes - m_write_channel.bytes_transferred;
                        uint32_t burst_size = std::min(remaining, (uint32_t)256);

                        if (m_sram && burst_size > 0)
                        {
                            std::vector<uint8_t> buf(burst_size);
                            m_sram->read_bank_data(m_write_channel.bank_id, bank_src, buf.data(), burst_size);
                            // REPLACED BYTE COPY: a TLM call instead of writing m_dram directly.
                            tlm_transfer(dram_dst, buf.data(), burst_size, /*is_write=*/true);
                        }

                        m_write_channel.bytes_transferred += burst_size;
                        if (m_write_channel.bytes_transferred >= m_write_channel.size_bytes)
                            m_write_channel.active = false;
                    }
                }
            }
            if (m_write_burst_cycles_left == 0 && m_write_channel.active)
                m_write_burst_cycles_left = 8;

            // 2. AXI Read Port (round-robin, schedule unchanged)
            if (m_read_burst_cycles_left > 0)
            {
                m_read_burst_cycles_left--;
                if (m_read_burst_cycles_left == 0)
                {
                    int ch = m_current_read_ch;
                    if (ch >= 0 && ch < 4 && m_channels[ch].active)
                    {
                        uint32_t dram_src = m_channels[ch].dram_addr + m_channels[ch].bytes_transferred;
                        uint32_t bank_dst = m_channels[ch].bank_offset + m_channels[ch].bytes_transferred;
                        uint32_t remaining = m_channels[ch].size_bytes - m_channels[ch].bytes_transferred;
                        uint32_t burst_size = std::min(remaining, (uint32_t)256);

                        if (m_sram && burst_size > 0)
                        {
                            std::vector<uint8_t> buf(burst_size);
                            // REPLACED BYTE COPY: read through TLM instead of reading m_dram directly.
                            tlm_transfer(dram_src, buf.data(), burst_size, /*is_write=*/false);
                            m_sram->write_bank_data(m_channels[ch].bank_id, bank_dst, buf.data(), burst_size);
                        }

                        m_channels[ch].bytes_transferred += burst_size;
                        if (m_channels[ch].bytes_transferred >= m_channels[ch].size_bytes)
                            m_channels[ch].active = false;
                    }
                    m_current_read_ch = -1;
                }
            }
            if (m_read_burst_cycles_left == 0)
            {
                int selected_ch = -1;
                for (int i = 0; i < 4; i++)
                {
                    int ch = (m_rr_ptr + i) % 4;
                    if (m_channels[ch].active) { selected_ch = ch; m_rr_ptr = (ch + 1) % 4; break; }
                }
                if (selected_ch != -1)
                {
                    m_current_read_ch = selected_ch;
                    m_read_burst_cycles_left = 8;
                }
            }
        }
    };
} // namespace sauria

#endif // SAURIA_DMA_TLM_H
