// tb_oracle_b.sv -- replays, cycle by cycle, the files written by tb_rtl_ref_gvu_bdump (and mulb.txt of
// tb_rtl_ref_gvu_fu --dump) on the RTL of the LUT unit, the comparator, the 48x40 multiplier, the stream registers, the
// Scratchpad and the load / store unit, and counts the cycles whose RTL outputs differ from the outputs of the ported C++
// block stored in the file. Each top prints "ORACLE <block> cycles <n> mismatches <m>" and DETAIL / LATENCY / DIRECTED lines.
// Built with Verilator 5 (--binary --timing). Plusarg +dir=<vector directory>.
`timescale 1ns/1ps

// gvu_lut_unit with one lane. Phase 1: the four tables are placed directly in gvu_lut_mem (hierarchical assignment), then
// every output is compared in every cycle of lutb.txt. Phase 2: lutb_wr.txt drives the write port; o_init_done, the
// write address and the write counter are compared in every cycle and the four tables with lutb_mem.txt at the end.
module tb_b_lut;
  import gvu_pkg::*;
  string dir;
  integer fd, n, rc, bad, bad_hs, bad_data, bad_es, bad_ad, nw, bad_w, bad_mem;
  logic clk = 0, rstn = 0;
  always #5 clk = ~clk;
  logic wen; lut_type_e wsel, rsel; logic [127:0] wdata; logic init_done;
  logic v, rdy, o_rdy, o_v, comp, o_ad; lut_index_type_e mode; logic [4:0] base, W, sh;
  logic [0:0][7:0] i8, o8; logic [0:0][31:0] i32; logic [0:0][15:0] o16; logic [0:0][4:0] oes; logic [0:0] oz;
  gvu_lut_unit #(.NUM_LANE(1)) u (.i_clk(clk), .i_rstn(rstn), .i_pipe_flush(1'b0), .i_lut_wen(wen), .i_lut_wsel(wsel), .i_lut_wdata(wdata), .o_init_done(init_done),
    .i_lookup_valid(v), .o_lookup_ready(o_rdy), .i_lut_rsel(rsel), .i_lut_idx_mode(mode), .i_lut_compens(comp), .i_base(base), .i_W(W), .i_lerp_shift(sh),
    .i_8b_lookup_idx(i8), .i_32b_lookup_idx(i32), .o_lookup_valid(o_v), .i_lookup_ready(rdy),
    .o_8b_lookup_data(o8), .o_16b_lookup_data(o16), .o_e_shift(oes), .o_pipe_almost_done(o_ad));
  logic [31:0] f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, e_rdy, e_v, e_z, e_8, e_16, e_es, e_ad, e_done, e_wa, e_wc, t0, t1, t2, t3; logic [127:0] w;
  logic bad_now;
  initial begin
    if (!$value$plusargs("dir=%s", dir)) dir = ".";
    wen = 0; v = 0; rdy = 1; comp = 0; rsel = LUT_RECIP; wsel = LUT_ACT; mode = LUT_PWL; base = 0; W = 16; sh = 8; i8 = '0; i32 = '0; wdata = '0;
    repeat (3) @(negedge clk); rstn = 1;
    fd = $fopen({dir, "/lutb_tab.txt"}, "r");
    for (int k = 0; k < 512; k++) begin
      rc = $fscanf(fd, "%h %h %h %h\n", t0, t1, t2, t3);
      if (k < 256) begin u.i_lut_mem.lut_act_mem[k] = t0[7:0]; u.i_lut_mem.lut_exp_mem[k] = t1[7:0]; u.i_lut_mem.lut_recip_mem[k] = t2[15:0]; end
      u.i_lut_mem.lut_rsqrt_mem[k] = t3[15:0];
    end
    $fclose(fd);
    fd = $fopen({dir, "/lutb.txt"}, "r"); n = 0; bad = 0; bad_hs = 0; bad_data = 0; bad_es = 0; bad_ad = 0;
    while (fd != 0 && !$feof(fd)) begin
      rc = $fscanf(fd, "%h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h\n", f0, f1, f2, f9, f3, f4, f5, f6, f7, f8, e_rdy, e_v, e_z, e_8, e_16, e_es, e_ad); if (rc != 17) break;
      @(negedge clk); v = f0[0]; rsel = lut_type_e'(f1[1:0]); mode = lut_index_type_e'(f2[0]); comp = f9[0]; base = f3[4:0]; W = f4[4:0]; sh = f5[4:0]; i8[0] = f6[7:0]; i32[0] = f7; rdy = f8[0];
      #1; n++; bad_now = 0;
      if (o_rdy !== e_rdy[0] || o_v !== e_v[0]) begin bad_hs++; bad_now = 1; end
      if (o_ad !== e_ad[0]) begin bad_ad++; bad_now = 1; end
      if (e_v[0]) begin
        if (f1[1:0] < 2) begin if (o8[0] !== e_8[7:0]) begin bad_data++; bad_now = 1; end end
        else begin
          if (o16[0] !== e_16[15:0]) begin bad_data++; bad_now = 1; end
          if (oes[0] !== e_es[4:0]) begin bad_es++; bad_now = 1; end
        end
      end
      if (bad_now) begin bad++; if (bad <= 12) $display("FIRST lut cycle %0d sel %0d mode %0d comp %0d base %0d W %0d sh %0d i8 %h x %h valid %0d ready %0d: rtl ready %0d valid %0d o8 %h o16 %h es %0d ad %0d, file ready %0d valid %0d o8 %h o16 %h es %0d ad %0d", n, f1[1:0], f2[0], f9[0], f3[4:0], f4[4:0], f5[4:0], f6[7:0], f7, f0[0], f8[0], o_rdy, o_v, o8[0], o16[0], oes[0], o_ad, e_rdy[0], e_v[0], e_8[7:0], e_16[15:0], e_es[4:0], e_ad[0]); end
    end
    $fclose(fd);
    $display("ORACLE lut cycles %0d mismatches %0d", n, bad);
    $display("DETAIL lut handshake %0d data %0d e_shift %0d almost_done %0d", bad_hs, bad_data, bad_es, bad_ad);
    @(negedge clk); v = 0;
    fd = $fopen({dir, "/lutb_wr.txt"}, "r"); nw = 0; bad_w = 0;
    while (fd != 0 && !$feof(fd)) begin
      rc = $fscanf(fd, "%h %h %h %h %h %h\n", f0, f1, w, e_done, e_wa, e_wc); if (rc != 6) break;
      @(negedge clk); wen = f0[0]; wsel = lut_type_e'(f1[1:0]); wdata = w;
      #1; nw++;
      if (init_done !== e_done[0] || u.i_lut_mem.lut_waddr_r !== e_wa[8:0] || u.i_lut_mem.wr_cnt_r !== e_wc[2:0]) begin
        bad_w++; if (bad_w <= 8) $display("FIRST lut write cycle %0d wen %0d wsel %0d: rtl done %0d addr %0d cnt %0d, file done %0d addr %0d cnt %0d", nw, f0[0], f1[1:0], init_done, u.i_lut_mem.lut_waddr_r, u.i_lut_mem.wr_cnt_r, e_done[0], e_wa[8:0], e_wc[2:0]);
      end
    end
    $fclose(fd);
    @(negedge clk); wen = 0; #1;
    fd = $fopen({dir, "/lutb_mem.txt"}, "r"); bad_mem = 0;
    for (int k = 0; k < 512; k++) begin
      rc = $fscanf(fd, "%h %h %h %h\n", t0, t1, t2, t3);
      if (k < 256 && (u.i_lut_mem.lut_act_mem[k] !== t0[7:0] || u.i_lut_mem.lut_exp_mem[k] !== t1[7:0] || u.i_lut_mem.lut_recip_mem[k] !== t2[15:0])) bad_mem++;
      if (u.i_lut_mem.lut_rsqrt_mem[k] !== t3[15:0]) bad_mem++;
    end
    $display("ORACLE lut_write cycles %0d mismatches %0d", nw, bad_w + bad_mem);
    $display("DETAIL lut_write init_done_or_counters %0d table_entries %0d", bad_w, bad_mem);
    $finish;
  end
endmodule

// gvu_vcmp32 with two lanes, both operating modes, random valid / ready. Every output in every cycle.
module tb_b_cmp_core #(parameter bit OUT_PIPE = 1'b0, parameter string FILE = "/cmpb0.txt", parameter string NAME = "cmp_out_pipe_0");
  import gvu_pkg::*;
  string dir;
  integer fd, n, rc, bad, bad_hs, bad_res;
  logic clk = 0, rst_n = 0;
  always #5 clk = ~clk;
  logic iv, ir, ov, ordy, sg, bc, fi, la; cmp_mode_e cm; cmp_op_e op; elem_w_e ew; logic [1:0][31:0] s0, s1, r;
  gvu_vcmp32 #(.OUT_PIPE(OUT_PIPE), .LANES(2)) u (.clk, .rst_n, .in_valid(iv), .in_ready(ir), .in_cmp_mode(cm), .in_op(op), .in_elem_w(ew), .in_signed(sg),
    .in_src1_bcast(bc), .in_first(fi), .in_last(la), .in_src0(s0), .in_src1(s1), .out_valid(ov), .out_ready(ordy), .out_res(r));
  logic [31:0] g [0:16];
  initial begin
    if (!$value$plusargs("dir=%s", dir)) dir = ".";
    iv = 0; ordy = 1; cm = CMP_ELEMENTWISE; op = CMP_MAX; ew = EW_32; sg = 0; bc = 0; fi = 0; la = 0; s0 = '0; s1 = '0;
    repeat (3) @(negedge clk); rst_n = 1;
    fd = $fopen({dir, FILE}, "r"); n = 0; bad = 0; bad_hs = 0; bad_res = 0;
    while (fd != 0 && !$feof(fd)) begin
      rc = $fscanf(fd, "%h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h\n", g[0], g[1], g[2], g[3], g[4], g[5], g[6], g[7], g[8], g[9], g[10], g[11], g[12], g[13], g[14], g[15], g[16]); if (rc != 17) break;
      @(negedge clk); iv = g[0][0]; cm = cmp_mode_e'(g[1][0]); op = cmp_op_e'(g[2][0]); ew = elem_w_e'(g[3][1:0]); sg = g[4][0]; bc = g[5][0]; fi = g[6][0]; la = g[7][0];
      s0[0] = g[8]; s0[1] = g[9]; s1[0] = g[10]; s1[1] = g[11]; ordy = g[12][0];
      #1; n++;
      if (ir !== g[13][0] || ov !== g[14][0]) begin bad_hs++; if (bad_hs <= 6) $display("FIRST %s cycle %0d: rtl in_ready %0d out_valid %0d, file %0d %0d", NAME, n, ir, ov, g[13][0], g[14][0]); end
      // the result is compared when it is valid, and always with the output register (its contents are visible)
      else if ((g[14][0] || OUT_PIPE) && (r[0] !== g[15] || r[1] !== g[16])) begin bad_res++; if (bad_res <= 6) $display("FIRST %s cycle %0d: rtl res %h %h, file %h %h", NAME, n, r[1], r[0], g[16], g[15]); end
    end
    bad = bad_hs + bad_res;
    $display("ORACLE %s cycles %0d mismatches %0d", NAME, n, bad);
    $display("DETAIL %s handshake %0d result %0d", NAME, bad_hs, bad_res);
    $finish;
  end
endmodule
module tb_b_cmp0; tb_b_cmp_core #(.OUT_PIPE(1'b0), .FILE("/cmpb0.txt"), .NAME("cmp_out_pipe_0")) t (); endmodule
module tb_b_cmp1; tb_b_cmp_core #(.OUT_PIPE(1'b1), .FILE("/cmpb1.txt"), .NAME("cmp_out_pipe_1")) t (); endmodule

// gvu_vmul48x40, one lane, the four modes through their own interface (0: 40x32 and 32x8; 1: 32x32 with 16x8; 2: three
// 16x8). One beat at a time: value and latency (cycles from the accepted beat to out_valid). Then one directed case: the
// mode input changes to an interface-1 mode while an interface-0 beat is in flight.
module tb_b_mulb;
  import gvu_pkg::*;
  string dir;
  integer fd, n, bad, rc, lat, k;
  integer lmin [0:3], lmax [0:3], cnt [0:3], badm [0:3];
  logic clk = 0, rst_n = 0;
  always #5 clk = ~clk;
  logic v0, r0, ov0; logic [1:0] v1, r1, ov1; logic [2:0] v2, r2, ov2; mulb_mode_e md;
  logic [0:0][47:0] a; logic [0:0][39:0] b; logic [0:0][71:0] p; logic [0:0][31:0] aux;
  logic or0; logic [1:0] or1; logic [2:0] or2;
  gvu_vmul48x40 #(.LANES(1)) u (.clk, .rst_n, .in_valid_0(v0), .in_ready_0(r0), .in_valid_1(v1), .in_ready_1(r1), .in_valid_2(v2), .in_ready_2(r2),
    .in_mode(md), .in_src0(a), .in_src1(b), .out_valid_0(ov0), .out_ready_0(or0), .out_mode_0(), .out_valid_1(ov1), .out_ready_1(or1), .out_mode_1(),
    .out_valid_2(ov2), .out_ready_2(or2), .out_mode_2(), .out_prod(p), .out_aux(aux));
  logic [31:0] m, e_aux; logic [47:0] xa; logic [39:0] xb; logic [71:0] e72, exp_a; logic rdy_now, vld_now, wrong;
  function automatic logic in_rdy(input logic [1:0] mm); in_rdy = (mm == 2'd1) ? (&r1) : (mm == 2'd3) ? (&r2) : r0; endfunction
  function automatic logic out_vld(input logic [1:0] mm); out_vld = (mm == 2'd1) ? (&ov1) : (mm == 2'd3) ? (&ov2) : ov0; endfunction
  initial begin
    if (!$value$plusargs("dir=%s", dir)) dir = ".";
    for (int j = 0; j < 4; j++) begin lmin[j] = 99; lmax[j] = 0; cnt[j] = 0; badm[j] = 0; end
    v0 = 0; v1 = '0; v2 = '0; or0 = 1; or1 = '1; or2 = '1; md = MB_40X32; a = '0; b = '0;
    repeat (3) @(negedge clk); rst_n = 1;
    fd = $fopen({dir, "/mulb.txt"}, "r"); n = 0; bad = 0;
    while (fd != 0 && !$feof(fd)) begin
      rc = $fscanf(fd, "%h %h %h %h %h\n", m, xa, xb, e72, e_aux); if (rc != 5) break;
      k = m[1:0];
      @(negedge clk); md = mulb_mode_e'(m[1:0]); a[0] = xa; b[0] = xb; lat = 0; #1;
      while (in_rdy(m[1:0]) !== 1'b1 && lat < 16) begin @(negedge clk); #1; lat++; end
      n++; cnt[k]++;
      if (in_rdy(m[1:0]) !== 1'b1) begin bad++; badm[k]++; continue; end
      if (k == 1) v1 = 2'b11; else if (k == 3) v2 = 3'b111; else v0 = 1;
      @(negedge clk); v0 = 0; v1 = '0; v2 = '0; lat = 1; #1;
      while (out_vld(m[1:0]) !== 1'b1 && lat < 16) begin @(negedge clk); #1; lat++; end
      wrong = (out_vld(m[1:0]) !== 1'b1) || (p[0] !== e72) || (k == 1 && aux[0] !== e_aux);
      if (wrong) begin bad++; badm[k]++; if (bad <= 8) $display("FIRST mul48x40 mode %0d a %h b %h: rtl valid %0d prod %h aux %h, file prod %h aux %h", k, xa, xb, out_vld(m[1:0]), p[0], aux[0], e72, e_aux); end
      if (lat < lmin[k]) lmin[k] = lat;
      if (lat > lmax[k]) lmax[k] = lat;
    end
    $fclose(fd);
    $display("ORACLE mul48x40 (four modes) vectors %0d mismatches %0d", n, bad);
    $display("DETAIL mul48x40 vectors / mismatches: 40x32 %0d / %0d, 32x32_16x8 %0d / %0d, 32x8 %0d / %0d, 3x16x8 %0d / %0d", cnt[0], badm[0], cnt[1], badm[1], cnt[2], badm[2], cnt[3], badm[3]);
    $display("LATENCY mul48x40 40x32 %0d..%0d 32x32_16x8 %0d..%0d 32x8 %0d..%0d 3x16x8 %0d..%0d", lmin[0], lmax[0], lmin[1], lmax[1], lmin[2], lmax[2], lmin[3], lmax[3]);
    // directed: a 40x32 beat is accepted, then in_mode moves to 32x32_16x8 (same latency, so no stall) with beats on interface 1
    repeat (8) @(negedge clk);
    md = MB_40X32; a[0] = 48'h0000_1234_5678; b[0] = 40'h00_0000_0011; #1;
    exp_a = 72'($signed(40'h00_1234_5678) * $signed(32'h0000_0011));
    v0 = 1; @(negedge clk); v0 = 0;
    md = MB_32X32_16X8; a[0] = 48'h0003_0000_0007; b[0] = 40'h05_0000_0009; v1 = 2'b11; #1; rdy_now = &r1;
    lat = 0; vld_now = 0;
    repeat (6) begin @(negedge clk); #1; lat++; if (ov0) vld_now = 1; end
    v1 = '0;
    $display("DIRECTED mul48x40 mode change with a beat in flight: interface 1 ready in the next cycle %0d; out_valid_0 seen while in_mode = 32x32_16x8 (6 cycles) %0d", rdy_now, vld_now);
    md = MB_40X32; lat = 0; #1;
    while (ov0 !== 1'b1 && lat < 16) begin @(negedge clk); #1; lat++; end
    $display("DIRECTED mul48x40 back to 40x32: out_valid_0 %0d after %0d cycles, out_prod %h, product of the 40x32 beat %h, equal %0d", ov0, lat, p[0], exp_a, p[0] === exp_a);
    $finish;
  end
endmodule

// stream_vrf: three v32int64 and four v32int32 stream registers with their narrow views. Port order of the file:
// vint64[0..2], vint32[0..5], vint16[0..5], vint8[0..3]. Every ready, valid and data output in every cycle.
module tb_b_svrf;
  string dir;
  integer fd, n, rc, bad, bad_hs, bad_data, okr;
  logic clk = 0, rstn = 0;
  always #5 clk = ~clk;
  logic [1:0] c32; logic c64_0, c64_2;
  logic [2:0] f64, v64, r64, ov64, or64; logic [2:0][2047:0] d64, od64;
  logic [5:0] f32, v32, r32, ov32, or32; logic [5:0][1023:0] d32, od32;
  logic [5:0] f16, v16, r16, ov16, or16; logic [5:0][511:0] d16, od16;
  logic [3:0] f8, v8, r8, ov8, or8; logic [3:0][255:0] d8, od8;
  stream_vrf u (.i_clk(clk), .i_rstn(rstn), .i_vint32_0_cfg_mode(c32), .i_vint64_0_cfg_mode(c64_0), .i_vint64_2_cfg_mode(c64_2),
    .i_vint64_flush(f64), .i_vint64_valid(v64), .o_vint64_ready(or64), .i_vint64_data(d64), .o_vint64_valid(ov64), .i_vint64_ready(r64), .o_vint64_data(od64),
    .i_vint32_flush(f32), .i_vint32_valid(v32), .o_vint32_ready(or32), .i_vint32_data(d32), .o_vint32_valid(ov32), .i_vint32_ready(r32), .o_vint32_data(od32),
    .i_vint16_flush(f16), .i_vint16_valid(v16), .o_vint16_ready(or16), .i_vint16_data(d16), .o_vint16_valid(ov16), .i_vint16_ready(r16), .o_vint16_data(od16),
    .i_vint8_flush(f8), .i_vint8_valid(v8), .o_vint8_ready(or8), .i_vint8_data(d8), .o_vint8_valid(ov8), .i_vint8_ready(r8), .o_vint8_data(od8));
  logic [31:0] g0, g1, g2; logic [31:0] pf [0:18], pv [0:18], pr [0:18], eor [0:18], eov [0:18]; logic [2047:0] pd [0:18], eod [0:18];
  logic hs_now, data_now;
  initial begin
    if (!$value$plusargs("dir=%s", dir)) dir = ".";
    c32 = 0; c64_0 = 0; c64_2 = 0; f64 = '0; v64 = '0; r64 = '0; d64 = '0; f32 = '0; v32 = '0; r32 = '0; d32 = '0; f16 = '0; v16 = '0; r16 = '0; d16 = '0; f8 = '0; v8 = '0; r8 = '0; d8 = '0;
    repeat (3) @(negedge clk); rstn = 1;
    fd = $fopen({dir, "/svrf.txt"}, "r"); n = 0; bad = 0; bad_hs = 0; bad_data = 0;
    while (fd != 0 && !$feof(fd)) begin
      rc = $fscanf(fd, "%h %h %h", g0, g1, g2); if (rc != 3) break;
      okr = 1;
      for (int k = 0; k < 19; k++) begin rc = $fscanf(fd, "%h %h %h %h", pf[k], pv[k], pr[k], pd[k]); if (rc != 4) okr = 0; end
      for (int k = 0; k < 19; k++) begin rc = $fscanf(fd, "%h %h %h", eor[k], eov[k], eod[k]); if (rc != 3) okr = 0; end
      rc = $fscanf(fd, "\n");
      if (!okr) break;
      @(negedge clk); c32 = g0[1:0]; c64_0 = g1[0]; c64_2 = g2[0];
      for (int k = 0; k < 3; k++) begin f64[k] = pf[k][0]; v64[k] = pv[k][0]; r64[k] = pr[k][0]; d64[k] = pd[k]; end
      for (int k = 0; k < 6; k++) begin f32[k] = pf[3+k][0]; v32[k] = pv[3+k][0]; r32[k] = pr[3+k][0]; d32[k] = pd[3+k][1023:0]; end
      for (int k = 0; k < 6; k++) begin f16[k] = pf[9+k][0]; v16[k] = pv[9+k][0]; r16[k] = pr[9+k][0]; d16[k] = pd[9+k][511:0]; end
      for (int k = 0; k < 4; k++) begin f8[k] = pf[15+k][0]; v8[k] = pv[15+k][0]; r8[k] = pr[15+k][0]; d8[k] = pd[15+k][255:0]; end
      #1; n++; hs_now = 0; data_now = 0;
      for (int k = 0; k < 3; k++) begin if (or64[k] !== eor[k][0] || ov64[k] !== eov[k][0]) hs_now = 1; if (od64[k] !== eod[k]) data_now = 1; end
      for (int k = 0; k < 6; k++) begin if (or32[k] !== eor[3+k][0] || ov32[k] !== eov[3+k][0]) hs_now = 1; if (od32[k] !== eod[3+k][1023:0]) data_now = 1; end
      for (int k = 0; k < 6; k++) begin if (or16[k] !== eor[9+k][0] || ov16[k] !== eov[9+k][0]) hs_now = 1; if (od16[k] !== eod[9+k][511:0]) data_now = 1; end
      for (int k = 0; k < 4; k++) begin if (or8[k] !== eor[15+k][0] || ov8[k] !== eov[15+k][0]) hs_now = 1; if (od8[k] !== eod[15+k][255:0]) data_now = 1; end
      if (hs_now) bad_hs++;
      if (data_now) bad_data++;
      if (hs_now || data_now) begin
        bad++;
        if (bad <= 6) $display("FIRST stream_vrf cycle %0d cfg %0d %0d %0d: rtl ready/valid 64 %b %b 32 %b %b 16 %b %b 8 %b %b", n, g0[1:0], g1[0], g2[0], or64, ov64, or32, ov32, or16, ov16, or8, ov8);
      end
    end
    $display("ORACLE stream_vrf cycles %0d mismatches %0d", n, bad);
    $display("DETAIL stream_vrf ready_or_valid %0d data %0d", bad_hs, bad_data);
    $finish;
  end
endmodule

// gvu_scratchpad with the bank depths of gvu_pkg.sv and 32 lanes: every accelerator input is applied before the edge and
// the eight read outputs of the same cycle are compared (read data follows its address by two clocks).
module tb_b_spad;
  import gvu_pkg::*;
  string dir;
  integer fd, n, rc, bad, bs, b0, b1, b2;
  logic clk = 0, rstn = 0;
  always #5 clk = ~clk;
  logic sb_sel, s_en0, s_wen0, s_en1, s_wen1, s_wm1; logic [SBANK_AW-1:0] s_a0; logic [SBANK_AW+1:0] s_a1; logic [3:0] s_wm0; logic [31:0] s_wd0, s_rd0; logic [7:0] s_wd1, s_rd1;
  logic v0_sel, v0_en, v0_wen; logic [VBANK0_AW-1:0] v0_a; logic [31:0] v0_wm; logic [255:0] v0_wd, v0_rd;
  logic v1_sel; logic [1:0] v1_ps; logic v1_en0, v1_wen0, v1_en1, v1_wen1, v1_en2, v1_wen2;
  logic [VBANK1_AW-1:0] v1_a0; logic [VBANK1_AW:0] v1_a1; logic [VBANK1_AW+1:0] v1_a2;
  logic [127:0] v1_wm0; logic [1023:0] v1_wd0, v1_rd0; logic [63:0] v1_wm1; logic [511:0] v1_wd1, v1_rd1; logic [31:0] v1_wm2; logic [255:0] v1_wd2, v1_rd2;
  logic v2_sel, v2_ps, v2_en0, v2_wen0, v2_en1, v2_wen1; logic [VBANK2_AW-1:0] v2_a0; logic [VBANK2_AW+1:0] v2_a1;
  logic [127:0] v2_wm0; logic [1023:0] v2_wd0, v2_rd0; logic [31:0] v2_wm1; logic [255:0] v2_wd1, v2_rd1;
  gvu_scratchpad u (.i_clk(clk), .i_rstn(rstn),
    .i_sbank_buf_sel(sb_sel), .i_sbank_en_0(s_en0), .i_sbank_wen_0(s_wen0), .i_sbank_addr_0(s_a0), .i_sbank_wmask_0(s_wm0), .i_sbank_wdata_0(s_wd0), .o_sbank_rdata_0(s_rd0),
    .i_sbank_en_1(s_en1), .i_sbank_wen_1(s_wen1), .i_sbank_addr_1(s_a1), .i_sbank_wmask_1(s_wm1), .i_sbank_wdata_1(s_wd1), .o_sbank_rdata_1(s_rd1),
    .i_vbank0_buf_sel(v0_sel), .i_vbank0_en(v0_en), .i_vbank0_wen(v0_wen), .i_vbank0_addr(v0_a), .i_vbank0_wmask(v0_wm), .i_vbank0_wdata(v0_wd), .o_vbank0_rdata(v0_rd),
    .i_vbank1_buf_sel(v1_sel), .i_vbank1_port_sel(v1_ps),
    .i_vbank1_en_0(v1_en0), .i_vbank1_wen_0(v1_wen0), .i_vbank1_addr_0(v1_a0), .i_vbank1_wmask_0(v1_wm0), .i_vbank1_wdata_0(v1_wd0), .o_vbank1_rdata_0(v1_rd0),
    .i_vbank1_en_1(v1_en1), .i_vbank1_wen_1(v1_wen1), .i_vbank1_addr_1(v1_a1), .i_vbank1_wmask_1(v1_wm1), .i_vbank1_wdata_1(v1_wd1), .o_vbank1_rdata_1(v1_rd1),
    .i_vbank1_en_2(v1_en2), .i_vbank1_wen_2(v1_wen2), .i_vbank1_addr_2(v1_a2), .i_vbank1_wmask_2(v1_wm2), .i_vbank1_wdata_2(v1_wd2), .o_vbank1_rdata_2(v1_rd2),
    .i_vbank2_buf_sel(v2_sel), .i_vbank2_port_sel(v2_ps),
    .i_vbank2_en_0(v2_en0), .i_vbank2_wen_0(v2_wen0), .i_vbank2_addr_0(v2_a0), .i_vbank2_wmask_0(v2_wm0), .i_vbank2_wdata_0(v2_wd0), .o_vbank2_rdata_0(v2_rd0),
    .i_vbank2_en_1(v2_en1), .i_vbank2_wen_1(v2_wen1), .i_vbank2_addr_1(v2_a1), .i_vbank2_wmask_1(v2_wm1), .i_vbank2_wdata_1(v2_wd1), .o_vbank2_rdata_1(v2_rd1),
    .i_dma_sbank_en(1'b0), .i_dma_sbank_wen(1'b0), .i_dma_sbank_addr('0), .i_dma_sbank_wmask('0), .i_dma_sbank_wdata('0), .o_dma_sbank_rdata(),
    .i_dma_vbank_en(1'b0), .i_dma_vbank_wen(1'b0), .i_dma_vbank_addr('0), .i_dma_vbank_wdata('0), .i_dma_vbank_wmask('0), .o_dma_vbank_rdata());
  logic [31:0] g [0:40]; logic [1023:0] w [0:8], e [0:5]; logic [31:0] e_s0, e_s1;
  initial begin
    if (!$value$plusargs("dir=%s", dir)) dir = ".";
    // reset for two edges (the word-address delay registers of the narrow ports have one), then the memories and their
    // read registers, which have no reset, are cleared: the port starts from zero, Verilator too
    s_a1 = '0; v1_a1 = '0; v1_a2 = '0; v2_a1 = '0;   // the narrow ports delay their address by two edges: start from 0
    rstn = 0; @(negedge clk); @(negedge clk);
`ifndef VERILATOR
    for (int k = 0; k < SBANK_DEPTH; k++) begin u.i_sbank_buf0.mem[k] = '0; u.i_sbank_buf1.mem[k] = '0; end
    for (int k = 0; k < VBANK0_DEPTH; k++) begin u.i_vbank0_buf0.mem[k] = '0; u.i_vbank0_buf1.mem[k] = '0; end
    for (int k = 0; k < VBANK1_DEPTH; k++) begin u.i_vbank1_buf0.mem[k] = '0; u.i_vbank1_buf1.mem[k] = '0; end
    for (int k = 0; k < VBANK2_DEPTH; k++) begin u.i_vbank2_buf0.mem[k] = '0; u.i_vbank2_buf1.mem[k] = '0; end
    u.i_sbank_buf0.o_rdata_0 = '0; u.i_sbank_buf0.o_rdata_1 = '0; u.i_sbank_buf1.o_rdata_0 = '0; u.i_sbank_buf1.o_rdata_1 = '0;
    u.i_vbank0_buf0.o_rdata = '0; u.i_vbank0_buf1.o_rdata = '0; u.i_vbank1_buf0.o_rdata = '0; u.i_vbank1_buf1.o_rdata = '0;
    u.i_vbank2_buf0.o_rdata = '0; u.i_vbank2_buf1.o_rdata = '0;
