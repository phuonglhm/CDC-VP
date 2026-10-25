// tb_oracle_maxpool.sv -- MAXPOOL on gvu_top.sv (latest hardware RTL drop), one run, written as a cycle trace that
// tools/has/tb_gvu_top_maxpool.cpp --trace writes for the port (GvuTop, has/rtl_ref_gvu_top.h) on the same input.
// The input tensor (input.txt of a vector set of target/test/maxpool, int8 values, [C][H][W]) is placed in vbank0 buffer 0
// by hierarchical assignment as the hardware bench tb/gvu/tb_gvu_top.sv does (row c * H * Wt + h * Wt + w_tile, bytes past
// W left 0), only the first C channels (one run). The opcode GVU_MAXPOOL is started one negedge after the reset, and in
// every cycle (sampled 1 ns after the negedge where the inputs change) a line is written when vbank0 is enabled, vbank2
// port 1 is written or o_pipe_done is high:
//   T <cycle> <in POOLING_MAX> <vbank0_en> <vbank0_addr> <vbank2_wen_1> <vbank2_addr_1> <vbank2_wmask_1 hex> <data hex>
// data = vbank2_wdata_1 with the bytes of the lanes not in the mask set to 0, lane 31 first. The enable drops after the
// edge that follows o_pipe_done (as tb_gvu_top.sv); the run ends two cycles after o_pipe_done, or with "TIMEOUT" after
// +max cycles. Plusargs: +in=<input.txt> +out=<trace file> +h +w +k +s +c [+max].
// shift_reg, spill_register_flushable and fifo_v3 come from the *_standin.sv files. Built with Verilator 5
// (--binary --timing) by run_verilator_maxpool.sh.
`timescale 1ns/1ps

module tb_oracle_maxpool;
  import gvu_pkg::*;
  localparam int unsigned L = GVU_LANES;
  string fin, fout;
  integer fd, fo, rc, h, w, k, s, c, maxc, wt, oh, n, done_at, v, runs, runs_done, restart;
  logic clk = 0, rstn = 0, en = 0;
  always #5 clk = ~clk;
  gvu_pooling_mode_t pmode;
  logic [2:0] pstride;
  logic [7:0] pnwt, pc, poh;
  logic [VBANK0_AW-1:0] pcm, phm;
  logic pipe_done;
  logic [8*L-1:0] d;

  gvu_top u (.i_clk(clk), .i_rstn(rstn), .i_pipeline_en(en), .i_gvu_op(GVU_MAXPOOL),
    .i_lut_act_reset(1'b0), .i_lut_exp_reset(1'b0), .i_lut_recip_reset(1'b0), .i_lut_rsqrt_reset(1'b0),
    .i_psum_valid(1'b0), .o_psum_ready(), .i_psum_data('0), .i_oc_last(1'b0), .i_oc_tile_last(1'b0),
    .i_gemm_fused_skip_act(1'b0), .i_gemm_fused_z(8'sd0), .o_lut_init_done(), .o_pipe_done(pipe_done),
    .i_num_tile(9'd1), .i_token_len(9'd0), .i_attn_z(8'sd0), .i_attn_scale(32'sd0), .i_attn_shift(8'd0),
    .i_pooling_mode(pmode), .i_pooling_stride(pstride), .i_pooling_num_w_tile(pnwt), .i_pooling_c(pc), .i_pooling_oh(poh),
    .i_pooling_c_move(pcm), .i_pooling_h_move(phm),
    .i_sbank_buf_sel(1'b0), .i_vbank0_buf_sel(1'b0), .i_vbank1_buf_sel(1'b0), .i_vbank2_buf_sel(1'b0),
    .i_dma_sbank_en(1'b0), .i_dma_sbank_wen(1'b0), .i_dma_sbank_addr('0), .i_dma_sbank_wmask('0), .i_dma_sbank_wdata('0), .o_dma_sbank_rdata(),
    .i_dma_vbank_en(1'b0), .i_dma_vbank_wen(1'b0), .i_dma_vbank_addr('0), .i_dma_vbank_wdata('0), .i_dma_vbank_wmask('0), .o_dma_vbank_rdata());

  initial begin
    if (!$value$plusargs("in=%s", fin)) $fatal(1, "+in missing");
    if (!$value$plusargs("out=%s", fout)) $fatal(1, "+out missing");
    if (!$value$plusargs("h=%d", h)) h = 20;
    if (!$value$plusargs("w=%d", w)) w = 20;
    if (!$value$plusargs("k=%d", k)) k = 2;
    if (!$value$plusargs("s=%d", s)) s = 1;
    if (!$value$plusargs("c=%d", c)) c = 1;
    if (!$value$plusargs("max=%d", maxc)) maxc = 800000;
    if (!$value$plusargs("runs=%d", runs)) runs = 1;   // runs back to back on the same data, no reset in between
    wt = (w + L - 1) / L;
    oh = (h - k) / s + 1;
    pmode = gvu_pooling_mode_t'(k - 2); pstride = 3'(s); pnwt = 8'(wt); pc = 8'(c); poh = 8'(oh);
    pcm = VBANK0_AW'(wt * h); phm = VBANK0_AW'(wt * s);
`ifndef VERILATOR
    // the lanes the input does not fill are read by the windows; the port and Verilator see 0 there, a 4-state simulator x
    for (int r = 0; r < VBANK0_DEPTH; r++) begin u.i_scratchpad_wrapper.i_vbank0_buf0.mem[r] = '0; u.i_scratchpad_wrapper.i_vbank0_buf1.mem[r] = '0; end
    for (int r = 0; r < VBANK2_DEPTH; r++) begin u.i_scratchpad_wrapper.i_vbank2_buf0.mem[r] = '0; u.i_scratchpad_wrapper.i_vbank2_buf1.mem[r] = '0; end
