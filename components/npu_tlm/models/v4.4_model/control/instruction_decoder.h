// SystemC Model for SAURIA NPU Instruction Decoder & Executor
// Handles flat, unrolled instruction stream for GEMM_FUSED, FUSED_ATTN, LAYERNORM, ELEM_WISE.

#ifndef SAURIA_INSTRUCTION_DECODER_H
#define SAURIA_INSTRUCTION_DECODER_H

#include "sauria_types.h"
#include "debug.h"
#include "control/sauria_dma.h"
#include <queue>
#include <iostream>
#include <cmath>
#include <algorithm>

namespace sauria
{
    struct SauriaRichInstruction
    {
        uint8_t opcode{0}; // 0x12=GEMM_FUSED, 0x13=FUSED_ATTN, 0x14=LAYERNORM, 0x15=ELEM_WISE, 0x05=SET_NSPLIT
        
        // GEMM_FUSED / common fields
        uint32_t in_addr{0};
        uint32_t w_addr{0};
        uint32_t out_addr{0};
        uint32_t bias_addr{0};
        uint32_t m{32};
        uint32_t k{32};
        uint32_t n{32};
        uint32_t kh{1};
        uint32_t kw{1};
        uint32_t stride{1};
        uint32_t pad{0};
        uint32_t act_type{0}; // 0=None, 1=ReLU, 2=SiLU
        uint32_t has_skip{0};
        uint32_t skip_addr{0};
        float in_scale{1.0f};
        float w_scale{1.0f};
        float out_scale{1.0f};

        // FUSED_ATTN fields
        uint32_t q_addr{0};
        uint32_t k_addr{0};
        uint32_t v_addr{0};
        uint32_t seq_len{0};
        uint32_t num_heads{0};
        uint32_t head_dim{0};
        float attn_scale{1.0f};

        // LAYERNORM fields
        uint32_t gamma_addr{0};
        uint32_t beta_addr{0};
        uint32_t dim{0};
        uint32_t eps_shift{0};

        // ELEM_WISE fields
        uint32_t a_addr{0};
        uint32_t b_addr{0};
        uint32_t len{0};
        uint32_t mode{0}; // 0=ADD, 1=MAX_POOL
        float scale_a{1.0f};
        float scale_b{1.0f};
        float scale_out{1.0f};
    };

    template <
        int X_DIM = 32,
        int Y_DIM = 32,
        typename T_ACT = float,
        typename T_WEI = float,
        typename T_PSUM = float,
        int SRAMA_CAP = 1024,
        int SRAMB_CAP = 1024,
        int SRAMC_CAP = 2048>
    class InstructionDecoder : public sc_module
    {
    public:
        // Clock & Reset
        sc_in<bool> i_clk{"i_clk"};
        sc_in<bool> i_rstn{"i_rstn"};

        // Host bus interface for writing to Queues
        sc_in<uint32_t> i_host_addr{"i_host_addr"};
        sc_in<host_data_t> i_host_wdata{"i_host_wdata"};
        sc_in<bool> i_host_wren{"i_host_wren"};

        // Status from controllers
        sc_in<bool> i_ctrl_active_a{"i_ctrl_active_a"};
        sc_in<bool> i_ctrl_active_b{"i_ctrl_active_b"};

        // Mux flag
        sc_out<bool> o_use_instr_mode{"o_use_instr_mode"};

        // Output to top level/registers
        sc_out<uint32_t> o_nsplit{"o_nsplit"};
        sc_out<bool> o_trigger_start_a{"o_trigger_start_a"};
        sc_out<bool> o_trigger_start_b{"o_trigger_start_b"};

        // Decoded address registers (for backward compatibility)
        sc_out<uint32_t> o_wei_addr_a{"o_wei_addr_a"};
        sc_out<uint32_t> o_ifmap_addr_a{"o_ifmap_addr_a"};
        sc_out<uint32_t> o_wei_addr_b{"o_wei_addr_b"};
        sc_out<uint32_t> o_ifmap_addr_b{"o_ifmap_addr_b"};

        // Pointers for execution
        SauriaDma<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM, SRAMA_CAP, SRAMB_CAP, SRAMC_CAP>* m_dma{nullptr};
        Sram<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM, SRAMA_CAP, SRAMB_CAP, SRAMC_CAP>* m_sram{nullptr};
        std::vector<uint8_t>* m_dram{nullptr};

        SC_CTOR(InstructionDecoder)
        {
            SC_METHOD(decode_process);
            sensitive << i_clk.pos();

            SC_METHOD(host_write_process);
            sensitive << i_clk.pos();
        }

        void set_dma(SauriaDma<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM, SRAMA_CAP, SRAMB_CAP, SRAMC_CAP>* dma)
        {
            m_dma = dma;
        }

        void set_sram(Sram<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM, SRAMA_CAP, SRAMB_CAP, SRAMC_CAP>* sram)
        {
            m_sram = sram;
        }

        void set_dram(std::vector<uint8_t>* dram)
        {
            m_dram = dram;
        }

        size_t get_queue_a_size() const { return queue_a.size(); }
        size_t get_queue_b_size() const { return queue_b.size(); }
        int get_state_a() const { return static_cast<int>(state_a); }
        int get_state_b() const { return static_cast<int>(state_b); }

    private:
        std::queue<SauriaRichInstruction> queue_a;
        std::queue<SauriaRichInstruction> queue_b;

        // Configuration registers for assembling rich instructions
        uint32_t r_in_addr{0};
        uint32_t r_w_addr{0};
        uint32_t r_out_addr{0};
        uint32_t r_bias_addr{0};
        uint32_t r_m{32};
        uint32_t r_k{32};
        uint32_t r_n{32};
        uint32_t r_kh{1};
        uint32_t r_kw{1};
        uint32_t r_stride{1};
        uint32_t r_pad{0};
        uint32_t r_act_type{0};
        uint32_t r_has_skip{0};
        uint32_t r_skip_addr{0};
        float r_in_scale{1.0f};
        float r_w_scale{1.0f};
        float r_out_scale{1.0f};

        uint32_t r_q_addr{0};
        uint32_t r_k_addr{0};
        uint32_t r_v_addr{0};
        uint32_t r_seq_len{0};
        uint32_t r_num_heads{0};
        uint32_t r_head_dim{0};
        float r_attn_scale{1.0f};

        uint32_t r_gamma_addr{0};
        uint32_t r_beta_addr{0};
        uint32_t r_dim{0};
        uint32_t r_eps_shift{0};

        uint32_t r_a_addr{0};
        uint32_t r_b_addr{0};
        uint32_t r_len{0};
        uint32_t r_mode{0};
        float r_scale_a{1.0f};
        float r_scale_b{1.0f};
        float r_scale_out{1.0f};

