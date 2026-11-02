// tb_gvu_top_maxpool_vectors.cpp -- testbench of GVU top (gvu_top) on two fixed max pool datasets
// (target/test/maxpool/{tile,vec}), validating the Max Pooling Pipeline (POOLING_MAX) of MAS Section 5.2.6
// with the verification method of tb_gvu_top.cpp / tb_gvu_top.sv. Written by the owner of the vector unit;
// tools/has/tb_gvu_top_maxpool.cpp is the other max pool bench (generated cases, the SPPF case, the 9 HW golden sets).
//
// Runs cycle by cycle on GvuTop (C++17 path). The SystemC path is kept but disabled: ScGvuTop has no pooling ports yet.
//
// Build and run from the repository root:
//   g++ -std=c++17 -O2 -Wall -I. tools/has/tb_gvu_top_maxpool_vectors.cpp -o tb_gvu_top_maxpool_vectors
//   ./tb_gvu_top_maxpool_vectors [tile|vec]
//     tile : multi W-tile mode (C=3, H=80, W=80, K=5x5, stride=1 -> 3x76x76 = 17,328 elements, default)
//     vec  : single W-tile mode (C=40, H=20, W=20, K=3x3, stride=1 -> 40x18x18 = 12,960 elements)

#if 0 // C++ path only: ScGvuTop has no pooling ports yet
#include <systemc.h>
#define GVU_TB_USE_SYSTEMC 1
#elif defined(SYSTEMC_INCLUDED) || defined(SC_MODULE) || defined(HAVE_SYSTEMC)
#include <systemc.h>
#define GVU_TB_USE_SYSTEMC 1
#else
#define GVU_TB_USE_SYSTEMC 0
#endif

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include "has/rtl_ref_gvu_top.h"

// Hardware parameters matching tb_gvu_top.sv
static constexpr unsigned NUM_LANE      = 32;
static constexpr double   ClockCycle    = 10.0;          // 10 ns (100 MHz)
static constexpr double   ResetDuration = 50.0;          // 50 ns (5 cycles)
static constexpr unsigned MaxCycle      = 100000;
static constexpr unsigned MinCycle      = 10;
static constexpr rtl_ref_gvu::GvuOpcode GVU_OP = rtl_ref_gvu::GVU_OP_MAXPOOL;

// Base paths to test datasets
static const std::string BASE_DATA_DIR = "target/test/maxpool/";

// Helper to print FSM state names matching gvu_pkg.sv
static const char *fsm_state_name(uint32_t s)
{
    switch (static_cast<rtl_ref_gvu::control_fsm_t>(s))
    {
    case rtl_ref_gvu::IDLE:           return "IDLE";
    case rtl_ref_gvu::LUT_ACT_INIT:   return "LUT_ACT_INIT";
    case rtl_ref_gvu::LUT_EXP_INIT:   return "LUT_EXP_INIT";
    case rtl_ref_gvu::LUT_RECIP_INIT: return "LUT_RECIP_INIT";
    case rtl_ref_gvu::LUT_RSQRT_INIT: return "LUT_RSQRT_INIT";
    case rtl_ref_gvu::SOFTMAX_ST1:    return "SOFTMAX_ST1";
    case rtl_ref_gvu::SOFTMAX_ST2:    return "SOFTMAX_ST2";
    case rtl_ref_gvu::SOFTMAX_ST3:    return "SOFTMAX_ST3";
    case rtl_ref_gvu::LAYERNORM_ST1:  return "LAYERNORM_ST1";
    case rtl_ref_gvu::LAYERNORM_ST2:  return "LAYERNORM_ST2";
    case rtl_ref_gvu::LAYERNORM_ST3:  return "LAYERNORM_ST3";
    case rtl_ref_gvu::LAYERNORM_ST4:  return "LAYERNORM_ST4";
    case rtl_ref_gvu::LAYERNORM_ST5:  return "LAYERNORM_ST5";
    case rtl_ref_gvu::GEMM_FUSED_ST1: return "GEMM_FUSED_ST1";
    case rtl_ref_gvu::RESIDUAL_ST1:   return "RESIDUAL_ST1";
    case rtl_ref_gvu::RESIDUAL_ST2:   return "RESIDUAL_ST2";
    case rtl_ref_gvu::POOLING_MAX:    return "POOLING_MAX";
    default:                          return "UNKNOWN";
    }
}