`endif
    fd = $fopen(fin, "r");
    if (fd == 0) $fatal(1, "cannot open %s", fin);
    for (int ch = 0; ch < c; ch++)
      for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) begin
          rc = $fscanf(fd, "%d", v);
          u.i_scratchpad_wrapper.i_vbank0_buf0.mem[ch * h * wt + y * wt + x / L][(x % L) * 8 +: 8] = 8'(v);
        end
    $fclose(fd);
    fo = $fopen(fout, "w");
    repeat (4) @(negedge clk); rstn = 1; @(negedge clk); en = 1;
    done_at = -1; runs_done = 0; restart = 0;
    for (n = 0; n < maxc; n++) begin
      #1;
      if (u.vbank0_en || u.vbank2_wen_1 || pipe_done) begin
        d = '0;
        for (int l = 0; l < L; l++) if (u.vbank2_wmask_1[l]) d[8*l +: 8] = u.vbank2_wdata_1[8*l +: 8];
        $fwrite(fo, "T %0d %0d %0d %0d %0d %0d %08h %064h\n", n, u.gvu_pipe_ctrl_state == POOLING_MAX, u.vbank0_en, u.vbank0_en ? u.vbank0_addr : 0,
                u.vbank2_wen_1, u.vbank2_wen_1 ? u.vbank2_addr_1 : 0, u.vbank2_wen_1 ? u.vbank2_wmask_1 : 0, d);
      end
      if (pipe_done && done_at < 0) done_at = n;
      restart = 0;
      if (done_at >= 0 && n >= done_at + 2) begin
        runs_done++;
        if (runs_done == runs) break;
        $fwrite(fo, "DONE %0d\n", done_at);
        done_at = -1;
        restart = 1;
      end
      @(negedge clk);
      if (restart) en = 1;             // next run on the same data, no reset
      else if (done_at >= 0) en = 0;   // as tb_gvu_top.sv: the enable drops after the edge that follows o_pipe_done
    end
    if (done_at < 0) $fwrite(fo, "TIMEOUT %0d\n", n);
    else $fwrite(fo, "DONE %0d\n", done_at);
    $fclose(fo);
    $display("MAXPOOL trace %s cycles %0d done %0d", fout, n, done_at);
    $finish;
  end
endmodule
