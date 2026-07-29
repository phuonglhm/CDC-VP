// SPDX-License-Identifier: SHL-0.51
//
// RTL side of the chimney request-path **timing** cross-check. It instantiates
// the unmodified frozen `hw/floo_axi_chimney.sv` with the parameter set of
// `hw/test/floo_test_pkg.sv`.
//
// Difference from `tb_floo_axi_chimney_req_trace.sv`: that testbench issues one
// AXI beat at a time and drains the resulting flit before the next, so the
// chimney's request arbiter never arbitrates. This one drives AW, W, and AR
// concurrently and applies back-pressure on the `req` link, and records a
// cycle trace rather than a flit list.
//
// It also drops the `ApplTime`/`TestTime` phase arithmetic that the content
// harnesses use. Four separate defects in those came from mixing explicit
// delays with sequential driving; the cycle harnesses drive at `clk = 0`,
// sample pre-edge, raise the clock, and sample post-edge, with no phase
// offsets at all.
//
// The response link is held idle, so the reorder-buffer counters only fill.
// The stimulus generator bounds the offers per ID below the counter capacity,
// keeping the run clear of the saturation corner that
// `run_rob_crosscheck.sh` already signs on its own.

`include "axi/typedef.svh"
`include "floo_noc/typedef.svh"