// Structure to hold dataset vectors for Max Pooling (MAS Section 5.2.6 & tb_gvu_top.sv)
struct MaxPoolDataset
{
    std::string mode_str{"tile"};
    bool tile_mode{true};

    // Tensor & pooling configuration matching tb_gvu_top.sv
    unsigned N{1};
    unsigned C{3};
    unsigned H{80};
    unsigned W{80};
    unsigned K{5};
    unsigned stride{1};
    unsigned OH{76};
    unsigned OW{76};
    unsigned W_TILE{3};
    unsigned O_LANE_TILE{32};
    uint8_t  pooling_mode{3};       // 0=2x2, 1=3x3, 2=4x4, 3=5x5
    uint16_t pooling_c_move{240};   // W_TILE * H
    uint16_t pooling_h_move{3};     // W_TILE * stride

    std::vector<int8_t> input_mem;
    std::vector<int8_t> golden_output;

    bool load(bool is_tile)
    {
        tile_mode = is_tile;
        mode_str = is_tile ? "tile" : "vec";
        N = 1;
        H = is_tile ? 80 : 20;
        W = is_tile ? 80 : 20;
        K = is_tile ? 5 : 3;
        stride = 1;
        OH = (H - K) / stride + 1;
        OW = (W - K) / stride + 1;
        O_LANE_TILE = (stride == 1) ? NUM_LANE
                    : (K == stride) ? ((NUM_LANE - K) / stride + 1)
                                    : ((NUM_LANE - K) / stride + 2);
        W_TILE = H / NUM_LANE + (H % NUM_LANE != 0);
        C = (25 * 1024) / (H * W_TILE * NUM_LANE); // 3 for tile (80x80), 40 for vec (20x20)
        pooling_mode = static_cast<uint8_t>(K - 2);
        pooling_c_move = static_cast<uint16_t>(W_TILE * H);
        pooling_h_move = static_cast<uint16_t>(W_TILE * stride);

        const size_t in_elems = static_cast<size_t>(N) * C * H * W;
        const size_t out_elems = static_cast<size_t>(N) * C * OH * OW;
        input_mem.assign(in_elems, 0);
        golden_output.assign(out_elems, 0);

        const std::string dir = BASE_DATA_DIR + mode_str + "/";
        std::cout << "[tb_gvu_top_maxpool] Loading dataset from: " << dir << std::endl;

        // 1. Load input.txt (int8)
        {
            std::ifstream f(dir + "input.txt");
            if (!f.is_open())
            {
                std::cerr << "[tb_gvu_top_maxpool] ERROR: Could not open " << dir << "input.txt" << std::endl;
                return false;
            }
            int val;
            for (size_t i = 0; i < input_mem.size() && (f >> val); ++i)
                input_mem[i] = static_cast<int8_t>(val);
        }

        // 2. Load golden_output.txt (int8)
        {
            std::ifstream f(dir + "golden_output.txt");
            if (!f.is_open())
            {
                std::cerr << "[tb_gvu_top_maxpool] ERROR: Could not open " << dir << "golden_output.txt" << std::endl;
                return false;
            }
            int val;
            for (size_t i = 0; i < golden_output.size() && (f >> val); ++i)
                golden_output[i] = static_cast<int8_t>(val);
        }

        std::cout << "[tb_gvu_top_maxpool] Successfully loaded N=" << N << " C=" << C
                  << " H=" << H << " W=" << W << " (" << in_elems << " input elements) -> "
                  << "OH=" << OH << " OW=" << OW << " (" << out_elems << " output elements), "
                  << "K=" << K << "x" << K << " stride=" << stride << " W_TILE=" << W_TILE << "." << std::endl;
        return true;
    }

