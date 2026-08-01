// SPDX-License-Identifier: SHL-0.51
//
// Complete, signal-driven AXI NoC composition for the frozen FlooNoC v0
// configuration.
//
// The physical fabric is two independent meshes over the same coordinates:
//
//   req: manager chimney -> subordinate chimney
//   rsp: subordinate chimney -> manager chimney
//
// This is not a modelling choice. The generated `floo_axi_mesh_noc.sv`
// instantiates two `floo_router` instances per node, one for each physical
// channel. `axi_mesh_noc` below preserves that raw-flit fabric as a separately
// named type for RTL cross-checks and legacy endpoint-transactor tests.
//
// `axi_noc` is the A-2 composition: every mesh node owns a complete
// `axi_chimney_node`, and users drive AXI channel signals through
// `manager(node)` and `subordinate(node)`. Since A-3, `noc_interconnect` drives
// those signals cycle by cycle. Keeping the raw and composed types separate
// still gives RTL mesh harnesses a clean flit-level boundary.
//
// The generated topology's route-port order remains North, East, South, West,
// Eject, matching `direction` in `floo_types.hpp`.

#pragma once

#include "floo_noc_model/axi_chimney.hpp"
#include "floo_noc_model/floo_mesh.hpp"

#include <systemc>

#include <array>
#include <cstdint>

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

/// Signals on the AXI manager side of one chimney.
///
/// The manager drives AW/W/AR and B/R ready. The chimney drives the reciprocal
/// ready/valid signals and the B/R payloads. Destinations are explicit because
/// address decoding belongs to the platform wrapper, not to the timed chimney.
struct axi_manager_signals {
    sc_core::sc_signal<axi_aw_chan> aw;
    sc_core::sc_signal<bool> aw_valid;
    sc_core::sc_signal<bool> aw_ready;
    sc_core::sc_signal<coordinate> aw_dest;

    sc_core::sc_signal<axi_w_chan> w;
    sc_core::sc_signal<bool> w_valid;
    sc_core::sc_signal<bool> w_ready;

    sc_core::sc_signal<axi_ar_chan> ar;
    sc_core::sc_signal<bool> ar_valid;
    sc_core::sc_signal<bool> ar_ready;
    sc_core::sc_signal<coordinate> ar_dest;

    sc_core::sc_signal<axi_b_chan> b;
    sc_core::sc_signal<bool> b_valid;
    sc_core::sc_signal<bool> b_ready;

    sc_core::sc_signal<axi_r_chan> r;
    sc_core::sc_signal<bool> r_valid;
    sc_core::sc_signal<bool> r_ready;
};

/// Signals on the AXI subordinate side of one chimney.
///
/// The chimney drives AW/W/AR and B/R ready. The subordinate drives request
/// ready plus the B/R payload and valid signals.
struct axi_subordinate_signals {
    sc_core::sc_signal<axi_aw_chan> aw;
    sc_core::sc_signal<bool> aw_valid;
    sc_core::sc_signal<bool> aw_ready;

    sc_core::sc_signal<axi_w_chan> w;
    sc_core::sc_signal<bool> w_valid;
    sc_core::sc_signal<bool> w_ready;

    sc_core::sc_signal<axi_ar_chan> ar;
    sc_core::sc_signal<bool> ar_valid;
    sc_core::sc_signal<bool> ar_ready;

    sc_core::sc_signal<axi_b_chan> b;
    sc_core::sc_signal<bool> b_valid;
    sc_core::sc_signal<bool> b_ready;

    sc_core::sc_signal<axi_r_chan> r;
    sc_core::sc_signal<bool> r_valid;
    sc_core::sc_signal<bool> r_ready;
};

/// The two physical meshes without any chimney attached.
///
/// Raw-flit users must name this type deliberately. It preserves the pre-A-2
/// `axi_noc` API for mesh cross-checks and the legacy endpoint tests; the
/// production TLM wrapper uses the chimney-backed `axi_noc` below.
template <unsigned Width, unsigned Height, unsigned InFifoDepth = 2,
          unsigned OutFifoDepth = 2>