`endif
    rstn = 1;
    fd = $fopen({dir, "/spadb.txt"}, "r"); n = 0; bad = 0; bs = 0; b0 = 0; b1 = 0; b2 = 0;
    while (fd != 0 && !$feof(fd)) begin
      rc = $fscanf(fd, "%h %h %h %h %h %h %h %h %h %h %h", g[0], g[1], g[2], g[3], g[4], g[5], g[6], g[7], g[8], g[9], g[10]); if (rc != 11) break;
      rc = $fscanf(fd, "%h %h %h %h %h %h", g[11], g[12], g[13], g[14], g[15], w[0]); if (rc != 6) break;
      rc = $fscanf(fd, "%h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h", g[16], g[17], g[18], g[19], g[20], w[1], w[2], g[21], g[22], g[23], w[3], w[4], g[24], g[25], g[26], g[27], w[5]); if (rc != 17) break;
      rc = $fscanf(fd, "%h %h %h %h %h %h %h %h %h %h %h %h", g[28], g[29], g[30], g[31], g[32], w[6], w[7], g[33], g[34], g[35], g[36], w[8]); if (rc != 12) break;
      rc = $fscanf(fd, "%h %h %h %h %h %h %h %h\n", e_s0, e_s1, e[0], e[1], e[2], e[3], e[4], e[5]); if (rc != 8) break;
      @(negedge clk);
      sb_sel = g[0][0]; s_en0 = g[1][0]; s_wen0 = g[2][0]; s_a0 = g[3][SBANK_AW-1:0]; s_wm0 = g[4][3:0]; s_wd0 = g[5]; s_en1 = g[6][0]; s_wen1 = g[7][0]; s_a1 = g[8][SBANK_AW+1:0]; s_wm1 = g[9][0]; s_wd1 = g[10][7:0];
      v0_sel = g[11][0]; v0_en = g[12][0]; v0_wen = g[13][0]; v0_a = g[14][VBANK0_AW-1:0]; v0_wm = g[15]; v0_wd = w[0][255:0];
      v1_sel = g[16][0]; v1_ps = g[17][1:0]; v1_en0 = g[18][0]; v1_wen0 = g[19][0]; v1_a0 = g[20][VBANK1_AW-1:0]; v1_wm0 = w[1][127:0]; v1_wd0 = w[2];
      v1_en1 = g[21][0]; v1_wen1 = g[22][0]; v1_a1 = g[23][VBANK1_AW:0]; v1_wm1 = w[3][63:0]; v1_wd1 = w[4][511:0];
      v1_en2 = g[24][0]; v1_wen2 = g[25][0]; v1_a2 = g[26][VBANK1_AW+1:0]; v1_wm2 = g[27]; v1_wd2 = w[5][255:0];
      v2_sel = g[28][0]; v2_ps = g[29][0]; v2_en0 = g[30][0]; v2_wen0 = g[31][0]; v2_a0 = g[32][VBANK2_AW-1:0]; v2_wm0 = w[6][127:0]; v2_wd0 = w[7];
      v2_en1 = g[33][0]; v2_wen1 = g[34][0]; v2_a1 = g[35][VBANK2_AW+1:0]; v2_wm1 = g[36]; v2_wd1 = w[8][255:0];
      #1; n++;
      if (s_rd0 !== e_s0 || s_rd1 !== e_s1[7:0]) begin bs++; if (bs <= 4) $display("FIRST scratchpad sbank cycle %0d: rtl %h %h, file %h %h", n, s_rd0, s_rd1, e_s0, e_s1[7:0]); end
      if (v0_rd !== e[0][255:0]) begin b0++; if (b0 <= 4) $display("FIRST scratchpad vbank0 cycle %0d: rtl %h, file %h", n, v0_rd, e[0][255:0]); end
      if (v1_rd0 !== e[1] || v1_rd1 !== e[2][511:0] || v1_rd2 !== e[3][255:0]) begin b1++; if (b1 <= 4) $display("FIRST scratchpad vbank1 cycle %0d: rtl port 2 %h, file %h", n, v1_rd2, e[3][255:0]); end
      if (v2_rd0 !== e[4] || v2_rd1 !== e[5][255:0]) begin b2++; if (b2 <= 4) $display("FIRST scratchpad vbank2 cycle %0d: rtl port 1 %h, file %h", n, v2_rd1, e[5][255:0]); end
    end
    bad = bs + b0 + b1 + b2;
    $display("ORACLE scratchpad cycles %0d mismatches %0d", n, bad);
    $display("DETAIL scratchpad sbank %0d vbank0 %0d vbank1 %0d vbank2 %0d", bs, b0, b1, b2);
    $finish;
  end
endmodule

// gvu_vlsu: every output in every cycle (the address outputs from the second cycle on: the address registers have no reset
// and the first cycle of the file carries no request, so that they are loaded).
module tb_b_vlsu;
  import gvu_pkg::*;
  string dir;
  integer fd, n, rc, bad, bad_addr, bad_ctl;
  logic clk = 0, rstn = 0;
  always #5 clk = ~clk;
  logic s_rd0, s_wr0, s_rd1, s_wr1, s_wm1, v0_rd, v0_wr, v1_rd, v1_wr, v2_ps, v2_rd, v2_wr; logic [1:0] v1_ps;
  logic [SBANK_AW-1:0] s_sa0; logic [SBANK_AW+1:0] s_sa1; logic [3:0] s_wm0; logic [VBANK0_AW-1:0] v0_sa; logic [31:0] v0_lm, v1_lm, v2_lm;
  logic [VBANK1_AW+1:0] v1_sa; logic [VBANK2_AW+1:0] v2_sa; agu_mode_t s_md0, s_md1, v0_md, v1_md, v2_md;
  logic o_s_en0, o_s_wen0, o_s_en1, o_s_wen1, o_s_wm1, o_v0_en, o_v0_wen, o_v1_en0, o_v1_wen0, o_v1_en1, o_v1_wen1, o_v1_en2, o_v1_wen2, o_v2_ps, o_v2_en0, o_v2_wen0, o_v2_en1, o_v2_wen1;
  logic [1:0] o_v1_ps; logic [SBANK_AW-1:0] o_s_a0; logic [SBANK_AW+1:0] o_s_a1; logic [3:0] o_s_wm0; logic [VBANK0_AW-1:0] o_v0_a; logic [31:0] o_v0_wm, o_v1_wm2, o_v2_wm1;
  logic [VBANK1_AW-1:0] o_v1_a0; logic [VBANK1_AW:0] o_v1_a1; logic [VBANK1_AW+1:0] o_v1_a2; logic [127:0] o_v1_wm0, o_v2_wm0; logic [63:0] o_v1_wm1;
  logic [VBANK2_AW-1:0] o_v2_a0; logic [VBANK2_AW+1:0] o_v2_a1;
  logic agu_rst;
  gvu_vlsu u (.i_clk(clk), .i_rstn(rstn), .i_agu_addr_reset(agu_rst),
    // pooling inputs of the RTL since the 18/10 drop: not driven by the dump (no pooling stimulus), tied to 0 as Verilator
    // does for an unconnected input (a 4-state simulator would leave them floating)
    .i_pooling_enable(1'b0), .i_pooling_mode(POOLING_2X2), .i_pooling_stride('0), .i_pooling_num_w_tile('0), .i_pooling_c('0),
    .i_pooling_oh('0), .i_pooling_c_move('0), .i_pooling_h_move('0), .i_pooling_rdata('0), .i_pooling_valid('0),
    .o_pooling_window_valid(), .o_pooling_window_data(), .o_feeder_fisrt(), .o_feeder_last(), .o_pooling_last_wr(),
    .i_sbank_rd_0(s_rd0), .i_sbank_wr_0(s_wr0), .i_sbank_agu_mode_0(s_md0), .i_sbank_start_addr_0(s_sa0), .i_sbank_wmask_0(s_wm0),
    .i_sbank_rd_1(s_rd1), .i_sbank_wr_1(s_wr1), .i_sbank_agu_mode_1(s_md1), .i_sbank_start_addr_1(s_sa1), .i_sbank_wmask_1(s_wm1),
    .i_vbank0_rd(v0_rd), .i_vbank0_wr(v0_wr), .i_vbank0_agu_mode(v0_md), .i_vbank0_start_addr(v0_sa), .i_vbank0_lane_wmask(v0_lm),
    .i_vbank1_port_sel(v1_ps), .i_vbank1_rd(v1_rd), .i_vbank1_wr(v1_wr), .i_vbank1_agu_mode(v1_md), .i_vbank1_start_addr(v1_sa), .i_vbank1_lane_wmask(v1_lm),
    .i_vbank2_port_sel(v2_ps), .i_vbank2_rd(v2_rd), .i_vbank2_wr(v2_wr), .i_vbank2_agu_mode(v2_md), .i_vbank2_start_addr(v2_sa), .i_vbank2_lane_wmask(v2_lm),
    .o_sbank_en_0(o_s_en0), .o_sbank_wen_0(o_s_wen0), .o_sbank_addr_0(o_s_a0), .o_sbank_wmask_0(o_s_wm0), .o_sbank_en_1(o_s_en1), .o_sbank_wen_1(o_s_wen1), .o_sbank_addr_1(o_s_a1), .o_sbank_wmask_1(o_s_wm1),
    .o_vbank0_en(o_v0_en), .o_vbank0_wen(o_v0_wen), .o_vbank0_addr(o_v0_a), .o_vbank0_wmask(o_v0_wm),
    .o_vbank1_port_sel(o_v1_ps), .o_vbank1_en_0(o_v1_en0), .o_vbank1_wen_0(o_v1_wen0), .o_vbank1_addr_0(o_v1_a0), .o_vbank1_wmask_0(o_v1_wm0),
    .o_vbank1_en_1(o_v1_en1), .o_vbank1_wen_1(o_v1_wen1), .o_vbank1_addr_1(o_v1_a1), .o_vbank1_wmask_1(o_v1_wm1),
    .o_vbank1_en_2(o_v1_en2), .o_vbank1_wen_2(o_v1_wen2), .o_vbank1_addr_2(o_v1_a2), .o_vbank1_wmask_2(o_v1_wm2),
    .o_vbank2_port_sel(o_v2_ps), .o_vbank2_en_0(o_v2_en0), .o_vbank2_wen_0(o_v2_wen0), .o_vbank2_addr_0(o_v2_a0), .o_vbank2_wmask_0(o_v2_wm0),
    .o_vbank2_en_1(o_v2_en1), .o_vbank2_wen_1(o_v2_wen1), .o_vbank2_addr_1(o_v2_a1), .o_vbank2_wmask_1(o_v2_wm1));
  logic [31:0] g [0:26], e [0:33]; logic [127:0] m0, m1, m2;
  logic ctl_bad, addr_bad;
  initial begin
    if (!$value$plusargs("dir=%s", dir)) dir = ".";
    // reset for two edges first: the pooling counters of the RTL (18/10 on) have an asynchronous reset that a 4-state
    // simulator never sees if rstn starts at 1 (Verilator starts every register at 0)
    rstn = 0; @(negedge clk); @(negedge clk); rstn = 1;
    fd = $fopen({dir, "/vlsub.txt"}, "r"); n = 0; bad = 0; bad_addr = 0; bad_ctl = 0;
    while (fd != 0 && !$feof(fd)) begin
      rc = $fscanf(fd, "%h %h %h %h %h %h %h %h %h %h %h %h %h %h %h", g[0], g[1], g[2], g[3], g[4], g[5], g[6], g[7], g[8], g[9], g[10], g[11], g[12], g[13], g[14]); if (rc != 15) break;
      rc = $fscanf(fd, "%h %h %h %h %h %h %h %h %h %h %h %h", g[15], g[16], g[17], g[18], g[19], g[20], g[21], g[22], g[23], g[24], g[25], g[26]); if (rc != 12) break;
      rc = $fscanf(fd, "%h %h %h %h %h %h %h %h %h %h %h %h", e[0], e[1], e[2], e[3], e[4], e[5], e[6], e[7], e[8], e[9], e[10], e[11]); if (rc != 12) break;
      rc = $fscanf(fd, "%h %h %h %h %h %h %h %h %h %h %h %h %h", e[12], e[13], e[14], e[15], m0, e[16], e[17], e[18], m1, e[19], e[20], e[21], e[22]); if (rc != 13) break;
      rc = $fscanf(fd, "%h %h %h %h %h %h %h %h %h\n", e[23], e[24], e[25], e[26], m2, e[27], e[28], e[29], e[30]); if (rc != 9) break;
      @(negedge clk);
      s_rd0 = g[0][0]; s_wr0 = g[1][0]; agu_rst = g[1][1]; s_md0 = agu_mode_t'(g[2][1:0]); s_sa0 = g[3][SBANK_AW-1:0]; s_wm0 = g[4][3:0];
      s_rd1 = g[5][0]; s_wr1 = g[6][0]; s_md1 = agu_mode_t'(g[7][1:0]); s_sa1 = g[8][SBANK_AW+1:0]; s_wm1 = g[9][0];
      v0_rd = g[10][0]; v0_wr = g[11][0]; v0_md = agu_mode_t'(g[12][1:0]); v0_sa = g[13][VBANK0_AW-1:0]; v0_lm = g[14];
      v1_ps = g[15][1:0]; v1_rd = g[16][0]; v1_wr = g[17][0]; v1_md = agu_mode_t'(g[18][1:0]); v1_sa = g[19][VBANK1_AW+1:0]; v1_lm = g[20];
      v2_ps = g[21][0]; v2_rd = g[22][0]; v2_wr = g[23][0]; v2_md = agu_mode_t'(g[24][1:0]); v2_sa = g[25][VBANK2_AW+1:0]; v2_lm = g[26];
      #1; n++;
      ctl_bad = o_s_en0 !== e[0][0] || o_s_wen0 !== e[1][0] || o_s_wm0 !== e[3][3:0] || o_s_en1 !== e[4][0] || o_s_wen1 !== e[5][0] || o_s_wm1 !== e[7][0] ||
                o_v0_en !== e[8][0] || o_v0_wen !== e[9][0] || o_v0_wm !== e[11] ||
                o_v1_ps !== e[12][1:0] || o_v1_en0 !== e[13][0] || o_v1_wen0 !== e[14][0] || o_v1_wm0 !== m0 || o_v1_en1 !== e[16][0] || o_v1_wen1 !== e[17][0] || o_v1_wm1 !== m1[63:0] ||
                o_v1_en2 !== e[19][0] || o_v1_wen2 !== e[20][0] || o_v1_wm2 !== e[22] ||
                o_v2_ps !== e[23][0] || o_v2_en0 !== e[24][0] || o_v2_wen0 !== e[25][0] || o_v2_wm0 !== m2 || o_v2_en1 !== e[27][0] || o_v2_wen1 !== e[28][0] || o_v2_wm1 !== e[30];
      addr_bad = n > 1 && (o_s_a0 !== e[2][SBANK_AW-1:0] || o_s_a1 !== e[6][SBANK_AW+1:0] || o_v0_a !== e[10][VBANK0_AW-1:0] || o_v1_a0 !== e[15][VBANK1_AW-1:0] || o_v1_a1 !== e[18][VBANK1_AW:0] ||
                 o_v1_a2 !== e[21][VBANK1_AW+1:0] || o_v2_a0 !== e[26][VBANK2_AW-1:0] || o_v2_a1 !== e[29][VBANK2_AW+1:0]);
      if (ctl_bad) bad_ctl++;
      if (addr_bad) begin bad_addr++; if (bad_addr <= 6) $display("FIRST vlsu cycle %0d: rtl addr %0d %0d %0d %0d %0d %0d %0d %0d", n, o_s_a0, o_s_a1, o_v0_a, o_v1_a0, o_v1_a1, o_v1_a2, o_v2_a0, o_v2_a1); end
      if (ctl_bad || addr_bad) bad++;
    end
    $display("ORACLE vlsu cycles %0d mismatches %0d", n, bad);
    $display("DETAIL vlsu enables_or_masks %0d addresses %0d", bad_ctl, bad_addr);
    $finish;
  end
endmodule

// gvu_lut_unit with 32 lanes and its default parameters: tables placed directly in the memory, every lane with its own
// input. The handshake outputs (those of lane 0) and the almost-done output are compared in every cycle, the data and
// E_shift of all 32 lanes in every cycle with a valid output. The zero-flag column of the file is read and not used.
module tb_b_lut32;
  import gvu_pkg::*;
  string dir;
  integer fd, n, rc, bad, bad_hs, bad_data;
  logic clk = 0, rstn = 0;
  always #5 clk = ~clk;
  lut_type_e rsel; lut_index_type_e mode; logic v, rdy, o_rdy, o_v, comp, o_ad; logic [4:0] base;
  logic [31:0][7:0] i8, o8; logic [31:0][31:0] i32; logic [31:0][15:0] o16; logic [31:0][4:0] oes; logic [31:0] oz;
  gvu_lut_unit #(.NUM_LANE(32)) u (.i_clk(clk), .i_rstn(rstn), .i_pipe_flush(1'b0), .i_lut_wen(1'b0), .i_lut_wsel(LUT_ACT), .i_lut_wdata('0), .o_init_done(),
    .i_lookup_valid(v), .o_lookup_ready(o_rdy), .i_lut_rsel(rsel), .i_lut_idx_mode(mode), .i_lut_compens(comp), .i_base(base), .i_W(5'd16), .i_lerp_shift(5'd8),
    .i_8b_lookup_idx(i8), .i_32b_lookup_idx(i32), .o_lookup_valid(o_v), .i_lookup_ready(rdy),
    .o_8b_lookup_data(o8), .o_16b_lookup_data(o16), .o_e_shift(oes), .o_pipe_almost_done(o_ad));
  logic [31:0] f0, f1, f2, f3, f4, f5, e_rdy, e_v, e_ad, e_z, t0, t1, t2, t3; logic [255:0] x8, e8, ees; logic [1023:0] x32; logic [511:0] e16;
  logic hs_now, data_now;
  initial begin
    if (!$value$plusargs("dir=%s", dir)) dir = ".";
    v = 0; rdy = 1; comp = 0; rsel = LUT_RECIP; mode = LUT_PWL; base = 0; i8 = '0; i32 = '0;
    repeat (3) @(negedge clk); rstn = 1;
    fd = $fopen({dir, "/lutb_tab.txt"}, "r");
    for (int k = 0; k < 512; k++) begin
      rc = $fscanf(fd, "%h %h %h %h\n", t0, t1, t2, t3);
      if (k < 256) begin u.i_lut_mem.lut_act_mem[k] = t0[7:0]; u.i_lut_mem.lut_exp_mem[k] = t1[7:0]; u.i_lut_mem.lut_recip_mem[k] = t2[15:0]; end
      u.i_lut_mem.lut_rsqrt_mem[k] = t3[15:0];
    end
    $fclose(fd);
    fd = $fopen({dir, "/lut32.txt"}, "r"); n = 0; bad = 0; bad_hs = 0; bad_data = 0;
    while (fd != 0 && !$feof(fd)) begin
      rc = $fscanf(fd, "%h %h %h %h %h %h %h %h %h %h %h %h %h %h %h\n", f0, f1, f2, f3, f4, f5, x8, x32, e_rdy, e_v, e_ad, e8, e16, ees, e_z); if (rc != 15) break;
      @(negedge clk); v = f0[0]; rsel = lut_type_e'(f1[1:0]); mode = lut_index_type_e'(f2[0]); comp = f3[0]; base = f4[4:0]; rdy = f5[0]; i8 = x8; i32 = x32;
      #1; n++; hs_now = 0; data_now = 0;
      if (o_rdy !== e_rdy[0] || o_v !== e_v[0] || o_ad !== e_ad[0]) hs_now = 1;
      // lane data while the output is valid (without a valid beat the registers show left-over contents)
      if (e_v[0]) for (int l = 0; l < 32; l++) begin
        if (f1[1:0] < 2) begin if (o8[l] !== e8[8*l +: 8]) data_now = 1; end
        else if (o16[l] !== e16[16*l +: 16] || oes[l] !== ees[8*l +: 5]) data_now = 1;
      end
      if (hs_now) bad_hs++;
      if (data_now) bad_data++;
      if (hs_now || data_now) begin bad++; if (bad <= 6) $display("FIRST lut32 cycle %0d sel %0d mode %0d: rtl ready %0d valid %0d ad %0d o16[0] %h es[0] %0d; file %0d %0d %0d %h %0d", n, f1[1:0], f2[0], o_rdy, o_v, o_ad, o16[0], oes[0], e_rdy[0], e_v[0], e_ad[0], e16[15:0], ees[4:0]); end
    end
    $display("ORACLE lut_32_lanes cycles %0d mismatches %0d", n, bad);
    $display("DETAIL lut_32_lanes handshake %0d lane_data %0d", bad_hs, bad_data);
    $finish;
  end
endmodule