    // Preloads vbank0 buffer 0 matching tb_gvu_top.sv lines 247-260:
    // pooling_vbank0_addr = n*C*H*W_TILE + c*H*W_TILE + h*W_TILE + w_tile
    void preload_scratchpad(rtl_ref_gvu::GvuTop &dut) const
    {
        for (unsigned n = 0; n < N; ++n)
        {
            for (unsigned c = 0; c < C; ++c)
            {
                for (unsigned h = 0; h < H; ++h)
                {
                    for (unsigned wt = 0; wt < W_TILE; ++wt)
                    {
                        std::vector<uint8_t> row(NUM_LANE, 0);
                        for (unsigned l = 0; l < NUM_LANE && (wt * NUM_LANE + l) < W; ++l)
                        {
                            const unsigned w = wt * NUM_LANE + l;
                            const size_t inp_addr = ((static_cast<size_t>(n) * C + c) * H + h) * W + w;
                            row[l] = static_cast<uint8_t>(input_mem[inp_addr]);
                        }
                        const unsigned vbank0_addr = ((n * C + c) * H + h) * W_TILE + wt;
                        dut.scratchpad().sram(0, 0).poke(vbank0_addr, row);
                    }
                }
            }
        }
    }

    // Verifies vbank2 buffer 0 against golden_output matching tb_gvu_top.sv lines 395-410:
    // pooling_vbank2_addr = n*C*OH*W_TILE + c*OH*W_TILE + oh*W_TILE + w_tile
    unsigned verify_output(rtl_ref_gvu::GvuTop &dut) const
    {
        unsigned mismatches = 0;

        std::cout << "\n[tb_gvu_top_maxpool] ==================== VERIFICATION RESULTS ====================" << std::endl;
        std::cout << "[tb_gvu_top_maxpool] Comparing 100% of vbank2 (MaxPool int8 output) against golden reference..." << std::endl;

        for (unsigned n = 0; n < N; ++n)
        {
            for (unsigned c = 0; c < C; ++c)
            {
                for (unsigned oh = 0; oh < OH; ++oh)
                {
                    for (unsigned ow = 0; ow < OW; ++ow)
                    {
                        const unsigned w_tile = ow / O_LANE_TILE;
                        const unsigned w_idx  = ow % O_LANE_TILE;
                        const unsigned vbank2_addr = ((n * C + c) * OH + oh) * W_TILE + w_tile;
                        const size_t oup_addr = ((static_cast<size_t>(n) * C + c) * OH + oh) * OW + ow;

                        const int8_t exp = golden_output[oup_addr];
                        const auto word = dut.scratchpad().sram(2, 0).word(vbank2_addr >> 2);
                        const int8_t act = static_cast<int8_t>(word[(vbank2_addr & 3u) * NUM_LANE + w_idx]);

                        if (exp != act)
                        {
                            if (mismatches < 10)
                            {
                                std::cerr << "[MISMATCH] n: " << n
                                          << " c: " << std::setw(2) << c
                                          << " oh: " << std::setw(2) << oh
                                          << " ow: " << std::setw(2) << ow
                                          << " | Expected: " << std::setw(4) << static_cast<int>(exp)
                                          << " Received: " << std::setw(4) << static_cast<int>(act) << std::endl;
                            }
                            mismatches++;
                        }
                    }
                }
            }
        }

        // Print sample output comparison (first 8 output columns of row 0 across lanes 0..3)
        std::cout << "\n[tb_gvu_top_maxpool] Sample Output Comparison (Row oh=0..7 for Channels/Lanes 0..3):" << std::endl;
        std::cout << "   oh | Lane 0 (Exp/Act) | Lane 1 (Exp/Act) | Lane 2 (Exp/Act) | Lane 3 (Exp/Act)" << std::endl;
        std::cout << "  ----+------------------+------------------+------------------+------------------" << std::endl;
        for (unsigned oh = 0; oh < std::min(8u, OH); ++oh)
        {
            std::cout << "  " << std::setw(3) << oh << " | ";
            for (unsigned ow = 0; ow < std::min(4u, OW); ++ow)
            {
                const unsigned vbank2_addr = oh * W_TILE + (ow / O_LANE_TILE);
                const unsigned w_idx = ow % O_LANE_TILE;
                const int8_t exp = golden_output[oh * OW + ow];
                const auto word = dut.scratchpad().sram(2, 0).word(vbank2_addr >> 2);
                const int8_t act = static_cast<int8_t>(word[(vbank2_addr & 3u) * NUM_LANE + w_idx]);
                std::cout << std::setw(4) << static_cast<int>(exp) << " / " << std::setw(4) << static_cast<int>(act) << "   | ";
            }
            std::cout << std::endl;
        }

        const unsigned total_elements = N * C * OH * OW;
        std::cout << "\n[tb_gvu_top_maxpool] Total elements verified: " << total_elements << std::endl;
        std::cout << "[tb_gvu_top_maxpool] Total mismatches:        " << mismatches << " / " << total_elements << std::endl;
        std::cout << "[tb_gvu_top_maxpool] Accuracy pass rate:      " << std::fixed << std::setprecision(2)
                  << (100.0 * (total_elements - mismatches) / total_elements) << "%" << std::endl;

        return mismatches;
    }
};

