// SPDX-License-Identifier: SHL-0.51
//
// RTL side of the chimney **manager-side response** cross-check — Step A-1,
// the fourth and last chimney quadrant. It instantiates the unmodified frozen
// `hw/floo_axi_chimney.sv` with the parameter set of `hw/test/floo_test_pkg.sv`.
//
// What is different from the other three chimney testbenches: they all pin
// `floo_rsp_in.valid = 1'b0`. This one drives it. Everything downstream of the
// incoming `rsp` link — the B/R channel decode, `floo_rsp_o.ready`, and the
// reorder-buffer counter release — has never been compared against RTL before,
// which is why the model's `rlast-ignored` defect survived three signed
// cross-checks.
//
// Payload fields are traced **qualified by their valid**. That is not a
// weakening: `floo_rsp_chan_t` is a union, so `axi_b.payload` and
// `axi_r.payload` alias the same bits, and the RTL's B output is a
// combinational pass-through of those bits. With an R flit on the link the B
// payload therefore holds R bits reinterpreted, which is meaningless by the AXI
// contract and would only ever compare a SystemVerilog struct layout against a
// C++ one. Flit *content* is already signed by `run_chimney_rsp_crosscheck.sh`.
//
// Cycle-driven, with no `ApplTime`/`TestTime` phase arithmetic.

`include "axi/typedef.svh"
`include "floo_noc/typedef.svh"

