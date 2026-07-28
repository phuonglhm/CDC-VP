// SPDX-License-Identifier: SHL-0.51
//
// RTL side of the AXI sizing cross-check. It evaluates the unmodified frozen
// `floo_pkg` sizing functions over a list of AXI configurations and prints the
// result, so the SystemC mirror can be compared against the real arithmetic
// rather than against a transcription of it.
//
// The functions under test are `floo_pkg::axi_chan_mapping`,
// `get_axi_chan_width`, `get_max_axi_payload_bits`, and `get_axi_rsvd_bits`,
// which in turn call `axi_pkg::{aw,w,b,ar,r}_width`. No shim is involved: the
// compile comes from the Bender-generated file list.

module tb_axi_sizing_trace;

  import floo_pkg::*;

  string  config_path;
  string  trace_path;
  integer config_fd;
  integer trace_fd;
  integer scan_result;
  string  header_line;

  integer addr_width;
  integer data_width;
  integer user_width;
  integer in_id_width;
  integer out_id_width;

  axi_cfg_t cfg;

  initial begin
    if (!$value$plusargs("CONFIG_FILE=%s", config_path)) begin
      $fatal(1, "missing +CONFIG_FILE=<path>");
    end
    if (!$value$plusargs("TRACE_FILE=%s", trace_path)) begin
      $fatal(1, "missing +TRACE_FILE=<path>");
    end

    config_fd = $fopen(config_path, "r");
    if (config_fd == 0) begin
      $fatal(1, "cannot open config file %s", config_path);
    end
    trace_fd = $fopen(trace_path, "w");
    if (trace_fd == 0) begin
      $fatal(1, "cannot create trace file %s", trace_path);
    end

    scan_result = $fgets(header_line, config_fd);
    $fwrite(trace_fd,
            "addr,data,user,in_id,out_id,");
    $fwrite(trace_fd,
            "w_aw,w_w,w_ar,w_b,w_r,map_aw,map_w,map_ar,map_b,map_r,");
    $fwrite(trace_fd,
            "max_req,max_rsp,rsvd_aw,rsvd_w,rsvd_ar,rsvd_b,rsvd_r\n");

    while (!$feof(config_fd)) begin
      scan_result = $fscanf(config_fd, "%d,%d,%d,%d,%d\n",
                            addr_width, data_width, user_width,
                            in_id_width, out_id_width);
      if (scan_result == 5) begin
        cfg.AddrWidth  = addr_width;
        cfg.DataWidth  = data_width;
        cfg.UserWidth  = user_width;
        cfg.InIdWidth  = in_id_width;
        cfg.OutIdWidth = out_id_width;

        $fwrite(trace_fd, "%0d,%0d,%0d,%0d,%0d,",
                addr_width, data_width, user_width, in_id_width, out_id_width);
        $fwrite(trace_fd, "%0d,%0d,%0d,%0d,%0d,",
                get_axi_chan_width(cfg, AxiAw),
                get_axi_chan_width(cfg, AxiW),
                get_axi_chan_width(cfg, AxiAr),
                get_axi_chan_width(cfg, AxiB),
                get_axi_chan_width(cfg, AxiR));
        $fwrite(trace_fd, "%0d,%0d,%0d,%0d,%0d,",
                axi_chan_mapping(AxiAw),
                axi_chan_mapping(AxiW),
                axi_chan_mapping(AxiAr),
                axi_chan_mapping(AxiB),
                axi_chan_mapping(AxiR));
        $fwrite(trace_fd, "%0d,%0d,",
                get_max_axi_payload_bits(cfg, FlooReq),
                get_max_axi_payload_bits(cfg, FlooRsp));
        $fwrite(trace_fd, "%0d,%0d,%0d,%0d,%0d\n",
                get_axi_rsvd_bits(cfg, AxiAw),
                get_axi_rsvd_bits(cfg, AxiW),
                get_axi_rsvd_bits(cfg, AxiAr),
                get_axi_rsvd_bits(cfg, AxiB),
                get_axi_rsvd_bits(cfg, AxiR));
      end
    end

    $fclose(config_fd);
    $fclose(trace_fd);
    $finish;
  end

endmodule