        uint32_t r_nsplit{32}; // Default split (32 rows for Lane A, 32 rows for Lane B in 64x64 PE array)
        bool r_use_instr_mode{false};

    public:
        // State machine states for lane execution
        enum State { IDLE, DMA_READ_WAIT, COMPUTE_WAIT, DMA_WRITE_WAIT, WAIT_BARRIER };
        State state_a{IDLE};
        State state_b{IDLE};
    private:

        uint32_t compute_cycles_left_a{0};
        uint32_t compute_cycles_left_b{0};

        void host_write_process()
        {
            if (!i_rstn.read())
            {
                while (!queue_a.empty()) queue_a.pop();
                while (!queue_b.empty()) queue_b.pop();
                r_use_instr_mode = false;
                o_use_instr_mode.write(false);
                return;
            }

            if (i_host_wren.read())
            {
                uint32_t addr = i_host_addr.read();
                host_data_t host_wdata = i_host_wdata.read();
                uint32_t data = static_cast<uint32_t>(host_wdata[0]);
                float data_f = static_cast<float>(host_wdata[0]);

                // 1. Assembling instructions via MMIO config registers
                if (addr == 0x40000400) r_in_addr = data;
                else if (addr == 0x40000404) r_w_addr = data;
                else if (addr == 0x40000408) r_out_addr = data;
                else if (addr == 0x4000040C) r_bias_addr = data;
                else if (addr == 0x40000410) r_m = data;
                else if (addr == 0x40000414) r_k = data;
                else if (addr == 0x40000418) r_n = data;
                else if (addr == 0x4000041C) r_kh = data;
                else if (addr == 0x40000420) r_kw = data;
                else if (addr == 0x40000424) r_stride = data;
                else if (addr == 0x40000428) r_pad = data;
                else if (addr == 0x4000042C) r_act_type = data;
                else if (addr == 0x40000430) r_has_skip = data;
                else if (addr == 0x40000434) r_skip_addr = data;
                else if (addr == 0x40000438) r_in_scale = data_f;
                else if (addr == 0x4000043C) r_w_scale = data_f;
                else if (addr == 0x40000440) r_out_scale = data_f;
                else if (addr == 0x40000444) { r_q_addr = data; r_gamma_addr = data; r_a_addr = data; }
                else if (addr == 0x40000448) { r_k_addr = data; r_b_addr = data; }
                else if (addr == 0x4000044C) { r_v_addr = data; r_beta_addr = data; }
                else if (addr == 0x40000450) { r_seq_len = data; r_len = data; }
                else if (addr == 0x40000454) {
                    if ((data & 0xFFFF0000) != 0) {
                        r_num_heads = data & 0xFFFF;
                        r_dim = (data >> 16) & 0xFF;
                        r_mode = (data >> 24) & 0xFF;
                    } else {
                        r_num_heads = data; r_dim = data; r_mode = data;
                    }
                }
                else if (addr == 0x40000458) { r_head_dim = data; r_eps_shift = data; r_scale_a = data_f; }
                else if (addr == 0x4000045C) { r_attn_scale = data_f; r_scale_b = data_f; }
                else if (addr == 0x40000460) { r_scale_out = data_f; }

                // 2. Trigger registers
                else if (addr == 0x40000310) // Push rich instruction to Lane A
                {
                    SauriaRichInstruction inst;
                    inst.opcode = data & 0xFF;
                    inst.in_addr = r_in_addr; inst.w_addr = r_w_addr; inst.out_addr = r_out_addr; inst.bias_addr = r_bias_addr;
                    inst.m = r_m; inst.k = r_k; inst.n = r_n; inst.kh = r_kh; inst.kw = r_kw; inst.stride = r_stride; inst.pad = r_pad;
                    inst.act_type = r_act_type; inst.has_skip = r_has_skip; inst.skip_addr = r_skip_addr;
                    inst.in_scale = r_in_scale; inst.w_scale = r_w_scale; inst.out_scale = r_out_scale;
                    inst.q_addr = r_q_addr; inst.k_addr = r_k_addr; inst.v_addr = r_v_addr;
                    inst.seq_len = r_seq_len; inst.num_heads = r_num_heads; inst.head_dim = r_head_dim; inst.attn_scale = r_attn_scale;
                    inst.gamma_addr = r_gamma_addr; inst.beta_addr = r_beta_addr; inst.dim = r_dim; inst.eps_shift = r_eps_shift;
                    inst.a_addr = r_a_addr; inst.b_addr = r_b_addr; inst.len = r_len; inst.mode = r_mode;
                    inst.scale_a = r_scale_a; inst.scale_b = r_scale_b; inst.scale_out = r_scale_out;

                    queue_a.push(inst);
                    r_use_instr_mode = true;
                    o_use_instr_mode.write(true);
                    std::cout << "[INST DECODER] Pushed rich instruction to Queue A, Opcode = 0x" 
                              << std::hex << (int)inst.opcode << std::dec << std::endl;
                }
                else if (addr == 0x40000314) // Push rich instruction to Lane B
                {
                    SauriaRichInstruction inst;
                    inst.opcode = data & 0xFF;
                    inst.in_addr = r_in_addr; inst.w_addr = r_w_addr; inst.out_addr = r_out_addr; inst.bias_addr = r_bias_addr;
                    inst.m = r_m; inst.k = r_k; inst.n = r_n; inst.kh = r_kh; inst.kw = r_kw; inst.stride = r_stride; inst.pad = r_pad;
                    inst.act_type = r_act_type; inst.has_skip = r_has_skip; inst.skip_addr = r_skip_addr;
                    inst.in_scale = r_in_scale; inst.w_scale = r_w_scale; inst.out_scale = r_out_scale;
                    inst.q_addr = r_q_addr; inst.k_addr = r_k_addr; inst.v_addr = r_v_addr;
                    inst.seq_len = r_seq_len; inst.num_heads = r_num_heads; inst.head_dim = r_head_dim; inst.attn_scale = r_attn_scale;
                    inst.gamma_addr = r_gamma_addr; inst.beta_addr = r_beta_addr; inst.dim = r_dim; inst.eps_shift = r_eps_shift;
                    inst.a_addr = r_a_addr; inst.b_addr = r_b_addr; inst.len = r_len; inst.mode = r_mode;
                    inst.scale_a = r_scale_a; inst.scale_b = r_scale_b; inst.scale_out = r_scale_out;

                    queue_b.push(inst);
                    r_use_instr_mode = true;
                    o_use_instr_mode.write(true);
                    std::cout << "[INST DECODER] Pushed rich instruction to Queue B, Opcode = 0x" 
                              << std::hex << (int)inst.opcode << std::dec << std::endl;
                }

                // 3. Legacy 64-bit instruction writes
                static uint32_t temp_word_a = 0;
                static uint32_t temp_word_b = 0;

                if (addr == 0x40000300)
                {
                    temp_word_a = data;
                    r_use_instr_mode = true;
                    o_use_instr_mode.write(true);
                }
                else if (addr == 0x40000304)
                {
                    uint64_t instr = ((uint64_t)data << 32) | temp_word_a;
                    SauriaRichInstruction rich_inst;
                    uint8_t op_hi = (instr >> 56) & 0xFF;
                    uint8_t op_lo = instr & 0xFF;
                    rich_inst.opcode = (op_hi != 0) ? op_hi : op_lo;
                    
                    if (rich_inst.opcode == 0x05)
                    {
                        rich_inst.opcode = 0x05;
                        rich_inst.n = instr & 0x7F; // nsplit
                    }
                    else if (rich_inst.opcode == 0x12)
                    {
                        rich_inst.w_addr = (instr >> 32) & 0x00FFFFFF;
                        rich_inst.in_addr = (instr >> 16) & 0xFFFF;
                        rich_inst.out_addr = CFG_OUT_BASE_ADDR;
                    }
                    queue_a.push(rich_inst);
                    r_use_instr_mode = true;
                    o_use_instr_mode.write(true);
                    std::cout << "[INST DECODER] Pushed legacy instruction to Queue A: 0x" << std::hex << instr << std::dec << std::endl;
                }
                else if (addr == 0x40000308)
                {
                    temp_word_b = data;
                    r_use_instr_mode = true;
                    o_use_instr_mode.write(true);
                }
                else if (addr == 0x4000030c)
                {
                    uint64_t instr = ((uint64_t)data << 32) | temp_word_b;
                    SauriaRichInstruction rich_inst;
                    uint8_t op_hi = (instr >> 56) & 0xFF;
                    uint8_t op_lo = instr & 0xFF;
                    rich_inst.opcode = (op_hi != 0) ? op_hi : op_lo;
                    
                    if (rich_inst.opcode == 0x05)
                    {
                        rich_inst.opcode = 0x05;
                        rich_inst.n = instr & 0x7F;
                    }
                    else if (rich_inst.opcode == 0x12)
                    {
                        rich_inst.w_addr = (instr >> 32) & 0x00FFFFFF;
                        rich_inst.in_addr = (instr >> 16) & 0xFFFF;
                        rich_inst.out_addr = CFG_OUT_BASE_ADDR;
                    }
                    queue_b.push(rich_inst);
                    r_use_instr_mode = true;
                    o_use_instr_mode.write(true);
                    std::cout << "[INST DECODER] Pushed legacy instruction to Queue B: 0x" << std::hex << instr << std::dec << std::endl;
                }
            }
        }

