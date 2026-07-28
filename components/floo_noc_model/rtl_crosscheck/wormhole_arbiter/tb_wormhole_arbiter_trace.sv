// SPDX-License-Identifier: SHL-0.51
//
// RTL side of the wormhole-arbiter cross-check. It instantiates the unmodified
// frozen `hw/floo_wormhole_arbiter.sv` over the locked common_cells
// `rr_arb_tree`/`lzc`/`cf_math_pkg`.
//
// `NumRoutes` is a top-level parameter so the same testbench covers the
// five-port router configuration and a power-of-two tree shape.
//
// The trace exports the arbiter's internal registers through hierarchical
// references so the comparison pins the model's state, not only its outputs.

module tb_wormhole_arbiter_trace #(
  parameter int unsigned NumRoutes = 5
);

  localparam int unsigned StimRoutes = 5;
  localparam int unsigned PayloadWidth = 32;

  typedef struct packed {
    logic last;
  } hdr_t;

  typedef struct packed {
    hdr_t                     hdr;
    logic [PayloadWidth-1:0]  payload;
  } flit_t;

  logic                  clk_i;
  logic                  rst_ni;
  logic [NumRoutes-1:0]  valid_i;
  logic [NumRoutes-1:0]  ready_o;
  flit_t [NumRoutes-1:0] data_i;
  logic                  valid_o;
  logic                  ready_i;
  flit_t                 data_o;

  floo_wormhole_arbiter #(
    .NumRoutes ( NumRoutes ),
    .flit_t    ( flit_t    )
  ) dut (
    .clk_i,
    .rst_ni,
    .valid_i,
    .ready_o,
    .data_i,
    .valid_o,
    .ready_i,
    .data_o
  );

  // Internal state, mirrored by the SystemC model.
  wire [NumRoutes-1:0] valid_q = dut.valid_q;
  wire                 last_q  = dut.last_q;
  wire [NumRoutes-1:0] req_q   = dut.i_rr_arb_packets.gen_arbiter.gen_int_rr.gen_lock.req_q;
  wire                 lock_q  = dut.i_rr_arb_packets.gen_arbiter.gen_int_rr.gen_lock.lock_q;
  wire [2:0]           rr_q    = dut.i_rr_arb_packets.gen_arbiter.rr_q;

  string  stimulus_path;
  string  trace_path;
  integer stimulus_fd;
  integer trace_fd;
  integer scan_result;
  integer cycle;
  integer rst_n_value;
  integer ready_value;
  integer valid_mask;
  integer last_mask;
  string  header_line;

  logic [NumRoutes-1:0] pre_ready;
  logic                 pre_valid;
  flit_t                pre_data;
  integer               pre_selected;

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
    ready_i = 1'b0;
    data_i  = '0;

    scan_result = $fgets(header_line, stimulus_fd);
    $fwrite(trace_fd,
            "cycle,pre_ready,pre_valid,pre_data,pre_selected,");
    $fwrite(trace_fd,
            "post_ready,post_valid,post_data,post_selected,");
    $fwrite(trace_fd, "valid_q,last_q,rr_q,lock_q,req_q\n");

    while (!$feof(stimulus_fd)) begin
      scan_result = $fscanf(
        stimulus_fd,
        "%d,%d,%d,%h,%h\n",
        cycle,
        rst_n_value,
        ready_value,
        valid_mask,
        last_mask
      );

      if (scan_result == 5) begin
        clk_i  = 1'b0;
        rst_ni = rst_n_value[0];
        ready_i = ready_value[0];
        for (int unsigned route = 0; route < NumRoutes; route++) begin
          valid_i[route]          = valid_mask[route];
          data_i[route].hdr.last  = last_mask[route];
          data_i[route].payload   = PayloadWidth'((cycle + 1) * 16 + route);
        end

        #1;
        pre_ready    = ready_o;
        pre_valid    = valid_o;
        pre_data     = data_o;
        pre_selected = dut.valid_selected_idx;

        clk_i = 1'b1;
        #1;
        $fwrite(trace_fd, "%0d,%0h,%0d,%0h,%0d,",
                cycle, pre_ready, pre_valid, pre_data.payload, pre_selected);
        $fwrite(trace_fd, "%0h,%0d,%0h,%0d,",
                ready_o, valid_o, data_o.payload, dut.valid_selected_idx);
        $fwrite(trace_fd, "%0h,%0d,%0d,%0d,%0h\n",
                valid_q, last_q, rr_q, lock_q, req_q);
        #1;
        clk_i = 1'b0;
        #1;
      end
    end

    $fclose(stimulus_fd);
    $fclose(trace_fd);
    $finish;
  end

  // StimRoutes only documents the stimulus width; the low NumRoutes bits are
  // used when the testbench is narrower.
  if (NumRoutes > StimRoutes) begin : gen_width_check
    initial $fatal(1, "stimulus carries only %0d routes", StimRoutes);
  end

endmodule