class axi_mesh_noc : public sc_core::sc_module {
public:
    static constexpr unsigned num_nodes = Width * Height;

    sc_core::sc_in<bool> i_clk{"i_clk"};
    sc_core::sc_in<bool> i_rst_n{"i_rst_n"};

    SC_HAS_PROCESS(axi_mesh_noc);

    explicit axi_mesh_noc(sc_core::sc_module_name name)
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
        return floo_mesh<axi_req_flit, Width, Height, InFifoDepth,
                         OutFifoDepth>::node_index(x, y);
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

/// One complete FlooNoC chimney.
///
/// Manager side:
///   `axi_chimney_request` + `axi_chimney_manager_response`
///
/// Subordinate side:
///   `axi_chimney_response`
///
/// The internal B/R pop and RoB-ready links are part of this composition. They
/// are deliberately not exposed for an external testbench to drive.
template <unsigned AxiIdBits = 3, unsigned OutIdWidth = 3,
          unsigned MaxTxns = 32, unsigned MaxTxnsPerId = 32>
class axi_chimney_node : public sc_core::sc_module {
public:
    static constexpr std::uint64_t downstream_id =
        axi_chimney_response<OutIdWidth, MaxTxns>::downstream_id;

    sc_core::sc_in<bool> i_clk{"i_clk"};
    sc_core::sc_in<bool> i_rst_n{"i_rst_n"};

    // Request mesh: inject from manager side, eject into subordinate side.
    sc_core::sc_out<axi_req_flit> o_req_inject_data{"o_req_inject_data"};
    sc_core::sc_out<bool> o_req_inject_valid{"o_req_inject_valid"};
    sc_core::sc_in<bool> i_req_inject_ready{"i_req_inject_ready"};
    sc_core::sc_in<axi_req_flit> i_req_eject_data{"i_req_eject_data"};
    sc_core::sc_in<bool> i_req_eject_valid{"i_req_eject_valid"};
    sc_core::sc_out<bool> o_req_eject_ready{"o_req_eject_ready"};

    // Response mesh: inject from subordinate side, eject into manager side.
    sc_core::sc_out<axi_rsp_flit> o_rsp_inject_data{"o_rsp_inject_data"};
    sc_core::sc_out<bool> o_rsp_inject_valid{"o_rsp_inject_valid"};
    sc_core::sc_in<bool> i_rsp_inject_ready{"i_rsp_inject_ready"};
    sc_core::sc_in<axi_rsp_flit> i_rsp_eject_data{"i_rsp_eject_data"};
    sc_core::sc_in<bool> i_rsp_eject_valid{"i_rsp_eject_valid"};
    sc_core::sc_out<bool> o_rsp_eject_ready{"o_rsp_eject_ready"};

    SC_HAS_PROCESS(axi_chimney_node);

