// SPDX-License-Identifier: SHL-0.51

#pragma once

#include "floo_noc_model/floo_router.hpp"
#include "floo_noc_model/floo_types.hpp"

#include <systemc>

#include <cstdint>

namespace floo::model {

/// Aggregate, passive view of every state that can retain a flit in one mesh.
///
/// Counts rather than a single Boolean make a failed clock-gating assertion
/// diagnosable: they say whether the residue is a FIFO entry, a wormhole lock,
/// or a live endpoint boundary.
struct mesh_activity {
    std::uint64_t input_fifo_entries{};
    std::uint64_t output_fifo_entries{};
    unsigned route_locks{};
    unsigned arbiter_locks{};
    unsigned inject_valids{};
    unsigned eject_valids{};

    bool quiescent() const
    {
        return input_fifo_entries == 0 && output_fifo_entries == 0
            && route_locks == 0 && arbiter_locks == 0
            && inject_valids == 0 && eject_valids == 0;
    }
};

template <
    typename FlitT,
    unsigned Width,
    unsigned Height,
    unsigned InFifoDepth = 2,
    unsigned OutFifoDepth = 2>
class floo_mesh : public sc_core::sc_module {
public:
    static_assert(Width > 0 && Height > 0,
                  "FlooNoC mesh dimensions must be non-zero");

    static constexpr unsigned num_nodes = Width * Height;
    static constexpr unsigned num_ports = floo_router<FlitT, InFifoDepth, OutFifoDepth>::num_ports;

    sc_core::sc_in<bool> i_clk{"i_clk"};
    sc_core::sc_in<bool> i_rst_n{"i_rst_n"};

    sc_core::sc_vector<sc_core::sc_in<FlitT>> i_inject_data{
        "i_inject_data", num_nodes};
    sc_core::sc_vector<sc_core::sc_in<bool>> i_inject_valid{
        "i_inject_valid", num_nodes};
    sc_core::sc_vector<sc_core::sc_out<bool>> o_inject_ready{
        "o_inject_ready", num_nodes};

    sc_core::sc_vector<sc_core::sc_out<FlitT>> o_eject_data{
        "o_eject_data", num_nodes};
    sc_core::sc_vector<sc_core::sc_out<bool>> o_eject_valid{
        "o_eject_valid", num_nodes};
    sc_core::sc_vector<sc_core::sc_in<bool>> i_eject_ready{
        "i_eject_ready", num_nodes};

    SC_HAS_PROCESS(floo_mesh);

