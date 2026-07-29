// SPDX-License-Identifier: SHL-0.51
//
// RTL side of the **inter-node** cross-check: a rectangular grid of the
// unmodified frozen `hw/floo_axi_router.sv`.
//
// Why a router grid and not the FlooGen top. `floo_axi_mesh_noc.sv` exposes
// only AXI ports per endpoint, so comparing against it would require a full
// chimney at all twenty nodes and would fold chimney behaviour into a mesh
// measurement. `floo_axi_router` has clean flit-level ports, so the grid is
// the isolatable unit — the same reasoning that put the reorder-buffer
// cross-check on `floo_rob_wrapper` rather than through the chimney.
//
// The wiring below is the rule read out of the generated netlist. For
// `router_0_1`, FlooGen emits
//
//   req_in[0] <- router_0_2   (y+1, North)
//   req_in[1] <- router_1_1   (x+1, East)
//   req_in[2] <- router_0_0   (y-1, South)
//   req_in[3] <- (x-1)        (West)
//   req_in[4] <- the local endpoint (Eject)
//
// and `ROUTER_i_j_ID` is `'{x: i+1, y: j}` there only because FlooGen reserves
// the x=0 column for its HBM endpoints. This testbench is a plain rectangular
// mesh with every edge tied off, matching `floo_mesh` in the model, so it
// places node (x,y) at coordinate (x,y).
//
// Each `floo_req_t` link struct carries `valid`/`req` forward and `ready`
// backward, so one wire per direction pair is enough; `floo_axi_router`'s own
// `gen_chimney_req`/`gen_chimney_rsp` blocks split them.
//
// Stimulus and trace carry one line per node per cycle, so the grid size is a
// parameter rather than a column count.

`include "axi/typedef.svh"
`include "floo_noc/typedef.svh"

