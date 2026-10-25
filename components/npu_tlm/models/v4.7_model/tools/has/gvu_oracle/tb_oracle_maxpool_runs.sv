// tb_oracle_maxpool_runs.sv -- MAXPOOL on gvu_top.sv over a whole tensor in runs of +group channels, back to back WITHOUT a reset
// between runs, as the data flow controller drives it on a network layer (for example the 288 channels of the YOLOv8m SPPF max pool
// in 21 runs of 14). Same cycle trace as tb_oracle_maxpool.sv (one line per cycle with vbank0 enabled, vbank2 port 1 written or
// o_pipe_done high; "DONE <cycle>" after each run):
//   T <cycle> <in POOLING_MAX> <vbank0_en> <vbank0_addr> <vbank2_wen_1> <vbank2_addr_1> <vbank2_wmask_1 hex> <data hex>
// Run r loads channels r*group .. r*group+n-1 of input.txt ([C][H][W] int8, the tensor already padded as the controller puts it)
// into vbank0 buffer 0 rows 0.. by hierarchical assignment while the unit is idle (row c*H*Wt + h*Wt + w_tile, bytes past W left 0),
// sets POOLING C = n and starts the run: i_pipeline_en goes high one negedge after the reset for run 0, and two cycles after the
// done of the previous run for the next ones; it drops after the edge that follows o_pipe_done (as tb/gvu/tb_gvu_top.sv). Every run
// writes vbank2 from row 0, so chk_maxpool_runs.py rebuilds each run from the trace.
// Without patch B12 (gvu_pooling_feeder.sv keeps first_load_cnt between runs) every run after the first goes wrong; with it
// each run equals the run of a freshly reset unit.
// Plusargs: +in=<input.txt> +out=<trace> +h +w +k +s +c (channels in total) [+group (default c)] [+agu=1] [+max (cycles per run)].
// +agu=1 (needs the AGU_ND proposal, compile with +define+GVU_AGU_ND): the vbank2 writes go through an identity AGU_ND destination
// program (port vbank2, base 0, n0 = n*OH*Wt, s0 = 1) as on the instruction path of the model; the trace must not change.
// Compile with +define+GVU_R3 when the residual-add proposal is applied (new inputs i_res_*, tied to 0 here).
// shift_reg, spill_register_flushable and fifo_v3 come from the *_standin.sv files (not in the RTL drop).
`timescale 1ns/1ps

module tb_oracle_maxpool_runs;
  import gvu_pkg::*;
  localparam int unsigned L = GVU_LANES;
  string fin, fout;
  integer fd, fo, rc, h, w, k, s, c, grp, maxc, wt, oh, n, done_at, run, nruns, c0, nc, agu, total;
  logic clk = 0, rstn = 0, en = 0;
  always #5 clk = ~clk;
  gvu_pooling_mode_t pmode;
  logic [2:0] pstride;
  logic [7:0] pnwt, pc, poh;
  logic [VBANK0_AW-1:0] pcm, phm;
  logic pipe_done;
  logic [8*L-1:0] d;
  integer vals [];
`ifdef GVU_AGU_ND
  agu_nd_ctrl_t actrl;
  agu_nd_desc_t adst;
