// SPDX-License-Identifier: Apache-2.0

#include "floo_noc_model/floo_mesh.hpp"
#include "floo_noc_model/floo_types.hpp"

#include <systemc>

#include <cstdint>
#include <iostream>
#include <string>

namespace {

using flit_t = floo::model::test_flit;
constexpr unsigned width = 2;
constexpr unsigned height = 2;
constexpr unsigned num_nodes = width * height;

flit_t make_flit(std::uint64_t payload)
{
    flit_t flit;
    flit.payload = payload;
    flit.hdr.src_id = floo::model::coordinate{0, 0};
    flit.hdr.dst_id = floo::model::coordinate{1, 1};
    flit.hdr.last = true;
    return flit;
}

class mesh_testbench : public sc_core::sc_module {
public:
    sc_core::sc_in<bool> i_clk{"i_clk"};
    sc_core::sc_out<bool> o_rst_n{"o_rst_n"};
    sc_core::sc_vector<sc_core::sc_out<flit_t>> o_inject_data{
        "o_inject_data", num_nodes};
    sc_core::sc_vector<sc_core::sc_out<bool>> o_inject_valid{
        "o_inject_valid", num_nodes};
    sc_core::sc_vector<sc_core::sc_in<bool>> i_inject_ready{
        "i_inject_ready", num_nodes};
    sc_core::sc_vector<sc_core::sc_in<flit_t>> i_eject_data{
        "i_eject_data", num_nodes};
    sc_core::sc_vector<sc_core::sc_in<bool>> i_eject_valid{
        "i_eject_valid", num_nodes};
    sc_core::sc_vector<sc_core::sc_out<bool>> o_eject_ready{
        "o_eject_ready", num_nodes};

    SC_HAS_PROCESS(mesh_testbench);

    explicit mesh_testbench(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
    {
        SC_THREAD(run);
    }

private:
    void expect(bool condition, const std::string& message)
    {
        if (!condition) {
            SC_REPORT_ERROR("test_floo_mesh", message.c_str());
        }
    }

    void settle()
    {
        // Multi-hop ready paths need several delta cycles to settle.
        for (unsigned delta = 0; delta < 24; ++delta) {
            wait(sc_core::SC_ZERO_TIME);
        }
    }

    void run()
    {
        constexpr unsigned source =
            floo::model::floo_mesh<flit_t, width, height>::node_index(0, 0);
        constexpr unsigned destination =
            floo::model::floo_mesh<flit_t, width, height>::node_index(1, 1);

        o_rst_n.write(false);
        for (unsigned node = 0; node < num_nodes; ++node) {
            o_inject_data[node].write(flit_t{});
            o_inject_valid[node].write(false);
            o_eject_ready[node].write(node != destination);
        }

        wait(i_clk.posedge_event());
        wait(i_clk.posedge_event());
        o_rst_n.write(true);
        wait(i_clk.negedge_event());

        // Inject while destination is blocked. Intermediate router FIFOs must
        // retain the flit and propagate back-pressure without changing data.
        o_inject_data[source].write(make_flit(0x1234));
        o_inject_valid[source].write(true);
        settle();
        expect(i_inject_ready[source].read(),
               "source endpoint could not inject into an empty mesh");

        wait(i_clk.posedge_event());
        settle();
        o_inject_valid[source].write(false);

        bool reached_destination = false;
        for (unsigned cycle = 0; cycle < 12; ++cycle) {
            wait(i_clk.posedge_event());
            settle();
            for (unsigned node = 0; node < num_nodes; ++node) {
                if (node != destination) {
                    expect(!i_eject_valid[node].read(),
                           "flit ejected at the wrong endpoint");
                }
            }
            if (i_eject_valid[destination].read()) {
                reached_destination = true;
                break;
            }
        }

        expect(reached_destination,
               "flit did not reach destination within the hop timeout");
        expect(i_eject_data[destination].read().payload.to_uint64() == 0x1234,
               "destination payload was corrupted");

        const flit_t stalled_value = i_eject_data[destination].read();
        wait(i_clk.posedge_event());
        settle();
        expect(i_eject_valid[destination].read(),
               "destination valid dropped under back-pressure");
        expect(i_eject_data[destination].read() == stalled_value,
               "destination data changed under back-pressure");

        o_eject_ready[destination].write(true);
        settle();
        wait(i_clk.posedge_event());
        settle();
        expect(!i_eject_valid[destination].read(),
               "destination did not consume the accepted flit");

        o_eject_ready[destination].write(false);
        sc_core::sc_stop();
    }
};

} // namespace

int sc_main(int, char**)
{
    sc_core::sc_clock clk{"clk", sc_core::sc_time(10, sc_core::SC_NS)};
    sc_core::sc_signal<bool> rst_n{"rst_n"};
    sc_core::sc_vector<sc_core::sc_signal<flit_t>> inject_data{
        "inject_data", num_nodes};
    sc_core::sc_vector<sc_core::sc_signal<bool>> inject_valid{
        "inject_valid", num_nodes};
    sc_core::sc_vector<sc_core::sc_signal<bool>> inject_ready{
        "inject_ready", num_nodes};
    sc_core::sc_vector<sc_core::sc_signal<flit_t>> eject_data{
        "eject_data", num_nodes};
    sc_core::sc_vector<sc_core::sc_signal<bool>> eject_valid{
        "eject_valid", num_nodes};
    sc_core::sc_vector<sc_core::sc_signal<bool>> eject_ready{
        "eject_ready", num_nodes};

    floo::model::floo_mesh<flit_t, width, height, 2> dut{"dut"};
    dut.i_clk(clk);
    dut.i_rst_n(rst_n);

    mesh_testbench tb{"tb"};
    tb.i_clk(clk);
    tb.o_rst_n(rst_n);

    for (unsigned node = 0; node < num_nodes; ++node) {
        dut.i_inject_data[node](inject_data[node]);
        dut.i_inject_valid[node](inject_valid[node]);
        dut.o_inject_ready[node](inject_ready[node]);
        dut.o_eject_data[node](eject_data[node]);
        dut.o_eject_valid[node](eject_valid[node]);
        dut.i_eject_ready[node](eject_ready[node]);

        tb.o_inject_data[node](inject_data[node]);
        tb.o_inject_valid[node](inject_valid[node]);
        tb.i_inject_ready[node](inject_ready[node]);
        tb.i_eject_data[node](eject_data[node]);
        tb.i_eject_valid[node](eject_valid[node]);
        tb.o_eject_ready[node](eject_ready[node]);
    }

    sc_core::sc_start();

    const int errors =
        sc_core::sc_report_handler::get_count(sc_core::SC_ERROR);
    if (errors == 0) {
        std::cout << "PASS: 2x2 FlooNoC mesh multi-hop delivery\n";
    }
    return errors == 0 ? 0 : 1;
}
