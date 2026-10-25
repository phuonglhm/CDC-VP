// fifo_v3_standin.sv -- stand-in for the module fifo_v3 that vlsu/gvu_pooling_feeder.sv (latest hardware RTL drop) instantiates and the
// RTL delivery does not contain (B8 of the review 1051). Written from the behaviour of fifo_v3 in the common_cells library,
// for the parameters the feeder uses (FALL_THROUGH 1, DATA_WIDTH 8, DEPTH 5, flush_i and testmode_i tied to 0): a push into
// an empty FIFO is visible on data_o in the same cycle (empty_o low), and with a pop in that cycle the pointers do not move;
// a push while full is dropped, a pop while empty is ignored. The port has/rtl_ref_gvu_pool_feeder.h (FifoFt8) models the
// same behaviour. Whether the hardware team uses the common_cells module is an open question until the file is delivered.
module fifo_v3 #(
  parameter bit          FALL_THROUGH = 1'b0,
  parameter int unsigned DATA_WIDTH   = 32,
  parameter int unsigned DEPTH        = 8,
  parameter type         dtype        = logic [DATA_WIDTH-1:0],
  parameter int unsigned ADDR_DEPTH   = (DEPTH > 1) ? $clog2(DEPTH) : 1
) (
  input  logic                  clk_i,
  input  logic                  rst_ni,
  input  logic                  flush_i,
  input  logic                  testmode_i,
  output logic                  full_o,
  output logic                  empty_o,
  output logic [ADDR_DEPTH-1:0] usage_o,
  input  dtype                  data_i,
  input  logic                  push_i,
  output dtype                  data_o,
  input  logic                  pop_i
);
  localparam int unsigned FifoDepth = (DEPTH > 0) ? DEPTH : 1;
  logic [ADDR_DEPTH-1:0] read_pointer_n, read_pointer_q, write_pointer_n, write_pointer_q;
  logic [ADDR_DEPTH:0]   status_cnt_n, status_cnt_q;
  dtype [FifoDepth-1:0]  mem_n, mem_q;

  assign usage_o = status_cnt_q[ADDR_DEPTH-1:0];
  assign full_o  = (status_cnt_q == FifoDepth[ADDR_DEPTH:0]);
  assign empty_o = (status_cnt_q == 0) & ~(FALL_THROUGH & push_i);

  always_comb begin
    read_pointer_n  = read_pointer_q;
    write_pointer_n = write_pointer_q;
    status_cnt_n    = status_cnt_q;
    data_o          = mem_q[read_pointer_q];
    mem_n           = mem_q;
    if (push_i && ~full_o) begin
      mem_n[write_pointer_q] = data_i;
      if (write_pointer_q == FifoDepth[ADDR_DEPTH-1:0] - 1) write_pointer_n = '0;
      else write_pointer_n = write_pointer_q + 1;
      status_cnt_n = status_cnt_q + 1;
    end
    if (pop_i && ~empty_o) begin
      if (read_pointer_n == FifoDepth[ADDR_DEPTH-1:0] - 1) read_pointer_n = '0;
      else read_pointer_n = read_pointer_q + 1;
      status_cnt_n = status_cnt_n - 1;
    end
    if (push_i && pop_i && ~full_o && ~empty_o) status_cnt_n = status_cnt_q;
    if (FALL_THROUGH && (status_cnt_q == 0) && push_i) begin
      data_o = data_i;
      if (pop_i) begin
        status_cnt_n    = status_cnt_q;
        read_pointer_n  = read_pointer_q;
        write_pointer_n = write_pointer_q;
      end
    end
  end

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (~rst_ni) begin
      read_pointer_q  <= '0;
      write_pointer_q <= '0;
      status_cnt_q    <= '0;
      mem_q           <= '0;
    end else if (flush_i) begin
      read_pointer_q  <= '0;
      write_pointer_q <= '0;
      status_cnt_q    <= '0;
    end else begin
      read_pointer_q  <= read_pointer_n;
      write_pointer_q <= write_pointer_n;
      status_cnt_q    <= status_cnt_n;
      mem_q           <= mem_n;
    end
  end
endmodule
