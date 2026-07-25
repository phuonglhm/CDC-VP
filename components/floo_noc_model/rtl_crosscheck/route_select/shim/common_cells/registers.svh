// Minimal common_cells register macro shim for the leaf RTL cross-check.
// The DUT source remains the unmodified FlooNoC hw/floo_route_select.sv.

`ifndef FLOO_NOC_MODEL_COMMON_CELLS_REGISTERS_SVH
`define FLOO_NOC_MODEL_COMMON_CELLS_REGISTERS_SVH

`define FF(__q, __d, __reset_value) \
  always_ff @(posedge clk_i or negedge rst_ni) begin \
    if (!rst_ni) begin \
      __q <= __reset_value; \
    end else begin \
      __q <= __d; \
    end \
  end

`define FFL(__q, __d, __load, __reset_value) \
  always_ff @(posedge clk_i or negedge rst_ni) begin \
    if (!rst_ni) begin \
      __q <= __reset_value; \
    end else if (__load) begin \
      __q <= __d; \
    end \
  end

`endif
