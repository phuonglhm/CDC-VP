// SPDX-License-Identifier: SHL-0.51
//
// RTL side of the chimney response-path cross-check.
//
// The chimney is exercised as a *subordinate*: request flits arrive on
// `floo_req_i`, the chimney reissues them on its `axi_out` port, the testbench
// answers on `axi_out_rsp_i`, and the resulting response flits are captured on
// `floo_rsp_o`.
//
// This is what exercises `floo_meta_buffer.sv`: the chimney must remember the
// requester's `src_id` and the manager's original AXI ID, reissue downstream
// under its own ID, and restore both when the response comes back.
//
// Scope: flit content and per-transaction ordering, not chimney timing.
//
// Transactions are issued in batches of `BatchSize` before any response is
// driven, so several requests are outstanding at once. That is deliberate:
// with one transaction in flight the metadata FIFOs never hold more than one
// entry, and a negative control that swapped the read and write FIFOs still
// passed. Batching exercises FIFO ordering and the separation of the read and
// write buffers.
//
// Timing convention follows `hw/tb/tb_floo_axi_chimney.sv`: stimulus is applied
// at `ApplTime` after a clock edge and handshakes are sampled at `TestTime`.

`include "axi/typedef.svh"
`include "floo_noc/typedef.svh"

module tb_floo_axi_chimney_rsp_trace;

  import floo_pkg::*;

  localparam time CyclTime = 10ns;
  localparam time ApplTime = 2ns;
  localparam time TestTime = 8ns;
  // Number of transactions issued before their responses are driven.
  localparam int unsigned BatchSize = 3;

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

  initial begin
    clk = 1'b0;
    forever #(CyclTime / 2) clk = ~clk;
  end

  integer guard;
  integer beat_seq;
  integer trace_count;
  integer trace_fd;

  initial begin
    #(CyclTime * 4000);
    $fatal(1, "watchdog: stalled at transaction %0d with %0d flits",
           beat_seq, trace_count);
  end

  // Outgoing response handshake: `floo_rsp_o.valid && floo_rsp_i.ready`.
  wire floo_rsp_fire = floo_rsp_out.valid && floo_rsp_in.ready;

  task automatic record_flit();
    automatic hdr_t hdr = floo_rsp_out.rsp.generic.hdr;
    automatic logic [63:0] rsp_id, rsp_field;
    rsp_id = '0; rsp_field = '0;
    case (hdr.axi_ch)
      AxiB: begin
        rsp_id    = floo_rsp_out.rsp.axi_b.payload.id;
        rsp_field = floo_rsp_out.rsp.axi_b.payload.resp;
      end
      AxiR: begin
        rsp_id    = floo_rsp_out.rsp.axi_r.payload.id;
        rsp_field = floo_rsp_out.rsp.axi_r.payload.data;
      end
      default: ;
    endcase
    $fwrite(trace_fd, "%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0h,%0h\n",
            trace_count, hdr.axi_ch,
            hdr.dst_id.x, hdr.dst_id.y, hdr.src_id.x, hdr.src_id.y,
            hdr.last, hdr.atop, hdr.rob_req, hdr.rob_idx, rsp_id, rsp_field);
    trace_count = trace_count + 1;
  endtask

  always @(posedge clk) begin
    if (rst_n && floo_rsp_fire) begin
      record_flit();
    end
  end

  string  stimulus_path;
  string  trace_path;
  integer stimulus_fd;
  integer scan_result;
  string  header_line;

  integer beat_ch;
  integer beat_src_x;
  integer beat_src_y;
  integer beat_id;
  integer beat_addr;
  logic [63:0] beat_data;
  integer beat_resp;

  task automatic await_flit(input integer target);
    guard = 0;
    while (trace_count < target && guard < 400) begin
      @(posedge clk);
      guard = guard + 1;
    end
    if (trace_count < target) begin
      $fatal(1, "no response flit for transaction %0d", beat_seq);
    end
  endtask

  // Synchronous BFM idiom: assert with a non-blocking assignment so the value
  // takes effect after the edge, and sample the handshake at the edge, which
  // reads the pre-edge value. This avoids the ApplTime/TestTime phase
  // arithmetic that repeatedly left a `valid` asserted across two edges.
  task automatic send_req_flit();
    guard = 0;
    floo_req_in.valid <= 1'b1;
    forever begin
      @(posedge clk);
      if (floo_req_out.ready) begin
        floo_req_in.valid <= 1'b0;
        return;
      end
      guard = guard + 1;
      if (guard > 200) begin
        $fatal(1, "chimney never accepted request flit %0d", beat_seq);
      end
    end
  endtask

  // The downstream subordinate is permanently ready. Making its readiness
  // conditional on the testbench sequence deadlocks the chimney: it cannot
  // accept an inbound request flit until it can pass that request downstream.
  //
  // The ID the chimney reissues under is captured by a concurrent monitor, so
  // the response can echo back whatever the RTL actually chose.
  logic [2:0] observed_aw_id, observed_ar_id;
  integer     observed_aw_count, observed_ar_count;

  always @(posedge clk) begin
    if (!rst_n) begin
      observed_aw_count <= 0;
      observed_ar_count <= 0;
    end else begin
      if (axi_out_req.aw_valid && axi_out_rsp.aw_ready) begin
        observed_aw_id    <= axi_out_req.aw.id;
        observed_aw_count <= observed_aw_count + 1;
      end
      if (axi_out_req.ar_valid && axi_out_rsp.ar_ready) begin
        observed_ar_id    <= axi_out_req.ar.id;
        observed_ar_count <= observed_ar_count + 1;
      end
    end
  end

  task automatic await_downstream(input integer is_write, input integer target);
    guard = 0;
    forever begin
      @(posedge clk);
      if (( is_write && observed_aw_count >= target) ||
          (!is_write && observed_ar_count >= target)) begin
        return;
      end
      guard = guard + 1;
      if (guard > 200) begin
        $fatal(1, "no downstream request for transaction %0d", beat_seq);
      end
    end
  endtask

  task automatic drive_response(input integer is_write,
                                input logic [2:0] out_id);
    guard = 0;
    if (is_write) begin
      axi_out_rsp.b       <= '0;
      axi_out_rsp.b.id    <= out_id;
      axi_out_rsp.b.resp  <= beat_resp[1:0];
      axi_out_rsp.b_valid <= 1'b1;
    end else begin
      axi_out_rsp.r       <= '0;
      axi_out_rsp.r.id    <= out_id;
      axi_out_rsp.r.data  <= beat_data;
      axi_out_rsp.r.resp  <= beat_resp[1:0];
      axi_out_rsp.r.last  <= 1'b1;
      axi_out_rsp.r_valid <= 1'b1;
    end
    forever begin
      @(posedge clk);
      if ((is_write && axi_out_req.b_ready) ||
          (!is_write && axi_out_req.r_ready)) begin
        axi_out_rsp.b_valid <= 1'b0;
        axi_out_rsp.r_valid <= 1'b0;
        return;
      end
      guard = guard + 1;
      if (guard > 200) begin
        $fatal(1, "response not accepted for transaction %0d", beat_seq);
      end
    end
  endtask

  integer write_count, read_count;

  // Parsed stimulus, buffered so a whole batch can be issued first.
  integer      q_ch   [256];
  integer      q_src_x[256];
  integer      q_src_y[256];
  integer      q_id   [256];
  integer      q_addr [256];
  logic [63:0] q_data [256];
  integer      q_resp [256];
  integer      q_count;
  integer      q_index;
  integer      batch_start;
  integer      batch_end;
  integer      k;
  // Counted per transaction, never derived from `trace_count`: the response
  // flit can be captured before the wait is even entered, and
  // `await_flit(trace_count + 1)` would then wait for a flit that never comes.
  integer expected_rsp;

  initial begin
    if (!$value$plusargs("STIM_FILE=%s", stimulus_path)) $fatal(1, "missing +STIM_FILE");
    if (!$value$plusargs("TRACE_FILE=%s", trace_path)) $fatal(1, "missing +TRACE_FILE");

    stimulus_fd = $fopen(stimulus_path, "r");
    if (stimulus_fd == 0) $fatal(1, "cannot open %s", stimulus_path);
    trace_fd = $fopen(trace_path, "w");
    if (trace_fd == 0) $fatal(1, "cannot create %s", trace_path);

    trace_count  = 0;
    write_count  = 0;
    read_count   = 0;
    expected_rsp = 0;
    rst_n       = 1'b0;
    axi_in_req  = '0;
    axi_out_rsp = '0;
    floo_req_in = '0;
    floo_rsp_in = '0;
    // Always ready to take the chimney's outgoing flits.
    floo_req_in.ready = 1'b1;
    floo_rsp_in.ready = 1'b1;
    // The manager side is idle throughout; it must still accept responses.
    axi_in_req.b_ready = 1'b1;
    axi_in_req.r_ready = 1'b1;
    // Permanently ready downstream subordinate.
    axi_out_rsp.aw_ready = 1'b1;
    axi_out_rsp.w_ready  = 1'b1;
    axi_out_rsp.ar_ready = 1'b1;
    node_id.x = 2'd1;
    node_id.y = 2'd1;
    node_id.port_id = 1'b0;

    $fwrite(trace_fd,
            "seq,axi_ch,dst_x,dst_y,src_x,src_y,last,atop,rob_req,rob_idx,rsp_id,rsp_field\n");

    repeat (5) @(posedge clk);
    rst_n = 1'b1;
    repeat (2) @(posedge clk);

    scan_result = $fgets(header_line, stimulus_fd);

    // Pass 1: read the whole stimulus.
    q_count = 0;
    while (!$feof(stimulus_fd)) begin
      scan_result = $fscanf(stimulus_fd, "%d,%d,%d,%d,%d,%h,%h,%d\n",
                            beat_seq, beat_ch, beat_src_x, beat_src_y,
                            beat_id, beat_addr, beat_data, beat_resp);
      if (scan_result == 8) begin
        q_ch[q_count]    = beat_ch;
        q_src_x[q_count] = beat_src_x;
        q_src_y[q_count] = beat_src_y;
        q_id[q_count]    = beat_id;
        q_addr[q_count]  = beat_addr;
        q_data[q_count]  = beat_data;
        q_resp[q_count]  = beat_resp;
        q_count = q_count + 1;
      end
    end

    // Pass 2: issue each batch of requests, then answer them in order.
    batch_start = 0;
    while (batch_start < q_count) begin
      batch_end = batch_start + BatchSize;
      if (batch_end > q_count) batch_end = q_count;

      for (k = batch_start; k < batch_end; k = k + 1) begin
        beat_seq = k;
        floo_req_in.req = '0;
        if (q_ch[k] == AxiAw) begin
          floo_req_in.req.axi_aw.hdr.dst_id.x  = node_id.x;
          floo_req_in.req.axi_aw.hdr.dst_id.y  = node_id.y;
          floo_req_in.req.axi_aw.hdr.src_id.x  = q_src_x[k][1:0];
          floo_req_in.req.axi_aw.hdr.src_id.y  = q_src_y[k][1:0];
          floo_req_in.req.axi_aw.hdr.last      = 1'b0;
          floo_req_in.req.axi_aw.hdr.axi_ch    = AxiAw;
          floo_req_in.req.axi_aw.hdr.rob_req   = 1'b1;
          floo_req_in.req.axi_aw.payload       = '0;
          floo_req_in.req.axi_aw.payload.id    = q_id[k][2:0];
          floo_req_in.req.axi_aw.payload.addr  = q_addr[k];
          floo_req_in.req.axi_aw.payload.size  = 3'd3;
          floo_req_in.req.axi_aw.payload.burst = axi_pkg::BURST_INCR;
          send_req_flit();

          floo_req_in.req = '0;
          floo_req_in.req.axi_w.hdr.dst_id.x = node_id.x;
          floo_req_in.req.axi_w.hdr.dst_id.y = node_id.y;
          floo_req_in.req.axi_w.hdr.src_id.x = q_src_x[k][1:0];
          floo_req_in.req.axi_w.hdr.src_id.y = q_src_y[k][1:0];
          floo_req_in.req.axi_w.hdr.last     = 1'b1;
          floo_req_in.req.axi_w.hdr.axi_ch   = AxiW;
          floo_req_in.req.axi_w.hdr.rob_req  = 1'b1;
          floo_req_in.req.axi_w.payload      = '0;
          floo_req_in.req.axi_w.payload.data = q_data[k];
          floo_req_in.req.axi_w.payload.strb = '1;
          floo_req_in.req.axi_w.payload.last = 1'b1;
          send_req_flit();

          write_count = write_count + 1;
          await_downstream(1, write_count);
        end else begin
          floo_req_in.req.axi_ar.hdr.dst_id.x  = node_id.x;
          floo_req_in.req.axi_ar.hdr.dst_id.y  = node_id.y;
          floo_req_in.req.axi_ar.hdr.src_id.x  = q_src_x[k][1:0];
          floo_req_in.req.axi_ar.hdr.src_id.y  = q_src_y[k][1:0];
          floo_req_in.req.axi_ar.hdr.last      = 1'b1;
          floo_req_in.req.axi_ar.hdr.axi_ch    = AxiAr;
          floo_req_in.req.axi_ar.hdr.rob_req   = 1'b1;
          floo_req_in.req.axi_ar.payload       = '0;
          floo_req_in.req.axi_ar.payload.id    = q_id[k][2:0];
          floo_req_in.req.axi_ar.payload.addr  = q_addr[k];
          floo_req_in.req.axi_ar.payload.size  = 3'd3;
          floo_req_in.req.axi_ar.payload.burst = axi_pkg::BURST_INCR;
          send_req_flit();

          read_count = read_count + 1;
          await_downstream(0, read_count);
        end
      end

      // Answer the batch in issue order, per direction.
      for (k = batch_start; k < batch_end; k = k + 1) begin
        beat_seq  = k;
        beat_data = q_data[k];
        beat_resp = q_resp[k];
        if (q_ch[k] == AxiAw) begin
          drive_response(1, observed_aw_id);
        end else begin
          drive_response(0, observed_ar_id);
        end
        expected_rsp = expected_rsp + 1;
        await_flit(expected_rsp);
      end

      batch_start = batch_end;
    end

    repeat (10) @(posedge clk);
    $fclose(stimulus_fd);
    $fclose(trace_fd);
    $finish;
  end

endmodule
