// Copyright 2026 Barcelona Supercomputing Center (BSC)
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
// SystemC Testbench to Verify 5 Distinct NPU PE Utilization Optimization Strategies
//

#include "npu_top.h"
#include "npu_profile.h"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>

using namespace sauria;

// Global pointer for FSM tracking
sauria::NpuTop<32, 32, int8_t, int8_t, int32_t, 2048, 2048, 4096, 16, 5, 1>* g_npu_ptr = nullptr;

struct Config {
  int K = 64;
  float threshold = 0.0f;
  int select = 0;
  int act_incntlim = 32;
  int act_incntstep = 32;
  int act_outcntlim = 32;
  int act_outcntstep = 32;
  int wei_incntlim = 2048;
  int wei_incntstep = 32;
  int cxlim = 64;
  int cxstep = 32;
  int cklim = 1024;
  int ckstep = 32;
  int incntlim = 63;
  int act_reps = 1;
  int wei_reps = 1;
  int dil_pat = 1;
  unsigned int rows_active = 0xFFFFFFFF;
  unsigned int cols_active = 0xFFFFFFFF;
};

std::vector<std::vector<float>> load_matrix(const std::string &filepath, int expected_rows, int expected_cols) {
  std::vector<std::vector<float>> mat;
  std::ifstream infile(filepath);
  if (!infile.is_open()) {
    std::cerr << "[ERROR] Could not open matrix file: " << filepath << std::endl;
    return mat;
  }
  std::string line;
  while (std::getline(infile, line)) {
    if (line.empty())
      continue;
    std::istringstream iss(line);
    std::vector<float> row;
    float val;
    while (iss >> val) {
      row.push_back(val);
    }
    if (!row.empty()) {
      mat.push_back(row);
    }
  }
  if (mat.size() != (size_t)expected_rows) {
    std::cerr << "[WARNING] Matrix row count mismatch: " << filepath << " has "
              << mat.size() << " rows, expected " << expected_rows << std::endl;
  }
  return mat;
}

class TestbenchUtilization : public sc_module {
public:
  sc_in<bool> i_clk{"i_clk"};
  sc_out<bool> o_rstn{"o_rstn"};
  sc_out<bool> o_soft_reset{"o_soft_reset"};

  sc_out<bool> o_start{"o_start"};
  sc_in<bool> i_done{"i_done"};
  sc_in<bool> i_deadlock{"i_deadlock"};

  sc_out<uint32_t> o_mvm_k{"o_mvm_k"};
  sc_out<uint32_t> o_total_contexts{"o_total_contexts"};

  sc_out<uint32_t> o_host_addr{"o_host_addr"};
  sc_out<bool> o_host_wren{"o_host_wren"};
  sc_out<bool> o_host_rden{"o_host_rden"};
  sc_out<host_data_t> o_host_wdata{"o_host_wdata"};
  sc_out<host_mask_t> o_host_wmask{"o_host_wmask"};
  sc_in<host_data_t> i_host_rdata{"i_host_rdata"};

  sc_out<float> o_threshold{"o_threshold"};
  sc_out<sc_bv<3>> o_select{"o_select"};

  bool tile_done_latched{false};
  uint64_t tile_done_cycle{0};

  std::map<std::string, uint64_t> state_cycles_total;
  bool tracking_active{false};

  void done_monitor() {
    while (true) {
      wait();
      if (i_done.read()) {
        tile_done_latched = true;
        tile_done_cycle = (uint64_t)(sc_time_stamp() / sc_time(2, SC_NS));
      }
    }
  }

  void track_fsm_process() {
    while (true) {
      wait();
      if (tracking_active && g_npu_ptr != nullptr && g_npu_ptr->ctrl_inst_a != nullptr) {
        auto current_state = g_npu_ptr->ctrl_inst_a->get_state();
        if (current_state != sauria::Control<32, 32, 5, 1>::IDLE) {
          std::string name = g_npu_ptr->ctrl_inst_a->state_name(current_state);
          state_cycles_total[name]++;
        }
      }
    }
  }

