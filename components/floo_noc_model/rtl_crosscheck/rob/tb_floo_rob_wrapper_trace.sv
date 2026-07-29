// SPDX-License-Identifier: SHL-0.51
//
// RTL side of the `NoRoB` ordering cross-check. It instantiates the unmodified
// frozen `hw/floo_rob_wrapper.sv` with `RoBType = NoRoB`, over the locked axi
// `axi_demux_id_counters` and common_cells `delta_counter`.
//
// No behavioural replacement is used. `floo_pkg` is the real package, because
// the wrapper needs its `rob_type_e` enumeration to select the branch.
//
// The trace exports the three internal signals that decide admission plus the
// whole counter bank through hierarchical references, so the comparison pins
// the model's state and not only its boundary. That matters here: `full_o` is
// a global OR across the bank, and a model with correct per-ID counters but a
// per-ID full would still agree on the boundary until a counter saturates.

module tb_floo_rob_wrapper_trace
  import floo_pkg::*;
#(
  parameter int unsigned AxiIdBits      = 2,
  parameter int unsigned MaxRoTxnsPerId = 4
);

  localparam int unsigned NumIds       = 1 << AxiIdBits;
  localparam int unsigned CounterWidth = $clog2(MaxRoTxnsPerId);
  localparam int unsigned DestWidth    = 4;

  typedef logic [AxiIdBits-1:0]    ax_id_t;
  typedef logic [DestWidth-1:0]    dest_t;
  typedef logic [7:0]              ax_len_t;
  typedef logic                    rob_idx_t;

  // The `NoRoB` branch passes the response straight through, so the response
  // channel only has to carry the `id` field the counter pops by.
  typedef struct packed {
    ax_id_t id;
  } rsp_chan_t;

  logic      clk_i;
  logic      rst_ni;
  logic      ax_valid_i;
  logic      ax_ready_o;
  ax_len_t   ax_len_i;
  ax_id_t    ax_id_i;
  dest_t     ax_dest_i;
  logic      ax_valid_o;
  logic      ax_ready_i;
  logic      ax_rob_req_o;
  rob_idx_t  ax_rob_idx_o;
  logic      rsp_valid_i;
  logic      rsp_ready_o;
  rsp_chan_t rsp_i;
  logic      rsp_rob_req_i;
  rob_idx_t  rsp_rob_idx_i;
  logic      rsp_last_i;
  logic      rsp_valid_o;
  logic      rsp_ready_i;
  rsp_chan_t rsp_o;

  floo_rob_wrapper #(
    .RoBType        ( NoRoB          ),
    .MaxRoTxnsPerId ( MaxRoTxnsPerId ),
    .ax_len_t       ( ax_len_t       ),
    .ax_id_t        ( ax_id_t        ),
    .rsp_chan_t     ( rsp_chan_t     ),
    .rsp_data_t     ( logic          ),
    .rsp_meta_t     ( logic          ),
    .rob_idx_t      ( rob_idx_t      ),
    .dest_t         ( dest_t         ),
    .sram_cfg_t     ( logic          )
  ) dut (
    .clk_i,
    .rst_ni,
    .sram_cfg_i ( 1'b0 ),
    .ax_valid_i,
    .ax_ready_o,
    .ax_len_i,
    .ax_id_i,
    .ax_dest_i,
    .ax_valid_o,
    .ax_ready_i,
    .ax_rob_req_o,
    .ax_rob_idx_o,
    .rsp_valid_i,
    .rsp_ready_o,
    .rsp_i,
    .rsp_rob_req_i,
    .rsp_rob_idx_i,
    .rsp_last_i,
    .rsp_valid_o,
    .rsp_ready_i,
    .rsp_o
  );

  // Internal admission signals, mirrored by the SystemC model.
  wire        in_flight    = dut.gen_no_rob.in_flight;
  wire dest_t prev_dest    = dut.gen_no_rob.prev_dest;
  wire        counter_full = dut.gen_no_rob.counter_full;

  // The counter bank itself. `in_flight` inside the generate loop is the
  // `delta_counter`'s `q_o`, i.e. the low `CounterWidth` bits.
  wire [CounterWidth-1:0] cnt [NumIds];
  wire dest_t             sel [NumIds];
  for (genvar id = 0; id < NumIds; id++) begin : gen_probe
    assign cnt[id] = dut.gen_no_rob.i_axi_demux_id_counters.gen_counters[id].in_flight;
    assign sel[id] = dut.gen_no_rob.i_axi_demux_id_counters.mst_select_q[id];
  end

  string  stimulus_path;
  string  trace_path;
  integer stimulus_fd;
  integer trace_fd;
  integer scan_result;
  integer cycle;
  integer rst_n_value;
  integer ax_valid_value;
  integer ax_id_value;
  integer ax_dest_value;
  integer ax_ready_value;
  integer rsp_valid_value;
  integer rsp_id_value;
  integer rsp_last_value;
  integer rsp_ready_value;
  string  header_line;

  logic  pre_ax_ready;
  logic  pre_ax_valid;
  logic  pre_rsp_ready;
  logic  pre_rsp_valid;
  logic  pre_in_flight;
  dest_t pre_prev_dest;
  logic  pre_full;

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

    clk_i         = 1'b0;
    rst_ni        = 1'b0;
    ax_valid_i    = 1'b0;
    ax_len_i      = '0;
    ax_id_i       = '0;
    ax_dest_i     = '0;
    ax_ready_i    = 1'b0;
    rsp_valid_i   = 1'b0;
    rsp_i         = '0;
    rsp_rob_req_i = 1'b0;
    rsp_rob_idx_i = '0;
    rsp_last_i    = 1'b0;
    rsp_ready_i   = 1'b0;

    scan_result = $fgets(header_line, stimulus_fd);
    $fwrite(trace_fd, "cycle,");
    $fwrite(trace_fd, "pre_ax_ready,pre_ax_valid,pre_rsp_ready,pre_rsp_valid,");
    $fwrite(trace_fd, "pre_in_flight,pre_prev_dest,pre_full,");
    $fwrite(trace_fd,
            "post_ax_ready,post_ax_valid,post_rsp_ready,post_rsp_valid,");
    $fwrite(trace_fd, "post_in_flight,post_prev_dest,post_full,");
    $fwrite(trace_fd, "rob_req,rob_idx");
    for (int unsigned id = 0; id < NumIds; id++) begin
      $fwrite(trace_fd, ",cnt%0d", id);
    end
    for (int unsigned id = 0; id < NumIds; id++) begin
      $fwrite(trace_fd, ",sel%0d", id);
    end
    $fwrite(trace_fd, "\n");

    while (!$feof(stimulus_fd)) begin
      scan_result = $fscanf(
        stimulus_fd,
        "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
        cycle,
        rst_n_value,
        ax_valid_value,
        ax_id_value,
        ax_dest_value,
        ax_ready_value,
        rsp_valid_value,
        rsp_id_value,
        rsp_last_value,
        rsp_ready_value
      );

      if (scan_result == 10) begin
        clk_i       = 1'b0;
        rst_ni      = rst_n_value[0];
        ax_valid_i  = ax_valid_value[0];
        ax_id_i     = ax_id_t'(ax_id_value);
        ax_dest_i   = dest_t'(ax_dest_value);
        ax_ready_i  = ax_ready_value[0];
        rsp_valid_i = rsp_valid_value[0];
        rsp_i.id    = ax_id_t'(rsp_id_value);
        rsp_last_i  = rsp_last_value[0];
        rsp_ready_i = rsp_ready_value[0];

        #1;
        pre_ax_ready  = ax_ready_o;
        pre_ax_valid  = ax_valid_o;
        pre_rsp_ready = rsp_ready_o;
        pre_rsp_valid = rsp_valid_o;
        pre_in_flight = in_flight;
        pre_prev_dest = prev_dest;
        pre_full      = counter_full;

        clk_i = 1'b1;
        #1;
        $fwrite(trace_fd, "%0d,", cycle);
        $fwrite(trace_fd, "%0d,%0d,%0d,%0d,",
                pre_ax_ready, pre_ax_valid, pre_rsp_ready, pre_rsp_valid);
        $fwrite(trace_fd, "%0d,%0d,%0d,",
                pre_in_flight, pre_prev_dest, pre_full);
        $fwrite(trace_fd, "%0d,%0d,%0d,%0d,",
                ax_ready_o, ax_valid_o, rsp_ready_o, rsp_valid_o);
        $fwrite(trace_fd, "%0d,%0d,%0d,",
                in_flight, prev_dest, counter_full);
        $fwrite(trace_fd, "%0d,%0d", ax_rob_req_o, ax_rob_idx_o);
        for (int unsigned id = 0; id < NumIds; id++) begin
          $fwrite(trace_fd, ",%0d", cnt[id]);
        end
        for (int unsigned id = 0; id < NumIds; id++) begin
          $fwrite(trace_fd, ",%0d", sel[id]);
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