static MaxPoolDataset g_dataset;

#if GVU_TB_USE_SYSTEMC

// ============================================================================================================
// SystemC Testbench Implementation
// ============================================================================================================

SC_MODULE(TbGvuTopMaxPool)
{
    sc_clock clk;
    sc_signal<bool> rstn;

    sc_signal<bool, SC_MANY_WRITERS> pipeline_en;
    sc_signal<uint32_t> gvu_op;

    sc_signal<bool> psum_valid;
    sc_signal<bool> psum_ready;
    sc_signal<uint32_t> psum_data;

    sc_signal<uint32_t> num_tile;
    sc_signal<uint32_t> token_len;
    sc_signal<int32_t> attn_z;
    sc_signal<int32_t> attn_scale;
    sc_signal<uint32_t> attn_shift;

    sc_signal<uint32_t> pooling_mode;
    sc_signal<uint32_t> pooling_stride;
    sc_signal<uint32_t> pooling_num_w_tile;
    sc_signal<uint32_t> pooling_c;
    sc_signal<uint32_t> pooling_oh;
    sc_signal<uint32_t> pooling_c_move;
    sc_signal<uint32_t> pooling_h_move;

    sc_signal<bool> softmax_lut_init_done;
    sc_signal<bool> softmax_pipe_done;
    sc_signal<bool> maxpool_pipe_done;
    sc_signal<uint32_t> gvu_pipe_ctrl_state;

    sc_signal<bool> sbank_buf_sel;
    sc_signal<bool> vbank0_buf_sel;
    sc_signal<bool> vbank1_buf_sel;
    sc_signal<bool> vbank2_buf_sel;

    sc_signal<bool> scratch_dma_en;
    sc_signal<bool> scratch_dma_wen;
    sc_signal<uint32_t> scratch_dma_addr;

    rtl_ref_gvu::ScGvuTop dut;

    uint64_t cycle_count{0};
    bool done_seen{false};
    bool timed_out{false};
    int exit_code{0};

    SC_CTOR(TbGvuTopMaxPool)
        : clk("clk", ClockCycle, SC_NS, 0.5, 0.0, SC_NS, true),
          rstn("rstn"),
          pipeline_en("pipeline_en"),
          gvu_op("gvu_op"),
          psum_valid("psum_valid"),
          psum_ready("psum_ready"),
          psum_data("psum_data"),
          num_tile("num_tile"),
          token_len("token_len"),
          attn_z("attn_z"),
          attn_scale("attn_scale"),
          attn_shift("attn_shift"),
          pooling_mode("pooling_mode"),
          pooling_stride("pooling_stride"),
          pooling_num_w_tile("pooling_num_w_tile"),
          pooling_c("pooling_c"),
          pooling_oh("pooling_oh"),
          pooling_c_move("pooling_c_move"),
          pooling_h_move("pooling_h_move"),
          softmax_lut_init_done("softmax_lut_init_done"),
          softmax_pipe_done("softmax_pipe_done"),
          maxpool_pipe_done("maxpool_pipe_done"),
          gvu_pipe_ctrl_state("gvu_pipe_ctrl_state"),
          sbank_buf_sel("sbank_buf_sel"),
          vbank0_buf_sel("vbank0_buf_sel"),
          vbank1_buf_sel("vbank1_buf_sel"),
          vbank2_buf_sel("vbank2_buf_sel"),
          scratch_dma_en("scratch_dma_en"),
          scratch_dma_wen("scratch_dma_wen"),
          scratch_dma_addr("scratch_dma_addr"),
          dut("i_dut")
    {
        dut.i_clk(clk);
        dut.i_rstn(rstn);
        dut.i_pipeline_en(pipeline_en);
        dut.i_gvu_op(gvu_op);
        dut.i_psum_valid(psum_valid);
        dut.o_psum_ready(psum_ready);
        dut.i_psum_data(psum_data);
        dut.i_num_tile(num_tile);
        dut.i_token_len(token_len);
        dut.i_attn_z(attn_z);
        dut.i_attn_scale(attn_scale);
        dut.i_attn_shift(attn_shift);
        dut.i_pooling_mode(pooling_mode);
        dut.i_pooling_stride(pooling_stride);
        dut.i_pooling_num_w_tile(pooling_num_w_tile);
        dut.i_pooling_c(pooling_c);
        dut.i_pooling_oh(pooling_oh);
        dut.i_pooling_c_move(pooling_c_move);
        dut.i_pooling_h_move(pooling_h_move);
        dut.o_softmax_lut_init_done(softmax_lut_init_done);
        dut.o_softmax_pipe_done(softmax_pipe_done);
        dut.o_maxpool_pipe_done(maxpool_pipe_done);
        dut.o_gvu_pipe_ctrl_state(gvu_pipe_ctrl_state);
        dut.i_sbank_buf_sel(sbank_buf_sel);
        dut.i_vbank0_buf_sel(vbank0_buf_sel);
        dut.i_vbank1_buf_sel(vbank1_buf_sel);
        dut.i_vbank2_buf_sel(vbank2_buf_sel);
        dut.i_scratch_dma_en(scratch_dma_en);
        dut.i_scratch_dma_wen(scratch_dma_wen);
        dut.i_scratch_dma_addr(scratch_dma_addr);

        g_dataset.preload_scratchpad(dut.core());

        SC_THREAD(reset_proc);
        SC_THREAD(stimulus_proc);
        SC_THREAD(monitor_proc);
        SC_THREAD(timeout_proc);
        SC_METHOD(cycle_proc);
        sensitive << clk.posedge_event();
    }

    void reset_proc()
    {
        rstn.write(false);
        wait(ResetDuration, SC_NS);
        rstn.write(true);
    }

    void stimulus_proc()
    {
        pipeline_en.write(false);
        gvu_op.write(static_cast<uint32_t>(GVU_OP));
        psum_valid.write(false);
        psum_data.write(0);
        num_tile.write(1);
        token_len.write(64);
        attn_z.write(0);
        attn_scale.write(1);
        attn_shift.write(0);
        pooling_mode.write(g_dataset.pooling_mode);
        pooling_stride.write(g_dataset.stride);
        pooling_num_w_tile.write(g_dataset.W_TILE);
        pooling_c.write(g_dataset.N * g_dataset.C);
        pooling_oh.write(g_dataset.OH);
        pooling_c_move.write(g_dataset.pooling_c_move);
        pooling_h_move.write(g_dataset.pooling_h_move);
        sbank_buf_sel.write(false);
        vbank0_buf_sel.write(false);
        vbank1_buf_sel.write(false);
        vbank2_buf_sel.write(false);
        scratch_dma_en.write(false);
        scratch_dma_wen.write(false);
        scratch_dma_addr.write(0);

        wait(clk.posedge_event());
        while (!rstn.read())
            wait(clk.posedge_event());
        wait(clk.posedge_event());

        pipeline_en.write(true);

        std::cout << "[tb_gvu_top_maxpool] Stimulus enabled at " << sc_time_stamp()
                  << ": mode=" << g_dataset.mode_str << " K=" << g_dataset.K
                  << " stride=" << g_dataset.stride << " C=" << g_dataset.C
                  << " OH=" << g_dataset.OH << " W_TILE=" << g_dataset.W_TILE << std::endl;
    }

    void monitor_proc()
    {
        std::cout << "Seed: " << 0 << std::endl;

        for (unsigned i = 0; i < MinCycle; ++i)
            wait(clk.posedge_event());

        while (!maxpool_pipe_done.read())
            wait(clk.posedge_event());

        done_seen = true;
        pipeline_en.write(false);
        std::cout << "\n[tb_gvu_top_maxpool] o_maxpool_pipe_done asserted at " << sc_time_stamp()
                  << " (cycle " << cycle_count << ")" << std::endl;

        wait(100, SC_NS);
        std::cout << "Simulation end!" << std::endl;

        unsigned mismatches = g_dataset.verify_output(dut.core());
        exit_code = (mismatches == 0) ? 0 : 1;

        wait(50, SC_NS);
        sc_stop();
    }

    void timeout_proc()
    {
        for (unsigned i = 0; i < MaxCycle; ++i)
            wait(clk.posedge_event());

        if (!done_seen)
        {
            std::cout << "[FATAL] Simulation timeout!" << std::endl;
            timed_out = true;
            exit_code = 1;
            sc_stop();
        }
    }

    void cycle_proc()
    {
        if (rstn.read())
        {
            cycle_count++;
            static uint32_t last_state = 0xFFFFFFFFu;
            uint32_t cur_state = gvu_pipe_ctrl_state.read();
            if (cur_state != last_state)
            {
                std::cout << "[tb_gvu_top_maxpool] Cycle " << std::setw(5) << cycle_count
                          << " (" << std::setw(8) << sc_time_stamp() << ") State: "
                          << std::setw(15) << std::left << fsm_state_name(cur_state)
                          << std::endl;
                last_state = cur_state;
            }
        }
    }
};