    explicit axi_chimney_node(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
    {
        request_.i_clk(i_clk);
        request_.i_rst_n(i_rst_n);
        request_.i_node_id(node_id_);
        request_.i_aw_valid(manager_.aw_valid);
        request_.o_aw_ready(manager_.aw_ready);
        request_.i_aw(manager_.aw);
        request_.i_aw_dest(manager_.aw_dest);
        request_.i_w_valid(manager_.w_valid);
        request_.o_w_ready(manager_.w_ready);
        request_.i_w(manager_.w);
        request_.i_ar_valid(manager_.ar_valid);
        request_.o_ar_ready(manager_.ar_ready);
        request_.i_ar(manager_.ar);
        request_.i_ar_dest(manager_.ar_dest);
        request_.o_req_data(o_req_inject_data);
        request_.o_req_valid(o_req_inject_valid);
        request_.i_req_ready(i_req_inject_ready);
        request_.i_b_pop(b_pop_);
        request_.i_b_pop_id(b_pop_id_);
        request_.i_r_pop(r_pop_);
        request_.i_r_pop_id(r_pop_id_);
        request_.i_r_pop_last(r_pop_last_);
        request_.o_b_rsp_ready(b_rob_ready_);
        request_.o_r_rsp_ready(r_rob_ready_);
        request_.i_b_rsp_ready(manager_.b_ready);
        request_.i_r_rsp_ready(manager_.r_ready);

        response_.i_clk(i_clk);
        response_.i_rst_n(i_rst_n);
        response_.i_node_id(node_id_);
        response_.i_req_data(i_req_eject_data);
        response_.i_req_valid(i_req_eject_valid);
        response_.o_req_ready(o_req_eject_ready);
        response_.o_axi_aw(subordinate_.aw);
        response_.o_axi_aw_valid(subordinate_.aw_valid);
        response_.i_axi_aw_ready(subordinate_.aw_ready);
        response_.o_axi_w(subordinate_.w);
        response_.o_axi_w_valid(subordinate_.w_valid);
        response_.i_axi_w_ready(subordinate_.w_ready);
        response_.o_axi_ar(subordinate_.ar);
        response_.o_axi_ar_valid(subordinate_.ar_valid);
        response_.i_axi_ar_ready(subordinate_.ar_ready);
        response_.i_axi_b(subordinate_.b);
        response_.i_axi_b_valid(subordinate_.b_valid);
        response_.o_axi_b_ready(subordinate_.b_ready);
        response_.i_axi_r(subordinate_.r);
        response_.i_axi_r_valid(subordinate_.r_valid);
        response_.o_axi_r_ready(subordinate_.r_ready);
        response_.o_rsp_data(o_rsp_inject_data);
        response_.o_rsp_valid(o_rsp_inject_valid);
        response_.i_rsp_ready(i_rsp_inject_ready);

        manager_response_.i_rsp_data(i_rsp_eject_data);
        manager_response_.i_rsp_valid(i_rsp_eject_valid);
        manager_response_.o_rsp_ready(o_rsp_eject_ready);
        manager_response_.i_b_rob_ready(b_rob_ready_);
        manager_response_.i_r_rob_ready(r_rob_ready_);
        manager_response_.o_b_pop(b_pop_);
        manager_response_.o_b_pop_id(b_pop_id_);
        manager_response_.o_r_pop(r_pop_);
        manager_response_.o_r_pop_id(r_pop_id_);
        manager_response_.o_r_pop_last(r_pop_last_);
        manager_response_.o_axi_b(manager_.b);
        manager_response_.o_axi_b_valid(manager_.b_valid);
        manager_response_.o_axi_r(manager_.r);
        manager_response_.o_axi_r_valid(manager_.r_valid);
    }

    void set_node_id(const coordinate& id) { node_id_.write(id); }
    coordinate node_id() const { return node_id_.read(); }

    axi_manager_signals& manager() { return manager_; }
    axi_subordinate_signals& subordinate() { return subordinate_; }

    unsigned b_outstanding(unsigned axi_id) const
    {
        return request_.b_outstanding(axi_id);
    }
    unsigned r_outstanding(unsigned axi_id) const
    {
        return request_.r_outstanding(axi_id);
    }
    unsigned aw_meta_occupancy() const
    {
        return response_.aw_meta_occupancy();
    }
    unsigned ar_meta_occupancy() const
    {
        return response_.ar_meta_occupancy();
    }

private:
    axi_manager_signals manager_{};
    axi_subordinate_signals subordinate_{};
    sc_core::sc_signal<coordinate> node_id_{"node_id"};

    sc_core::sc_signal<bool> b_pop_{"b_pop"};
    sc_core::sc_signal<unsigned> b_pop_id_{"b_pop_id"};
    sc_core::sc_signal<bool> r_pop_{"r_pop"};
    sc_core::sc_signal<unsigned> r_pop_id_{"r_pop_id"};
    sc_core::sc_signal<bool> r_pop_last_{"r_pop_last"};
    sc_core::sc_signal<bool> b_rob_ready_{"b_rob_ready"};
    sc_core::sc_signal<bool> r_rob_ready_{"r_rob_ready"};

    axi_chimney_request<AxiIdBits, MaxTxnsPerId> request_{"request"};
    axi_chimney_response<OutIdWidth, MaxTxns> response_{"response"};
    axi_chimney_manager_response manager_response_{"manager_response"};
};

/// Complete signal-driven AXI NoC: one chimney per node over the req/rsp mesh.
///
/// Defaults are the frozen v0 configuration:
/// `AxiIdBits=3`, `OutIdWidth=3`, `MaxTxns=32`, `MaxTxnsPerId=32`,
/// `InFifoDepth=2`, `OutFifoDepth=2`.
template <unsigned Width, unsigned Height, unsigned InFifoDepth = 2,
          unsigned OutFifoDepth = 2, unsigned AxiIdBits = 3,
          unsigned OutIdWidth = 3, unsigned MaxTxns = 32,
          unsigned MaxTxnsPerId = 32>
class axi_noc : public sc_core::sc_module {
public:
    static constexpr unsigned num_nodes = Width * Height;
    using chimney_type =
        axi_chimney_node<AxiIdBits, OutIdWidth, MaxTxns, MaxTxnsPerId>;

