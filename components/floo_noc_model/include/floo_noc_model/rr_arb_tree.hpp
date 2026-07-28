// SPDX-License-Identifier: SHL-0.51
//
// SystemC mirror of the arbitration primitives that FlooNoC
// `hw/floo_wormhole_arbiter.sv` instantiates, using the dependency revision
// locked in the frozen `Bender.lock` (common_cells 1.39.0 @ 9ca8a76):
//
//   common_cells/src/rr_arb_tree.sv
//   common_cells/src/lzc.sv
//   common_cells/src/cf_math_pkg.sv  (idx_width only)
//
// The frozen instantiation is:
//
//   ExtPrio = 1'b0, AxiVldRdy = 1'b1, LockIn = 1'b1, FairArb = 1'b1,
//   DataType = logic, data_i = '0, flush_i = 1'b0,
//   gnt_o, req_o, and data_o unconnected.
//
// Only `idx_o` leaves the tree in that configuration, so the data multiplexer
// and the per-input grant decode are not modeled. `req_o` is modeled because
// the lock and round-robin state depend on it. `flush_i` is not modeled because
// the frozen instantiation ties it low.
//
// The arbitration tree, the trailing-zero counters, and the fair next-index
// computation are reproduced structurally rather than replaced by a behavioural
// equivalent, so the model can be read side by side with the RTL. Request sets
// are carried as bit masks, which keeps the registered state usable directly in
// `sc_signal`.

#pragma once

#include <array>
#include <cstddef>

namespace floo::model {

namespace arb_detail {

/// `$clog2` as used by the frozen RTL for tree sizing.
constexpr unsigned clog2(unsigned value)
{
    unsigned bits = 0;
    while ((1u << bits) < value) {
        ++bits;
    }
    return bits;
}

/// `cf_math_pkg::idx_width`.
constexpr unsigned idx_width(unsigned num_idx)
{
    return num_idx > 1u ? clog2(num_idx) : 1u;
}

constexpr bool bit_of(unsigned mask, unsigned index)
{
    return ((mask >> index) & 1u) != 0;
}

struct lzc_result {
    unsigned cnt{};
    bool empty{};
};

/// Mirrors `lzc` with `MODE = 1'b0` (trailing zero count).
///
/// The all-zero result is taken from the same node tree the RTL builds rather
/// than from that module's doc comment, because for a non power-of-two `Width`
/// the two do not agree.
template <unsigned Width>
lzc_result lzc_trailing(unsigned in)
{
    if constexpr (Width <= 1) {
        const bool set = bit_of(in, 0);
        return {static_cast<unsigned>(!set), !set};
    } else {
        constexpr unsigned num_levels = clog2(Width);
        constexpr std::size_t num_nodes = 1u << num_levels;

        std::array<bool, num_nodes> sel_nodes{};
        std::array<unsigned, num_nodes> index_nodes{};

        for (unsigned level = num_levels; level-- > 0;) {
            const unsigned count = 1u << level;
            for (unsigned k = 0; k < count; ++k) {
                const std::size_t node = (1u << level) - 1 + k;
                if (level == num_levels - 1) {
                    if (k * 2 < Width - 1) {
                        sel_nodes[node] =
                            bit_of(in, k * 2) || bit_of(in, k * 2 + 1);
                        index_nodes[node] =
                            bit_of(in, k * 2) ? k * 2 : k * 2 + 1;
                    } else if (k * 2 == Width - 1) {
                        sel_nodes[node] = bit_of(in, k * 2);
                        index_nodes[node] = k * 2;
                    } else {
                        sel_nodes[node] = false;
                        index_nodes[node] = 0;
                    }
                } else {
                    const std::size_t left = (1u << (level + 1)) - 1 + k * 2;
                    const std::size_t right = left + 1;
                    sel_nodes[node] = sel_nodes[left] || sel_nodes[right];
                    index_nodes[node] =
                        sel_nodes[left] ? index_nodes[left] : index_nodes[right];
                }
            }
        }

        return {index_nodes[0], !sel_nodes[0]};
    }
}

} // namespace arb_detail

/// Mirrors `rr_arb_tree` in the frozen configuration. Stateless by design: the
/// registers (`rr_q`, `lock_q`, `req_q`) live in the enclosing `sc_module` so
/// they participate in normal SystemC sensitivity.
template <unsigned NumIn>
struct rr_arb_tree {
    static_assert(NumIn > 0, "rr_arb_tree needs at least one input");
    static_assert(NumIn <= 32, "request masks are carried in an unsigned");

