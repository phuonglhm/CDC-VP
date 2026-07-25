// SPDX-License-Identifier: SHL-0.51

module tb_route_select_trace;
  import floo_pkg::*;

  typedef struct packed {
    logic [1:0] x;
    logic [1:0] y;
    logic       port_id;
  } id_t;

  typedef struct packed {
    id_t         dst_id;
    id_t         src_id;
    logic        last;
    collect_op_e collective_op;
  } hdr_t;

  typedef struct packed {
    hdr_t        hdr;
    logic [63:0] payload;
  } flit_t;

  logic       clk_i;
  logic       rst_ni;
  logic       test_enable_i;
  id_t        xy_id_i;
  logic       id_route_map_i;
  flit_t      channel_i;
  logic       valid_i;
  logic       ready_i;
  flit_t      channel_o;
  logic [4:0] route_sel_o;
  logic [2:0] route_sel_id_o;

  floo_route_select #(
    .NumRoutes     (5),
    .RouteAlgo     (XYRouting),
    .LockRouting   (1'b1),
    .IdWidth       ($bits(id_t)),
    .NumAddrRules  (1),
    .RouteSelWidth (3),
    .EnMultiCast   (1'b0),
    .flit_t        (flit_t),
    .addr_rule_t   (logic),
    .id_t          (id_t)
  ) dut (
    .clk_i,
    .rst_ni,
    .test_enable_i,
    .xy_id_i,
    .id_route_map_i,
    .channel_i,
    .valid_i,
    .ready_i,
    .channel_o,
    .route_sel_o,
    .route_sel_id_o
  );

  string stimulus_path;
  string trace_path;
  integer stimulus_fd;
  integer trace_fd;
  integer scan_result;
  integer cycle;
  integer rst_n_value;
  integer router_x;
  integer router_y;
  integer dst_x;
  integer dst_y;
  integer dst_port;
  integer last_value;
  integer valid_value;
  integer ready_value;
  string header_line;

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

    clk_i = 1'b0;
    rst_ni = 1'b0;
    test_enable_i = 1'b0;
    xy_id_i = '0;
    id_route_map_i = '0;
    channel_i = '0;
    valid_i = 1'b0;
    ready_i = 1'b0;

    scan_result = $fgets(header_line, stimulus_fd);
    $fwrite(trace_fd, "cycle,route,locked\n");

    while (!$feof(stimulus_fd)) begin
      scan_result = $fscanf(
        stimulus_fd,
        "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
        cycle,
        rst_n_value,
        router_x,
        router_y,
        dst_x,
        dst_y,
        dst_port,
        last_value,
        valid_value,
        ready_value
      );

      if (scan_result == 10) begin
        clk_i = 1'b0;
        rst_ni = rst_n_value[0];
        xy_id_i.x = router_x[1:0];
        xy_id_i.y = router_y[1:0];
        xy_id_i.port_id = 1'b0;
        channel_i.hdr.dst_id.x = dst_x[1:0];
        channel_i.hdr.dst_id.y = dst_y[1:0];
        channel_i.hdr.dst_id.port_id = dst_port[0];
        channel_i.hdr.last = last_value[0];
        channel_i.hdr.collective_op = Unicast;
        valid_i = valid_value[0];
        ready_i = ready_value[0];

        #1;
        clk_i = 1'b1;
        #1;
        $fwrite(
          trace_fd,
          "%0d,%0d,%0d\n",
          cycle,
          route_sel_id_o,
          dut.gen_lock.locked_route_q
        );
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