  void print_state_cycles(const std::string &case_name) {
    std::cout << "\n[FSM State Cycle Breakdown - " << case_name << "]" << std::endl;
    uint64_t total_fsm_cycles = 0;
    for (auto const& x : state_cycles_total) {
      std::cout << "  * " << x.first << ": " << x.second << " cycles" << std::endl;
      total_fsm_cycles += x.second;
    }
    std::cout << "  * Total Active FSM Cycles: " << total_fsm_cycles << " cycles" << std::endl;
  }

  SC_CTOR(TestbenchUtilization) {
    SC_THREAD(test_process);
    sensitive << i_clk.pos();

    SC_THREAD(done_monitor);
    sensitive << i_clk.pos();

    SC_THREAD(track_fsm_process);
    sensitive << i_clk.pos();
  }

private:
  static const int X_DIM = 32;
  static const int Y_DIM = 32;

  const int subwords_a = Y_DIM / 4;
  const int mask_a = subwords_a - 1;
  const int shift_a = (subwords_a == 8) ? 3 : ((subwords_a == 4) ? 2 : ((subwords_a == 2) ? 1 : 0));

  const int subwords_b = X_DIM / 4;
  const int mask_b = subwords_b - 1;
  const int shift_b = (subwords_b == 8) ? 3 : ((subwords_b == 4) ? 2 : ((subwords_b == 2) ? 1 : 0));

  const int subwords_c = Y_DIM / 4;
  const int mask_c = subwords_c - 1;
  const int shift_c = (subwords_c == 8) ? 3 : ((subwords_c == 4) ? 2 : ((subwords_c == 2) ? 1 : 0));

  uint32_t get_srama_addr(uint32_t phys_addr, uint32_t sub_word) {
    return SRAMA_OFFSET | ((phys_addr << shift_a) | (sub_word & mask_a));
  }

  uint32_t get_sramb_addr(uint32_t phys_addr, uint32_t sub_word) {
    return SRAMB_OFFSET | ((phys_addr << shift_b) | (sub_word & mask_b));
  }

  uint32_t get_sramc_addr(uint32_t phys_addr, uint32_t sub_word) {
    return SRAMC_OFFSET | ((phys_addr << shift_c) | (sub_word & mask_c));
  }

  void write_host_mem(uint32_t addr, const host_data_t &data, const host_mask_t &mask) {
    wait();
    o_host_addr.write(addr);
    o_host_wdata.write(data);
    o_host_wmask.write(mask);
    o_host_wren.write(true);
    o_host_rden.write(false);
    wait();
    o_host_wren.write(false);
    wait();
  }

  host_data_t read_host_mem(uint32_t addr) {
    wait();
    o_host_addr.write(addr);
    o_host_wren.write(false);
    o_host_rden.write(true);
    wait();
    wait();
    o_host_rden.write(false);
    wait();
    return i_host_rdata.read();
  }

  void reset_system() {
    o_rstn.write(false);
    o_soft_reset.write(false);
    o_start.write(false);
    o_mvm_k.write(0);
    o_total_contexts.write(0);
    o_host_addr.write(0);
    o_host_wren.write(false);
    o_host_rden.write(false);
    o_host_wdata.write(host_data_t());
    o_host_wmask.write(host_mask_t());
    o_threshold.write(0.0f);
    o_select.write(sc_bv<3>("000"));
    wait(3);
    o_rstn.write(true);
    wait(2);
  }