    static constexpr unsigned num_levels = arb_detail::clog2(NumIn);

    struct decision {
        /// assign req_d = (lock_q) ? req_q : req_i;
        unsigned req_d{};
        bool req_o{};
        unsigned idx_o{};
    };

    static decision evaluate(
        unsigned req_i, bool lock_q, unsigned req_q, unsigned rr_q)
    {
        decision out;
        out.req_d = lock_q ? req_q : req_i;

        if constexpr (NumIn == 1) {
            out.req_o = arb_detail::bit_of(out.req_d, 0);
            out.idx_o = 0;
            return out;
        } else {
            constexpr std::size_t num_nodes = 1u << num_levels;
            std::array<bool, num_nodes> req_nodes{};
            std::array<unsigned, num_nodes> index_nodes{};

            for (unsigned level = num_levels; level-- > 0;) {
                const unsigned count = 1u << level;
                // rr_q[NumLevels-1-level]
                const bool rr_bit =
                    arb_detail::bit_of(rr_q, num_levels - 1 - level);

                for (unsigned l = 0; l < count; ++l) {
                    const std::size_t node = (1u << level) - 1 + l;

                    if (level == num_levels - 1) {
                        if (l * 2 < NumIn - 1) {
                            const bool left = arb_detail::bit_of(out.req_d, l * 2);
                            const bool right =
                                arb_detail::bit_of(out.req_d, l * 2 + 1);
                            req_nodes[node] = left || right;
                            // assign sel = ~req_d[l*2] | req_d[l*2+1] & rr_q[..]
                            const bool sel = !left || (right && rr_bit);
                            index_nodes[node] = static_cast<unsigned>(sel);
                        } else if (l * 2 == NumIn - 1) {
                            req_nodes[node] = arb_detail::bit_of(out.req_d, l * 2);
                            index_nodes[node] = 0;
                        } else {
                            req_nodes[node] = false;
                            index_nodes[node] = 0;
                        }
                    } else {
                        const std::size_t left = (1u << (level + 1)) - 1 + l * 2;
                        const std::size_t right = left + 1;
                        req_nodes[node] = req_nodes[left] || req_nodes[right];
                        const bool sel =
                            !req_nodes[left] || (req_nodes[right] && rr_bit);
                        // {sel, index_nodes[child][NumLevels-level-2:0]}
                        const unsigned child_bits = num_levels - level - 1;
                        const unsigned child_mask = (1u << child_bits) - 1;
                        const unsigned child =
                            sel ? index_nodes[right] : index_nodes[left];
                        index_nodes[node] =
                            (static_cast<unsigned>(sel) << child_bits)
                            | (child & child_mask);
                    }
                }
            }

            out.req_o = req_nodes[0];
            out.idx_o = index_nodes[0];
            return out;
        }
    }

    /// `FairArb` next state: jump to the next unserved request above `rr_q`,
    /// wrapping through the lower mask when the upper one is empty.
    static unsigned next_rr(unsigned req_d, unsigned rr_q)
    {
        unsigned upper_mask = 0;
        unsigned lower_mask = 0;
        for (unsigned i = 0; i < NumIn; ++i) {
            if (!arb_detail::bit_of(req_d, i)) {
                continue;
            }
            if (i > rr_q) {
                upper_mask |= 1u << i;
            } else {
                lower_mask |= 1u << i;
            }
        }

        const auto upper = arb_detail::lzc_trailing<NumIn>(upper_mask);
        const auto lower = arb_detail::lzc_trailing<NumIn>(lower_mask);
        return upper.empty ? lower.cnt : upper.cnt;
    }
};

} // namespace floo::model
