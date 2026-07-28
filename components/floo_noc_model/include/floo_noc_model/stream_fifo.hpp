// SPDX-License-Identifier: SHL-0.51
//
// SystemC mirror of the input-buffer hierarchy that FlooNoC `hw/floo_router.sv`
// instantiates, using the dependency revisions locked in the frozen
// `Bender.lock` (common_cells 1.39.0 @ 9ca8a76):
//
//   common_cells/src/stream_fifo_optimal_wrap.sv
//     Depth == 2 -> common_cells/src/spill_register_flushable.sv (Bypass = 0)
//     Depth  > 2 -> common_cells/src/stream_fifo.sv (FALL_THROUGH = 0)
//                   -> common_cells/src/fifo_v3.sv
//
// `floo_router.sv` ties `flush_i` to 1'b0, drives `testmode_i` from
// `test_enable_i`, and leaves `usage_o` unconnected. The flush path and the
// clock-gate control are therefore not modeled, and `o_occupancy` is a
// model-only debug output rather than a mirror of `usage_o`.
//
// Handshake note: in both RTL branches `ready_o`, `valid_o`, and `data_o` are
// functions of registers only. A full buffer never accepts a new item in the
// same cycle its head is popped.

#pragma once

#include <systemc>

#include <type_traits>

namespace floo::model {

/// Mirrors `spill_register_flushable` with `Bypass = 1'b0` and `flush_i = 1'b0`.
template <typename T>
class spill_register : public sc_core::sc_module {
public:
    static constexpr unsigned depth = 2;

    sc_core::sc_in<bool> i_clk{"i_clk"};
    sc_core::sc_in<bool> i_rst_n{"i_rst_n"};

    sc_core::sc_in<T> i_data{"i_data"};
    sc_core::sc_in<bool> i_valid{"i_valid"};
    sc_core::sc_out<bool> o_ready{"o_ready"};

    sc_core::sc_out<T> o_data{"o_data"};
    sc_core::sc_out<bool> o_valid{"o_valid"};
    sc_core::sc_in<bool> i_ready{"i_ready"};

    /// Model-only debug output; the RTL spill register has no usage output.
    sc_core::sc_out<unsigned> o_occupancy{"o_occupancy"};

    SC_HAS_PROCESS(spill_register);

    explicit spill_register(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
    {
        SC_METHOD(comb);
        sensitive << a_data_q_ << a_full_q_ << b_data_q_ << b_full_q_;

        SC_METHOD(seq);
        sensitive << i_clk.pos() << i_rst_n.neg();
        dont_initialize();
    }

private:
    sc_core::sc_signal<T> a_data_q_{"a_data_q"};
    sc_core::sc_signal<bool> a_full_q_{"a_full_q"};
    sc_core::sc_signal<T> b_data_q_{"b_data_q"};
    sc_core::sc_signal<bool> b_full_q_{"b_full_q"};

    void comb()
    {
        const bool a_full = a_full_q_.read();
        const bool b_full = b_full_q_.read();

        // assign ready_o = !a_full_q || !b_full_q;
        o_ready.write(!a_full || !b_full);
        // assign valid_o = a_full_q | b_full_q;
        o_valid.write(a_full || b_full);
        // assign data_o = b_full_q ? b_data_q : a_data_q;
        o_data.write(b_full ? b_data_q_.read() : a_data_q_.read());

        o_occupancy.write(
            static_cast<unsigned>(a_full) + static_cast<unsigned>(b_full));
    }

    void seq()
    {
        if (!i_rst_n.read()) {
            a_data_q_.write(T{});
            a_full_q_.write(false);
            b_data_q_.write(T{});
            b_full_q_.write(false);
            return;
        }

        const bool a_full = a_full_q_.read();
        const bool b_full = b_full_q_.read();
        const bool ready = !a_full || !b_full;

        // flush_i is tied low by floo_router.sv, so the flush terms drop out.
        const bool a_fill = i_valid.read() && ready;
        const bool a_drain = a_full && !b_full;
        const bool b_fill = a_drain && !i_ready.read();
        const bool b_drain = b_full && i_ready.read();

        if (a_fill) {
            a_data_q_.write(i_data.read());
        }
        if (a_fill || a_drain) {
            a_full_q_.write(a_fill);
        }
        if (b_fill) {
            b_data_q_.write(a_data_q_.read());
        }
        if (b_fill || b_drain) {
            b_full_q_.write(b_fill);
        }
    }
};

/// Mirrors `stream_fifo` with `FALL_THROUGH = 1'b0` over `fifo_v3`.
template <typename T, unsigned Depth>
class stream_fifo : public sc_core::sc_module {
public:
    static_assert(Depth > 0, "fifo_v3 asserts DEPTH > 0");

    static constexpr unsigned depth = Depth;