  void program_config(const Config &cfg) {
    host_mask_t full_mask;
    full_mask.data.fill(true);

    // Set profile to V4 LINEAR first to match v4 model configuration layout
    host_data_t prof_data;
    prof_data.data.fill(0.0f);
    prof_data[0] = (float)PROFILE_V4_LINEAR;
    write_host_mem(CFG_PROFILE_ADDR, prof_data, full_mask);

    // incntlim = K + X_DIM
    host_data_t con_data;
    con_data[0] = (float)(cfg.K + X_DIM);
    write_host_mem(CFG_REGS_OFFSET | (CFG_CON_OFFSET + 0x00), con_data, full_mask);

    // act_reps
    host_data_t act_reps_data;
    act_reps_data[0] = (float)cfg.act_reps;
    write_host_mem(CFG_REGS_OFFSET | (CFG_CON_OFFSET + 0x04), act_reps_data, full_mask);

    // wei_reps
    host_data_t wei_reps_data;
    wei_reps_data[0] = (float)cfg.wei_reps;
    write_host_mem(CFG_REGS_OFFSET | (CFG_CON_OFFSET + 0x08), wei_reps_data, full_mask);

    // ncontexts
    host_data_t ncontexts_data;
    ncontexts_data[0] = (float)(cfg.act_reps);
    write_host_mem(CFG_REGS_OFFSET | (CFG_CON_OFFSET + 0x0C), ncontexts_data, full_mask);

    // rows_active
    host_data_t rows_active_data;
    rows_active_data[0] = (float)(cfg.rows_active & 0xFF);
    rows_active_data[1] = (float)((cfg.rows_active >> 8) & 0xFF);
    rows_active_data[2] = (float)((cfg.rows_active >> 16) & 0xFF);
    rows_active_data[3] = (float)((cfg.rows_active >> 24) & 0xFF);
    write_host_mem(CFG_REGS_OFFSET | (CFG_ACT_OFFSET + 0x00), rows_active_data, full_mask);

    // dil_pat
    host_data_t dil_pat_data;
    dil_pat_data[0] = (float)cfg.dil_pat;
    write_host_mem(CFG_REGS_OFFSET | (CFG_ACT_OFFSET + 0x28), dil_pat_data, full_mask);
  }

  int run_npu_core() {
    wait();
    o_start.write(true);
    wait();
    tile_done_latched = false;
    o_start.write(false);

    int timeout = 500000;
    while (timeout-- > 0) {
      if (tile_done_latched) {
        break;
      }
      wait();
    }
    return 0;
  }

  void test_process() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "       SAURIA NPU UTILIZATION OPTIMIZATION TESTBENCH       " << std::endl;
    std::cout << "==========================================================" << std::endl;

    host_mask_t full_mask;
    full_mask.data.fill(true);

    Config cfg_base;
    cfg_base.K = 64;

    // ----------------------------------------------------
    // CASE 1: Baseline Single-Tile (K=64)
    // ----------------------------------------------------
    std::cout << "\n>>> CASE 1: Baseline Single-Tile (K=64) <<<" << std::endl;
    auto mat_A64 = load_matrix("tb_data_utilization/A_c1.txt", Y_DIM, 64);
    auto mat_B64 = load_matrix("tb_data_utilization/B_c1.txt", 64 + X_DIM, X_DIM);

    reset_system();
    program_config(cfg_base);

    for (int k = 0; k < 64; k++) {
      for (int sw = 0; sw < subwords_a; sw++) {
        host_data_t wdata;
        for (int i = 0; i < 4; i++) wdata[i] = mat_A64[sw * 4 + i][k];
        write_host_mem(get_srama_addr(k, sw), wdata, full_mask);
      }
    }
    for (int addr_idx = 0; addr_idx < 64 + X_DIM; addr_idx++) {
      for (int sw = 0; sw < subwords_b; sw++) {
        host_data_t wdata;
        for (int i = 0; i < 4; i++) wdata[i] = mat_B64[addr_idx][sw * 4 + i];
        write_host_mem(get_sramb_addr(addr_idx, sw), wdata, full_mask);
      }
    }

    o_select.write(sc_bv<3>("111"));
    wait(2);

    state_cycles_total.clear();
    tracking_active = true;

    uint64_t start_cycles_c1 = (uint64_t)(sc_time_stamp() / sc_time(2, SC_NS));
    run_npu_core();
    uint64_t end_cycles_c1 = tile_done_cycle;

    tracking_active = false;
    int cycles_case1 = end_cycles_c1 - start_cycles_c1;

    float util_case1 = (64.0f / cycles_case1) * 100.0f;
    float flops_case1 = 2.0f * Y_DIM * X_DIM * 64;
    float gops_case1 = flops_case1 / (cycles_case1 * 2.0f);

    std::cout << "  * Simulation Cycles: " << cycles_case1 << std::endl;
    std::cout << "  * PE Active Utilization: " << util_case1 << "%" << std::endl;
    std::cout << "  * Performance: " << gops_case1 << " GOPS" << std::endl;
    print_state_cycles("Case 1: Baseline Single-Tile");