`endif

  gvu_top u (.i_clk(clk), .i_rstn(rstn), .i_pipeline_en(en), .i_gvu_op(GVU_MAXPOOL),
    .i_lut_act_reset(1'b0), .i_lut_exp_reset(1'b0), .i_lut_recip_reset(1'b0), .i_lut_rsqrt_reset(1'b0),
    .i_psum_valid(1'b0), .o_psum_ready(), .i_psum_data('0), .i_oc_last(1'b0), .i_oc_tile_last(1'b0),
    .i_gemm_fused_skip_act(1'b0), .i_gemm_fused_z(8'sd0), .o_lut_init_done(), .o_pipe_done(pipe_done),
    .i_num_tile(9'd1), .i_token_len(9'd0), .i_attn_z(8'sd0), .i_attn_scale(32'sd0), .i_attn_shift(8'd0),
    .i_pooling_mode(pmode), .i_pooling_stride(pstride), .i_pooling_num_w_tile(pnwt), .i_pooling_c(pc), .i_pooling_oh(poh),
    .i_pooling_c_move(pcm), .i_pooling_h_move(phm),
`ifdef GVU_R3
    .i_res_scale_a('0), .i_res_scale_b('0), .i_res_scale_out('0), .i_res_shift_a('0), .i_res_shift_b('0), .i_res_shift_out('0),
    .i_res_round(round_mode_e'(0)), .i_res_out_int16(1'b0), .i_res_zp_a('0), .i_res_zp_b('0), .i_res_zp_out('0),
    .i_res_a_row('0), .i_res_b_row('0), .i_res_sum_row('0), .i_res_last_lane_mask('0),
`endif
`ifdef GVU_AGU_ND
    .i_agu_nd_ctrl(actrl), .i_agu_nd_src('0), .i_agu_nd_dst(adst),
`endif
    .i_sbank_buf_sel(1'b0), .i_vbank0_buf_sel(1'b0), .i_vbank1_buf_sel(1'b0), .i_vbank2_buf_sel(1'b0),
    .i_dma_sbank_en(1'b0), .i_dma_sbank_wen(1'b0), .i_dma_sbank_addr('0), .i_dma_sbank_wmask('0), .i_dma_sbank_wdata('0), .o_dma_sbank_rdata(),
    .i_dma_vbank_en(1'b0), .i_dma_vbank_wen(1'b0), .i_dma_vbank_addr('0), .i_dma_vbank_wdata('0), .i_dma_vbank_wmask('0), .o_dma_vbank_rdata());

  task automatic load_run(input integer first, input integer count);
    for (int ch = 0; ch < count; ch++)
      for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
          u.i_scratchpad_wrapper.i_vbank0_buf0.mem[ch * h * wt + y * wt + x / L][(x % L) * 8 +: 8] = 8'(vals[((first + ch) * h + y) * w + x]);
  endtask

  initial begin
    if (!$value$plusargs("in=%s", fin)) $fatal(1, "+in missing");
    if (!$value$plusargs("out=%s", fout)) $fatal(1, "+out missing");
    if (!$value$plusargs("h=%d", h)) h = 24;
    if (!$value$plusargs("w=%d", w)) w = 24;
    if (!$value$plusargs("k=%d", k)) k = 5;
    if (!$value$plusargs("s=%d", s)) s = 1;
    if (!$value$plusargs("c=%d", c)) c = 1;
    if (!$value$plusargs("group=%d", grp)) grp = c;
    if (!$value$plusargs("agu=%d", agu)) agu = 0;
    if (!$value$plusargs("max=%d", maxc)) maxc = 800000;
    wt = (w + L - 1) / L;
    oh = (h - k) / s + 1;
    nruns = (c + grp - 1) / grp;
    pmode = gvu_pooling_mode_t'(k - 2); pstride = 3'(s); pnwt = 8'(wt); poh = 8'(oh);
    pcm = VBANK0_AW'(wt * h); phm = VBANK0_AW'(wt * s);
`ifdef GVU_AGU_ND
    actrl = '0; adst = '0;
`else
    if (agu != 0) $fatal(1, "+agu needs +define+GVU_AGU_ND");
`endif
`ifndef VERILATOR
    // the lanes the input does not fill are read by the windows; the port and Verilator see 0 there, a 4-state simulator x
    for (int r = 0; r < VBANK0_DEPTH; r++) begin u.i_scratchpad_wrapper.i_vbank0_buf0.mem[r] = '0; u.i_scratchpad_wrapper.i_vbank0_buf1.mem[r] = '0; end
    for (int r = 0; r < VBANK2_DEPTH; r++) begin u.i_scratchpad_wrapper.i_vbank2_buf0.mem[r] = '0; u.i_scratchpad_wrapper.i_vbank2_buf1.mem[r] = '0; end
`endif
    vals = new[c * h * w];
    fd = $fopen(fin, "r");
    if (fd == 0) $fatal(1, "cannot open %s", fin);
    for (int e = 0; e < c * h * w; e++) begin rc = $fscanf(fd, "%d", vals[e]); if (rc != 1) $fatal(1, "%s: fewer than %0d values", fin, c * h * w); end
    $fclose(fd);
    fo = $fopen(fout, "w");
    total = 0;
    repeat (4) @(negedge clk); rstn = 1;
    for (run = 0; run < nruns; run++) begin
      c0 = run * grp; nc = (c - c0 < grp) ? c - c0 : grp;
      load_run(c0, nc);
      pc = 8'(nc);
`ifdef GVU_AGU_ND
      if (agu == 1) begin
        actrl.dst_port = 3'd4; actrl.dst_on = 1'b1;
        adst.base = '0; adst.n_m1[0] = 10'(nc * oh * wt - 1); adst.n_m1[1] = '0; adst.n_m1[2] = '0; adst.n_m1[3] = '0;
        adst.s[0] = 16'd1; adst.s[1] = '0; adst.s[2] = '0; adst.s[3] = '0;
      end
`endif
      @(negedge clk); en = 1;
      done_at = -1;
      for (n = 0; n < maxc; n++) begin
        #1;
        if (u.vbank0_en || u.vbank2_wen_1 || pipe_done) begin
          d = '0;
          for (int l = 0; l < L; l++) if (u.vbank2_wmask_1[l]) d[8*l +: 8] = u.vbank2_wdata_1[8*l +: 8];
          $fwrite(fo, "T %0d %0d %0d %0d %0d %0d %08h %064h\n", total + n, u.gvu_pipe_ctrl_state == POOLING_MAX, u.vbank0_en, u.vbank0_en ? u.vbank0_addr : 0,
                  u.vbank2_wen_1, u.vbank2_wen_1 ? u.vbank2_addr_1 : 0, u.vbank2_wen_1 ? u.vbank2_wmask_1 : 0, d);
        end
        if (pipe_done && done_at < 0) done_at = n;
        if (done_at >= 0 && n >= done_at + 2) break;
        @(negedge clk);
        if (done_at >= 0) en = 0;   // as tb_gvu_top.sv: the enable drops after the edge that follows o_pipe_done
      end
      if (done_at < 0) begin $fwrite(fo, "TIMEOUT %0d\n", total + n); break; end
      $fwrite(fo, "DONE %0d\n", total + done_at);
      total = total + n + 1;
    end
    $fclose(fo);
    $display("MAXPOOL runs trace %s runs %0d cycles %0d", fout, run, total);
    $finish;
  end
endmodule