module tb_floo_axi_chimney_timing_trace;

  import floo_pkg::*;

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

  // Composed state the model mirrors, read through hierarchical references.
  // `aw_w_sel_e` is `enum logic {SelAw, SelW}` declared inside the chimney, so
  // `SelAw` is 0. The literal is used rather than a hierarchical reference to
  // the enum member, which crashes Verilator 5.022.
  wire       sel_aw      = (dut.aw_w_sel_q == 1'b0);
  wire [1:0] arb_valid_q = dut.i_req_wormhole_arbiter.valid_q;
  wire       arb_last_q  = dut.i_req_wormhole_arbiter.last_q;
  wire [2:0] arb_rr_q    =
      dut.i_req_wormhole_arbiter.i_rr_arb_packets.gen_arbiter.rr_q;
  wire       arb_lock_q  =
      dut.i_req_wormhole_arbiter.i_rr_arb_packets.gen_arbiter.gen_int_rr.gen_lock.lock_q;
  wire [1:0] arb_req_q   =
      dut.i_req_wormhole_arbiter.i_rr_arb_packets.gen_arbiter.gen_int_rr.gen_lock.req_q;

  string  stimulus_path;
  string  trace_path;
  integer stimulus_fd;
  integer trace_fd;
  integer scan_result;
  integer cycle;
  integer rst_n_value;
  integer aw_valid_value;
  integer aw_id_value;
  integer aw_addr_value;
  integer w_valid_value;
  integer w_last_value;
  integer w_data_value;
  integer ar_valid_value;
  integer ar_id_value;
  integer ar_addr_value;
  integer req_ready_value;
  string  header_line;

  logic          pre_aw_ready, pre_w_ready, pre_ar_ready, pre_req_valid;
  logic [2:0]    pre_ch;
  logic [3:0]    pre_dst;
  logic          pre_last;
  logic [63:0]   pre_payload;

  // The flit payload reduced to one comparable number per channel, matching
  // the model side. The full payload is already signed by the two content
  // cross-checks; this trace has to pin *which* flit is on the link *when*.
  function automatic logic [63:0] payload_of();
    case (floo_req_out.req.generic.hdr.axi_ch)
      AxiAw:   return {32'(floo_req_out.req.axi_aw.payload.id),
                       32'(floo_req_out.req.axi_aw.payload.addr)};
      AxiW:    return {32'(floo_req_out.req.axi_w.payload.strb),
                       32'(floo_req_out.req.axi_w.payload.data)};
      AxiAr:   return {32'(floo_req_out.req.axi_ar.payload.id),
                       32'(floo_req_out.req.axi_ar.payload.addr)};
      default: return 64'd0;
    endcase
  endfunction

  function automatic logic [3:0] dst_of();
    return {2'(floo_req_out.req.generic.hdr.dst_id.y),
            2'(floo_req_out.req.generic.hdr.dst_id.x)};
  endfunction

  initial begin
    if (!$value$plusargs("STIM_FILE=%s", stimulus_path)) begin
      $fatal(1, "missing +STIM_FILE=<path>");
    end
    if (!$value$plusargs("TRACE_FILE=%s", trace_path)) begin
      $fatal(1, "missing +TRACE_FILE=<path>");
    end

    stimulus_fd = $fopen(stimulus_path, "r");
    if (stimulus_fd == 0) begin
      $fatal(1, "cannot open stimulus file %s", stimulus_path);
    end
    trace_fd = $fopen(trace_path, "w");
    if (trace_fd == 0) begin
      $fatal(1, "cannot create trace file %s", trace_path);
    end

    clk          = 1'b0;
    rst_n        = 1'b0;
    node_id      = '0;
    axi_in_req   = '0;
    axi_out_rsp  = '0;
    floo_req_in  = '0;
    floo_rsp_in  = '0;

    // The subordinate side is permanently ready so it never becomes the
    // limiting factor. It is idle anyway: the response link is held quiet, so
    // nothing reaches the meta buffer.
    axi_out_rsp.aw_ready = 1'b1;
    axi_out_rsp.w_ready  = 1'b1;
    axi_out_rsp.ar_ready = 1'b1;

    scan_result = $fgets(header_line, stimulus_fd);
    $fwrite(trace_fd, "cycle,");
    $fwrite(trace_fd, "pre_aw_ready,pre_w_ready,pre_ar_ready,pre_req_valid,");
    $fwrite(trace_fd, "pre_ch,pre_dst,pre_last,pre_payload,");
    $fwrite(trace_fd, "post_aw_ready,post_w_ready,post_ar_ready,post_req_valid,");
    $fwrite(trace_fd, "post_ch,post_dst,post_last,post_payload,");
    $fwrite(trace_fd, "sel_aw,arb_valid_q,arb_last_q,arb_rr_q,arb_lock_q,arb_req_q\n");

    while (!$feof(stimulus_fd)) begin
      scan_result = $fscanf(
        stimulus_fd,
        "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
        cycle, rst_n_value,
        aw_valid_value, aw_id_value, aw_addr_value,
        w_valid_value, w_last_value, w_data_value,
        ar_valid_value, ar_id_value, ar_addr_value,
        req_ready_value
      );

      if (scan_result == 12) begin
        clk   = 1'b0;
        rst_n = rst_n_value[0];

        axi_in_req.aw        = '0;
        axi_in_req.aw.id     = aw_id_value;
        axi_in_req.aw.addr   = aw_addr_value;
        axi_in_req.aw.size   = 3'd3;
        axi_in_req.aw.burst  = axi_pkg::BURST_INCR;
        axi_in_req.aw_valid  = aw_valid_value[0];

        axi_in_req.w         = '0;
        axi_in_req.w.data    = w_data_value;
        axi_in_req.w.strb    = '1;
        axi_in_req.w.last    = w_last_value[0];
        axi_in_req.w_valid   = w_valid_value[0];

        axi_in_req.ar        = '0;
        axi_in_req.ar.id     = ar_id_value;
        axi_in_req.ar.addr   = ar_addr_value;
        axi_in_req.ar.size   = 3'd3;
        axi_in_req.ar.burst  = axi_pkg::BURST_INCR;
        axi_in_req.ar_valid  = ar_valid_value[0];

        axi_in_req.b_ready   = 1'b1;
        axi_in_req.r_ready   = 1'b1;
        floo_req_in.ready    = req_ready_value[0];
        floo_rsp_in.valid    = 1'b0;
        floo_rsp_in.ready    = 1'b1;

        #1;
        pre_aw_ready  = axi_in_rsp.aw_ready;
        pre_w_ready   = axi_in_rsp.w_ready;
        pre_ar_ready  = axi_in_rsp.ar_ready;
        pre_req_valid = floo_req_out.valid;
        pre_ch        = floo_req_out.req.generic.hdr.axi_ch;
        pre_dst       = dst_of();
        pre_last      = floo_req_out.req.generic.hdr.last;
        pre_payload   = payload_of();

        clk = 1'b1;
        #1;
        $fwrite(trace_fd, "%0d,", cycle);
        $fwrite(trace_fd, "%0d,%0d,%0d,%0d,",
                pre_aw_ready, pre_w_ready, pre_ar_ready, pre_req_valid);
        $fwrite(trace_fd, "%0d,%0d,%0d,%0d,",
                pre_ch, pre_dst, pre_last, pre_payload);
        $fwrite(trace_fd, "%0d,%0d,%0d,%0d,",
                axi_in_rsp.aw_ready, axi_in_rsp.w_ready,
                axi_in_rsp.ar_ready, floo_req_out.valid);
        $fwrite(trace_fd, "%0d,%0d,%0d,%0d,",
                floo_req_out.req.generic.hdr.axi_ch, dst_of(),
                floo_req_out.req.generic.hdr.last, payload_of());
        $fwrite(trace_fd, "%0d,%0d,%0d,%0d,%0d,%0d\n",
                sel_aw, arb_valid_q, arb_last_q, arb_rr_q, arb_lock_q,
                arb_req_q);

        #1;
        clk = 1'b0;
        #1;
      end
    end

    $fclose(stimulus_fd);
    $fclose(trace_fd);
    $finish;
  end

endmodule