    // ----------------------------------------------------
    // CASE 2: Double-Buffered Serial Multi-Tiling (4 Tiles, K=64)
    // ----------------------------------------------------
    std::cout << "\n>>> CASE 2: Double-Buffered Serial Multi-Tiling (4 Tiles, K=64) <<<" << std::endl;
    std::vector<std::vector<std::vector<float>>> mat_A_c2(4);
    std::vector<std::vector<std::vector<float>>> mat_B_c2(4);
    for (int t = 0; t < 4; t++) {
      mat_A_c2[t] = load_matrix("tb_data_utilization/A_c2_t" + std::to_string(t) + ".txt", Y_DIM, 64);
      mat_B_c2[t] = load_matrix("tb_data_utilization/B_c2_t" + std::to_string(t) + ".txt", 64 + X_DIM, X_DIM);
    }

    reset_system();
    program_config(cfg_base);

    // Host programs Tile 0 to bank 0
    uint64_t t_write_start = (uint64_t)(sc_time_stamp() / sc_time(2, SC_NS));
    for (int k = 0; k < 64; k++) {
      for (int sw = 0; sw < subwords_a; sw++) {
        host_data_t wdata;
        for (int i = 0; i < 4; i++) wdata[i] = mat_A_c2[0][sw * 4 + i][k];
        write_host_mem(get_srama_addr(k, sw), wdata, full_mask);
      }
    }
    for (int addr_idx = 0; addr_idx < 64 + X_DIM; addr_idx++) {
      for (int sw = 0; sw < subwords_b; sw++) {
        host_data_t wdata;
        for (int i = 0; i < 4; i++) wdata[i] = mat_B_c2[0][addr_idx][sw * 4 + i];
        write_host_mem(get_sramb_addr(addr_idx, sw), wdata, full_mask);
      }
    }
    uint64_t t_write_end = (uint64_t)(sc_time_stamp() / sc_time(2, SC_NS));
    int T_write = t_write_end - t_write_start;

    // We run 4 tiles in a double-buffered pipeline
    std::vector<int> cycles_tiles(4, 0);
    uint64_t overlapped_wall_clock = T_write;

    state_cycles_total.clear();
    tracking_active = true;

    for (int t = 0; t < 4; t++) {
      // Swap buffers:
      // Even tiles: select = "111" (NPU reads bank 0, Host write bank 1)
      // Odd tiles: select = "000" (NPU reads bank 1, Host write bank 0)
      if (t % 2 == 0) o_select.write(sc_bv<3>("111"));
      else o_select.write(sc_bv<3>("000"));
      wait(2);

      uint64_t start_tile = (uint64_t)(sc_time_stamp() / sc_time(2, SC_NS));
      o_start.write(true);
      wait();
      tile_done_latched = false;
      o_start.write(false);

      // OVERLAP: Host writes Tile t+1
      if (t < 3) {
        for (int k = 0; k < 64; k++) {
          for (int sw = 0; sw < subwords_a; sw++) {
            host_data_t wdata;
            for (int i = 0; i < 4; i++) wdata[i] = mat_A_c2[t+1][sw * 4 + i][k];
            write_host_mem(get_srama_addr(k, sw), wdata, full_mask);
          }
        }
        for (int addr_idx = 0; addr_idx < 64 + X_DIM; addr_idx++) {
          for (int sw = 0; sw < subwords_b; sw++) {
            host_data_t wdata;
            for (int i = 0; i < 4; i++) wdata[i] = mat_B_c2[t+1][addr_idx][sw * 4 + i];
            write_host_mem(get_sramb_addr(addr_idx, sw), wdata, full_mask);
          }
        }
      }

      // OVERLAP: Host reads Tile t-1 outputs
      if (t > 0) {
        std::vector<std::vector<float>> read_C(Y_DIM, std::vector<float>(X_DIM, 0.0f));
        for (int x = 0; x < X_DIM; x++) {
          for (int sw = 0; sw < subwords_c; sw++) {
            host_data_t chunk = read_host_mem(get_sramc_addr(x, sw));
            for (int i = 0; i < 4; i++) read_C[sw * 4 + i][x] = chunk[i];
          }
        }
      }

      // Wait for NPU Tile t Done
      int timeout = 500000;
      while (timeout-- > 0) {
        if (tile_done_latched) break;
        wait();
      }
      uint64_t done_tile = tile_done_cycle;
      cycles_tiles[t] = done_tile - start_tile;

      o_soft_reset.write(true);
      wait(2);
      o_soft_reset.write(false);
      wait(2);
      program_config(cfg_base);
    }
    tracking_active = false;

