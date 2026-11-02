// tb_oracle_cfgregs_neo.sv -- replay of the vectors of tb_rtl_ref_gvu_config_regs_neo --dump on gvu_config_regs.sv of neo_lite (ModelSim ASE, 4 states).
// cfg_reg has no reset in the RTL (x in a 4-state simulator): it is zeroed here like Verilator starts it; o_rdata is compared only when its value is defined by a read
// of an existing register. Prints "ORACLE cfgregs_neo cycles <n> mismatches <m>". Plusarg +file=<vector file>.
`timescale 1ns/1ps
module tb_oracle_cfgregs_neo;
  import gvu_pkg::*;
  string file;
  integer fd, rc, n, bad;
  logic clk = 0, rstn = 0;
  always #5 clk = ~clk;
  logic [5:0] addr; logic wren, rden, pdone; logic [31:0] wmask, wdata, rdata;
  logic pipe_en, done_intr, lut_a, lut_e, lut_r, lut_s, sb, v0, v1, v2, skip; gvu_opcode_t op; logic signed [7:0] gz, az, rz; logic [15:0] ntile, tlen, rlen; logic signed [31:0] ascale;
  logic [7:0] ashift; logic rsub; logic signed [1:0][31:0] rscale; logic signed [63:0] rbias; logic [5:0] rshift; gvu_pooling_mode_t pmode; logic [2:0] pstride; logic [7:0] pnw, pc, poh;
  logic [VBANK0_AW-1:0] pcm, phm;
  gvu_config_regs dut (.i_clk(clk), .i_rstn(rstn), .i_addr(addr), .i_wren(wren), .i_wmask(wmask), .i_wdata(wdata), .i_rden(rden), .o_rdata(rdata),
    .o_gvu_pipe_en(pipe_en), .o_gvu_op(op), .o_lut_act_reset(lut_a), .o_lut_exp_reset(lut_e), .o_lut_recip_reset(lut_r), .o_lut_rsqrt_reset(lut_s),
    .o_sbank_buf_sel(sb), .o_vbank0_buf_sel(v0), .o_vbank1_buf_sel(v1), .o_vbank2_buf_sel(v2), .o_gemm_fused_skip_act(skip), .o_gemm_fused_z(gz),
    .o_num_tile(ntile), .o_token_len(tlen), .o_attn_z(az), .o_attn_scale(ascale), .o_attn_shift(ashift),
    .o_residual_sequence_len(rlen), .o_residual_sub_mode(rsub), .o_residual_dequant_scale(rscale), .o_residual_bias(rbias), .o_residual_out_z(rz), .o_residual_out_shift(rshift),
    .o_pooling_mode(pmode), .o_pooling_stride(pstride), .o_pooling_num_w_tile(pnw), .o_pooling_c(pc), .o_pooling_oh(poh), .o_pooling_c_move(pcm), .o_pooling_h_move(phm),
    .i_gvu_pipe_done(pdone), .o_done_intr(done_intr));
  logic [31:0] i_rstn, i_addr, i_wren, i_rden, i_wmask, i_wdata, i_pd;
  logic [63:0] o [0:33], o_prev [0:33];
  logic bad_now;
  int rd_known = 0; int rd_known_prev = 0;
  // o[]: 0 pipe_en 1 done 2 op 3..6 lut a/e/r/s 7..10 buf sb/v0/v1/v2 11 skip 12 z 13 ntile 14 tlen 15 az 16 ascale 17 ashift 18 rlen 19 rsub 20 rscale_a 21 rscale_b 22 rbias
  //       23 rz 24 rshift 25 pmode 26 pstride 27 pnw 28 pc 29 poh 30 pcm 31 phm 32 rdata 33 (zero)
  initial begin
    if (!$value$plusargs("file=%s", file)) file = "cfgregs_neo.txt";
`ifndef VERILATOR
    dut.cfg_reg = '0;
`endif
    addr = 0; wren = 0; rden = 0; wmask = 0; wdata = 0; pdone = 0;
    fd = $fopen(file, "r"); n = 0; bad = 0;
    while (fd != 0 && !$feof(fd)) begin
      rc = $fscanf(fd, "%h %h %h %h %h %h %h | ", i_rstn, i_addr, i_wren, i_rden, i_wmask, i_wdata, i_pd);
      if (rc != 7) break;
      rc = $fscanf(fd, "%h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h", o[0], o[1], o[2], o[3], o[4], o[5], o[6], o[7], o[8], o[9], o[10], o[11], o[12], o[13], o[14], o[15], o[16]);
      rc = $fscanf(fd, "%h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h
", o[17], o[18], o[19], o[20], o[21], o[22], o[23], o[24], o[25], o[26], o[27], o[28], o[29], o[30], o[31], o[32], o[33]);
      @(negedge clk);
      if (n > 0) begin
        bad_now = !(pipe_en === o_prev[0][0] && done_intr === o_prev[1][0] && op === o_prev[2][3:0] &&
                    lut_a === o_prev[3][0] && lut_e === o_prev[4][0] && lut_r === o_prev[5][0] && lut_s === o_prev[6][0] &&
                    sb === o_prev[7][0] && v0 === o_prev[8][0] && v1 === o_prev[9][0] && v2 === o_prev[10][0] &&
                    skip === o_prev[11][0] && gz === o_prev[12][7:0] && ntile === o_prev[13][15:0] && tlen === o_prev[14][15:0] && az === o_prev[15][7:0] &&
                    ascale === o_prev[16][31:0] && ashift === o_prev[17][7:0] && rlen === o_prev[18][15:0] && rsub === o_prev[19][0] &&
                    rscale[0] === o_prev[20][31:0] && rscale[1] === o_prev[21][31:0] && rbias === o_prev[22] && rz === o_prev[23][7:0] && rshift === o_prev[24][5:0] &&
                    pmode === o_prev[25][1:0] && pstride === o_prev[26][2:0] && pnw === o_prev[27][7:0] && pc === o_prev[28][7:0] && poh === o_prev[29][7:0] &&
                    pcm === o_prev[30][VBANK0_AW-1:0] && phm === o_prev[31][VBANK0_AW-1:0] && (rd_known_prev == 0 || rdata === o_prev[32][31:0]));
        if (bad_now) begin
          bad++;
          if (bad <= 6) $display("FIRST cfgregs_neo cycle %0d: rtl pe %b dn %b op %0d buf %b%b%b%b tlen %0d ntile %0d rbias %h rdata %h; file pe %b dn %b op %0d buf %b%b%b%b tlen %0d ntile %0d rbias %h rdata %h",
            n - 1, pipe_en, done_intr, op, sb, v0, v1, v2, tlen, ntile, rbias, rdata, o_prev[0][0], o_prev[1][0], o_prev[2][3:0], o_prev[7][0], o_prev[8][0], o_prev[9][0], o_prev[10][0], o_prev[14][15:0], o_prev[13][15:0], o_prev[22], o_prev[32][31:0]);
        end
      end
      rstn = i_rstn[0]; addr = i_addr[5:0]; wren = i_wren[0]; rden = i_rden[0]; wmask = i_wmask; wdata = i_wdata; pdone = i_pd[0];
      rd_known_prev = i_rden[0] ? ((i_addr[5:2] < 14) ? 1 : 0) : rd_known_prev;   // o_rdata is defined from the first read of an existing register on
      for (int k = 0; k < 34; k++) o_prev[k] = o[k];
      n++;
    end
    @(negedge clk);
    $display("ORACLE cfgregs_neo cycles %0d mismatches %0d", n, bad);
    $finish;
  end

endmodule
