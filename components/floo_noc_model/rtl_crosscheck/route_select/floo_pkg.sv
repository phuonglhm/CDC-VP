// SPDX-License-Identifier: SHL-0.51
//
// Leaf-compilation shim containing only floo_pkg declarations referenced by
// floo_route_select.sv. Values match the frozen FlooNoC revision.

package floo_pkg;

  typedef enum logic [1:0] {
    IdTable,
    SourceRouting,
    XYRouting,
    YXRouting
  } route_algo_e;

  typedef enum logic [2:0] {
    North = 3'd0,
    East  = 3'd1,
    South = 3'd2,
    West  = 3'd3,
    Eject = 3'd4,
    NumDirections
  } route_direction_e;

  typedef enum logic [3:0] {
    Unicast   = 4'b0000,
    Multicast = 4'b0001,
    LsbAnd    = 4'b0010,
    FpAdd     = 4'b0011,
    FpMul     = 4'b0100,
    FpMin     = 4'b0101,
    FpMax     = 4'b0110,
    IntAdd    = 4'b0111,
    IntMul    = 4'b1000,
    IntMinS   = 4'b1001,
    IntMinU   = 4'b1010,
    IntMaxS   = 4'b1011,
    IntMaxU   = 4'b1100,
    SelectAW  = 4'b1101,
    CollectB  = 4'b1110,
    SeqAW     = 4'b1111
  } collect_op_e;

  function automatic integer unsigned floo_iomsb(
      input integer unsigned width);
    return (width != 32'd0) ? unsigned'(width - 1) : 32'd0;
  endfunction

endpackage