    explicit floo_mesh(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , routers_("routers", num_nodes)
        , router_ids_("router_ids", num_nodes)
        , router_in_data_("router_in_data", num_nodes * num_ports)
        , router_in_valid_("router_in_valid", num_nodes * num_ports)
        , router_in_ready_("router_in_ready", num_nodes * num_ports)
        , router_out_data_("router_out_data", num_nodes * num_ports)
        , router_out_valid_("router_out_valid", num_nodes * num_ports)
        , router_out_ready_("router_out_ready", num_nodes * num_ports)
        , router_occupancy_("router_occupancy", num_nodes * num_ports)
        , router_selected_("router_selected", num_nodes * num_ports)
        , router_locked_("router_locked", num_nodes * num_ports)
    {
        for (unsigned y = 0; y < Height; ++y) {
            for (unsigned x = 0; x < Width; ++x) {
                const unsigned node = node_index(x, y);
                router_ids_[node].write(coordinate{x, y});

                auto& router = routers_[node];
                router.i_clk(i_clk);
                router.i_rst_n(i_rst_n);
                router.i_router_id(router_ids_[node]);

                for (unsigned port = 0; port < num_ports; ++port) {
                    const unsigned index = signal_index(node, port);
                    router.i_data[port](router_in_data_[index]);
                    router.i_valid[port](router_in_valid_[index]);
                    router.o_ready[port](router_in_ready_[index]);
                    router.o_data[port](router_out_data_[index]);
                    router.o_valid[port](router_out_valid_[index]);
                    router.i_ready[port](router_out_ready_[index]);
                    router.o_input_occupancy[port](router_occupancy_[index]);
                    router.o_output_selected[port](router_selected_[index]);
                    router.o_output_locked[port](router_locked_[index]);
                }
            }
        }

        SC_METHOD(connect_links);
        for (unsigned node = 0; node < num_nodes; ++node) {
            sensitive << i_inject_data[node]
                      << i_inject_valid[node]
                      << i_eject_ready[node];
        }
        for (unsigned index = 0; index < num_nodes * num_ports; ++index) {
            sensitive << router_in_ready_[index]
                      << router_out_data_[index]
                      << router_out_valid_[index];
        }
    }

    static constexpr unsigned node_index(unsigned x, unsigned y)
    {
        return y * Width + x;
    }

    mesh_activity activity() const
    {
        mesh_activity result{};
        for (unsigned node = 0; node < num_nodes; ++node) {
            if (i_inject_valid[node].read()) {
                ++result.inject_valids;
            }
            if (o_eject_valid[node].read()) {
                ++result.eject_valids;
            }
            for (unsigned port = 0; port < num_ports; ++port) {
                const auto& router = routers_[node];
                result.input_fifo_entries +=
                    router.input_fifo_occupancy(port);
                result.output_fifo_entries +=
                    router.output_fifo_occupancy(port);
                if (router.route_locked(port)) {
                    ++result.route_locks;
                }
                if (router.arbiter_locked(port)) {
                    ++result.arbiter_locks;
                }
            }
        }
        return result;
    }

    bool quiescent() const { return activity().quiescent(); }

private:
    sc_core::sc_vector<floo_router<FlitT, InFifoDepth, OutFifoDepth>> routers_;
    sc_core::sc_vector<sc_core::sc_signal<coordinate>> router_ids_;

    sc_core::sc_vector<sc_core::sc_signal<FlitT>> router_in_data_;
    sc_core::sc_vector<sc_core::sc_signal<bool>> router_in_valid_;
    sc_core::sc_vector<sc_core::sc_signal<bool>> router_in_ready_;
    sc_core::sc_vector<sc_core::sc_signal<FlitT>> router_out_data_;
    sc_core::sc_vector<sc_core::sc_signal<bool>> router_out_valid_;
    sc_core::sc_vector<sc_core::sc_signal<bool>> router_out_ready_;

    sc_core::sc_vector<sc_core::sc_signal<unsigned>> router_occupancy_;
    sc_core::sc_vector<sc_core::sc_signal<unsigned>> router_selected_;
    sc_core::sc_vector<sc_core::sc_signal<bool>> router_locked_;

    static constexpr unsigned signal_index(unsigned node, unsigned port)
    {
        return node * num_ports + port;
    }

    void connect_neighbor(
        unsigned destination_node,
        direction destination_input,
        unsigned source_node,
        direction source_output)
    {
        const unsigned destination =
            signal_index(destination_node, to_port(destination_input));
        const unsigned source =
            signal_index(source_node, to_port(source_output));

        router_in_data_[destination].write(router_out_data_[source].read());
        router_in_valid_[destination].write(router_out_valid_[source].read());
        router_out_ready_[source].write(router_in_ready_[destination].read());
    }

    void connect_links()
    {
        for (unsigned node = 0; node < num_nodes; ++node) {
            for (unsigned port = 0; port < num_ports; ++port) {
                router_in_data_[signal_index(node, port)].write(FlitT{});
                router_in_valid_[signal_index(node, port)].write(false);
                router_out_ready_[signal_index(node, port)].write(false);
            }
        }

        for (unsigned y = 0; y < Height; ++y) {
            for (unsigned x = 0; x < Width; ++x) {
                const unsigned node = node_index(x, y);

                // Local endpoint connects to the Eject router port in both
                // directions: endpoint injection enters the Eject input;
                // destination traffic leaves the Eject output.
                const unsigned local =
                    signal_index(node, to_port(direction::eject));
                router_in_data_[local].write(i_inject_data[node].read());
                router_in_valid_[local].write(i_inject_valid[node].read());
                o_inject_ready[node].write(router_in_ready_[local].read());

                o_eject_data[node].write(router_out_data_[local].read());
                o_eject_valid[node].write(router_out_valid_[local].read());
                router_out_ready_[local].write(i_eject_ready[node].read());

                if (x > 0) {
                    connect_neighbor(
                        node, direction::west,
                        node_index(x - 1, y), direction::east);
                }
                if (x + 1 < Width) {
                    connect_neighbor(
                        node, direction::east,
                        node_index(x + 1, y), direction::west);
                }
                if (y > 0) {
                    connect_neighbor(
                        node, direction::south,
                        node_index(x, y - 1), direction::north);
                }
                if (y + 1 < Height) {
                    connect_neighbor(
                        node, direction::north,
                        node_index(x, y + 1), direction::south);
                }
            }
        }
    }
};

} // namespace floo::model