    // Read back final tile outputs
    uint64_t t_read_start = (uint64_t)(sc_time_stamp() / sc_time(2, SC_NS));
    std::vector<std::vector<float>> read_C3(Y_DIM, std::vector<float>(X_DIM, 0.0f));
    for (int x = 0; x < X_DIM; x++) {
      for (int sw = 0; sw < subwords_c; sw++) {
        host_data_t chunk = read_host_mem(get_sramc_addr(x, sw));
        for (int i = 0; i < 4; i++) read_C3[sw * 4 + i][x] = chunk[i];
      }
    }
    uint64_t t_read_end = (uint64_t)(sc_time_stamp() / sc_time(2, SC_NS));
    int T_read = t_read_end - t_read_start;

    // Formula for overlapped wall-clock
    int non_overlapped_wall_clock = 4 * (T_write + cycles_case1 + T_read);
    overlapped_wall_clock = T_write;
    for (int t = 0; t < 4; t++) {
      int overlap_load = (t < 3) ? T_write : 0;
      int overlap_read = (t > 0) ? T_read : 0;
      overlapped_wall_clock += std::max(cycles_tiles[t], overlap_load + overlap_read);
    }
    overlapped_wall_clock += T_read;
    float time_saving_c2 = (1.0f - (float)overlapped_wall_clock / non_overlapped_wall_clock) * 100.0f;

    std::cout << "  * Total Compute Cycles (4 Tiles): " << (cycles_tiles[0] + cycles_tiles[1] + cycles_tiles[2] + cycles_tiles[3]) << std::endl;
    std::cout << "  * Non-Overlapped Wall-Clock: " << non_overlapped_wall_clock << " cycles" << std::endl;
    std::cout << "  * Double-Buffered Overlapped Wall-Clock: " << overlapped_wall_clock << " cycles" << std::endl;
    std::cout << "  * Wall-Clock Latency Reduction: " << time_saving_c2 << "%" << std::endl;
    print_state_cycles("Case 2: Double-Buffered Serial Multi-Tiling (4 Tiles)");

    // ----------------------------------------------------
    // CASE 3: Weight-Stationary Batching (N=4, K=64)
    // ----------------------------------------------------
    std::cout << "\n>>> CASE 3: Weight-Stationary Batching (N=4, K=64) <<<" << std::endl;
    std::vector<std::vector<std::vector<float>>> mat_A_c3(4);
    for (int b = 0; b < 4; b++) {
      mat_A_c3[b] = load_matrix("tb_data_utilization/A_c3_b" + std::to_string(b) + ".txt", Y_DIM, 64);
    }
    auto mat_B_c3 = load_matrix("tb_data_utilization/B_c3_b0.txt", 64 + X_DIM, X_DIM);

    reset_system();
    program_config(cfg_base);

    // 1. Program Weight matrix B ONCE into SRAM B
    std::cout << "[TB] Programming weights ONCE for batch size N=4..." << std::endl;
    uint64_t t_w_b_start = (uint64_t)(sc_time_stamp() / sc_time(2, SC_NS));
    for (int addr_idx = 0; addr_idx < 64 + X_DIM; addr_idx++) {
      for (int sw = 0; sw < subwords_b; sw++) {
        host_data_t wdata;
        for (int i = 0; i < 4; i++) wdata[i] = mat_B_c3[addr_idx][sw * 4 + i];
        write_host_mem(get_sramb_addr(addr_idx, sw), wdata, full_mask);
      }
    }
    uint64_t t_w_b_end = (uint64_t)(sc_time_stamp() / sc_time(2, SC_NS));
    int T_write_weights = t_w_b_end - t_w_b_start;

    // 2. Loop for N=4 activation inputs
    int batch_compute_cycles = 0;
    int batch_program_act_cycles = 0;