    sc_core::sc_in<bool> i_clk{"i_clk"};
    sc_core::sc_in<bool> i_rst_n{"i_rst_n"};

    sc_core::sc_in<T> i_data{"i_data"};
    sc_core::sc_in<bool> i_valid{"i_valid"};
    sc_core::sc_out<bool> o_ready{"o_ready"};

    sc_core::sc_out<T> o_data{"o_data"};
    sc_core::sc_out<bool> o_valid{"o_valid"};
    sc_core::sc_in<bool> i_ready{"i_ready"};

    /// Untruncated fill level. `fifo_v3` exposes `status_cnt_q` truncated to
    /// `$clog2(DEPTH)` bits on `usage_o`, which `floo_router.sv` leaves
    /// unconnected.
    sc_core::sc_out<unsigned> o_occupancy{"o_occupancy"};

    SC_HAS_PROCESS(stream_fifo);

    explicit stream_fifo(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , mem_q_("mem_q", Depth)
    {
        SC_METHOD(comb);
        sensitive << status_cnt_q_ << read_pointer_q_;
        for (const auto& entry : mem_q_) {
            sensitive << entry;
        }

        SC_METHOD(seq);
        sensitive << i_clk.pos() << i_rst_n.neg();
        dont_initialize();
    }

private:
    sc_core::sc_vector<sc_core::sc_signal<T>> mem_q_;
    sc_core::sc_signal<unsigned> status_cnt_q_{"status_cnt_q"};
    sc_core::sc_signal<unsigned> read_pointer_q_{"read_pointer_q"};
    sc_core::sc_signal<unsigned> write_pointer_q_{"write_pointer_q"};

    static unsigned increment(unsigned pointer)
    {
        return pointer == Depth - 1 ? 0u : pointer + 1;
    }

    void comb()
    {
        const unsigned count = status_cnt_q_.read();

        // assign ready_o = ~full; assign valid_o = ~empty;
        o_ready.write(count != Depth);
        o_valid.write(count != 0);
        // data_o is mem_q[read_pointer_q] regardless of the empty flag.
        o_data.write(mem_q_[read_pointer_q_.read()].read());
        o_occupancy.write(count);
    }

    void seq()
    {
        if (!i_rst_n.read()) {
            status_cnt_q_.write(0);
            read_pointer_q_.write(0);
            write_pointer_q_.write(0);
            for (auto& entry : mem_q_) {
                entry.write(T{});
            }
            return;
        }

        const unsigned count = status_cnt_q_.read();
        const bool full = count == Depth;
        const bool empty = count == 0;

        // assign push = valid_i & ~full; assign pop = ready_i & ~empty;
        const bool push = i_valid.read() && !full;
        const bool pop = i_ready.read() && !empty;

        if (push) {
            mem_q_[write_pointer_q_.read()].write(i_data.read());
            write_pointer_q_.write(increment(write_pointer_q_.read()));
        }
        if (pop) {
            read_pointer_q_.write(increment(read_pointer_q_.read()));
        }

        status_cnt_q_.write(count + static_cast<unsigned>(push)
                            - static_cast<unsigned>(pop));
    }
};

/// Mirrors `stream_fifo_optimal_wrap`: a spill register at depth 2 and a
/// `stream_fifo` above it. Depth 0 and 1 are rejected, matching the RTL
/// `$fatal`.
template <typename T, unsigned Depth>
class stream_fifo_optimal_wrap : public sc_core::sc_module {
public:
    static_assert(
        Depth >= 2,
        "stream_fifo_optimal_wrap rejects depth 0 and 1 (RTL $fatal)");

    using impl_type = std::conditional_t<
        Depth == 2, spill_register<T>, stream_fifo<T, Depth>>;

    static constexpr unsigned depth = Depth;

    sc_core::sc_in<bool> i_clk{"i_clk"};
    sc_core::sc_in<bool> i_rst_n{"i_rst_n"};

    sc_core::sc_in<T> i_data{"i_data"};
    sc_core::sc_in<bool> i_valid{"i_valid"};
    sc_core::sc_out<bool> o_ready{"o_ready"};

    sc_core::sc_out<T> o_data{"o_data"};
    sc_core::sc_out<bool> o_valid{"o_valid"};
    sc_core::sc_in<bool> i_ready{"i_ready"};

    sc_core::sc_out<unsigned> o_occupancy{"o_occupancy"};

    explicit stream_fifo_optimal_wrap(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , impl_("i_impl")
    {
        impl_.i_clk(i_clk);
        impl_.i_rst_n(i_rst_n);
        impl_.i_data(i_data);
        impl_.i_valid(i_valid);
        impl_.o_ready(o_ready);
        impl_.o_data(o_data);
        impl_.o_valid(o_valid);
        impl_.i_ready(i_ready);
        impl_.o_occupancy(o_occupancy);
    }

private:
    impl_type impl_;
};

} // namespace floo::model