        void decode_process()
        {
            if (!i_rstn.read())
            {
                r_nsplit = 32;
                o_nsplit.write(r_nsplit);
                o_trigger_start_a.write(false);
                o_trigger_start_b.write(false);
                o_wei_addr_a.write(0);
                o_ifmap_addr_a.write(0);
                o_wei_addr_b.write(0);
                o_ifmap_addr_b.write(0);
                state_a = IDLE;
                state_b = IDLE;
                compute_cycles_left_a = 0;
                compute_cycles_left_b = 0;
                return;
            }

            // --------------------------------------------------------
            // --- Lane A Decoder & Executor ---
            // --------------------------------------------------------
            switch (state_a)
            {
                case IDLE:
                {
                    o_trigger_start_a.write(false);
                    if (!queue_a.empty())
                    {
                        SauriaRichInstruction inst = queue_a.front();
                        if (inst.opcode == 0x05) // SET_NSPLIT
                        {
                            bool sa_busy = i_ctrl_active_a.read() || i_ctrl_active_b.read() || (state_b != IDLE);
                            if (sa_busy)
                            {
                                state_a = WAIT_BARRIER;
                            }
                            else
                            {
                                r_nsplit = inst.n;
                                o_nsplit.write(r_nsplit);
                                queue_a.pop();
                                std::cout << "[INST DECODER] SET_NSPLIT executed: nsplit=" << r_nsplit << std::endl;
                            }
                        }
                        else if (inst.opcode == 0x12) // GEMM_FUSED
                        {
                            std::cout << "[EXECUTOR] GEMM_FUSED_A starting..." << std::endl;
                            if (m_dma)
                            {
                                // CH0: Weights prefetch (Bank 0)
                                uint32_t wei_size = inst.k * inst.n * sizeof(T_WEI);
                                m_dma->start_read(0, inst.w_addr, 0, 0, wei_size);

                                // CH2: IFmap prefetch (Bank 2)
                                uint32_t act_size = inst.m * inst.k * sizeof(T_ACT);
                                m_dma->start_read(2, inst.in_addr, 2, 0, act_size);

                                // Optional Skip Connection prefetch (Bank 3)
                                if (inst.has_skip)
                                {
                                    uint32_t skip_size = inst.m * inst.n * sizeof(T_ACT);
                                    m_dma->start_read(3, inst.skip_addr, 3, 0, skip_size);
                                }
                            }
                            state_a = DMA_READ_WAIT;
                        }
                        else if (inst.opcode == 0x13) // FUSED_ATTN
                        {
                            std::cout << "[EXECUTOR] FUSED_ATTN_A starting..." << std::endl;
                            if (m_dma)
                            {
                                uint32_t num_h = inst.num_heads > 0 ? inst.num_heads : 1;
                                uint32_t sz = inst.seq_len * num_h * inst.head_dim * sizeof(T_ACT);
                                m_dma->start_read(0, inst.q_addr, 0, 0, sz); // Q to Bank 0
                                m_dma->start_read(1, inst.k_addr, 1, 0, sz); // K to Bank 1
                                m_dma->start_read(2, inst.v_addr, 2, 0, sz); // V to Bank 2
                            }
                            state_a = DMA_READ_WAIT;
                        }
                        else if (inst.opcode == 0x14) // LAYERNORM
                        {
                            std::cout << "[EXECUTOR] LAYERNORM_A starting..." << std::endl;
                            if (m_dma)
                            {
                                uint32_t scale_sz = inst.dim * sizeof(T_WEI);
                                uint32_t in_sz = inst.seq_len * inst.dim * sizeof(T_ACT);
                                m_dma->start_read(0, inst.gamma_addr, 0, 0, scale_sz); // Gamma to Bank 0
                                m_dma->start_read(1, inst.beta_addr, 1, 0, scale_sz);  // Beta to Bank 1
                                m_dma->start_read(2, inst.in_addr, 2, 0, in_sz);       // In to Bank 2
                            }
                            state_a = DMA_READ_WAIT;
                        }
                        else if (inst.opcode == 0x15) // ELEM_WISE
                        {
                            std::cout << "[EXECUTOR] ELEM_WISE_A starting..." << std::endl;
                            if (m_dma)
                            {
                                auto get_p_len = [](uint32_t addr, uint32_t tlen, uint32_t d) -> uint32_t {
                                    if (addr >= 0x00180000) return tlen;
                                    if (d > 0 && d < tlen) return d;
                                    if (tlen % 3072 == 0) return 3072;
                                    return 768;
                                };
                                uint32_t sz_a = get_p_len(inst.a_addr, inst.len, inst.dim) * sizeof(T_ACT);
                                m_dma->start_read(2, inst.a_addr, 2, 0, sz_a); // A to Bank 2
                                if (inst.mode != 1) // Modes 0, 2, 3, 4 need B
                                {
                                    uint32_t sz_b = get_p_len(inst.b_addr, inst.len, inst.dim) * sizeof(T_ACT);
                                    m_dma->start_read(3, inst.b_addr, 3, 0, sz_b); // B to Bank 3
                                }
                            }
                            state_a = DMA_READ_WAIT;
                        }
                        else
                        {
                            queue_a.pop(); // Drop invalid opcodes
                        }
                    }
                    break;
                }

                case DMA_READ_WAIT:
                {
                    bool dma_busy = false;
                    if (m_dma)
                    {
                        dma_busy = m_dma->is_read_active(0) || m_dma->is_read_active(1) || m_dma->is_read_active(2) || m_dma->is_read_active(3);
                    }
                    if (!dma_busy)
                    {
                        // Calculate computation cycle duration
                        SauriaRichInstruction inst = queue_a.front();
                        if (inst.opcode == 0x12) // GEMM
                        {
                            compute_cycles_left_a = (inst.m * inst.k * inst.n) / (X_DIM * Y_DIM) + 50;
                        }
                        else if (inst.opcode == 0x13) // ATTN
                        {
                            compute_cycles_left_a = (inst.seq_len * inst.head_dim * inst.seq_len) / (X_DIM * Y_DIM) + 
                                                     (inst.seq_len * inst.seq_len * inst.head_dim) / (X_DIM * Y_DIM) + 100;
                        }
                        else if (inst.opcode == 0x14) // LN
                        {
                            compute_cycles_left_a = (inst.seq_len * inst.dim) / Y_DIM + 30;
                        }
                        else if (inst.opcode == 0x15) // ELEM
                        {
                            compute_cycles_left_a = inst.len / Y_DIM + 20;
                        }
                        state_a = COMPUTE_WAIT;
                    }
                    break;
                }

                case COMPUTE_WAIT:
                {
                    if (compute_cycles_left_a > 0)
                    {
                        compute_cycles_left_a--;
#ifndef FX1_NO_PERF
                        if (m_dma && m_dma->perf)
                        {
                            const SauriaRichInstruction &inst = queue_a.front();
                            if (inst.opcode == 0x12 || inst.opcode == 0x13)
                            {
                                m_dma->perf->processing_cycles++;
                                m_dma->perf->mac_engine_cycles++;
                                m_dma->perf->sa_cycles++;
                                m_dma->perf->exec_cycles++;
                            }
                            else if (inst.opcode == 0x14)
                            {
                                m_dma->perf->reduction_engine_cycles++;
                            }
                            else if (inst.opcode == 0x15)
                            {
                                if (inst.mode == 1)
                                    m_dma->perf->pooling_engine_cycles++;
                                else
                                    m_dma->perf->activation_engine_cycles++;
                            }
                        }
#endif
                    }
                    
                    if (compute_cycles_left_a == 0)
                    {
                        // Perform the mathematical calculation on SRAM Banks directly!
                        SauriaRichInstruction inst = queue_a.front();
                        
                        if (inst.opcode == 0x12) // GEMM_FUSED
                        {
#ifndef FX1_NO_PERF
                            if (m_dma && m_dma->perf)
                            {
                                m_dma->perf->M += inst.m;
                                m_dma->perf->K += inst.k;
                                m_dma->perf->N += inst.n;
                                uint64_t macs = static_cast<uint64_t>(inst.m) * inst.k * inst.n;
                                m_dma->perf->mac_ops += macs;
                                m_dma->perf->active_pe_cycles += macs;
                                m_dma->perf->total_pe_cycles += macs;
                            }
#endif
                            emulate_gemm_fused(inst);
                            if (m_dma)
                            {
                                uint32_t out_sz = inst.m * inst.n * sizeof(T_ACT);
                                m_dma->start_write(inst.out_addr, 4, 0, out_sz); // Bank 4 to DRAM
                            }
                        }
                        else if (inst.opcode == 0x13) // FUSED_ATTN
                        {
                            emulate_fused_attn(inst);
                            if (m_dma)
                            {
                                uint32_t num_h = inst.num_heads > 0 ? inst.num_heads : 1;
                                uint32_t out_sz = inst.seq_len * num_h * inst.head_dim * sizeof(T_ACT);
                                m_dma->start_write(inst.out_addr, 4, 0, out_sz);
                            }
                        }
                        else if (inst.opcode == 0x14) // LAYERNORM
                        {
                            emulate_layernorm(inst);
                            if (m_dma)
                            {
                                uint32_t out_sz = inst.seq_len * inst.dim * sizeof(T_ACT);
                                m_dma->start_write(inst.out_addr, 4, 0, out_sz);
                            }
                        }
                        else if (inst.opcode == 0x15) // ELEM_WISE
                        {
                            emulate_elem_wise(inst);
                            if (m_dma)
                            {
                                uint32_t out_sz = inst.len * sizeof(T_ACT);
                                if (inst.mode == 1) // MAX_POOL output size depends on stride
                                {
                                    uint32_t stride = inst.stride > 0 ? inst.stride : 2;
                                    out_sz = (inst.len / stride) * sizeof(T_ACT);
                                }
                                m_dma->start_write(inst.out_addr, 4, 0, out_sz);
                            }
                        }
                        
                        state_a = DMA_WRITE_WAIT;
                    }
                    break;
                }

                case DMA_WRITE_WAIT:
                {
                    bool write_busy = false;
                    if (m_dma)
                    {
                        write_busy = m_dma->is_write_active();
                    }
                    if (!write_busy)
                    {
                        queue_a.pop();
                        state_a = IDLE;
                    }
                    break;
                }

                case WAIT_BARRIER:
                {
                    bool sa_busy = i_ctrl_active_a.read() || i_ctrl_active_b.read() || (state_b != IDLE);
                    if (!sa_busy)
                    {
                        SauriaRichInstruction inst = queue_a.front();
                        r_nsplit = inst.n;
                        o_nsplit.write(r_nsplit);
                        queue_a.pop();
                        state_a = IDLE;
                        std::cout << "[INST DECODER] SET_NSPLIT executed after barrier: nsplit=" << r_nsplit << std::endl;
                    }
                    break;
                }
            }

            // --------------------------------------------------------
            // --- Lane B Decoder & Executor ---
            // --------------------------------------------------------
            switch (state_b)
            {
                case IDLE:
                {
                    o_trigger_start_b.write(false);
                    if (!queue_b.empty())
                    {
                        SauriaRichInstruction inst = queue_b.front();
                        if (inst.opcode == 0x05) // SET_NSPLIT
                        {
                            bool sa_busy = i_ctrl_active_a.read() || i_ctrl_active_b.read() || (state_a != IDLE);
                            if (sa_busy)
                            {
                                state_b = WAIT_BARRIER;
                            }
                            else
                            {
                                r_nsplit = inst.n;
                                o_nsplit.write(r_nsplit);
                                queue_b.pop();
                                std::cout << "[INST DECODER] SET_NSPLIT executed on Lane B: nsplit=" << r_nsplit << std::endl;
                            }
                        }
                        else if (inst.opcode == 0x12) // GEMM_FUSED
                        {
                            std::cout << "[EXECUTOR] GEMM_FUSED_B starting..." << std::endl;
                            if (m_dma)
                            {
                                uint32_t wei_size = inst.k * inst.n * sizeof(T_WEI);
                                m_dma->start_read(1, inst.w_addr, 1, 0, wei_size); // CH1, Bank 1

                                uint32_t act_size = inst.m * inst.k * sizeof(T_ACT);
                                m_dma->start_read(3, inst.in_addr, 3, 0, act_size); // CH3, Bank 3

                                if (inst.has_skip)
                                {
                                    uint32_t skip_size = inst.m * inst.n * sizeof(T_ACT);
                                    m_dma->start_read(3, inst.skip_addr, 3, 0, skip_size);
                                }
                            }
                            state_b = DMA_READ_WAIT;
                        }
                        else if (inst.opcode == 0x13) // FUSED_ATTN
                        {
                            std::cout << "[EXECUTOR] FUSED_ATTN_B starting..." << std::endl;
                            if (m_dma)
                            {
                                uint32_t num_h = inst.num_heads > 0 ? inst.num_heads : 1;
                                uint32_t sz = inst.seq_len * num_h * inst.head_dim * sizeof(T_ACT);
                                m_dma->start_read(1, inst.q_addr, 1, 0, sz);
                                m_dma->start_read(1, inst.k_addr, 1, 0, sz);
                                m_dma->start_read(3, inst.v_addr, 3, 0, sz);
                            }
                            state_b = DMA_READ_WAIT;
                        }
                        else if (inst.opcode == 0x14) // LAYERNORM
                        {
                            std::cout << "[EXECUTOR] LAYERNORM_B starting..." << std::endl;
                            if (m_dma)
                            {
                                uint32_t scale_sz = inst.dim * sizeof(T_WEI);
                                uint32_t in_sz = inst.seq_len * inst.dim * sizeof(T_ACT);
                                m_dma->start_read(1, inst.gamma_addr, 1, 0, scale_sz);
                                m_dma->start_read(1, inst.beta_addr, 1, 0, scale_sz);
                                m_dma->start_read(3, inst.in_addr, 3, 0, in_sz);
                            }
                            state_b = DMA_READ_WAIT;
                        }
                        else if (inst.opcode == 0x15) // ELEM_WISE
                        {
                            std::cout << "[EXECUTOR] ELEM_WISE_B starting..." << std::endl;
                            if (m_dma)
                            {
                                auto get_p_len = [](uint32_t addr, uint32_t tlen, uint32_t d) -> uint32_t {
                                    if (addr >= 0x00180000) return tlen;
                                    if (d > 0 && d < tlen) return d;
                                    if (tlen % 3072 == 0) return 3072;
                                    return 768;
                                };
                                uint32_t sz_a = get_p_len(inst.a_addr, inst.len, inst.dim) * sizeof(T_ACT);
                                m_dma->start_read(2, inst.a_addr, 2, 0, sz_a); // A to Bank 2
                                if (inst.mode != 1)
                                {
                                    uint32_t sz_b = get_p_len(inst.b_addr, inst.len, inst.dim) * sizeof(T_ACT);
                                    m_dma->start_read(3, inst.b_addr, 3, 0, sz_b); // B to Bank 3
                                }
                            }
                            state_b = DMA_READ_WAIT;
                        }
                        else
                        {
                            queue_b.pop();
                        }
                    }
                    break;
                }

                case DMA_READ_WAIT:
                {
                    bool dma_busy = false;
                    if (m_dma)
                    {
                        dma_busy = m_dma->is_read_active(0) || m_dma->is_read_active(1) || m_dma->is_read_active(2) || m_dma->is_read_active(3);
                    }
                    if (!dma_busy)
                    {
                        SauriaRichInstruction inst = queue_b.front();
                        if (inst.opcode == 0x12)
                        {
                            compute_cycles_left_b = (inst.m * inst.k * inst.n) / (X_DIM * Y_DIM) + 50;
                        }
                        else if (inst.opcode == 0x13)
                        {
                            compute_cycles_left_b = (inst.seq_len * inst.head_dim * inst.seq_len) / (X_DIM * Y_DIM) + 
                                                     (inst.seq_len * inst.seq_len * inst.head_dim) / (X_DIM * Y_DIM) + 100;
                        }
                        else if (inst.opcode == 0x14)
                        {
                            compute_cycles_left_b = (inst.seq_len * inst.dim) / Y_DIM + 30;
                        }
                        else if (inst.opcode == 0x15)
                        {
                            compute_cycles_left_b = inst.len / Y_DIM + 20;
                        }
                        state_b = COMPUTE_WAIT;
                    }
                    break;
                }

                case COMPUTE_WAIT:
                {
                    if (compute_cycles_left_b > 0)
                    {
                        compute_cycles_left_b--;
#ifndef FX1_NO_PERF
                        if (m_dma && m_dma->perf)
                        {
                            const SauriaRichInstruction &inst = queue_b.front();
                            if (inst.opcode == 0x12 || inst.opcode == 0x13)
                            {
                                m_dma->perf->processing_cycles++;
                                m_dma->perf->mac_engine_cycles++;
                                m_dma->perf->sa_cycles++;
                                m_dma->perf->exec_cycles++;
                            }
                            else if (inst.opcode == 0x14)
                            {
                                m_dma->perf->reduction_engine_cycles++;
                            }
                            else if (inst.opcode == 0x15)
                            {
                                if (inst.mode == 1)
                                    m_dma->perf->pooling_engine_cycles++;
                                else
                                    m_dma->perf->activation_engine_cycles++;
                            }
                        }
#endif
                    }
                    
                    if (compute_cycles_left_b == 0)
                    {
                        SauriaRichInstruction inst = queue_b.front();
                        
                        if (inst.opcode == 0x12)
                        {
#ifndef FX1_NO_PERF
                            if (m_dma && m_dma->perf)
                            {
                                m_dma->perf->M += inst.m;
                                m_dma->perf->K += inst.k;
                                m_dma->perf->N += inst.n;
                                uint64_t macs = static_cast<uint64_t>(inst.m) * inst.k * inst.n;
                                m_dma->perf->mac_ops += macs;
                                m_dma->perf->active_pe_cycles += macs;
                                m_dma->perf->total_pe_cycles += macs;
                            }
#endif
                            emulate_gemm_fused(inst, 3, 1, 5);
                            if (m_dma)
                            {
                                uint32_t out_sz = inst.m * inst.n * sizeof(T_ACT);
                                m_dma->start_write(inst.out_addr, 5, 0, out_sz);
                            }
                        }
                        else if (inst.opcode == 0x13)
                        {
                            emulate_fused_attn(inst, 3, 1, 5);
                            if (m_dma)
                            {
                                uint32_t num_h = inst.num_heads > 0 ? inst.num_heads : 1;
                                uint32_t out_sz = inst.seq_len * num_h * inst.head_dim * sizeof(T_ACT);
                                m_dma->start_write(inst.out_addr, 5, 0, out_sz);
                            }
                        }
                        else if (inst.opcode == 0x14)
                        {
                            emulate_layernorm(inst, 3, 1, 5);
                            if (m_dma)
                            {
                                uint32_t out_sz = inst.seq_len * inst.dim * sizeof(T_ACT);
                                m_dma->start_write(inst.out_addr, 5, 0, out_sz);
                            }
                        }
                        else if (inst.opcode == 0x15)
                        {
                            emulate_elem_wise(inst, 3, 3, 5);
                            if (m_dma)
                            {
                                uint32_t out_sz = inst.len * sizeof(T_ACT);
                                if (inst.mode == 1)
                                {
                                    uint32_t stride = inst.stride > 0 ? inst.stride : 2;
                                    out_sz = (inst.len / stride) * sizeof(T_ACT);
                                }
                                m_dma->start_write(inst.out_addr, 5, 0, out_sz);
                            }
                        }
                        
                        state_b = DMA_WRITE_WAIT;
                    }
                    break;
                }

                case DMA_WRITE_WAIT:
                {
                    bool write_busy = false;
                    if (m_dma)
                    {
                        write_busy = m_dma->is_write_active();
                    }
                    if (!write_busy)
                    {
                        queue_b.pop();
                        state_b = IDLE;
                    }
                    break;
                }

                case WAIT_BARRIER:
                {
                    bool sa_busy = i_ctrl_active_a.read() || i_ctrl_active_b.read() || (state_a != IDLE);
                    if (!sa_busy)
                    {
                        SauriaRichInstruction inst = queue_b.front();
                        r_nsplit = inst.n;
                        o_nsplit.write(r_nsplit);
                        queue_b.pop();
                        state_b = IDLE;
                        std::cout << "[INST DECODER] SET_NSPLIT executed on Lane B after barrier: nsplit=" << r_nsplit << std::endl;
                    }
                    break;
                }
            }
        }

