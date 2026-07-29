// SPDX-License-Identifier: SHL-0.51
//
// RTL side of the five-port router cross-check. It instantiates the unmodified
// frozen `hw/floo_router.sv` in the parameter set frozen in `docs/P0_SCOPE.md`.
//
// No shim of any kind is used. The compile comes from the Bender-generated file
// list, so `floo_pkg`, `floo_route_select`, `floo_wormhole_arbiter`,
// `floo_vc_arbiter`, and the `common_cells` dependencies are all the real
// sources at the frozen revisions.
//
// Types are built from the frozen `floo_noc/typedef.svh` macros rather than
// hand-written structs, so the header layout is the real one.
//
// `floo_router.sv` keeps its `StableValidIn` and `StableValidOut` assertions
// active under Verilator, because `INC_ASSERT` is gated on `SYNTHESIS` and the
// generated file list only defines `TARGET_SYNTHESIS`. Assertion failures are
// therefore visible and are treated as stimulus defects.

`include "floo_noc/typedef.svh"

module tb_floo_router_trace #(
  // Every generated FlooNoC router has `OutFifoDepth = 2`; the FlooGen
  // templates hardcode it. `0` is the `gen_no_out_fifo` bypass, kept as a
  // second configuration so both RTL branches are signed.
  parameter int unsigned OutFifoDepth = 2
);

  import floo_pkg::*;

  localparam int unsigned NumRoutes    = 5;
  localparam int unsigned InFifoDepth  = 2;
  localparam int unsigned PayloadWidth = 32;
  localparam int unsigned RouterX      = 1;
  localparam int unsigned RouterY      = 1;

  `FLOO_TYPEDEF_XY_NODE_ID_T(id_t, logic [1:0], logic [1:0], logic)
  `FLOO_TYPEDEF_HDR_T(hdr_t, id_t, id_t, axi_ch_e, logic, logic, collect_op_e)

  typedef struct packed {
    hdr_t                    hdr;
    logic [PayloadWidth-1:0] payload;
  } flit_t;

  logic  clk_i;
  logic  rst_ni;
  id_t   xy_id;

  logic  [NumRoutes-1:0][0:0] valid_i, ready_o;
  flit_t [NumRoutes-1:0][0:0] data_i;
  logic  [NumRoutes-1:0][0:0] valid_o, ready_i;
  flit_t [NumRoutes-1:0][0:0] data_o;

  floo_router #(
    .NumRoutes       ( NumRoutes                   ),
    .NumVirtChannels ( 1                           ),
    .NumPhysChannels ( 1                           ),
    .InFifoDepth     ( InFifoDepth                 ),
    .OutFifoDepth    ( OutFifoDepth                ),
    .RouteAlgo       ( XYRouting                   ),
    .IdWidth         ( $bits(id_t)                 ),
    .id_t            ( id_t                        ),
    .NumAddrRules    ( 1                           ),
    .XYRouteOpt      ( 1'b1                        ),
    .NoLoopback      ( 1'b1                        ),
    .VcImpl          ( VcNaive                     ),
    .CollectiveCfg   ( CollectiveSupportDefaultCfg ),
    .RedCfg          ( '0                          ),
    .AxiCfgOffload   ( '0                          ),
    .AxiCfgParallel  ( '0                          ),
    .addr_rule_t     ( logic                       ),
    .flit_t          ( flit_t                      ),
    .hdr_t           ( hdr_t                       ),
    .red_req_t       ( logic                       ),
    .red_rsp_t       ( logic                       )
  ) dut (
    .clk_i,
    .rst_ni,
    .test_enable_i ( 1'b0  ),
    .xy_id_i       ( xy_id ),
    .id_route_map_i( '0    ),
    .valid_i,
    .ready_o,
    .data_i,
    .credit_o      (       ),
    .valid_o,
    .ready_i,
    .data_o,
    .credit_i      ( '0    ),
    .offload_req_o (       ),
    .offload_rsp_i ( '0    )
  );

  // The one-hot route mask per input, which the router uses instead of the
  // encoded index. The route-selector cross-check never compared it.
  logic [NumRoutes-1:0][NumRoutes-1:0] route_mask;
  for (genvar in = 0; in < NumRoutes; in++) begin : gen_mask_probe
    assign route_mask[in] = dut.route_mask[in][0];
  end

  string  stimulus_path;
  string  trace_path;
  integer stimulus_fd;
  integer trace_fd;
  integer scan_result;
  integer cycle;
  integer rst_n_value;
  integer valid_mask;
  integer last_mask;
  integer ready_mask;
  integer dst_code [NumRoutes];
  string  header_line;

  logic [NumRoutes-1:0] pre_ready;
  logic [NumRoutes-1:0] pre_valid;
  logic [PayloadWidth-1:0] pre_payload [NumRoutes];

  function automatic void write_outputs(
      input logic [NumRoutes-1:0] ready_bits,
      input logic [NumRoutes-1:0] valid_bits,
      input logic [PayloadWidth-1:0] payloads [NumRoutes]);
    $fwrite(trace_fd, "%0h,%0h", ready_bits, valid_bits);
    for (int unsigned out = 0; out < NumRoutes; out++) begin
      $fwrite(trace_fd, ",%0h", payloads[out]);
    end
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

    clk_i   = 1'b0;
    rst_ni  = 1'b0;
    valid_i = '0;
    ready_i = '0;
    data_i  = '0;
    xy_id.x = RouterX[1:0];
    xy_id.y = RouterY[1:0];
    xy_id.port_id = 1'b0;

    scan_result = $fgets(header_line, stimulus_fd);
    $fwrite(trace_fd, "cycle,pre_ready,pre_valid");
    for (int unsigned out = 0; out < NumRoutes; out++) begin
      $fwrite(trace_fd, ",pre_d%0d", out);
    end
    $fwrite(trace_fd, ",post_ready,post_valid");
    for (int unsigned out = 0; out < NumRoutes; out++) begin
      $fwrite(trace_fd, ",post_d%0d", out);
    end
    for (int unsigned in = 0; in < NumRoutes; in++) begin
      $fwrite(trace_fd, ",mask%0d", in);
    end
    $fwrite(trace_fd, "\n");

    while (!$feof(stimulus_fd)) begin
      scan_result = $fscanf(
        stimulus_fd,
        "%d,%d,%h,%h,%h,%h,%h,%h,%h,%h\n",
        cycle, rst_n_value, valid_mask, last_mask, ready_mask,
        dst_code[0], dst_code[1], dst_code[2], dst_code[3], dst_code[4]
      );

      if (scan_result == 10) begin
        clk_i  = 1'b0;
        rst_ni = rst_n_value[0];
        for (int unsigned in = 0; in < NumRoutes; in++) begin
          valid_i[in][0] = valid_mask[in];
          ready_i[in][0] = ready_mask[in];
          data_i[in][0]  = '0;
          data_i[in][0].hdr.dst_id.x       = dst_code[in][1:0];
          data_i[in][0].hdr.dst_id.y       = dst_code[in][3:2];
          data_i[in][0].hdr.dst_id.port_id = 1'b0;
          data_i[in][0].hdr.src_id         = xy_id;
          data_i[in][0].hdr.last           = last_mask[in];
          data_i[in][0].hdr.axi_ch         = AxiAw;
          data_i[in][0].hdr.collective_op  = Unicast;
          data_i[in][0].payload            = PayloadWidth'((cycle + 1) * 16 + in);
        end

        #1;
        for (int unsigned port = 0; port < NumRoutes; port++) begin
          pre_ready[port]   = ready_o[port][0];
          pre_valid[port]   = valid_o[port][0];
          pre_payload[port] = data_o[port][0].payload;
        end

        clk_i = 1'b1;
        #1;
        $fwrite(trace_fd, "%0d,", cycle);
        write_outputs(pre_ready, pre_valid, pre_payload);
        $fwrite(trace_fd, ",");
        begin
          logic [NumRoutes-1:0] post_ready;
          logic [NumRoutes-1:0] post_valid;
          logic [PayloadWidth-1:0] post_payload [NumRoutes];
          for (int unsigned port = 0; port < NumRoutes; port++) begin
            post_ready[port]   = ready_o[port][0];
            post_valid[port]   = valid_o[port][0];
            post_payload[port] = data_o[port][0].payload;
          end
          write_outputs(post_ready, post_valid, post_payload);
        end
        for (int unsigned in = 0; in < NumRoutes; in++) begin
          $fwrite(trace_fd, ",%0h", route_mask[in]);
        end
        $fwrite(trace_fd, "\n");

        #1;
        clk_i = 1'b0;
        #1;
      end
    end

    $fclose(stimulus_fd);
    $fclose(trace_fd);
    $finish;
  end

endmodule