    sc_core::sc_in<bool> i_clk{"i_clk"};
    sc_core::sc_in<bool> i_rst_n{"i_rst_n"};

    SC_HAS_PROCESS(axi_noc);

    explicit axi_noc(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , mesh_("mesh")
        , chimneys_("chimneys", num_nodes)
    {
        mesh_.i_clk(i_clk);
        mesh_.i_rst_n(i_rst_n);

        for (unsigned y = 0; y < Height; ++y) {
            for (unsigned x = 0; x < Width; ++x) {
                const unsigned node = node_index(x, y);
                auto& chimney = chimneys_[node];
                auto& req = mesh_.req(node);
                auto& rsp = mesh_.rsp(node);

                chimney.i_clk(i_clk);
                chimney.i_rst_n(i_rst_n);
                chimney.set_node_id(coordinate{x, y});

                chimney.o_req_inject_data(req.inject_data);
                chimney.o_req_inject_valid(req.inject_valid);
                chimney.i_req_inject_ready(req.inject_ready);
                chimney.i_req_eject_data(req.eject_data);
                chimney.i_req_eject_valid(req.eject_valid);
                chimney.o_req_eject_ready(req.eject_ready);

                chimney.o_rsp_inject_data(rsp.inject_data);
                chimney.o_rsp_inject_valid(rsp.inject_valid);
                chimney.i_rsp_inject_ready(rsp.inject_ready);
                chimney.i_rsp_eject_data(rsp.eject_data);
                chimney.i_rsp_eject_valid(rsp.eject_valid);
                chimney.o_rsp_eject_ready(rsp.eject_ready);
            }
        }
    }

    static constexpr unsigned node_index(unsigned x, unsigned y)
    {
        return axi_mesh_noc<Width, Height, InFifoDepth,
                            OutFifoDepth>::node_index(x, y);
    }

    static constexpr unsigned node_index(const coordinate& id)
    {
        return node_index(id.x.to_uint(), id.y.to_uint());
    }

    axi_manager_signals& manager(unsigned node)
    {
        return chimneys_[node].manager();
    }
    axi_subordinate_signals& subordinate(unsigned node)
    {
        return chimneys_[node].subordinate();
    }
    chimney_type& chimney(unsigned node) { return chimneys_[node]; }
    const chimney_type& chimney(unsigned node) const { return chimneys_[node]; }

private:
    axi_mesh_noc<Width, Height, InFifoDepth, OutFifoDepth> mesh_;
    sc_core::sc_vector<chimney_type> chimneys_;
};

} // namespace floo::model