module tb_floo_axi_chimney_mgr_rsp_trace;

  import floo_pkg::*;

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

  // The reorder-buffer counter banks. `NoRoB` keeps one `delta_counter` per AXI
  // id; `in_flight` is that counter's output. This is the state the whole step
  // is about: a multi-beat read burst must decrement it once, on `RLAST`.
  wire [5:0] r_cnt1 =
      dut.i_r_rob.gen_no_rob.i_axi_demux_id_counters.gen_counters[1].in_flight;
  wire [5:0] r_cnt2 =
      dut.i_r_rob.gen_no_rob.i_axi_demux_id_counters.gen_counters[2].in_flight;
  wire [5:0] b_cnt1 =
      dut.i_b_rob.gen_no_rob.i_axi_demux_id_counters.gen_counters[1].in_flight;

  string  stimulus_path, trace_path, header_line;
  integer stimulus_fd, trace_fd, scan_result;
  integer cycle, rst_n_value;
  integer aw_valid_value, aw_id_value, aw_addr_value;
  integer w_valid_value, w_data_value, w_last_value;
  integer ar_valid_value, ar_id_value, ar_addr_value, ar_len_value;
  integer req_ready_value;
  integer rsp_valid_value, rsp_ch_value, rsp_id_value, rsp_data_value;
  integer rsp_resp_value, rsp_last_value;
  integer b_ready_value, r_ready_value;

  logic       pre_rsp_ready, pre_b_valid, pre_r_valid, pre_r_last;
  logic [2:0] pre_b_id, pre_r_id;
  logic [1:0] pre_b_resp, pre_r_resp;
  logic [31:0] pre_r_data;
  logic       pre_aw_ready, pre_ar_ready;

  // A payload is only meaningful while its valid is high; see the header note.
  function automatic logic [2:0] b_id_of();
    return axi_in_rsp.b_valid ? 3'(axi_in_rsp.b.id) : 3'd0;
  endfunction
  function automatic logic [1:0] b_resp_of();
    return axi_in_rsp.b_valid ? axi_in_rsp.b.resp : 2'd0;
  endfunction
  function automatic logic [2:0] r_id_of();
    return axi_in_rsp.r_valid ? 3'(axi_in_rsp.r.id) : 3'd0;
  endfunction
  function automatic logic [1:0] r_resp_of();
    return axi_in_rsp.r_valid ? axi_in_rsp.r.resp : 2'd0;
  endfunction
  function automatic logic [31:0] r_data_of();
    return axi_in_rsp.r_valid ? 32'(axi_in_rsp.r.data) : 32'd0;
  endfunction
  function automatic logic r_last_of();
    return axi_in_rsp.r_valid ? axi_in_rsp.r.last : 1'b0;
  endfunction

  initial begin
    if (!$value$plusargs("STIM_FILE=%s", stimulus_path)) begin
      $fatal(1, "missing +STIM_FILE=<path>");
    end
    if (!$value$plusargs("TRACE_FILE=%s", trace_path)) begin
      $fatal(1, "missing +TRACE_FILE=<path>");
    end

    stimulus_fd = $fopen(stimulus_path, "r");
    if (stimulus_fd == 0) $fatal(1, "cannot open stimulus %s", stimulus_path);
    trace_fd = $fopen(trace_path, "w");
    if (trace_fd == 0) $fatal(1, "cannot create trace %s", trace_path);

    clk         = 1'b0;
    rst_n       = 1'b0;
    node_id     = '{x: 2, y: 2, port_id: 0};
    axi_in_req  = '0;
    axi_out_rsp = '0;
    floo_req_in = '0;
    floo_rsp_in = '0;

    // The subordinate port is idle for this run: no request flits arrive and
    // `axi_out` never answers, so nothing competes for the outgoing `rsp` link.
    floo_req_in.valid = 1'b0;
    floo_rsp_in.ready = 1'b1;

    scan_result = $fgets(header_line, stimulus_fd);
    $fwrite(trace_fd, "cycle,");
    $fwrite(trace_fd, "pre_rsp_ready,pre_b_valid,pre_b_id,pre_b_resp,");
    $fwrite(trace_fd, "pre_r_valid,pre_r_id,pre_r_data,pre_r_resp,");
    $fwrite(trace_fd, "pre_r_last,pre_aw_ready,pre_ar_ready,");
    $fwrite(trace_fd, "post_rsp_ready,post_b_valid,post_b_id,post_b_resp,");
    $fwrite(trace_fd, "post_r_valid,post_r_id,post_r_data,post_r_resp,");
    $fwrite(trace_fd, "post_r_last,post_aw_ready,post_ar_ready,");
    $fwrite(trace_fd, "r_cnt1,r_cnt2,b_cnt1\n");

    while (!$feof(stimulus_fd)) begin
      scan_result = $fscanf(
        stimulus_fd,
        "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
        cycle, rst_n_value,
        aw_valid_value, aw_id_value, aw_addr_value,
        w_valid_value, w_data_value, w_last_value,
        ar_valid_value, ar_id_value, ar_addr_value, ar_len_value,
        req_ready_value,
        rsp_valid_value, rsp_ch_value, rsp_id_value, rsp_data_value,
        rsp_resp_value, rsp_last_value,
        b_ready_value, r_ready_value
      );

      if (scan_result == 21) begin
        clk   = 1'b0;
        rst_n = rst_n_value[0];

        // ---- AXI manager port, request side ----------------------------
        axi_in_req            = '0;
        axi_in_req.aw_valid   = aw_valid_value[0];
        axi_in_req.aw.id      = aw_id_value[2:0];
        axi_in_req.aw.addr    = aw_addr_value;
        axi_in_req.aw.size    = 3'd3;
        axi_in_req.aw.burst   = axi_pkg::BURST_INCR;
        axi_in_req.w_valid    = w_valid_value[0];
        axi_in_req.w.data     = w_data_value;
        axi_in_req.w.strb     = '1;
        axi_in_req.w.last     = w_last_value[0];
        axi_in_req.ar_valid   = ar_valid_value[0];
        axi_in_req.ar.id      = ar_id_value[2:0];
        axi_in_req.ar.addr    = ar_addr_value;
        axi_in_req.ar.len     = ar_len_value[7:0];
        axi_in_req.ar.size    = 3'd3;
        axi_in_req.ar.burst   = axi_pkg::BURST_INCR;
        axi_in_req.b_ready    = b_ready_value[0];
        axi_in_req.r_ready    = r_ready_value[0];

        // ---- inbound `rsp` link ----------------------------------------
        //
        // `floo_rsp_chan_t` is a union: writing the B payload and the R payload
        // both target the same bits. Only the channel named by the header is
        // written, exactly as the request-side testbenches do.
        floo_rsp_in                         = '0;
        floo_rsp_in.valid                   = rsp_valid_value[0];
        floo_rsp_in.ready                   = 1'b1;
        floo_rsp_in.rsp.generic.hdr.axi_ch  = axi_ch_e'(rsp_ch_value);
        floo_rsp_in.rsp.generic.hdr.src_id  = '{x: 2'd1, y: 2'd2,
                                                port_id: 1'b0};
        floo_rsp_in.rsp.generic.hdr.dst_id  = node_id;
        floo_rsp_in.rsp.generic.hdr.last    = 1'b1;
        floo_rsp_in.rsp.generic.hdr.rob_req = 1'b1;
        floo_rsp_in.rsp.generic.hdr.rob_idx = '0;
        // No ATOP anywhere in v0: keeping this clear is what holds
        // `b_sel_atop`/`r_sel_atop` low, so `axi_ready_out[AxiB]` reduces to
        // `b_rob_ready_out` — the path the model implements.
        floo_rsp_in.rsp.generic.hdr.atop    = 1'b0;

        if (axi_ch_e'(rsp_ch_value) == AxiB) begin
          floo_rsp_in.rsp.axi_b.payload.id   = rsp_id_value[2:0];
          floo_rsp_in.rsp.axi_b.payload.resp = rsp_resp_value[1:0];
        end else begin
          floo_rsp_in.rsp.axi_r.payload.id   = rsp_id_value[2:0];
          floo_rsp_in.rsp.axi_r.payload.data = rsp_data_value;
          floo_rsp_in.rsp.axi_r.payload.resp = rsp_resp_value[1:0];
          floo_rsp_in.rsp.axi_r.payload.last = rsp_last_value[0];
        end

        // ---- outgoing `req` link drain ---------------------------------
        floo_req_in.ready = req_ready_value[0];
        floo_req_in.valid = 1'b0;

        axi_out_rsp = '0;

        #1;
        pre_rsp_ready = floo_rsp_out.ready;
        pre_b_valid   = axi_in_rsp.b_valid;
        pre_b_id      = b_id_of();
        pre_b_resp    = b_resp_of();
        pre_r_valid   = axi_in_rsp.r_valid;
        pre_r_id      = r_id_of();
        pre_r_data    = r_data_of();
        pre_r_resp    = r_resp_of();
        pre_r_last    = r_last_of();
        pre_aw_ready  = axi_in_rsp.aw_ready;
        pre_ar_ready  = axi_in_rsp.ar_ready;

        clk = 1'b1;
        #1;
        $fwrite(trace_fd, "%0d,", cycle);
        $fwrite(trace_fd, "%0d,%0d,%0d,%0d,",
                pre_rsp_ready, pre_b_valid, pre_b_id, pre_b_resp);
        $fwrite(trace_fd, "%0d,%0d,%0d,%0d,",
                pre_r_valid, pre_r_id, pre_r_data, pre_r_resp);
        $fwrite(trace_fd, "%0d,%0d,%0d,",
                pre_r_last, pre_aw_ready, pre_ar_ready);
        $fwrite(trace_fd, "%0d,%0d,%0d,%0d,",
                floo_rsp_out.ready, axi_in_rsp.b_valid, b_id_of(),
                b_resp_of());
        $fwrite(trace_fd, "%0d,%0d,%0d,%0d,",
                axi_in_rsp.r_valid, r_id_of(), r_data_of(), r_resp_of());
        $fwrite(trace_fd, "%0d,%0d,%0d,",
                r_last_of(), axi_in_rsp.aw_ready, axi_in_rsp.ar_ready);
        $fwrite(trace_fd, "%0d,%0d,%0d\n", r_cnt1, r_cnt2, b_cnt1);

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