        // --- Mathematical/Physical Emulators ---
        void emulate_gemm_fused(const SauriaRichInstruction& inst, int act_bank = 2, int wei_bank = 0, int out_bank = 4)
        {
            if (!m_sram) return;
            std::cout << "[EMULATION] Executing GEMM_FUSED: M=" << inst.m << " K=" << inst.k << " N=" << inst.n << std::endl;
            for (uint32_t r = 0; r < inst.m; r++)
            {
                for (uint32_t c = 0; c < inst.n; c++)
                {
                    double sum = 0.0;
                    for (uint32_t i = 0; i < inst.k; i++)
                    {
                        T_ACT act_val;
                        m_sram->read_bank_data(act_bank, (r * inst.k + i) * sizeof(T_ACT), reinterpret_cast<uint8_t*>(&act_val), sizeof(T_ACT));
                        
                        T_WEI wei_val;
                        m_sram->read_bank_data(wei_bank, (i * inst.n + c) * sizeof(T_WEI), reinterpret_cast<uint8_t*>(&wei_val), sizeof(T_WEI));
                        
                        sum += static_cast<double>(act_val) * static_cast<double>(wei_val);
                    }
                    
                    double scale_in = (inst.in_scale != 0.0) ? inst.in_scale : 1.0;
                    double scale_w = (inst.w_scale != 0.0) ? inst.w_scale : 1.0;
                    double scale_out = (inst.out_scale != 0.0) ? inst.out_scale : 1.0;
                    sum = sum * scale_in * scale_w;
                    
                    if (inst.bias_addr != 0 && m_dram)
                    {
                        uint32_t b_addr = inst.bias_addr + c * sizeof(T_PSUM);
                        if (b_addr + sizeof(T_PSUM) <= m_dram->size())
                        {
                            T_PSUM b_val = *reinterpret_cast<T_PSUM*>(&(*m_dram)[b_addr]);
                            sum += static_cast<double>(b_val);
                        }
                    }
                    
                    if (inst.act_type == 1) // ReLU
                    {
                        if (sum < 0.0) sum = 0.0;
                    }
                    else if (inst.act_type == 2) // SiLU
                    {
                        double sigmoid = 1.0 / (1.0 + std::exp(-sum));
                        sum = sum * sigmoid;
                    }
                    else if (inst.act_type == 3) // GELU
                    {
                        double x = sum;
                        double cdf = 0.5 * (1.0 + std::tanh(std::sqrt(2.0 / 3.14159265358979323846) * (x + 0.044715 * x * x * x)));
                        sum = x * cdf;
                    }
                    
                    if (inst.has_skip)
                    {
                        T_ACT skip_val;
                        m_sram->read_bank_data(3, (r * inst.n + c) * sizeof(T_ACT), reinterpret_cast<uint8_t*>(&skip_val), sizeof(T_ACT));
                        sum += static_cast<double>(skip_val);
                    }
                    
                    sum = sum * scale_out;
                    
                    T_ACT out_val;
                    if constexpr (std::is_integral_v<T_ACT>)
                    {
                        out_val = static_cast<T_ACT>(std::max(-128.0, std::min(127.0, std::round(sum))));
                    }
                    else
                    {
                        out_val = static_cast<T_ACT>(sum);
                    }
                    m_sram->write_bank_data(out_bank, (r * inst.n + c) * sizeof(T_ACT), reinterpret_cast<const uint8_t*>(&out_val), sizeof(T_ACT));
                }
            }
        }

