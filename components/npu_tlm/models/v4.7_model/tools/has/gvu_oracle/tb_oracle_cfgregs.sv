// tb_oracle_cfgregs.sv -- configuration register file of the vector unit (neo_core/gvu/gvu_config_regs.sv, latest RTL drop) against the
// port has/rtl_ref_gvu_config_regs.h, cycle by cycle. Replays the file written by tools/has/tb_rtl_ref_gvu_config_regs --dump:
//   rstn addr wren rden wmask wdata lut_init_done pipe_done | rd_seen rdata pipe_en op luts bufs skip z ntile tlen az ascale ashift pmode
//   pstride pnwt pc poh pcm phm      (hex; see the header of that program)
// The inputs change on the falling edge, the outputs are compared 1 ns after the rising edge. o_rdata is compared only after the first
// read (the register has no reset: x in a 4-state simulator). Plusargs: +dir=<vector dir> +file=<name> +label=<text>.
// Prints "ORACLE cfg_regs <label> cycles <n> mismatches <m>" and up to 6 lines "FIRST cycle ..." with the differing fields.
`timescale 1ns/1ps
module tb_oracle_cfgregs;
  import gvu_pkg::*;
  logic clk = 0; always #5 clk = ~clk;
  logic rstn = 0, wren = 0, rden = 0, lut_init_done = 0, pipe_done = 0;
  logic [3:0] addr = '0; logic [31:0] wmask = '1, wdata = '0, rdata;
  logic pipe_en, lr0, lr1, lr2, lr3, sb, v0, v1, v2, skip; logic signed [7:0] z, az; gvu_opcode_t op;
  logic [8:0] ntile, tlen; logic signed [31:0] ascale; logic [7:0] ashift;
  gvu_pooling_mode_t pmode; logic [2:0] pstride; logic [7:0] pnw, pc, poh; logic [VBANK0_AW-1:0] pcm, phm;
  gvu_config_regs dut (.i_clk(clk), .i_rstn(rstn), .i_addr(addr), .i_wren(wren), .i_wmask(wmask), .i_wdata(wdata), .i_rden(rden), .o_rdata(rdata),
    .o_gvu_pipe_en(pipe_en), .o_gvu_op(op), .o_lut_act_reset(lr0), .o_lut_exp_reset(lr1), .o_lut_recip_reset(lr2), .o_lut_rsqrt_reset(lr3),
    .o_sbank_buf_sel(sb), .o_vbank0_buf_sel(v0), .o_vbank1_buf_sel(v1), .o_vbank2_buf_sel(v2), .o_gemm_fused_skip_act(skip), .o_gemm_fused_z(z),
    .o_num_tile(ntile), .o_token_len(tlen), .o_attn_z(az), .o_attn_scale(ascale), .o_attn_shift(ashift), .o_pooling_mode(pmode),
    .o_pooling_stride(pstride), .o_pooling_num_w_tile(pnw), .o_pooling_c(pc), .o_pooling_oh(poh), .o_pooling_c_move(pcm), .o_pooling_h_move(phm),
    .i_lut_init_done(lut_init_done), .i_gvu_pipe_done(pipe_done));
  string dir, fname, label, bar; integer fd, rc, n = 0, bad = 0, shown = 0;
  logic [3:0] e_addr, e_op, e_luts, e_bufs; logic e_rstn, e_wren, e_rden, e_lut, e_pd, e_seen, e_pen, e_skip;
  logic [31:0] e_wmask, e_wdata, e_rdata, e_ascale; logic [7:0] e_z, e_az, e_ashift, e_pnwt, e_pc, e_poh; logic [8:0] e_ntile, e_tlen;
  logic [3:0] e_pmode, e_pstride; logic [9:0] e_pcm, e_phm;
  logic [3:0] luts, bufs; logic ok;
  initial begin
    if (!$value$plusargs("dir=%s", dir)) dir = ".";
    if (!$value$plusargs("file=%s", fname)) fname = "cfgregs_dlv.txt";
    if (!$value$plusargs("label=%s", label)) label = "";
    fd = $fopen({dir, "/", fname}, "r");
    if (fd == 0) $fatal(1, "cannot open %s/%s", dir, fname);
    while (!$feof(fd)) begin
      rc = $fscanf(fd, "%h %h %h %h %h %h %h %h %s %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h\n",
                   e_rstn, e_addr, e_wren, e_rden, e_wmask, e_wdata, e_lut, e_pd, bar, e_seen, e_rdata, e_pen, e_op, e_luts, e_bufs, e_skip, e_z, e_ntile,
                   e_tlen, e_az, e_ascale, e_ashift, e_pmode, e_pstride, e_pnwt, e_pc, e_poh, e_pcm, e_phm);
      if (rc != 29) break;
      @(negedge clk);
      rstn = e_rstn; addr = e_addr; wren = e_wren; rden = e_rden; wmask = e_wmask; wdata = e_wdata; lut_init_done = e_lut; pipe_done = e_pd;
      @(posedge clk); #1;
      luts = {lr3, lr2, lr1, lr0}; bufs = {v2, v1, v0, sb}; n++;
      ok = (pipe_en === e_pen) && (op === gvu_opcode_t'(e_op)) && (luts === e_luts) && (bufs === e_bufs) && (skip === e_skip) &&
           (z === e_z) && (ntile === e_ntile) && (tlen === e_tlen) && (az === e_az) && (ascale === e_ascale) && (ashift === e_ashift) &&
           (pmode === gvu_pooling_mode_t'(e_pmode[1:0])) && (pstride === e_pstride[2:0]) && (pnw === e_pnwt) && (pc === e_pc) && (poh === e_poh) &&
           (pcm === e_pcm) && (phm === e_phm) && (!e_seen || rdata === e_rdata);
      if (!ok) begin
        bad++;
        if (shown < 6) begin
          shown++;
          $display("FIRST cycle %0d (rstn %0d addr %0d wr %0d rd %0d wmask %08h wdata %08h lut %0d pd %0d): rdata %08h/%08h en %0d/%0d op %0d/%0d lut %h/%h buf %h/%h skip %0d/%0d z %02h/%02h nt %0d/%0d tl %0d/%0d az %02h/%02h sc %08h/%08h sh %02h/%02h pm %0d/%0d ps %0d/%0d nw %0d/%0d c %0d/%0d oh %0d/%0d cm %0d/%0d hm %0d/%0d  (rtl/model)",
                   n, e_rstn, e_addr, e_wren, e_rden, e_wmask, e_wdata, e_lut, e_pd, rdata, e_rdata, pipe_en, e_pen, op, e_op, luts, e_luts, bufs, e_bufs,
                   skip, e_skip, z, e_z, ntile, e_ntile, tlen, e_tlen, az, e_az, ascale, e_ascale, ashift, e_ashift, pmode, e_pmode, pstride, e_pstride,
                   pnw, e_pnwt, pc, e_pc, poh, e_poh, pcm, e_pcm, phm, e_phm);
        end
      end
    end
    $display("ORACLE cfg_regs %s cycles %0d mismatches %0d", label, n, bad);
    $finish;
  end
endmodule
