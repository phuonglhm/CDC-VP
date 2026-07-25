// SPDX-License-Identifier: SHL-0.51

#pragma once

#include <systemc>

namespace floo::model {

template <typename T, unsigned Depth>
class ready_valid_fifo : public sc_core::sc_module {
public:
    static_assert(Depth > 0, "vertical slice FIFO depth must be greater than zero");

    sc_core::sc_in<bool> i_clk{"i_clk"};
    sc_core::sc_in<bool> i_rst_n{"i_rst_n"};

    sc_core::sc_in<T> i_data{"i_data"};
    sc_core::sc_in<bool> i_valid{"i_valid"};
    sc_core::sc_out<bool> o_ready{"o_ready"};

    sc_core::sc_out<T> o_data{"o_data"};
    sc_core::sc_out<bool> o_valid{"o_valid"};
    sc_core::sc_in<bool> i_ready{"i_ready"};

    sc_core::sc_out<unsigned> o_occupancy{"o_occupancy"};

    SC_HAS_PROCESS(ready_valid_fifo);

    explicit ready_valid_fifo(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , storage_("storage", Depth)
    {
        SC_METHOD(comb);
        sensitive << i_valid << i_ready << i_data;
        sensitive << count_q_ << read_index_q_ << write_index_q_;
        for (const auto& entry : storage_) {
            sensitive << entry;
        }

        SC_METHOD(seq);
        sensitive << i_clk.pos() << i_rst_n.neg();
        dont_initialize();
    }

private:
    sc_core::sc_vector<sc_core::sc_signal<T>> storage_;
    sc_core::sc_signal<unsigned> count_q_{"count_q"};
    sc_core::sc_signal<unsigned> read_index_q_{"read_index_q"};
    sc_core::sc_signal<unsigned> write_index_q_{"write_index_q"};

    static unsigned increment(unsigned index)
    {
        return index + 1 == Depth ? 0 : index + 1;
    }

    void comb()
    {
        const unsigned count = count_q_.read();
        const bool output_valid = count != 0;
        const bool pop = output_valid && i_ready.read();

        // Match an "optimal" ready/valid FIFO: a full FIFO can accept a new
        // item in the same cycle that its current head is consumed.
        o_ready.write(count < Depth || pop);
        o_valid.write(output_valid);
        o_occupancy.write(count);
        o_data.write(
            output_valid ? storage_[read_index_q_.read()].read() : T{});
    }

    void seq()
    {
        if (!i_rst_n.read()) {
            count_q_.write(0);
            read_index_q_.write(0);
            write_index_q_.write(0);
            return;
        }

        const unsigned count = count_q_.read();
        const bool pop = count != 0 && i_ready.read();
        const bool push = i_valid.read() && (count < Depth || pop);

        if (push) {
            storage_[write_index_q_.read()].write(i_data.read());
            write_index_q_.write(increment(write_index_q_.read()));
        }
        if (pop) {
            read_index_q_.write(increment(read_index_q_.read()));
        }

        count_q_.write(count + static_cast<unsigned>(push)
                       - static_cast<unsigned>(pop));
    }
};

} // namespace floo::model
