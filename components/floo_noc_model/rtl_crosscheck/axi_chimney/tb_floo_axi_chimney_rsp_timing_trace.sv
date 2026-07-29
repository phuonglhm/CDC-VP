// SPDX-License-Identifier: SHL-0.51
//
// RTL side of the chimney **response-path and subordinate-side timing**
// cross-check. It instantiates the unmodified frozen `hw/floo_axi_chimney.sv`
// with the parameter set of `hw/test/floo_test_pkg.sv`.
//
// Difference from `tb_floo_axi_chimney_rsp_trace.sv`: that testbench compares
// response flit *content*, draining each flit before the next request. This one
// drives B and R answers concurrently, applies back-pressure on the outgoing
// `rsp` link and on `axi_out`'s AW, and records a cycle trace.
//
// `i_aw_out_queue` is the one spill register the frozen configuration does not
// bypass, so holding `axi_out_rsp.aw_ready` low is what reaches it.
//
// Cycle-driven, with no `ApplTime`/`TestTime` phase arithmetic.

`include "axi/typedef.svh"
`include "floo_noc/typedef.svh"

module tb_floo_axi_chimney_rsp_timing_trace;

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

  // Composed state the model mirrors.
  // The meta buffer sits inside the `gen_mgr_port` generate block, despite the
  // name: `ChimneyCfg.EnSbrPort` is what selects it.
  wire [5:0] aw_meta =
      dut.gen_mgr_port.i_floo_meta_buffer.gen_no_atop_fifos.i_aw_no_atop_fifo.status_cnt_q;
  wire [5:0] ar_meta =
      dut.gen_mgr_port.i_floo_meta_buffer.gen_no_atop_fifos.i_ar_no_atop_fifo.status_cnt_q;
  wire [1:0] arb_valid_q = dut.i_rsp_wormhole_arbiter.valid_q;
  wire       arb_last_q  = dut.i_rsp_wormhole_arbiter.last_q;
  wire [2:0] arb_rr_q    =
      dut.i_rsp_wormhole_arbiter.i_rr_arb_packets.gen_arbiter.rr_q;
  wire       arb_lock_q  =
      dut.i_rsp_wormhole_arbiter.i_rr_arb_packets.gen_arbiter.gen_int_rr.gen_lock.lock_q;
  wire [1:0] arb_req_q   =
      dut.i_rsp_wormhole_arbiter.i_rr_arb_packets.gen_arbiter.gen_int_rr.gen_lock.req_q;

  string  stimulus_path, trace_path, header_line;
  integer stimulus_fd, trace_fd, scan_result;
  integer cycle, rst_n_value;
  integer req_valid_value, req_ch_value, req_id_value, req_addr_value;
  integer req_last_value, req_data_value, req_src_x_value, req_src_y_value;
  integer aw_ready_value, w_ready_value, ar_ready_value;
  integer b_valid_value, b_id_value, b_resp_value;
  integer r_valid_value, r_id_value, r_data_value, r_resp_value, r_last_value;
  integer rsp_ready_value;

  logic        pre_req_ready, pre_aw_valid, pre_w_valid, pre_ar_valid;
  logic [2:0]  pre_aw_id, pre_ar_id;
  logic        pre_b_ready, pre_r_ready, pre_rsp_valid;
  logic [2:0]  pre_rsp_ch;
  logic [3:0]  pre_rsp_dst;
  logic [2:0]  pre_rsp_id;

  function automatic logic [3:0] rsp_dst_of();
    return {2'(floo_rsp_out.rsp.generic.hdr.dst_id.y),
            2'(floo_rsp_out.rsp.generic.hdr.dst_id.x)};
  endfunction

  function automatic logic [2:0] rsp_id_of();
    if (floo_rsp_out.rsp.generic.hdr.axi_ch == AxiB) begin
      return floo_rsp_out.rsp.axi_b.payload.id;
    end
    return floo_rsp_out.rsp.axi_r.payload.id;
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

    // The manager port is idle for this run: nothing is injected on the AXI
    // side, so `floo_req_o`/`axi_in_rsp` play no part.
    axi_in_req.b_ready = 1'b1;
    axi_in_req.r_ready = 1'b1;
    floo_rsp_in.valid  = 1'b0;

    scan_result = $fgets(header_line, stimulus_fd);
    $fwrite(trace_fd, "cycle,");
    $fwrite(trace_fd,
            "pre_req_ready,pre_aw_valid,pre_aw_id,pre_w_valid,pre_ar_valid,");
    $fwrite(trace_fd, "pre_ar_id,pre_b_ready,pre_r_ready,pre_rsp_valid,");
    $fwrite(trace_fd, "pre_rsp_ch,pre_rsp_dst,pre_rsp_id,");
    $fwrite(trace_fd,
            "post_req_ready,post_aw_valid,post_aw_id,post_w_valid,");
    $fwrite(trace_fd, "post_ar_valid,post_ar_id,post_b_ready,post_r_ready,");
    $fwrite(trace_fd, "post_rsp_valid,post_rsp_ch,post_rsp_dst,post_rsp_id,");
    $fwrite(trace_fd,
            "aw_meta,ar_meta,arb_valid_q,arb_last_q,arb_rr_q,arb_lock_q,");
    $fwrite(trace_fd, "arb_req_q\n");

    while (!$feof(stimulus_fd)) begin
      scan_result = $fscanf(
        stimulus_fd,
        "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
        cycle, rst_n_value,
        req_valid_value, req_ch_value, req_id_value, req_addr_value,
        req_last_value, req_data_value, req_src_x_value, req_src_y_value,
        aw_ready_value, w_ready_value, ar_ready_value,
        b_valid_value, b_id_value, b_resp_value,
        r_valid_value, r_id_value, r_data_value, r_resp_value, r_last_value,
        rsp_ready_value
      );

      if (scan_result == 22) begin
        clk   = 1'b0;
        rst_n = rst_n_value[0];

        floo_req_in                        = '0;
        floo_req_in.valid                  = req_valid_value[0];
        floo_req_in.req.generic.hdr.axi_ch = axi_ch_e'(req_ch_value);
        floo_req_in.req.generic.hdr.src_id = '{x: req_src_x_value[1:0],
                                               y: req_src_y_value[1:0],
                                               port_id: 1'b0};
        floo_req_in.req.generic.hdr.dst_id = node_id;
        floo_req_in.req.generic.hdr.last   = req_last_value[0];
        floo_req_in.req.generic.hdr.rob_req = 1'b1;
        floo_req_in.req.generic.hdr.rob_idx = '0;

        case (axi_ch_e'(req_ch_value))
          AxiAw: begin
            floo_req_in.req.axi_aw.payload.id    = req_id_value[2:0];
            floo_req_in.req.axi_aw.payload.addr  = req_addr_value;
            floo_req_in.req.axi_aw.payload.size  = 3'd3;
            floo_req_in.req.axi_aw.payload.burst = axi_pkg::BURST_INCR;
          end
          AxiW: begin
            floo_req_in.req.axi_w.payload.data = req_data_value;
            floo_req_in.req.axi_w.payload.strb = '1;
            floo_req_in.req.axi_w.payload.last = req_last_value[0];
          end
          default: begin
            floo_req_in.req.axi_ar.payload.id    = req_id_value[2:0];
            floo_req_in.req.axi_ar.payload.addr  = req_addr_value;
            floo_req_in.req.axi_ar.payload.size  = 3'd3;
            floo_req_in.req.axi_ar.payload.burst = axi_pkg::BURST_INCR;
          end
        endcase

        axi_out_rsp          = '0;
        axi_out_rsp.aw_ready = aw_ready_value[0];
        axi_out_rsp.w_ready  = w_ready_value[0];
        axi_out_rsp.ar_ready = ar_ready_value[0];
        axi_out_rsp.b_valid  = b_valid_value[0];
        axi_out_rsp.b.id     = b_id_value[2:0];
        axi_out_rsp.b.resp   = b_resp_value[1:0];
        axi_out_rsp.r_valid  = r_valid_value[0];
        axi_out_rsp.r.id     = r_id_value[2:0];
        axi_out_rsp.r.data   = r_data_value;
        axi_out_rsp.r.resp   = r_resp_value[1:0];
        axi_out_rsp.r.last   = r_last_value[0];

        floo_rsp_in.ready = rsp_ready_value[0];
        floo_rsp_in.valid = 1'b0;

        #1;
        pre_req_ready = floo_req_out.ready;
        pre_aw_valid  = axi_out_req.aw_valid;
        pre_aw_id     = axi_out_req.aw.id;
        pre_w_valid   = axi_out_req.w_valid;
        pre_ar_valid  = axi_out_req.ar_valid;
        pre_ar_id     = axi_out_req.ar.id;
        pre_b_ready   = axi_out_req.b_ready;
        pre_r_ready   = axi_out_req.r_ready;
        pre_rsp_valid = floo_rsp_out.valid;
        pre_rsp_ch    = floo_rsp_out.rsp.generic.hdr.axi_ch;
        pre_rsp_dst   = rsp_dst_of();
        pre_rsp_id    = rsp_id_of();

        clk = 1'b1;
        #1;
        $fwrite(trace_fd, "%0d,", cycle);
        $fwrite(trace_fd, "%0d,%0d,%0d,%0d,%0d,",
                pre_req_ready, pre_aw_valid, pre_aw_id, pre_w_valid,
                pre_ar_valid);
        $fwrite(trace_fd, "%0d,%0d,%0d,%0d,",
                pre_ar_id, pre_b_ready, pre_r_ready, pre_rsp_valid);
        $fwrite(trace_fd, "%0d,%0d,%0d,",
                pre_rsp_ch, pre_rsp_dst, pre_rsp_id);
        $fwrite(trace_fd, "%0d,%0d,%0d,%0d,",
                floo_req_out.ready, axi_out_req.aw_valid, axi_out_req.aw.id,
                axi_out_req.w_valid);
        $fwrite(trace_fd, "%0d,%0d,%0d,%0d,",
                axi_out_req.ar_valid, axi_out_req.ar.id, axi_out_req.b_ready,
                axi_out_req.r_ready);
        $fwrite(trace_fd, "%0d,%0d,%0d,%0d,",
                floo_rsp_out.valid, floo_rsp_out.rsp.generic.hdr.axi_ch,
                rsp_dst_of(), rsp_id_of());
        $fwrite(trace_fd, "%0d,%0d,%0d,%0d,%0d,%0d,",
                aw_meta, ar_meta, arb_valid_q, arb_last_q, arb_rr_q,
                arb_lock_q);
        $fwrite(trace_fd, "%0d\n", arb_req_q);

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