module tb_floo_mesh_trace #(
  parameter int unsigned NumX = 3,
  parameter int unsigned NumY = 3
);

  import floo_pkg::*;

  localparam int unsigned NumNodes = NumX * NumY;
  localparam int unsigned NumPorts = 5;

  // Port indices, `floo_pkg::route_direction_e`.
  localparam int unsigned PortNorth = 0;
  localparam int unsigned PortEast  = 1;
  localparam int unsigned PortSouth = 2;
  localparam int unsigned PortWest  = 3;
  localparam int unsigned PortEject = 4;

  localparam axi_cfg_t AxiCfg = '{AddrWidth: 32, DataWidth: 64, UserWidth: 1,
                                  InIdWidth: 3, OutIdWidth: 3};

  `FLOO_TYPEDEF_XY_NODE_ID_T(id_t, logic [1:0], logic [1:0], logic)
  `FLOO_TYPEDEF_HDR_T(hdr_t, id_t, id_t, axi_ch_e, logic)
  `FLOO_TYPEDEF_AXI_FROM_CFG(axi, AxiCfg)
  `FLOO_TYPEDEF_AXI_CHAN_ALL(axi, req, rsp, axi_in, AxiCfg, hdr_t)
  `FLOO_TYPEDEF_AXI_LINK_ALL(req, rsp, req, rsp)

  logic clk, rst_n;

  floo_req_t [NumNodes-1:0][NumPorts-1:0] req_in, req_out;
  floo_rsp_t [NumNodes-1:0][NumPorts-1:0] rsp_in, rsp_out;

  function automatic int unsigned node_of(input int unsigned x,
                                          input int unsigned y);
    return y * NumX + x;
  endfunction

  for (genvar y = 0; y < NumY; y++) begin : gen_row
    for (genvar x = 0; x < NumX; x++) begin : gen_col
      localparam int unsigned Node = y * NumX + x;
      localparam id_t NodeId = '{x: x[1:0], y: y[1:0], port_id: 1'b0};

      floo_axi_router #(
        .AxiCfg       ( AxiCfg     ),
        .RouteAlgo    ( XYRouting  ),
        .NumRoutes    ( NumPorts   ),
        .NumInputs    ( NumPorts   ),
        .NumOutputs   ( NumPorts   ),
        .InFifoDepth  ( 2          ),
        .OutFifoDepth ( 2          ),
        .id_t         ( id_t       ),
        .hdr_t        ( hdr_t      ),
        .floo_req_t   ( floo_req_t ),
        .floo_rsp_t   ( floo_rsp_t )
      ) i_router (
        .clk_i          ( clk           ),
        .rst_ni         ( rst_n         ),
        .test_enable_i  ( 1'b0          ),
        .id_i           ( NodeId        ),
        .id_route_map_i ( '0            ),
        .floo_req_i     ( req_in[Node]  ),
        .floo_rsp_o     ( rsp_out[Node] ),
        .floo_req_o     ( req_out[Node] ),
        .floo_rsp_i     ( rsp_in[Node]  )
      );

      // North neighbour is (x, y+1); its South port faces back.
      if (y + 1 < NumY) begin : gen_north
        assign req_in[Node][PortNorth] = req_out[node_of(x, y + 1)][PortSouth];
        assign rsp_in[Node][PortNorth] = rsp_out[node_of(x, y + 1)][PortSouth];
      end else begin : gen_no_north
        assign req_in[Node][PortNorth] = '0;
        assign rsp_in[Node][PortNorth] = '0;
      end

      if (x + 1 < NumX) begin : gen_east
        assign req_in[Node][PortEast] = req_out[node_of(x + 1, y)][PortWest];
        assign rsp_in[Node][PortEast] = rsp_out[node_of(x + 1, y)][PortWest];
      end else begin : gen_no_east
        assign req_in[Node][PortEast] = '0;
        assign rsp_in[Node][PortEast] = '0;
      end

      if (y > 0) begin : gen_south
        assign req_in[Node][PortSouth] = req_out[node_of(x, y - 1)][PortNorth];
        assign rsp_in[Node][PortSouth] = rsp_out[node_of(x, y - 1)][PortNorth];
      end else begin : gen_no_south
        assign req_in[Node][PortSouth] = '0;
        assign rsp_in[Node][PortSouth] = '0;
      end

      if (x > 0) begin : gen_west
        assign req_in[Node][PortWest] = req_out[node_of(x - 1, y)][PortEast];
        assign rsp_in[Node][PortWest] = rsp_out[node_of(x - 1, y)][PortEast];
      end else begin : gen_no_west
        assign req_in[Node][PortWest] = '0;
        assign rsp_in[Node][PortWest] = '0;
      end
    end
  end

  // Local endpoints, driven from the stimulus.
  floo_req_chan_t [NumNodes-1:0] inject_req;
  logic [NumNodes-1:0]           inject_req_valid, eject_req_ready;
  floo_rsp_chan_t [NumNodes-1:0] inject_rsp;
  logic [NumNodes-1:0]           inject_rsp_valid, eject_rsp_ready;

  for (genvar n = 0; n < NumNodes; n++) begin : gen_local
    assign req_in[n][PortEject].valid = inject_req_valid[n];
    assign req_in[n][PortEject].req   = inject_req[n];
    assign req_in[n][PortEject].ready = eject_req_ready[n];
    assign rsp_in[n][PortEject].valid = inject_rsp_valid[n];
    assign rsp_in[n][PortEject].rsp   = inject_rsp[n];
    assign rsp_in[n][PortEject].ready = eject_rsp_ready[n];
  end

  string  stimulus_path, trace_path, header_line;
  integer stimulus_fd, trace_fd, scan_result;
  integer cycle, node, rst_n_value;
  integer rq_valid, rq_ch, rq_dst_x, rq_dst_y, rq_last, rq_tag, rq_ej_ready;
  integer rs_valid, rs_dst_x, rs_dst_y, rs_tag, rs_ej_ready;

  logic [NumNodes-1:0]       pre_rq_inj_ready, pre_rq_ej_valid, pre_rq_ej_last;
  logic [NumNodes-1:0][2:0]  pre_rq_ej_ch;
  logic [NumNodes-1:0][3:0]  pre_rq_ej_dst;
  logic [NumNodes-1:0][31:0] pre_rq_ej_tag;
  logic [NumNodes-1:0]       pre_rs_inj_ready, pre_rs_ej_valid;
  logic [NumNodes-1:0][3:0]  pre_rs_ej_dst;
  logic [NumNodes-1:0][31:0] pre_rs_ej_tag;

  // The ejected flit reduced to one comparable number. `AxiAr`'s address and
  // `AxiW`'s data both live in the low bits, which is enough to identify which
  // flit is on which port in which cycle.
  function automatic logic [31:0] req_tag_of(input int unsigned n);
    case (req_out[n][PortEject].req.generic.hdr.axi_ch)
      AxiAw:   return 32'(req_out[n][PortEject].req.axi_aw.payload.addr);
      AxiW:    return 32'(req_out[n][PortEject].req.axi_w.payload.data);
      default: return 32'(req_out[n][PortEject].req.axi_ar.payload.addr);
    endcase
  endfunction

  function automatic logic [3:0] req_dst_of(input int unsigned n);
    return {2'(req_out[n][PortEject].req.generic.hdr.dst_id.y),
            2'(req_out[n][PortEject].req.generic.hdr.dst_id.x)};
  endfunction

  function automatic logic [31:0] rsp_tag_of(input int unsigned n);
    return 32'(rsp_out[n][PortEject].rsp.axi_b.payload.id);
  endfunction

  function automatic logic [3:0] rsp_dst_of(input int unsigned n);
    return {2'(rsp_out[n][PortEject].rsp.generic.hdr.dst_id.y),
            2'(rsp_out[n][PortEject].rsp.generic.hdr.dst_id.x)};
  endfunction

  task automatic sample_pre();
    for (int unsigned n = 0; n < NumNodes; n++) begin
      pre_rq_inj_ready[n] = req_out[n][PortEject].ready;
      pre_rq_ej_valid[n]  = req_out[n][PortEject].valid;
      pre_rq_ej_ch[n]     = req_out[n][PortEject].req.generic.hdr.axi_ch;
      pre_rq_ej_dst[n]    = req_dst_of(n);
      pre_rq_ej_last[n]   = req_out[n][PortEject].req.generic.hdr.last;
      pre_rq_ej_tag[n]    = req_tag_of(n);
      pre_rs_inj_ready[n] = rsp_out[n][PortEject].ready;
      pre_rs_ej_valid[n]  = rsp_out[n][PortEject].valid;
      pre_rs_ej_dst[n]    = rsp_dst_of(n);
      pre_rs_ej_tag[n]    = rsp_tag_of(n);
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
    if (stimulus_fd == 0) $fatal(1, "cannot open stimulus %s", stimulus_path);
    trace_fd = $fopen(trace_path, "w");
    if (trace_fd == 0) $fatal(1, "cannot create trace %s", trace_path);

    clk              = 1'b0;
    rst_n            = 1'b0;
    inject_req       = '0;
    inject_req_valid = '0;
    eject_req_ready  = '0;
    inject_rsp       = '0;
    inject_rsp_valid = '0;
    eject_rsp_ready  = '0;

    scan_result = $fgets(header_line, stimulus_fd);
    $fwrite(trace_fd, "cycle,node,");
    $fwrite(trace_fd,
            "pre_rq_inj_ready,pre_rq_ej_valid,pre_rq_ej_ch,pre_rq_ej_dst,");
    $fwrite(trace_fd, "pre_rq_ej_last,pre_rq_ej_tag,");
    $fwrite(trace_fd, "pre_rs_inj_ready,pre_rs_ej_valid,pre_rs_ej_dst,");
    $fwrite(trace_fd, "pre_rs_ej_tag,");
    $fwrite(trace_fd,
            "post_rq_inj_ready,post_rq_ej_valid,post_rq_ej_ch,post_rq_ej_dst,");
    $fwrite(trace_fd, "post_rq_ej_last,post_rq_ej_tag,");
    $fwrite(trace_fd, "post_rs_inj_ready,post_rs_ej_valid,post_rs_ej_dst,");
    $fwrite(trace_fd, "post_rs_ej_tag\n");

    while (!$feof(stimulus_fd)) begin
      clk = 1'b0;

      // One line per node makes the grid size a parameter rather than a
      // column count.
      for (int unsigned slot = 0; slot < NumNodes; slot++) begin
        scan_result = $fscanf(
          stimulus_fd, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
          cycle, node, rst_n_value,
          rq_valid, rq_ch, rq_dst_x, rq_dst_y, rq_last, rq_tag, rq_ej_ready,
          rs_valid, rs_dst_x, rs_dst_y, rs_tag, rs_ej_ready);
        if (scan_result != 15) begin
          if (slot != 0) $fatal(1, "truncated stimulus cycle");
          $fclose(stimulus_fd);
          $fclose(trace_fd);
          $finish;
        end
        if (node != slot) $fatal(1, "stimulus node out of order");

        rst_n = rst_n_value[0];

        inject_req[node]                        = '0;
        inject_req[node].generic.hdr.axi_ch     = axi_ch_e'(rq_ch);
        inject_req[node].generic.hdr.dst_id     = '{x: rq_dst_x[1:0],
                                                    y: rq_dst_y[1:0],
                                                    port_id: 1'b0};
        inject_req[node].generic.hdr.src_id     = '{x: 2'(node % NumX),
                                                    y: 2'(node / NumX),
                                                    port_id: 1'b0};
        inject_req[node].generic.hdr.last       = rq_last[0];
        case (axi_ch_e'(rq_ch))
          AxiAw:   inject_req[node].axi_aw.payload.addr = rq_tag;
          AxiW:    inject_req[node].axi_w.payload.data  = rq_tag;
          default: inject_req[node].axi_ar.payload.addr = rq_tag;
        endcase
        inject_req_valid[node] = rq_valid[0];
        eject_req_ready[node]  = rq_ej_ready[0];

        inject_rsp[node]                    = '0;
        inject_rsp[node].generic.hdr.axi_ch = AxiB;
        inject_rsp[node].generic.hdr.dst_id = '{x: rs_dst_x[1:0],
                                                y: rs_dst_y[1:0],
                                                port_id: 1'b0};
        inject_rsp[node].generic.hdr.src_id = '{x: 2'(node % NumX),
                                                y: 2'(node / NumX),
                                                port_id: 1'b0};
        inject_rsp[node].generic.hdr.last   = 1'b1;
        inject_rsp[node].axi_b.payload.id   = rs_tag[2:0];
        inject_rsp_valid[node] = rs_valid[0];
        eject_rsp_ready[node]  = rs_ej_ready[0];
      end

      #1;
      sample_pre();

      clk = 1'b1;
      #1;
      for (int unsigned n = 0; n < NumNodes; n++) begin
        $fwrite(trace_fd, "%0d,%0d,", cycle, n);
        $fwrite(trace_fd, "%0d,%0d,%0d,%0d,",
                pre_rq_inj_ready[n], pre_rq_ej_valid[n], pre_rq_ej_ch[n],
                pre_rq_ej_dst[n]);
        $fwrite(trace_fd, "%0d,%0d,", pre_rq_ej_last[n], pre_rq_ej_tag[n]);
        $fwrite(trace_fd, "%0d,%0d,%0d,",
                pre_rs_inj_ready[n], pre_rs_ej_valid[n], pre_rs_ej_dst[n]);
        $fwrite(trace_fd, "%0d,", pre_rs_ej_tag[n]);
        $fwrite(trace_fd, "%0d,%0d,%0d,%0d,",
                req_out[n][PortEject].ready, req_out[n][PortEject].valid,
                req_out[n][PortEject].req.generic.hdr.axi_ch, req_dst_of(n));
        $fwrite(trace_fd, "%0d,%0d,",
                req_out[n][PortEject].req.generic.hdr.last, req_tag_of(n));
        $fwrite(trace_fd, "%0d,%0d,%0d,",
                rsp_out[n][PortEject].ready, rsp_out[n][PortEject].valid,
                rsp_dst_of(n));
        $fwrite(trace_fd, "%0d\n", rsp_tag_of(n));
      end

      #1;
      clk = 1'b0;
      #1;
    end

    $fclose(stimulus_fd);
    $fclose(trace_fd);
    $finish;
  end

endmodule
