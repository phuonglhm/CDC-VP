// SPDX-License-Identifier: SHL-0.51
//
// Two-network single-AXI NoC: the `req` and `rsp` physical channels as
// separate rectangular meshes over the same coordinates.
//
// This shape is not a modelling choice, it is what the IP does. The
// FlooGen-generated `floo_axi_mesh_noc.sv` at the frozen revision instantiates
// `floo_axi_router`, and that module is literally two `floo_router` instances
// with identical parameters:
//
//   i_req_floo_router   flit_t = floo_req_generic_flit_t
//   i_rsp_floo_router   flit_t = floo_rsp_generic_flit_t
//
// carrying two independent `[NumRoutes-1:0]` port arrays. There is no shared
// arbitration, no shared buffering, and no ordering between the two networks.
// So the model is two `floo_mesh` instantiations, not one mesh carrying a
// tagged union.
//
// The generated topology also confirms the port index order this model already
// uses. For `router_0_1` at (1,1) FlooGen emits
//
//   req_in[0] <- router_0_2   (y+1, North)
//   req_in[1] <- router_1_1   (x+1, East)
//   req_in[2] <- router_0_0   (y-1, South)
//   req_in[3] <- hbm_ni_1     (x-1 edge, the West slot)
//   req_in[4] <- cluster_ni   (Eject)
//
// which matches `floo_pkg::route_direction_e` (North 0, East 1, South 2,
// West 3, Eject 4) and `direction` in `floo_types.hpp`.
//
// ## Verification status
//
// The routers inside both meshes are the RTL-signed `floo_router` model (214
// cycles). The link topology is checked against the FlooGen-generated netlist
// above, by reading it, not by a cycle comparison: a mesh-level cross-check
// needs the whole generated top elaborated against the model, which is a
// separate harness that does not exist yet. Treat inter-node **timing** as an
// estimate; what this header establishes is that the two networks are wired
// the way the IP wires them.

#pragma once

#include "floo_noc_model/axi_chimney_pack.hpp"
#include "floo_noc_model/floo_mesh.hpp"

#include <systemc>

#include <array>

namespace floo::model {

/// One physical channel's endpoint-facing signals, for one node.
template <typename FlitT>
struct network_port {
    sc_core::sc_signal<FlitT> inject_data;
    sc_core::sc_signal<bool> inject_valid;
    sc_core::sc_signal<bool> inject_ready;
    sc_core::sc_signal<FlitT> eject_data;
    sc_core::sc_signal<bool> eject_valid;
    sc_core::sc_signal<bool> eject_ready;
};

/// The `req` and `rsp` meshes side by side.
///
/// Endpoints attach through `req(node)` and `rsp(node)`: a manager injects on
/// `req` and ejects on `rsp`, a subordinate the other way round.
template <unsigned Width, unsigned Height, unsigned InFifoDepth = 2,
          unsigned OutFifoDepth = 2>
class axi_noc : public sc_core::sc_module {
public:
    static constexpr unsigned num_nodes = Width * Height;

    sc_core::sc_in<bool> i_clk{"i_clk"};
    sc_core::sc_in<bool> i_rst_n{"i_rst_n"};

    SC_HAS_PROCESS(axi_noc);

    explicit axi_noc(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , req_mesh_("req_mesh")
        , rsp_mesh_("rsp_mesh")
    {
        req_mesh_.i_clk(i_clk);
        req_mesh_.i_rst_n(i_rst_n);
        rsp_mesh_.i_clk(i_clk);
        rsp_mesh_.i_rst_n(i_rst_n);

        for (unsigned node = 0; node < num_nodes; ++node) {
            req_mesh_.i_inject_data[node](req_[node].inject_data);
            req_mesh_.i_inject_valid[node](req_[node].inject_valid);
            req_mesh_.o_inject_ready[node](req_[node].inject_ready);
            req_mesh_.o_eject_data[node](req_[node].eject_data);
            req_mesh_.o_eject_valid[node](req_[node].eject_valid);
            req_mesh_.i_eject_ready[node](req_[node].eject_ready);

            rsp_mesh_.i_inject_data[node](rsp_[node].inject_data);
            rsp_mesh_.i_inject_valid[node](rsp_[node].inject_valid);
            rsp_mesh_.o_inject_ready[node](rsp_[node].inject_ready);
            rsp_mesh_.o_eject_data[node](rsp_[node].eject_data);
            rsp_mesh_.o_eject_valid[node](rsp_[node].eject_valid);
            rsp_mesh_.i_eject_ready[node](rsp_[node].eject_ready);
        }
    }

    static constexpr unsigned node_index(unsigned x, unsigned y)
    {
        return floo_mesh<axi_req_flit, Width, Height, InFifoDepth, OutFifoDepth>::node_index(
            x, y);
    }

    static constexpr unsigned node_index(const coordinate& id)
    {
        return node_index(id.x.to_uint(), id.y.to_uint());
    }

    network_port<axi_req_flit>& req(unsigned node) { return req_[node]; }
    network_port<axi_rsp_flit>& rsp(unsigned node) { return rsp_[node]; }

private:
    floo_mesh<axi_req_flit, Width, Height, InFifoDepth, OutFifoDepth> req_mesh_;
    floo_mesh<axi_rsp_flit, Width, Height, InFifoDepth, OutFifoDepth> rsp_mesh_;

    std::array<network_port<axi_req_flit>, num_nodes> req_{};
    std::array<network_port<axi_rsp_flit>, num_nodes> rsp_{};
};

} // namespace floo::model
