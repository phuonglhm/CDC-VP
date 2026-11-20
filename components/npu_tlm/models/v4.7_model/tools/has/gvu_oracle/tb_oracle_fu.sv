// tb_oracle_fu.sv -- replays the vector files written by "tb_rtl_ref_gvu_fu --dump <dir>" on the RTL functional units of
// the vector unit and counts the beats whose RTL output differs from the value in the file (the output of the ported C++
// unit). One top module per unit; each prints "ORACLE <unit> vectors <n> mismatches <m>".
// Built with Verilator 5 (--binary --timing), see run_verilator.sh. Plusarg +dir=<vector directory>.
// The LUT unit, the Scratchpad wrapper and vrf.sv are replayed by tb_oracle_g2.sv, directed cases by tb_oracle_directed.sv.
// Not covered here: gvu_vmul48x40 modes MB_32X32_16X8 and MB_3X16X8 (pipe_en_1 has two drivers and pipe_en_2 none in the
// RTL as delivered, so their handshake is undefined).
`timescale 1ns/1ps

module tb_oracle_comb;
  import gvu_pkg::*;
  string dir;
  integer fd, n, bad, rc;
  integer badk [0:15];

  // ---- clz -------------------------------------------------------------------------------------------------------
  logic [31:0] clz_d; logic [4:0] clz_idx; logic clz_z;
  clz #(.Mode(1'b1), .DataWidth(32)) u_clz (.i_data(clz_d), .o_idx(clz_idx), .o_all_zeros(clz_z));

  // ---- adder32 ---------------------------------------------------------------------------------------------------
  adder_mode_type_e a32_mode; logic [3:0] a32_sub8; logic [1:0] a32_sub16; logic a32_sub32;
  logic [31:0] a32_a, a32_b, a32_r;
  gvu_adder32_lane u_a32 (.i_cfg_mode(a32_mode), .i_sub8_mode(a32_sub8), .i_8b_valid(4'hF), .o_8b_ready(), .o_8b_valid(), .i_8b_ready(4'hF),
    .i_sub16_mode(a32_sub16), .i_16b_valid(2'b11), .o_16b_ready(), .o_16b_valid(), .i_16b_ready(2'b11),
    .i_sub32_mode(a32_sub32), .i_32b_valid(1'b1), .o_32b_ready(), .o_32b_valid(), .i_32b_ready(1'b1),
    .i_a(a32_a), .i_b(a32_b), .o_r(a32_r));

  // ---- adder64, ADD_64_PIPE = 0 ----------------------------------------------------------------------------------
  adder_mode_type_e a64_mode; logic [1:0] a64_sub32; logic a64_sub64; logic [63:0] a64_a, a64_b, a64_r;
  gvu_adder64_lane #(.ADD_64_PIPE(1'b0), .DOUBLE_PIPE_REG(1'b0)) u_a64 (.i_clk(1'b0), .i_rstn(1'b1), .i_pipe_flush(1'b0), .i_cfg_mode(a64_mode),
    .i_sub32_mode(a64_sub32), .i_32b_valid(2'b11), .o_32b_ready(), .o_32b_valid(), .i_32b_ready(2'b11),
    .i_sub64_mode(a64_sub64), .i_64b_valid(1'b1), .o_64b_ready(), .o_64b_valid(), .i_64b_ready(1'b1),
    .i_a(a64_a), .i_b(a64_b), .o_r(a64_r));

  // ---- comparator, elementwise, OUT_PIPE = 0, one lane -----------------------------------------------------------
  elem_w_e cmp_ew; logic cmp_sg; cmp_op_e cmp_op; logic [0:0][31:0] cmp_a, cmp_b, cmp_r;
  gvu_vcmp32 #(.OUT_PIPE(1'b0), .LANES(1)) u_cmp (.clk(1'b0), .rst_n(1'b1), .in_valid(1'b1), .in_ready(), .in_cmp_mode(CMP_ELEMENTWISE),
    .in_op(cmp_op), .in_elem_w(cmp_ew), .in_signed(cmp_sg), .in_src1_bcast(1'b0), .in_first(1'b0), .in_last(1'b0),
    .in_src0(cmp_a), .in_src1(cmp_b), .out_valid(), .out_ready(1'b1), .out_res(cmp_r));

  // ---- clamp and shift: lane 0 of the packed bus -----------------------------------------------------------------
  logic signed [31:0] cl_lo, cl_hi; logic signed [0:0][31:0] cl_a, cl_y;
  gvu_vector_clamp32 #(.NUM_LANE(1)) u_clamp (.i_clamp_hi(cl_hi), .i_clamp_lo(cl_lo), .i_clamp_valid(1'b1), .o_clamp_ready(),
    .i_clamp_in(cl_a), .o_clamp_valid(), .i_clamp_ready(1'b1), .o_clamp_out(cl_y));
  gvu_data_format_t sh_fmt; gvu_shift_op_t sh_op; logic [5:0] sh_am; logic [V32_I32_BITS-1:0] sh_a, sh_y; logic sh_err;
  gvu_vector_shift u_shift (.req_valid(1'b1), .rsp_ready(1'b1), .format(sh_fmt), .op(sh_op), .amount(sh_am), .a(sh_a),
    .req_ready(), .rsp_valid(), .rsp_error(sh_err), .y(sh_y));

  function automatic logic [31:0] lane_mask(input logic [1:0] f);
    lane_mask = (f == 2'd0) ? 32'h0000_00FF : (f == 2'd1) ? 32'h0000_FFFF : 32'hFFFF_FFFF;
  endfunction

  logic [31:0] v0, v1, v2, v3, v4, v5, v6; logic [63:0] w0, w1, w2;
  initial begin
    if (!$value$plusargs("dir=%s", dir)) dir = ".";

    fd = $fopen({dir, "/clz.txt"}, "r"); n = 0; bad = 0;
    while (fd != 0 && !$feof(fd)) begin
      rc = $fscanf(fd, "%h %h %h\n", v0, v1, v2); if (rc != 3) break;
      clz_d = v0; #1; n++;
      if (clz_idx !== v1[4:0] || clz_z !== v2[0]) bad++;
    end
    $display("ORACLE clz vectors %0d mismatches %0d", n, bad);

    fd = $fopen({dir, "/adder32.txt"}, "r"); n = 0; bad = 0;
    while (fd != 0 && !$feof(fd)) begin
      rc = $fscanf(fd, "%h %h %h %h %h %h %h\n", v0, v1, v2, v3, v4, v5, v6); if (rc != 7) break;
      a32_mode = adder_mode_type_e'(v0[1:0]); a32_sub8 = v1[3:0]; a32_sub16 = v2[1:0]; a32_sub32 = v3[0]; a32_a = v4; a32_b = v5; #1; n++;
      if (a32_r !== v6) bad++;
    end
    $display("ORACLE adder32 vectors %0d mismatches %0d", n, bad);

    fd = $fopen({dir, "/adder64.txt"}, "r"); n = 0; bad = 0; for (int k = 0; k < 16; k++) badk[k] = 0;
    while (fd != 0 && !$feof(fd)) begin
      rc = $fscanf(fd, "%h %h %h %h %h %h\n", v0, v1, v2, w0, w1, w2); if (rc != 6) break;
      a64_mode = adder_mode_type_e'(v0[1:0]); a64_sub32 = v1[1:0]; a64_sub64 = v2[0]; a64_a = w0; a64_b = w1; #1; n++;
      if (a64_r !== w2) begin bad++; badk[{v0[1:0], (v0[1:0] == 2'd1) ? v1[1:0] : {1'b0, v2[0]}}]++; end
    end
    $display("ORACLE adder64 vectors %0d mismatches %0d", n, bad);
    $display("DETAIL adder64 mismatches: ADD_64 add %0d sub %0d; ADD_32 sub32=0 %0d 1 %0d 2 %0d 3 %0d; ADD_16 %0d; ADD_8 %0d", badk[0], badk[1], badk[4], badk[5], badk[6], badk[7], badk[8] + badk[9], badk[12] + badk[13]);

    fd = $fopen({dir, "/cmp.txt"}, "r"); n = 0; bad = 0;
    while (fd != 0 && !$feof(fd)) begin
      rc = $fscanf(fd, "%h %h %h %h %h %h\n", v0, v1, v2, v3, v4, v5); if (rc != 6) break;
      cmp_ew = elem_w_e'(v0[1:0]); cmp_sg = v1[0]; cmp_op = cmp_op_e'(v2[0]); cmp_a[0] = v3; cmp_b[0] = v4; #1; n++;
      if (cmp_r[0] !== v5) bad++;
    end
    $display("ORACLE cmp vectors %0d mismatches %0d", n, bad);

    fd = $fopen({dir, "/clamp32.txt"}, "r"); n = 0; bad = 0;
    while (fd != 0 && !$feof(fd)) begin
      rc = $fscanf(fd, "%h %h %h %h\n", v0, v1, v2, v3); if (rc != 4) break;
      cl_a[0] = v0; cl_lo = v1; cl_hi = v2; #1; n++;
      if (cl_y[0] !== v3) bad++;
    end
    $display("ORACLE clamp32 vectors %0d mismatches %0d", n, bad);

    fd = $fopen({dir, "/shift.txt"}, "r"); n = 0; bad = 0;
    while (fd != 0 && !$feof(fd)) begin
      rc = $fscanf(fd, "%h %h %h %h %h\n", v0, v1, v2, v3, v4); if (rc != 5) break;
      sh_fmt = gvu_data_format_t'(v0[1:0]); sh_op = gvu_shift_op_t'(v1[1:0]); sh_am = v2[5:0]; sh_a = '0; sh_a[31:0] = v3 & lane_mask(v0[1:0]); #1; n++;
      if (sh_err !== 1'b0 || (sh_y[31:0] & lane_mask(v0[1:0])) !== v4) bad++;
    end
    $display("ORACLE shift vectors %0d mismatches %0d", n, bad);
    $finish;
  end
endmodule

// Round-shift, IN_W = 64 and 72: one beat at a time. ORACLE / KNOWN keep their meaning (value and valid one cycle after the
// beat, the latency of the port); VALUE compares the value at the cycle the RTL output valid comes (up to 16 cycles), LATENCY
// prints the number of clock edges from the accepted beat to the output valid (minimum and maximum seen).
module tb_oracle_roundshift;
  import gvu_pkg::*;
  string dir;
  integer fd, n, bad, rc, kn, kbad, kbadv, badv, lat, lmin, lmax;
  logic ok1, okv;
`ifdef NEO_REV2
  localparam int EXP_LAT = 2;   // RTL revision 2: S2_PIPE = 1