    state_cycles_total.clear();
    tracking_active = true;

    for (int b = 0; b < 4; b++) {
      // Program activations
      uint64_t t_w_a_start = (uint64_t)(sc_time_stamp() / sc_time(2, SC_NS));
      for (int k = 0; k < 64; k++) {
        for (int sw = 0; sw < subwords_a; sw++) {
          host_data_t wdata;
          for (int i = 0; i < 4; i++) wdata[i] = mat_A_c3[b][sw * 4 + i][k];
          write_host_mem(get_srama_addr(k, sw), wdata, full_mask);
        }
      }
      uint64_t t_w_a_end = (uint64_t)(sc_time_stamp() / sc_time(2, SC_NS));
      batch_program_act_cycles += (t_w_a_end - t_w_a_start);

      // Execute NPU
      o_select.write(sc_bv<3>("111"));
      wait(2);

      uint64_t start_batch = (uint64_t)(sc_time_stamp() / sc_time(2, SC_NS));
      run_npu_core();
      uint64_t end_batch = tile_done_cycle;
      batch_compute_cycles += (end_batch - start_batch);

      // Clean up for next batch
      o_soft_reset.write(true);
      wait(2);
      o_soft_reset.write(false);
      wait(2);
      program_config(cfg_base);
      o_select.write(sc_bv<3>("000"));
      wait(2);
    }
    tracking_active = false;

    int total_stationary_cycles = T_write_weights + batch_program_act_cycles + batch_compute_cycles + (T_read * 4);
    int total_non_stationary_cycles = (T_write + cycles_case1 + T_read) * 4;
    float batch_saving = (1.0f - (float)total_stationary_cycles / total_non_stationary_cycles) * 100.0f;

    std::cout << "  * Weight Loading Cycles (T_write_weights): " << T_write_weights << " cycles" << std::endl;
    std::cout << "  * Activation Loading Cycles (4 batches): " << batch_program_act_cycles << " cycles" << std::endl;
    std::cout << "  * Compute Cycles (4 batches): " << batch_compute_cycles << " cycles" << std::endl;
    std::cout << "  * Non-Stationary Wall-Clock: " << total_non_stationary_cycles << " cycles" << std::endl;
    std::cout << "  * Weight-Stationary Wall-Clock (N=4): " << total_stationary_cycles << " cycles" << std::endl;
    std::cout << "  * Memory Programming Latency Saving: " << batch_saving << "%" << std::endl;
    print_state_cycles("Case 3: Weight-Stationary Batching (N=4)");

    // ----------------------------------------------------
    // CASE 4: Large Input / Spatial Tiling (8 Tiles, K=64)
    // ----------------------------------------------------
    std::cout << "\n>>> CASE 4: Large Input / Spatial Tiling (8 Tiles, K=64) <<<" << std::endl;
    std::vector<std::vector<std::vector<float>>> mat_A_c4(8);
    std::vector<std::vector<std::vector<float>>> mat_B_c4(8);
    for (int t = 0; t < 8; t++) {
      mat_A_c4[t] = load_matrix("tb_data_utilization/A_c4_t" + std::to_string(t) + ".txt", Y_DIM, 64);
      mat_B_c4[t] = load_matrix("tb_data_utilization/B_c4_t" + std::to_string(t) + ".txt", 64 + X_DIM, X_DIM);
    }

    reset_system();
    program_config(cfg_base);

    // Host programs Tile 0 to bank 0
    for (int k = 0; k < 64; k++) {
      for (int sw = 0; sw < subwords_a; sw++) {
        host_data_t wdata;
        for (int i = 0; i < 4; i++) wdata[i] = mat_A_c4[0][sw * 4 + i][k];
        write_host_mem(get_srama_addr(k, sw), wdata, full_mask);
      }
    }
    for (int addr_idx = 0; addr_idx < 64 + X_DIM; addr_idx++) {
      for (int sw = 0; sw < subwords_b; sw++) {
        host_data_t wdata;
        for (int i = 0; i < 4; i++) wdata[i] = mat_B_c4[0][addr_idx][sw * 4 + i];
        write_host_mem(get_sramb_addr(addr_idx, sw), wdata, full_mask);
      }
    }

