// tb_top_softmax.sv -- runs gvu_top.sv (softmax opcode) as delivered and reports what it does: the sequence of controller
// states with the cycle of every change, the table-load writes, the contents of the LUT tables after the load, the
// Scratchpad accesses of every stage, and whether the done output is reached. It is a report, not a pass / fail check:
// the lines start with TOP.
// The LUT tables are placed in buffer 0 of vbank1 by hierarchical assignment before the run (the DMA side of the
// Scratchpad is not connected): rows 0..3 hold the reciprocal table (256 words of 16 bits, word k = 0x4000 + k), rows 4..5
// the exponential table (256 bytes, byte k = 255 - k); rows 0.. of vbank0 hold the softmax input (row t: lane l = (7 * t + 3 * l) mod 256 as int8).
// shift_reg is not part of the RTL delivered; shift_reg_standin.sv supplies it.
// Built with Verilator 5 (--binary --timing). Plusargs: +len=<token length, default 8> +max=<cycles, default 4000>.
`timescale 1ns/1ps

module tb_top_softmax;
  import gvu_pkg::*;
  int unsigned len, maxc, cyc, n_wr, n_rd0, n_wr0, n_rd1, n_wr1, n_rd2, n_wr2, ok_recip, ok_exp, nz_recip, nz_exp, nz_out;
  logic clk = 0, rstn = 0, en = 0;
  always #5 clk = ~clk;
  logic lut_done, pipe_done;
  logic [8:0] token_len;
  gvu_top u (.i_clk(clk), .i_rstn(rstn), .i_pipeline_en(en), .i_gvu_op(GVU_SOFTMAX), .i_psum_valid(1'b0), .o_psum_ready(), .i_psum_data('0),
    .o_lut_init_done(lut_done), .o_pipe_done(pipe_done),
    .i_lut_act_reset(1'b0), .i_lut_exp_reset(1'b0), .i_lut_recip_reset(1'b0), .i_lut_rsqrt_reset(1'b0), .i_oc_last(1'b0), .i_oc_tile_last(1'b0),
    .i_gemm_fused_skip_act(1'b0), .i_gemm_fused_z(8'sd0), .i_pooling_mode(POOLING_2X2), .i_pooling_stride(3'd0), .i_pooling_num_w_tile(8'd0),
    .i_pooling_c(8'd0), .i_pooling_oh(8'd0), .i_pooling_c_move('0), .i_pooling_h_move('0), .i_num_tile(9'd1), .i_token_len(token_len),
    .i_attn_z(8'sd0), .i_attn_scale(32'sd35), .i_attn_shift(8'd8),
    .i_sbank_buf_sel(1'b0), .i_vbank0_buf_sel(1'b0), .i_vbank1_buf_sel(1'b0), .i_vbank2_buf_sel(1'b0),
    .i_dma_sbank_en(1'b0), .i_dma_sbank_wen(1'b0), .i_dma_sbank_addr('0), .i_dma_sbank_wmask('0), .i_dma_sbank_wdata('0), .o_dma_sbank_rdata(),
    .i_dma_vbank_en(1'b0), .i_dma_vbank_wen(1'b0), .i_dma_vbank_addr('0), .i_dma_vbank_wdata('0), .i_dma_vbank_wmask('0), .o_dma_vbank_rdata());
  control_fsm_t st, st_q;
  logic [1023:0] row; logic [255:0] row8;
  logic seen_done;
  assign st = u.gvu_pipe_ctrl_state;
  initial begin
    if (!$value$plusargs("len=%d", len)) len = 8;
    if (!$value$plusargs("max=%d", maxc)) maxc = 4000;
    token_len = len[8:0];
    for (int k = 0; k < 4; k++) begin
      for (int j = 0; j < 64; j++) row[16*j +: 16] = 16'(16'h4000 + 64 * k + j);
      u.i_scratchpad_wrapper.i_vbank1_buf0.mem[k] = row;
    end
    for (int k = 0; k < 2; k++) begin
      for (int j = 0; j < 128; j++) row[8*j +: 8] = 8'(255 - (128 * k + j));
      u.i_scratchpad_wrapper.i_vbank1_buf0.mem[4 + k] = row;
    end
    for (int t = 0; t < 300; t++) begin
      for (int l = 0; l < 32; l++) row8[8*l +: 8] = 8'(7 * t + 3 * l);
      u.i_scratchpad_wrapper.i_vbank0_buf0.mem[t] = row8;
    end
    repeat (4) @(negedge clk); rstn = 1; @(negedge clk); en = 1;
    st_q = IDLE; cyc = 0; seen_done = 0;
    n_wr = 0; n_rd0 = 0; n_wr0 = 0; n_rd1 = 0; n_wr1 = 0; n_rd2 = 0; n_wr2 = 0;
    $display("TOP token length %0d; states: 0 IDLE 1 LUT_ACT_INIT 2 LUT_EXP_INIT 3 LUT_RECIP_INIT 4 LUT_RSQRT_INIT 5 SOFTMAX_ST1 6 SOFTMAX_ST2 7 SOFTMAX_ST3", len);
    while (cyc < maxc && !(seen_done && st == IDLE)) begin
      #1;
      if (st !== st_q) begin
        $display("TOP cycle %0d: state %0d -> %0d; so far: table writes %0d, vbank0 rd %0d wr %0d, vbank1 rd %0d wr %0d, vbank2 rd %0d wr %0d", cyc, st_q, st, n_wr, n_rd0, n_wr0, n_rd1, n_wr1, n_rd2, n_wr2);
        if (st_q == LUT_RECIP_INIT || st_q == LUT_EXP_INIT) n_wr = 0;
        st_q = st;
      end
      if (u.pipe_ctrl_lut_init_wr) begin
        n_wr++;
        $display("TOP cycle %0d: table write %0d, table %0d, write address %0d, write counter %0d, done flag %0d, low data word %h", cyc, n_wr, u.pipe_ctrl_lut_init_sel, u.i_gvu_vlut.i_lut_mem.lut_waddr_r, u.i_gvu_vlut.i_lut_mem.wr_cnt_r, u.pipe_ctrl_lut_init_done, u.pipe_ctrl_lut_init_data[15:0]);
      end
      if (u.vbank0_en) begin if (u.vbank0_wen) n_wr0++; else n_rd0++; end
      if (u.vbank1_en_0 | u.vbank1_en_1 | u.vbank1_en_2) begin if (u.vbank1_wen_0 | u.vbank1_wen_1 | u.vbank1_wen_2) n_wr1++; else n_rd1++; end
      if (u.vbank2_en_0 | u.vbank2_en_1) begin if (u.vbank2_wen_0 | u.vbank2_wen_1) n_wr2++; else n_rd2++; end
      if (pipe_done && !seen_done) begin seen_done = 1; $display("TOP cycle %0d: o_pipe_done = 1 (state %0d)", cyc, st); end
      if (st == SOFTMAX_ST1 && st_q == SOFTMAX_ST1 && cyc % 100000 == 99999) $display("TOP cycle %0d: still in SOFTMAX_ST1", cyc);
      @(negedge clk); cyc++;
    end
    $display("TOP end at cycle %0d: state %0d, pipe_done seen %0d, lut_init_done output %0d; vbank0 rd %0d wr %0d, vbank1 rd %0d wr %0d, vbank2 rd %0d wr %0d", cyc, st, seen_done, lut_done, n_rd0, n_wr0, n_rd1, n_wr1, n_rd2, n_wr2);
    ok_recip = 0; ok_exp = 0; nz_recip = 0; nz_exp = 0;
    for (int k = 0; k < 256; k++) begin
      if (u.i_gvu_vlut.i_lut_mem.lut_recip_mem[k] === 16'(16'h4000 + k)) ok_recip++;
      if (u.i_gvu_vlut.i_lut_mem.lut_recip_mem[k] !== 16'd0) nz_recip++;
      if (u.i_gvu_vlut.i_lut_mem.lut_exp_mem[k] === 8'(255 - k)) ok_exp++;
      if (u.i_gvu_vlut.i_lut_mem.lut_exp_mem[k] !== 8'd0) nz_exp++;
    end
    $display("TOP reciprocal table: %0d of 256 entries hold the value placed in vbank1 (%0d non-zero); exponential table: %0d of 256 (%0d non-zero)", ok_recip, nz_recip, ok_exp, nz_exp);
    nz_out = 0;
    for (int t = 0; t < 300; t++) begin
      for (int l = 0; l < 32; l++) row8[8*l +: 8] = 8'(7 * t + 3 * l);
      if (u.i_scratchpad_wrapper.i_vbank0_buf0.mem[t] !== row8) nz_out++;
    end
    $display("TOP vbank0 rows changed by the run (attention score is written there in stage 3): %0d", nz_out);
    $display("TOP vbank1 row 0 after the run, low 64 bits: %h; vbank2 row 0 low 64 bits: %h", u.i_scratchpad_wrapper.i_vbank1_buf0.mem[0][63:0], u.i_scratchpad_wrapper.i_vbank2_buf0.mem[0][63:0]);
    $finish;
  end
endmodule