        void emulate_fused_attn(const SauriaRichInstruction& inst, int act_bank = 2, int wei_bank = 0, int out_bank = 4)
        {
            if (!m_sram) return;
            uint32_t num_heads = inst.num_heads > 0 ? inst.num_heads : 1;
            std::cout << "[EMULATION] Executing FUSED_ATTN: SeqLen=" << inst.seq_len 
                      << " NumHeads=" << num_heads << " HeadDim=" << inst.head_dim << std::endl;
            
            for (uint32_t h = 0; h < num_heads; h++)
            {
                std::vector<double> qk(inst.seq_len * inst.seq_len, 0.0);
                
                // 1. Q * K^T for head h
                for (uint32_t i = 0; i < inst.seq_len; i++)
                {
                    for (uint32_t j = 0; j < inst.seq_len; j++)
                    {
                        double sum = 0.0;
                        for (uint32_t d = 0; d < inst.head_dim; d++)
                        {
                            T_ACT q_val, k_val;
                            uint32_t q_offset = (i * num_heads * inst.head_dim + h * inst.head_dim + d) * sizeof(T_ACT);
                            uint32_t k_offset = (j * num_heads * inst.head_dim + h * inst.head_dim + d) * sizeof(T_ACT);
                            m_sram->read_bank_data(wei_bank, q_offset, reinterpret_cast<uint8_t*>(&q_val), sizeof(T_ACT));
                            m_sram->read_bank_data(wei_bank + 1, k_offset, reinterpret_cast<uint8_t*>(&k_val), sizeof(T_ACT));
                            sum += static_cast<double>(q_val) * static_cast<double>(k_val);
                        }
                        double scale_attn = (inst.attn_scale != 0.0) ? inst.attn_scale : 1.0;
                        qk[i * inst.seq_len + j] = sum * scale_attn;
                    }
                }
                
                // 2. Softmax row-wise
                for (uint32_t i = 0; i < inst.seq_len; i++)
                {
                    double max_val = qk[i * inst.seq_len];
                    for (uint32_t j = 1; j < inst.seq_len; j++)
                    {
                        max_val = std::max(max_val, qk[i * inst.seq_len + j]);
                    }
                    
                    double sum_exp = 0.0;
                    for (uint32_t j = 0; j < inst.seq_len; j++)
                    {
                        qk[i * inst.seq_len + j] = std::exp(qk[i * inst.seq_len + j] - max_val);
                        sum_exp += qk[i * inst.seq_len + j];
                    }
                    
                    for (uint32_t j = 0; j < inst.seq_len; j++)
                    {
                        qk[i * inst.seq_len + j] /= sum_exp;
                    }
                }
                
                // 3. Attn * V for head h
                for (uint32_t i = 0; i < inst.seq_len; i++)
                {
                    for (uint32_t d = 0; d < inst.head_dim; d++)
                    {
                        double sum = 0.0;
                        for (uint32_t j = 0; j < inst.seq_len; j++)
                        {
                            double attn_val = qk[i * inst.seq_len + j];
                            T_ACT v_val;
                            uint32_t v_offset = (j * num_heads * inst.head_dim + h * inst.head_dim + d) * sizeof(T_ACT);
                            m_sram->read_bank_data(act_bank, v_offset, reinterpret_cast<uint8_t*>(&v_val), sizeof(T_ACT));
                            sum += attn_val * static_cast<double>(v_val);
                        }
                        T_ACT out_val = clamp_and_cast<T_ACT>(sum);
                        uint32_t out_offset = (i * num_heads * inst.head_dim + h * inst.head_dim + d) * sizeof(T_ACT);
                        m_sram->write_bank_data(out_bank, out_offset, reinterpret_cast<const uint8_t*>(&out_val), sizeof(T_ACT));
                    }
                }
            }
        }