    // 2. Loop for 8 spatial tiles
    int total_c4_compute = 0;

    state_cycles_total.clear();
    tracking_active = true;

    for (int t = 0; t < 8; t++) {
      if (t % 2 == 0) o_select.write(sc_bv<3>("111"));
      else o_select.write(sc_bv<3>("000"));
      wait(2);

      uint64_t start_tile = (uint64_t)(sc_time_stamp() / sc_time(2, SC_NS));
      o_start.write(true);
      wait();
      tile_done_latched = false;
      o_start.write(false);

      // OVERLAP: Write Tile t+1
      if (t < 7) {
        for (int k = 0; k < 64; k++) {
          for (int sw = 0; sw < subwords_a; sw++) {
            host_data_t wdata;
            for (int i = 0; i < 4; i++) wdata[i] = mat_A_c4[t+1][sw * 4 + i][k];
            write_host_mem(get_srama_addr(k, sw), wdata, full_mask);
          }
        }
        for (int addr_idx = 0; addr_idx < 64 + X_DIM; addr_idx++) {
          for (int sw = 0; sw < subwords_b; sw++) {
            host_data_t wdata;
            for (int i = 0; i < 4; i++) wdata[i] = mat_B_c4[t+1][addr_idx][sw * 4 + i];
            write_host_mem(get_sramb_addr(addr_idx, sw), wdata, full_mask);
          }
        }
      }

      int timeout = 500000;
      while (timeout-- > 0) {
        if (tile_done_latched) break;
        wait();
      }
      uint64_t done_tile = tile_done_cycle;
      total_c4_compute += (done_tile - start_tile);

      o_soft_reset.write(true);
      wait(2);
      o_soft_reset.write(false);
      wait(2);
      program_config(cfg_base);
    }
    tracking_active = false;

    float util_c4 = (8.0f * 64.0f / total_c4_compute) * 100.0f;
    float flops_c4 = 2.0f * Y_DIM * X_DIM * 64 * 8;
    float gops_c4 = flops_c4 / (total_c4_compute * 2.0f);

    std::cout << "  * Spatial Tiles Run: 8 tiles" << std::endl;
    std::cout << "  * Total Compute Cycles: " << total_c4_compute << std::endl;
    std::cout << "  * PE Active Utilization: " << util_c4 << "%" << std::endl;
    std::cout << "  * Performance: " << gops_c4 << " GOPS" << std::endl;
    print_state_cycles("Case 4: Spatial Tiling (8 Tiles)");

    // ----------------------------------------------------
    // CASE 5: Wavefront Overlapping (Continuous Zero-Drain Pipeline Simulation)
    // ----------------------------------------------------
    std::cout << "\n>>> CASE 5: Wavefront Overlapping (Continuous Zero-Drain Pipeline Simulation) <<<" << std::endl;

    std::vector<int> sweep_tiles = {8, 16, 32, 64, 128, 256, 400, 1000};
    for (int N : sweep_tiles) {
      Config cfg_case5 = cfg_base;
      cfg_case5.act_reps = N;
      cfg_case5.wei_reps = N;

      reset_system();
      program_config(cfg_case5);

      // Load data once
      for (int k = 0; k < 64; k++) {
        for (int sw = 0; sw < subwords_a; sw++) {
          host_data_t wdata;
          for (int i = 0; i < 4; i++) wdata[i] = mat_A64[sw * 4 + i][k];
          write_host_mem(get_srama_addr(k, sw), wdata, full_mask);
        }
      }
      for (int addr_idx = 0; addr_idx < 64 + X_DIM; addr_idx++) {
        for (int sw = 0; sw < subwords_b; sw++) {
          host_data_t wdata;
          for (int i = 0; i < 4; i++) wdata[i] = mat_B64[addr_idx][sw * 4 + i];
          write_host_mem(get_sramb_addr(addr_idx, sw), wdata, full_mask);
        }
      }

      o_select.write(sc_bv<3>("111"));
      wait(2);

      state_cycles_total.clear();
      tracking_active = true;

      uint64_t start_cycles_c5 = (uint64_t)(sc_time_stamp() / sc_time(2, SC_NS));
      run_npu_core();
      uint64_t end_cycles_c5 = tile_done_cycle;

      tracking_active = false;
      int cycles_case5 = end_cycles_c5 - start_cycles_c5;

      float util_c5 = ((float)N * 64.0f / cycles_case5) * 100.0f;
      float flops_c5 = 2.0f * Y_DIM * X_DIM * 64 * N;
      float gops_c5 = flops_c5 / (cycles_case5 * 2.0f);

      std::cout << "\n--- SWEEP TILE COUNT N = " << N << " ---" << std::endl;
      std::cout << "  * Simulation Cycles: " << cycles_case5 << std::endl;
      std::cout << "  * PE Active Utilization: " << util_c5 << "%" << std::endl;
      std::cout << "  * Performance: " << gops_c5 << " GOPS" << std::endl;
      print_state_cycles("Case 5: Wavefront Overlapping (" + std::to_string(N) + " Tiles Continuous)");
    }

