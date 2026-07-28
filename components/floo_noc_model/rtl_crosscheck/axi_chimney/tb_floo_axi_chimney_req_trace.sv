// SPDX-License-Identifier: SHL-0.51
//
// RTL side of the chimney request-path cross-check.
//
// It instantiates the unmodified frozen `hw/floo_axi_chimney.sv` with the
// parameter set of `hw/test/floo_test_pkg.sv` and the real type macros, drives
// a list of AXI beats into the manager port, and records every request flit
// the chimney emits.
//
// Scope: this compares **flit content and per-beat ordering**, not chimney
// timing. Each AXI beat is issued on its own and held until accepted, and the
// emitted flit is drained before the next beat is issued, so the flit sequence
// follows the beat sequence with no dependence on the internal request
// arbiter. Chimney timing and arbitration remain uncompared.
//
// The upstream `hw/tb/tb_floo_axi_chimney.sv` is not reusable here: it depends
// on the class-based `axi_test` package, which Verilator does not support.

`include "axi/typedef.svh"
`include "floo_noc/typedef.svh"

module tb_floo_axi_chimney_req_trace;

  import floo_pkg::*;

  localparam time CyclTime = 10ns;
  // Stimulus is applied just after a clock edge and sampled well before the
  // next one, the convention `hw/tb/tb_floo_axi_chimney.sv` uses. Driving and
  // sampling exactly at the edge races the DUT and silently misses handshakes.
  localparam time ApplTime = 2ns;
  localparam time TestTime = 8ns;

  // Matches `hw/test/floo_test_pkg.sv`.
  localparam axi_cfg_t     AxiCfg     = '{AddrWidth: 32, DataWidth: 64,
                                          UserWidth: 1, InIdWidth: 3,
                                          OutIdWidth: 3};
  localparam chimney_cfg_t ChimneyCfg = ChimneyDefaultCfg;
  localparam route_cfg_t   RouteCfg   = '{RouteAlgo: XYRouting, UseIdTable: 0,
                                          XYAddrOffsetX: 16, XYAddrOffsetY: 20,
                                          IdAddrOffset: 0, NumSamRules: 1,
                                          NumRoutes: 1,
                                          CollectiveCfg: CollectiveDefaultCfg};

  `FLOO_TYPEDEF_XY_NODE_ID_T(id_t, logic [1:0], logic [1:0], logic)
  `FLOO_TYPEDEF_HDR_T(hdr_t, id_t, id_t, axi_ch_e, logic)
  `FLOO_TYPEDEF_AXI_FROM_CFG(axi, AxiCfg)
  `FLOO_TYPEDEF_AXI_CHAN_ALL(axi, req, rsp, axi_in, AxiCfg, hdr_t)
  `FLOO_TYPEDEF_AXI_LINK_ALL(req, rsp, req, rsp)

  logic clk, rst_n;

  axi_in_req_t  axi_in_req;
  axi_in_rsp_t  axi_in_rsp;
  axi_out_req_t axi_out_req;
  axi_out_rsp_t axi_out_rsp;
  floo_req_t    floo_req_out, floo_req_in;
  floo_rsp_t    floo_rsp_out, floo_rsp_in;
  id_t          node_id;

  floo_axi_chimney #(
    .AxiCfg        ( AxiCfg        ),
    .ChimneyCfg    ( ChimneyCfg    ),
    .RouteCfg      ( RouteCfg      ),
    .AtopSupport   ( 1'b1          ),
    .MaxAtomicTxns ( 4             ),
    .hdr_t         ( hdr_t         ),
    .axi_in_req_t  ( axi_in_req_t  ),
    .axi_in_rsp_t  ( axi_in_rsp_t  ),
    .axi_out_req_t ( axi_out_req_t ),
    .axi_out_rsp_t ( axi_out_rsp_t ),
    .id_t          ( id_t          ),
    .floo_req_t    ( floo_req_t    ),
    .floo_rsp_t    ( floo_rsp_t    )
  ) dut (
    .clk_i         ( clk          ),
    .rst_ni        ( rst_n        ),
    .sram_cfg_i    ( '0           ),
    .test_enable_i ( 1'b0         ),
    .axi_in_req_i  ( axi_in_req   ),
    .axi_in_rsp_o  ( axi_in_rsp   ),
    .axi_out_req_o ( axi_out_req  ),
    .axi_out_rsp_i ( axi_out_rsp  ),
    .id_i          ( node_id      ),
    .route_table_i ( '0           ),
    .floo_req_o    ( floo_req_out ),
    .floo_rsp_o    ( floo_rsp_out ),
    .floo_req_i    ( floo_req_in  ),
    .floo_rsp_i    ( floo_rsp_in  )
  );

  // Free-running clock.
  initial begin
    clk = 1'b0;
    forever #(CyclTime / 2) clk = ~clk;
  end

  // Waits for the cycle in which `ready` is asserted, then consumes the edge
  // that completes the transfer. `ready` is read at TestTime, so it is the
  // value the DUT holds for that whole cycle rather than one racing the edge.
  // The channel is selected by `axi_ch_e` code rather than by a `ref` to a
  // struct field: a reference to a non-simple variable is not supported by
  // every simulator.
  function automatic logic ready_of(input integer ch);
    case (ch)
      AxiAw:   return axi_in_rsp.aw_ready;
      AxiW:    return axi_in_rsp.w_ready;
      default: return axi_in_rsp.ar_ready;
    endcase
  endfunction

  task automatic await_ready(input integer ch);
    guard = 0;
    forever begin
      #(TestTime - ApplTime);
      if (ready_of(ch)) begin
        @(posedge clk);
        #ApplTime;
        return;
      end
      @(posedge clk);
      #ApplTime;
      guard = guard + 1;
      if (guard > 200) begin
        $fatal(1, "ready stuck low on channel %0d at beat %0d", ch, beat_seq);
      end
    end
  endtask

  // Watchdog: a stalled handshake must fail the run, never hang it.
  initial begin
    #(CyclTime * 2000);
    $fatal(1, "watchdog: stalled at beat %0d with %0d flits emitted",
           beat_seq, trace_count);
  end

  // The outgoing request handshake is `floo_req_o.valid && floo_req_i.ready`;
  // the `ready` inside `floo_req_o` belongs to the incoming direction.
  wire floo_req_fire = floo_req_out.valid && floo_req_in.ready;

  string  stimulus_path;
  string  trace_path;
  integer stimulus_fd;
  integer trace_fd;
  integer trace_count;

  task automatic record_flit();
    automatic hdr_t hdr = floo_req_out.req.generic.hdr;
    automatic logic [63:0] pa, pb, pc;
    pa = '0; pb = '0; pc = '0;
    case (hdr.axi_ch)
      AxiAw: begin
        pa = floo_req_out.req.axi_aw.payload.id;
        pb = floo_req_out.req.axi_aw.payload.addr;
        pc = floo_req_out.req.axi_aw.payload.len;
      end
      AxiW: begin
        pa = floo_req_out.req.axi_w.payload.data;
        pb = floo_req_out.req.axi_w.payload.strb;
        pc = floo_req_out.req.axi_w.payload.last;
      end
      AxiAr: begin
        pa = floo_req_out.req.axi_ar.payload.id;
        pb = floo_req_out.req.axi_ar.payload.addr;
        pc = floo_req_out.req.axi_ar.payload.len;
      end
      default: ;
    endcase
    $fwrite(trace_fd, "%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0h,%0h,%0h\n",
            trace_count, hdr.axi_ch,
            hdr.dst_id.x, hdr.dst_id.y, hdr.src_id.x, hdr.src_id.y,
            hdr.last, hdr.atop, hdr.rob_req, hdr.rob_idx, pa, pb, pc);
    trace_count = trace_count + 1;
  endtask

  // Capture every accepted request flit.
  always @(posedge clk) begin
    if (rst_n && floo_req_fire) begin
      record_flit();
    end
  end

  integer scan_result;
  integer beat_seq;
  integer beat_ch;
  integer beat_id;
  integer beat_addr;
  integer beat_len;
  integer beat_atop;
  // Wide and unsigned: `integer` is 32-bit signed, so a payload with bit 31
  // set would be sign-extended into the 64-bit AXI data field.
  logic [63:0] beat_data;
  logic [63:0] beat_strb;
  integer beat_last;
  string  header_line;
  integer expected_flits;
  integer guard;

  task automatic wait_for_flit(input integer target);
    guard = 0;
    while (trace_count < target && guard < 100) begin
      @(posedge clk);
      guard = guard + 1;
    end
    if (trace_count < target) begin
      $fatal(1, "chimney did not emit a flit for beat %0d", beat_seq);
    end
  endtask

  initial begin
    if (!$value$plusargs("STIM_FILE=%s", stimulus_path)) begin
      $fatal(1, "missing +STIM_FILE=<path>");
    end
    if (!$value$plusargs("TRACE_FILE=%s", trace_path)) begin
      $fatal(1, "missing +TRACE_FILE=<path>");
    end

    stimulus_fd = $fopen(stimulus_path, "r");
    if (stimulus_fd == 0) $fatal(1, "cannot open %s", stimulus_path);
    trace_fd = $fopen(trace_path, "w");
    if (trace_fd == 0) $fatal(1, "cannot create %s", trace_path);

    trace_count    = 0;
    expected_flits = 0;
    rst_n          = 1'b0;
    axi_in_req     = '0;
    axi_out_rsp    = '0;
    floo_req_in    = '0;
    floo_rsp_in    = '0;
    // Always ready to accept our outgoing flits; never send anything inbound.
    floo_req_in.ready = 1'b1;
    floo_rsp_in.ready = 1'b1;
    node_id.x       = 2'd1;
    node_id.y       = 2'd1;
    node_id.port_id = 1'b0;

    $fwrite(trace_fd,
            "seq,axi_ch,dst_x,dst_y,src_x,src_y,last,atop,rob_req,rob_idx,pa,pb,pc\n");

    repeat (5) @(posedge clk);
    rst_n = 1'b1;
    repeat (2) @(posedge clk);
    #ApplTime;

    scan_result = $fgets(header_line, stimulus_fd);

    while (!$feof(stimulus_fd)) begin
      // The `axi_ch` column is numeric so this format string stays portable:
      // the `%[^,]` scan set is not implemented by every simulator.
      scan_result = $fscanf(stimulus_fd, "%d,%d,%d,%h,%d,%h,%h,%h,%d\n",
                            beat_seq, beat_ch, beat_id, beat_addr, beat_len,
                            beat_atop, beat_data, beat_strb, beat_last);
      if (scan_result == 9) begin
        if (beat_ch == AxiAw) begin
          axi_in_req.aw       = '0;
          axi_in_req.aw.id    = beat_id[2:0];
          axi_in_req.aw.addr  = beat_addr;
          axi_in_req.aw.len   = beat_len[7:0];
          axi_in_req.aw.size  = 3'd3;
          axi_in_req.aw.burst = axi_pkg::BURST_INCR;
          axi_in_req.aw.atop  = beat_atop[5:0];
          axi_in_req.aw_valid = 1'b1;
          await_ready(AxiAw);
          axi_in_req.aw_valid = 1'b0;
        end else if (beat_ch == AxiW) begin
          axi_in_req.w      = '0;
          axi_in_req.w.data = beat_data;
          axi_in_req.w.strb = beat_strb[7:0];
          axi_in_req.w.last = beat_last[0];
          axi_in_req.w_valid = 1'b1;
          await_ready(AxiW);
          axi_in_req.w_valid = 1'b0;
        end else begin
          axi_in_req.ar       = '0;
          axi_in_req.ar.id    = beat_id[2:0];
          axi_in_req.ar.addr  = beat_addr;
          axi_in_req.ar.len   = beat_len[7:0];
          axi_in_req.ar.size  = 3'd3;
          axi_in_req.ar.burst = axi_pkg::BURST_INCR;
          axi_in_req.ar_valid = 1'b1;
          await_ready(AxiAr);
          axi_in_req.ar_valid = 1'b0;
        end

        // Drain this beat's flit before issuing the next one, so the emitted
        // order follows the beat order regardless of the internal arbiter.
        expected_flits = expected_flits + 1;
        wait_for_flit(expected_flits);
      end
    end

    repeat (10) @(posedge clk);
    $fclose(stimulus_fd);
    $fclose(trace_fd);
    $finish;
  end

endmodule