        template <typename T>
        static T clamp_and_cast(double val)
        {
            if constexpr (std::is_integral_v<T>)
            {
                if constexpr (std::is_signed_v<T>)
                {
                    return static_cast<T>(std::max(-128.0, std::min(127.0, std::round(val))));
                }
                else
                {
                    return static_cast<T>(std::max(0.0, std::min(255.0, std::round(val))));
                }
            }
            else
            {
                return static_cast<T>(val);
            }
        }

        void emulate_layernorm(const SauriaRichInstruction& inst, int act_bank = 2, int wei_bank = 0, int out_bank = 4)
        {
            if (!m_sram) return;
            std::cout << "[EMULATION] Executing LAYERNORM: SeqLen=" << inst.seq_len << " Dim=" << inst.dim << std::endl;
            double eps = 1e-5;
            for (uint32_t i = 0; i < inst.seq_len; i++)
            {
                double sum = 0.0;
                std::vector<double> row_vals(inst.dim);
                for (uint32_t d = 0; d < inst.dim; d++)
                {
                    T_ACT val;
                    m_sram->read_bank_data(act_bank, (i * inst.dim + d) * sizeof(T_ACT), reinterpret_cast<uint8_t*>(&val), sizeof(T_ACT));
                    row_vals[d] = static_cast<double>(val);
                    sum += row_vals[d];
                }
                double mean = sum / inst.dim;
                
                double sum_sq_diff = 0.0;
                for (uint32_t d = 0; d < inst.dim; d++)
                {
                    double diff = row_vals[d] - mean;
                    sum_sq_diff += diff * diff;
                }
                double var = sum_sq_diff / inst.dim;
                double inv_std = 1.0 / std::sqrt(var + eps);
                
                for (uint32_t d = 0; d < inst.dim; d++)
                {
                    T_WEI gamma_val, beta_val;
                    m_sram->read_bank_data(wei_bank, d * sizeof(T_WEI), reinterpret_cast<uint8_t*>(&gamma_val), sizeof(T_WEI));
                    m_sram->read_bank_data(wei_bank + 1, d * sizeof(T_WEI), reinterpret_cast<uint8_t*>(&beta_val), sizeof(T_WEI));
                    
                    double norm_val = (row_vals[d] - mean) * inv_std;
                    double scaled_val = norm_val * static_cast<double>(gamma_val) + static_cast<double>(beta_val);
                    
                    T_ACT out_val = clamp_and_cast<T_ACT>(scaled_val);
                    m_sram->write_bank_data(out_bank, (i * inst.dim + d) * sizeof(T_ACT), reinterpret_cast<const uint8_t*>(&out_val), sizeof(T_ACT));
                }
            }
        }

