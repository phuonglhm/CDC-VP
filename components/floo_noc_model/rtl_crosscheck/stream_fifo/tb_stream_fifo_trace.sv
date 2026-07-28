// SPDX-License-Identifier: SHL-0.51
//
// RTL side of the FIFO cross-check. It instantiates the unmodified
// common_cells `stream_fifo_optimal_wrap` with the parameters that FlooNoC
// `hw/floo_router.sv` uses for its input buffers: `flush_i` tied low,
// `testmode_i` low, and `usage_o` unconnected.
//
// Depth is a top-level parameter so the same testbench covers both wrap
// branches (2 -> spill register, >2 -> stream FIFO).

module tb_stream_fifo_trace #(
  parameter int unsigned Depth = 2
);

  localparam int unsigned DataWidth = 64;
  typedef logic [DataWidth-1:0] data_t;

  logic  clk_i;
  logic  rst_ni;
  data_t data_i;
  logic  valid_i;
  logic  ready_o;
  data_t data_o;
  logic  valid_o;
  logic  ready_i;

  stream_fifo_optimal_wrap #(
    .Depth  ( Depth  ),
    .type_t ( data_t )
  ) dut (
    .clk_i,
    .rst_ni,
    .flush_i    ( 1'b0 ),
    .testmode_i ( 1'b0 ),
    .usage_o    (      ),
    .data_i,
    .valid_i,
    .ready_o,
    .data_o,
    .valid_o,
    .ready_i
  );

  string  stimulus_path;
  string  trace_path;
  integer stimulus_fd;
  integer trace_fd;
  integer scan_result;
  integer cycle;
  integer rst_n_value;
  integer valid_value;
  integer ready_value;
  data_t  data_value;
  string  header_line;
  logic   pre_ready;
  logic   pre_valid;
  data_t  pre_data;

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
    data_i  = '0;
    valid_i = 1'b0;
    ready_i = 1'b0;

    scan_result = $fgets(header_line, stimulus_fd);
    $fwrite(trace_fd,
            "cycle,pre_ready,pre_valid,pre_data,post_ready,post_valid,post_data\n");

    while (!$feof(stimulus_fd)) begin
      scan_result = $fscanf(
        stimulus_fd,
        "%d,%d,%d,%d,%h\n",
        cycle,
        rst_n_value,
        valid_value,
        ready_value,
        data_value
      );

      if (scan_result == 5) begin
        clk_i   = 1'b0;
        rst_ni  = rst_n_value[0];
        data_i  = data_value;
        valid_i = valid_value[0];
        ready_i = ready_value[0];

        #1;
        // Pre-edge sample: the handshake view the environment acts on.
        pre_ready = ready_o;
        pre_valid = valid_o;
        pre_data  = data_o;

        clk_i = 1'b1;
        #1;
        $fwrite(trace_fd, "%0d,%0d,%0d,%0h,%0d,%0d,%0h\n",
                cycle, pre_ready, pre_valid, pre_data,
                ready_o, valid_o, data_o);
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
