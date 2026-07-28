// SPDX-License-Identifier: SHL-0.51
//
// `floo_wormhole_arbiter.sv` opens with `import floo_pkg::*;` but references no
// symbol from that package: its types come from the `flit_t` parameter, and its
// only package call is `cf_math_pkg::idx_width`.
//
// This deliberately empty package satisfies the import and nothing else. If a
// future revision of the arbiter starts using a `floo_pkg` declaration, this
// file stops elaborating instead of silently supplying a substitute value.
//
// The frozen `hw/floo_pkg.sv` is not used here because it depends on `axi_pkg`
// from the `axi` dependency, which no current cross-check needs.

package floo_pkg;
endpackage
