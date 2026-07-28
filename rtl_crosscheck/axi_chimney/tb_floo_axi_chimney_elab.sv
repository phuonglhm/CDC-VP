// SPDX-License-Identifier: SHL-0.51
//
// Elaboration probe for the future chimney cross-check.
//
// It instantiates the unmodified frozen `hw/floo_axi_chimney.sv` with the
// parameter set of `hw/test/floo_test_pkg.sv` and the real type macros, and
// nothing else. Its only job today is to prove the harness is buildable:
// linting it against the Bender-generated file list reports zero errors, so
// the remaining work for a real cross-check is stimulus and tracing, not type
// plumbing.
//
// Why not reuse the upstream `hw/tb/tb_floo_axi_chimney.sv`: that testbench
// depends on the class-based `axi_test` package, which Verilator does not
// support. It remains usable under VCS, which is installed on this host, and
// that is the alternative path if a class-based driver is ever wanted.
//
// Note the destination decode here is `UseIdTable: 0`, matching the upstream
// test package: the coordinate is extracted from address bits rather than
// looked up in a system address map. `floogen/examples/axi_mesh_xy.yml` uses
// the table mode instead. The model implements both.
`include "axi/typedef.svh"
`include "floo_noc/typedef.svh"

module tb_floo_axi_chimney_elab;
  import floo_pkg::*;

  localparam axi_cfg_t     AxiCfg     = '{AddrWidth:32, DataWidth:64,
                                          UserWidth:1, InIdWidth:3,
                                          OutIdWidth:3};
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
  floo_req_t    floo_req_o, floo_req_i;
  floo_rsp_t    floo_rsp_o, floo_rsp_i;
  id_t          id;

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
    .clk_i         ( clk         ),
    .rst_ni        ( rst_n       ),
    .sram_cfg_i    ( '0          ),
    .test_enable_i ( 1'b0        ),
    .axi_in_req_i  ( axi_in_req  ),
    .axi_in_rsp_o  ( axi_in_rsp  ),
    .axi_out_req_o ( axi_out_req ),
    .axi_out_rsp_i ( axi_out_rsp ),
    .id_i          ( id          ),
    .route_table_i ( '0          ),
    .floo_req_o    ( floo_req_o  ),
    .floo_rsp_o    ( floo_rsp_o  ),
    .floo_req_i    ( floo_req_i  ),
    .floo_rsp_i    ( floo_rsp_i  )
  );
endmodule