    std::cout << "\n==========================================================" << std::endl;
    std::cout << "               PE UTILIZATION VERIFICATION COMPLETE        " << std::endl;
    std::cout << "==========================================================" << std::endl;

    sc_stop();
  }
};

int sc_main(int argc, char *argv[]) {
  sc_clock clk("clk", 2, SC_NS);

  sc_signal<bool> rstn{"rstn"};
  sc_signal<bool> soft_reset{"soft_reset"};
  sc_signal<bool> start{"start"};
  sc_signal<bool> done{"done"};
  sc_signal<bool> deadlock{"deadlock"};

  sc_signal<uint32_t> mvm_k{"mvm_k"};
  sc_signal<uint32_t> total_contexts{"total_contexts"};

  sc_signal<uint32_t> host_addr{"host_addr"};
  sc_signal<bool> host_wren{"host_wren"};
  sc_signal<bool> host_rden{"host_rden"};
  sc_signal<host_data_t> host_wdata{"host_wdata"};
  sc_signal<host_mask_t> host_wmask{"host_wmask"};
  sc_signal<host_data_t> host_rdata{"host_rdata"};

  sc_signal<float> threshold{"threshold"};
  sc_signal<sc_bv<3>> select{"select"};

  PeConfig pe_cfg;
  pe_cfg.arithmetic_type = 1; // Fixed-point mode
  pe_cfg.mul_type = 0;        // Exact multiplier
  pe_cfg.add_type = 0;        // Exact adder
  pe_cfg.stages_mul = 1;
  pe_cfg.intermediate_pipeline_stage = true;
  pe_cfg.zero_gating_mult = false;

  NpuTop<32, 32, int8_t, int8_t, int32_t, 2048, 2048, 4096, 16, 5, 1> npu(
      "NpuTop_utilization", pe_cfg);
  g_npu_ptr = &npu;
  TestbenchUtilization tb("TestbenchUtilization_inst");

  npu.i_clk(clk);
  npu.i_rstn(rstn);
  npu.i_soft_reset(soft_reset);
  npu.i_start(start);
  npu.o_done(done);
  npu.o_deadlock(deadlock);
  npu.i_mvm_k(mvm_k);
  npu.i_total_contexts(total_contexts);
  npu.i_host_addr(host_addr);
  npu.i_host_wren(host_wren);
  npu.i_host_rden(host_rden);
  npu.i_host_wdata(host_wdata);
  npu.i_host_wmask(host_wmask);
  npu.o_host_rdata(host_rdata);
  npu.i_threshold(threshold);
  npu.i_select(select);

  tb.i_clk(clk);
  tb.o_rstn(rstn);
  tb.o_soft_reset(soft_reset);
  tb.o_start(start);
  tb.i_done(done);
  tb.i_deadlock(deadlock);
  tb.o_mvm_k(mvm_k);
  tb.o_total_contexts(total_contexts);
  tb.o_host_addr(host_addr);
  tb.o_host_wren(host_wren);
  tb.o_host_rden(host_rden);
  tb.o_host_wdata(host_wdata);
  tb.o_host_wmask(host_wmask);
  tb.i_host_rdata(host_rdata);
  tb.o_threshold(threshold);
  tb.o_select(select);

  sc_start();
  return 0;
}