`else
  localparam int EXP_LAT = 1;
`endif
  logic clk = 0, rst_n = 0;
  always #5 clk = ~clk;
  logic v64, v72; logic [5:0] s64; logic [6:0] s72; round_mode_e rm; narrow_mode_e nm; narrow_w_e nw;
  logic [0:0][63:0] d64; logic [0:0][71:0] d72; logic [0:0][31:0] o64, o72; logic [0:0] f64, f72; logic ov64, ov72;
  gvu_vroundshift64 #(.LANES(1)) u64 (.clk, .rst_n, .in_valid(v64), .in_ready(), .in_shift(s64), .in_round_mode(rm), .in_narrow_mode(nm), .in_narrow_w(nw),
    .in_data(d64), .out_valid(ov64), .out_ready(1'b1), .out_data(o64), .out_ovf(f64));
  gvu_vroundshift72 #(.LANES(1)) u72 (.clk, .rst_n, .in_valid(v72), .in_ready(), .in_shift(s72), .in_round_mode(rm), .in_narrow_mode(nm), .in_narrow_w(nw),
    .in_data(d72), .out_valid(ov72), .out_ready(1'b1), .out_data(o72), .out_ovf(f72));
  logic [31:0] a0, a1, a2, a3, e_data, e_ovf; logic [71:0] dat;
  initial begin
    if (!$value$plusargs("dir=%s", dir)) dir = ".";
    v64 = 0; v72 = 0; repeat (3) @(negedge clk); rst_n = 1;
    // Beats with a negative input and a shift other than 0 are counted apart ("KNOWN" lines): an RTL revision that takes
    // the lane out of a packed array shifts it logically there. Both counts are 0 on an RTL that shifts arithmetically.
    fd = $fopen({dir, "/rs64.txt"}, "r"); n = 0; bad = 0; kn = 0; kbad = 0; badv = 0; kbadv = 0; lmin = 99; lmax = 0;
    while (fd != 0 && !$feof(fd)) begin
      rc = $fscanf(fd, "%h %h %h %h %h %h %h\n", a0, a1, a2, a3, dat, e_data, e_ovf); if (rc != 7) break;
      @(negedge clk); s64 = a0[5:0]; rm = round_mode_e'(a1[2:0]); nm = narrow_mode_e'(a2[0]); nw = narrow_w_e'(a3[1:0]); d64[0] = dat[63:0]; v64 = 1;
      @(negedge clk); v64 = 0; lat = 1;
      while (ov64 !== 1'b1 && lat < 16) begin @(negedge clk); lat++; end
      okv = !(ov64 !== 1'b1 || o64[0] !== e_data || f64[0] !== e_ovf[0]);
      ok1 = okv && lat == EXP_LAT;   // ORACLE: value and valid at the latency of the revision
      if (lat < lmin) lmin = lat; if (lat > lmax) lmax = lat;
      if (dat[63] && a0[5:0] != 0) begin kn++; if (!ok1) kbad++; if (!okv) kbadv++; end
      else begin n++; if (!ok1) bad++; if (!okv) badv++; end
    end
    $display("ORACLE roundshift64 vectors %0d mismatches %0d", n, bad);
    $display("KNOWN roundshift64 negative input with a shift: vectors %0d differ %0d", kn, kbad);
    $display("VALUE roundshift64 vectors %0d mismatches %0d at the cycle of the output valid", n, badv);
    $display("VALUE roundshift64 negative input with a shift: vectors %0d differ %0d at the cycle of the output valid", kn, kbadv);
    $display("LATENCY roundshift64 %0d..%0d", lmin, lmax);
    fd = $fopen({dir, "/rs72.txt"}, "r"); n = 0; bad = 0; kn = 0; kbad = 0; badv = 0; kbadv = 0; lmin = 99; lmax = 0;
    while (fd != 0 && !$feof(fd)) begin
      rc = $fscanf(fd, "%h %h %h %h %h %h %h\n", a0, a1, a2, a3, dat, e_data, e_ovf); if (rc != 7) break;
      @(negedge clk); s72 = a0[6:0]; rm = round_mode_e'(a1[2:0]); nm = narrow_mode_e'(a2[0]); nw = narrow_w_e'(a3[1:0]); d72[0] = dat; v72 = 1;
      @(negedge clk); v72 = 0; lat = 1;
      while (ov72 !== 1'b1 && lat < 16) begin @(negedge clk); lat++; end
      okv = !(ov72 !== 1'b1 || o72[0] !== e_data || f72[0] !== e_ovf[0]);
      ok1 = okv && lat == EXP_LAT;   // ORACLE: value and valid at the latency of the revision
      if (lat < lmin) lmin = lat; if (lat > lmax) lmax = lat;
      if (dat[71] && a0[6:0] != 0) begin kn++; if (!ok1) kbad++; if (!okv) kbadv++; end
      else begin n++; if (!ok1) bad++; if (!okv) badv++; end
    end
    $display("KNOWN roundshift72 negative input with a shift: vectors %0d differ %0d", kn, kbad);
    $display("ORACLE roundshift72 vectors %0d mismatches %0d", n, bad);
    $display("VALUE roundshift72 vectors %0d mismatches %0d at the cycle of the output valid", n, badv);
    $display("VALUE roundshift72 negative input with a shift: vectors %0d differ %0d at the cycle of the output valid", kn, kbadv);
    $display("LATENCY roundshift72 %0d..%0d", lmin, lmax);
    $finish;
  end
endmodule

// Multipliers: one beat at a time (wait for ready, one valid cycle, wait for the output valid). The number of clock
// edges from the accepted beat to the output valid is printed per mode (minimum and maximum seen).
module tb_oracle_mula;
  import gvu_pkg::*;
  string dir;
  integer fd, n, bad, rc, lat;
  integer lmin [0:3], lmax [0:3];
  logic clk = 0, rst_n = 0;
  always #5 clk = ~clk;
  logic a_v0; logic [1:0] a_v1; mula_mode_e a_mode; logic [0:0][31:0] a_s0, a_s1; logic [0:0][63:0] a_p;
  logic a_r0, a_ov0; logic [1:0] a_r1, a_ov1;
  gvu_vmul32x32 #(.LANES(1)) u_a (.clk, .rst_n, .in_valid_0(a_v0), .in_ready_0(a_r0), .in_valid_1(a_v1), .in_ready_1(a_r1), .in_mode(a_mode),
    .in_src0(a_s0), .in_src1(a_s1), .out_valid_0(a_ov0), .out_ready_0(1'b1), .out_mode_0(), .out_valid_1(a_ov1), .out_ready_1(2'b11), .out_mode_1(), .out_prod(a_p));
  logic [31:0] m, x, y; logic [63:0] e64;
  initial begin
    if (!$value$plusargs("dir=%s", dir)) dir = ".";
    for (int k = 0; k < 4; k++) begin lmin[k] = 99; lmax[k] = 0; end
    a_v0 = 0; a_v1 = 0; a_mode = MA_32X32; repeat (3) @(negedge clk); rst_n = 1;
    fd = $fopen({dir, "/mula.txt"}, "r"); n = 0; bad = 0;
    while (fd != 0 && !$feof(fd)) begin
      rc = $fscanf(fd, "%h %h %h %h\n", m, x, y, e64); if (rc != 4) break;
      @(negedge clk); a_mode = mula_mode_e'(m[1:0]); a_s0[0] = x; a_s1[0] = y;
      if (a_mode == MA_16X16) begin
        while (a_r1 !== 2'b11) @(negedge clk);
        a_v1 = 2'b11; @(negedge clk); a_v1 = 2'b00; lat = 1;
        while (a_ov1 !== 2'b11 && lat < 16) begin @(negedge clk); lat++; end
      end else begin
        while (a_r0 !== 1'b1) @(negedge clk);
        a_v0 = 1; @(negedge clk); a_v0 = 0; lat = 1;
        while (a_ov0 !== 1'b1 && lat < 16) begin @(negedge clk); lat++; end
      end
      n++;
      if (a_p[0] !== e64) bad++;
      if (lat < lmin[m[1:0]]) lmin[m[1:0]] = lat;
      if (lat > lmax[m[1:0]]) lmax[m[1:0]] = lat;
    end
    $display("ORACLE mul32x32 vectors %0d mismatches %0d", n, bad);
    $display("LATENCY mul32x32 32x32 %0d..%0d 32x16 %0d..%0d 32x8 %0d..%0d 16x16 %0d..%0d", lmin[0], lmax[0], lmin[1], lmax[1], lmin[2], lmax[2], lmin[3], lmax[3]);
    $finish;
  end
endmodule

// gvu_vmul48x40, interface 0 only (modes MB_40X32 and MB_32X8). The RTL as delivered needs -Wno-ENUMVALUE to elaborate
// (out_mode_* are declared mula_mode_e and driven with mulb_mode_e).
module tb_oracle_mulb;
  import gvu_pkg::*;
  string dir;
  integer fd, n, bad, rc, lat;
  integer lmin [0:3], lmax [0:3];
  logic clk = 0, rst_n = 0;
  always #5 clk = ~clk;
  logic b_v0; mulb_mode_e b_mode; logic [0:0][47:0] b_s0; logic [0:0][39:0] b_s1; logic [0:0][71:0] b_p; logic [0:0][31:0] b_aux; logic b_r0, b_ov0;
  gvu_vmul48x40 #(.LANES(1)) u_b (.clk, .rst_n, .in_valid_0(b_v0), .in_ready_0(b_r0), .in_valid_1(2'b00), .in_ready_1(), .in_valid_2(3'b000), .in_ready_2(),
    .in_mode(b_mode), .in_src0(b_s0), .in_src1(b_s1), .out_valid_0(b_ov0), .out_ready_0(1'b1), .out_mode_0(), .out_valid_1(), .out_ready_1(2'b11), .out_mode_1(),
    .out_valid_2(), .out_ready_2(3'b111), .out_mode_2(), .out_prod(b_p), .out_aux(b_aux));
  logic [31:0] m, e_aux; logic [47:0] xa; logic [39:0] xb; logic [71:0] e72;
  initial begin
    if (!$value$plusargs("dir=%s", dir)) dir = ".";
    for (int k = 0; k < 4; k++) begin lmin[k] = 99; lmax[k] = 0; end
    b_v0 = 0; b_mode = MB_40X32; repeat (3) @(negedge clk); rst_n = 1;
    fd = $fopen({dir, "/mulb.txt"}, "r"); n = 0; bad = 0;
    while (fd != 0 && !$feof(fd)) begin
      rc = $fscanf(fd, "%h %h %h %h %h\n", m, xa, xb, e72, e_aux); if (rc != 5) break;
      if (m[1:0] == 2'd1 || m[1:0] == 2'd3) continue;
      @(negedge clk); b_mode = mulb_mode_e'(m[1:0]); b_s0[0] = xa; b_s1[0] = xb; lat = 0;
      while (b_r0 !== 1'b1 && lat < 16) begin @(negedge clk); lat++; end
      if (b_r0 !== 1'b1) begin n++; bad++; continue; end
      b_v0 = 1; @(negedge clk); b_v0 = 0; lat = 1;
      while (b_ov0 !== 1'b1 && lat < 16) begin @(negedge clk); lat++; end
      n++;
      if (b_p[0] !== e72) bad++;
      if (lat < lmin[m[1:0]]) lmin[m[1:0]] = lat;
      if (lat > lmax[m[1:0]]) lmax[m[1:0]] = lat;
    end
    $display("ORACLE mul48x40 (modes 40x32, 32x8) vectors %0d mismatches %0d", n, bad);
    $display("LATENCY mul48x40 40x32 %0d..%0d 32x8 %0d..%0d", lmin[0], lmax[0], lmin[2], lmax[2]);
    $finish;
  end
endmodule

// gvu_adder64_lane with ADD_64_PIPE = 1, ADD_64 beats of adder64.txt: one beat per cycle, the result is read one cycle
// later and compared with a - b (sub) or a + b computed here (the file holds the value of the combinational path).
module tb_oracle_adder64p;
  import gvu_pkg::*;
  string dir;
  integer fd, n, bad_add, bad_sub, rc;
  logic clk = 0, rst_n = 0;
  always #5 clk = ~clk;
  logic v, sub, ov; logic [63:0] a, b, r;
  gvu_adder64_lane #(.ADD_64_PIPE(1'b1), .DOUBLE_PIPE_REG(1'b0)) u (.i_clk(clk), .i_rstn(rst_n), .i_pipe_flush(1'b0), .i_cfg_mode(ADD_64),
    .i_sub32_mode(2'b00), .i_32b_valid(2'b00), .o_32b_ready(), .o_32b_valid(), .i_32b_ready(2'b11),
    .i_sub64_mode(sub), .i_64b_valid(v), .o_64b_ready(), .o_64b_valid(ov), .i_64b_ready(1'b1), .i_a(a), .i_b(b), .o_r(r));
  logic [31:0] v0, v1, v2; logic [63:0] w0, w1, w2, e;
  initial begin
    if (!$value$plusargs("dir=%s", dir)) dir = ".";
    v = 0; sub = 0; a = 0; b = 0; repeat (3) @(negedge clk); rst_n = 1;
    fd = $fopen({dir, "/adder64.txt"}, "r"); n = 0; bad_add = 0; bad_sub = 0;
    while (fd != 0 && !$feof(fd)) begin
      rc = $fscanf(fd, "%h %h %h %h %h %h\n", v0, v1, v2, w0, w1, w2); if (rc != 6) break;
      if (v0[1:0] != 2'd0) continue;
      @(negedge clk); sub = v2[0]; a = w0; b = w1; v = 1; e = v2[0] ? w0 - w1 : w0 + w1;
      @(negedge clk); v = 0; n++;
      // the low half was registered at the edge; the high half is computed from the registered operands, the inputs
      // only have to keep i_sub64_mode and the mode
      if (ov !== 1'b1 || r !== e) begin if (v2[0]) bad_sub++; else bad_add++; end
    end
    $display("ORACLE adder64_pipe vectors %0d mismatches %0d", n, bad_add + bad_sub);
    $display("DETAIL adder64_pipe mismatches: add %0d sub %0d", bad_add, bad_sub);
    $finish;
  end
endmodule