int sc_main(int argc, char *argv[])
{
    bool is_tile = true;
    if (argc > 1 && (std::string(argv[1]) == "vec" || std::string(argv[1]) == "single"))
        is_tile = false;

    if (!g_dataset.load(is_tile))
        return 1;

    TbGvuTopMaxPool tb("tb_gvu_top_maxpool");
    sc_start();

    std::cout << "\n[tb_gvu_top_maxpool] RESULT: " << (tb.exit_code == 0 ? "PASS" : "FAIL") << std::endl;
    return tb.exit_code;
}

#else

// ============================================================================================================
// Standalone C++17 Cycle Testbench Implementation (compiled when SystemC is absent)
// ============================================================================================================

int main(int argc, char *argv[])
{
    std::cout << "[tb_gvu_top_maxpool] Running in standalone C++17 cycle-accurate mode (MAXPOOL)." << std::endl;
    std::cout << "Seed: " << 0 << std::endl;

    bool is_tile = true;
    if (argc > 1 && (std::string(argv[1]) == "vec" || std::string(argv[1]) == "single"))
        is_tile = false;

    if (!g_dataset.load(is_tile))
        return 1;

    rtl_ref_gvu::GvuTop dut;
    g_dataset.preload_scratchpad(dut);

    rtl_ref_gvu::GvuTop::In in;

    // Reset phase: 5 cycles active-low (50 ns at 10 ns clock)
    for (unsigned c = 0; c < 5; ++c)
    {
        dut.tick(in, false);
    }

    // One idle cycle after reset deasserts
    dut.tick(in, true);

    // Apply configuration matching MAS Section 5.2.6 & tb_gvu_top.sv
    in.i_pooling_mode       = g_dataset.pooling_mode;
    in.i_pooling_stride     = g_dataset.stride;
    in.i_pooling_num_w_tile = g_dataset.W_TILE;
    in.i_pooling_c          = g_dataset.N * g_dataset.C;
    in.i_pooling_oh         = g_dataset.OH;
    in.i_pooling_c_move     = g_dataset.pooling_c_move;
    in.i_pooling_h_move     = g_dataset.pooling_h_move;

    in.i_sbank_buf_sel  = false;
    in.i_vbank0_buf_sel = false;
    in.i_vbank1_buf_sel = false;
    in.i_vbank2_buf_sel = false;
    in.i_gvu_op         = GVU_OP;
    in.i_pipeline_en    = true;

    std::cout << "[tb_gvu_top_maxpool] Stimulus enabled at cycle 6 (60 ns): mode=" << g_dataset.mode_str
              << " K=" << g_dataset.K << "x" << g_dataset.K
              << " stride=" << g_dataset.stride
              << " C=" << g_dataset.C << " H=" << g_dataset.H << " W=" << g_dataset.W
              << " OH=" << g_dataset.OH << " OW=" << g_dataset.OW
              << " W_TILE=" << g_dataset.W_TILE
              << " c_move=" << g_dataset.pooling_c_move
              << " h_move=" << g_dataset.pooling_h_move << std::endl;

    bool done_seen = false;
    uint32_t last_state = 0xFFFFFFFFu;
    uint64_t post_done_cycles = 0;
    uint64_t state_start_cycle = 7;

    for (uint64_t cycle = 7; cycle <= MaxCycle; ++cycle)
    {
        const rtl_ref_gvu::GvuTop::Out out = dut.tick(in, true);
        uint32_t cur_state = static_cast<uint32_t>(out.o_gvu_pipe_ctrl_state);

        if (cur_state != last_state)
        {
            if (last_state != 0xFFFFFFFFu)
            {
                std::cout << "  --> Stage " << fsm_state_name(last_state)
                          << " completed after " << (cycle - state_start_cycle) << " cycles ("
                          << (cycle - state_start_cycle) * 10 << " ns)" << std::endl;
            }

            std::cout << "[tb_gvu_top_maxpool] Cycle " << std::setw(5) << cycle
                      << " (" << std::setw(6) << cycle * 10 << " ns) State: "
                      << std::setw(15) << std::left << fsm_state_name(cur_state)
                      << std::endl;
            last_state = cur_state;
            state_start_cycle = cycle;
        }

        if (out.o_maxpool_pipe_done && !done_seen)
        {
            done_seen = true;
            in.i_pipeline_en = false;
            std::cout << "\n[tb_gvu_top_maxpool] o_maxpool_pipe_done asserted at " << cycle * 10
                      << " ns (cycle " << cycle << ")" << std::endl;
        }

        if (done_seen)
        {
            post_done_cycles++;
            if (post_done_cycles >= 15) // #100 + #50 ns = 15 cycles
            {
                std::cout << "Simulation end!" << std::endl;

                unsigned mismatches = g_dataset.verify_output(dut);
                if (mismatches == 0)
                {
                    std::cout << "\n[tb_gvu_top_maxpool] RESULT: PASS" << std::endl;
                    return 0;
                }
                else
                {
                    std::cout << "\n[tb_gvu_top_maxpool] RESULT: FAIL" << std::endl;
                    return 1;
                }
            }
        }
    }

    std::cout << "[FATAL] Simulation timeout!" << std::endl;
    std::cout << "[tb_gvu_top_maxpool] RESULT: FAIL" << std::endl;
    return 1;
}

#endif