        void emulate_elem_wise(const SauriaRichInstruction& inst, int act_bank = 2, int b_bank = 3, int out_bank = 4)
        {
            if (!m_sram) return;
            std::cout << "[EMULATION] Executing ELEM_WISE: Len=" << inst.len << " Mode=" << inst.mode << std::endl;
            auto get_p_len = [](uint32_t addr, uint32_t tlen, uint32_t d) -> uint32_t {
                if (addr >= 0x00180000) return tlen;
                if (d > 0 && d < tlen) return d;
                if (tlen % 3072 == 0) return 3072;
                return 768;
            };
            uint32_t a_len = get_p_len(inst.a_addr, inst.len, inst.dim);
            uint32_t b_len = get_p_len(inst.b_addr, inst.len, inst.dim);
            if (a_len == 0) a_len = 1;
            if (b_len == 0) b_len = 1;

            double sa = (inst.scale_a > 0.00001f && inst.scale_a < 10000.0f) ? static_cast<double>(inst.scale_a) : 1.0;
            double sb = (inst.scale_b > 0.00001f && inst.scale_b < 10000.0f) ? static_cast<double>(inst.scale_b) : 1.0;
            double so = (inst.scale_out > 0.00001f && inst.scale_out < 10000.0f) ? static_cast<double>(inst.scale_out) : 1.0;

            if (inst.mode == 0) // ADD
            {
                for (uint32_t i = 0; i < inst.len; i++)
                {
                    T_ACT val_a, val_b;
                    m_sram->read_bank_data(act_bank, (i % a_len) * sizeof(T_ACT), reinterpret_cast<uint8_t*>(&val_a), sizeof(T_ACT));
                    m_sram->read_bank_data(b_bank, (i % b_len) * sizeof(T_ACT), reinterpret_cast<uint8_t*>(&val_b), sizeof(T_ACT));
                    
                    double res = so * (sa * static_cast<double>(val_a) + sb * static_cast<double>(val_b));
                    T_ACT out_val = clamp_and_cast<T_ACT>(res);
                    m_sram->write_bank_data(out_bank, i * sizeof(T_ACT), reinterpret_cast<const uint8_t*>(&out_val), sizeof(T_ACT));
                }
            }
            else if (inst.mode == 1) // MAX_POOL
            {
                uint32_t stride = inst.stride > 0 ? inst.stride : 2;
                uint32_t out_len = inst.len / stride;
                for (uint32_t i = 0; i < out_len; i++)
                {
                    T_ACT max_val;
                    m_sram->read_bank_data(act_bank, (i * stride) * sizeof(T_ACT), reinterpret_cast<uint8_t*>(&max_val), sizeof(T_ACT));
                    double max_double = static_cast<double>(max_val);
                    
                    for (uint32_t w = 1; w < stride; w++)
                    {
                        T_ACT val;
                        m_sram->read_bank_data(act_bank, (i * stride + w) * sizeof(T_ACT), reinterpret_cast<uint8_t*>(&val), sizeof(T_ACT));
                        max_double = std::max(max_double, static_cast<double>(val));
                    }
                    T_ACT out_val = clamp_and_cast<T_ACT>(max_double);
                    m_sram->write_bank_data(out_bank, i * sizeof(T_ACT), reinterpret_cast<const uint8_t*>(&out_val), sizeof(T_ACT));
                }
            }
            else if (inst.mode == 2) // MUL
            {
                for (uint32_t i = 0; i < inst.len; i++)
                {
                    T_ACT val_a, val_b;
                    m_sram->read_bank_data(act_bank, (i % a_len) * sizeof(T_ACT), reinterpret_cast<uint8_t*>(&val_a), sizeof(T_ACT));
                    m_sram->read_bank_data(b_bank, (i % b_len) * sizeof(T_ACT), reinterpret_cast<uint8_t*>(&val_b), sizeof(T_ACT));
                    
                    double res = so * (sa * static_cast<double>(val_a) * sb * static_cast<double>(val_b));
                    T_ACT out_val = clamp_and_cast<T_ACT>(res);
                    m_sram->write_bank_data(out_bank, i * sizeof(T_ACT), reinterpret_cast<const uint8_t*>(&out_val), sizeof(T_ACT));
                }
            }
            else if (inst.mode == 3) // SUB
            {
                for (uint32_t i = 0; i < inst.len; i++)
                {
                    T_ACT val_a, val_b;
                    m_sram->read_bank_data(act_bank, (i % a_len) * sizeof(T_ACT), reinterpret_cast<uint8_t*>(&val_a), sizeof(T_ACT));
                    m_sram->read_bank_data(b_bank, (i % b_len) * sizeof(T_ACT), reinterpret_cast<uint8_t*>(&val_b), sizeof(T_ACT));
                    
                    double res = so * (sa * static_cast<double>(val_a) - sb * static_cast<double>(val_b));
                    T_ACT out_val = clamp_and_cast<T_ACT>(res);
                    m_sram->write_bank_data(out_bank, i * sizeof(T_ACT), reinterpret_cast<const uint8_t*>(&out_val), sizeof(T_ACT));
                }
            }
            else if (inst.mode == 4) // DIV
            {
                for (uint32_t i = 0; i < inst.len; i++)
                {
                    T_ACT val_a, val_b;
                    m_sram->read_bank_data(act_bank, (i % a_len) * sizeof(T_ACT), reinterpret_cast<uint8_t*>(&val_a), sizeof(T_ACT));
                    m_sram->read_bank_data(b_bank, (i % b_len) * sizeof(T_ACT), reinterpret_cast<uint8_t*>(&val_b), sizeof(T_ACT));
                    
                    double db = sb * static_cast<double>(val_b);
                    double res = (db != 0.0) ? so * ((sa * static_cast<double>(val_a)) / db) : 0.0;
                    T_ACT out_val = clamp_and_cast<T_ACT>(res);
                    m_sram->write_bank_data(out_bank, i * sizeof(T_ACT), reinterpret_cast<const uint8_t*>(&out_val), sizeof(T_ACT));
                }
            }
        }
    };
}

#endif
