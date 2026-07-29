// SPDX-License-Identifier: SHL-0.51
//
// Single-AXI channel types and the flit sizing arithmetic that FlooNoC derives
// from an AXI configuration.
//
// Sources of truth, all at the frozen revisions:
//
//   FlooNoC  hw/floo_pkg.sv                  axi_cfg_t, axi_ch_e,
//                                            axi_chan_mapping,
//                                            get_axi_chan_width,
//                                            get_max_axi_payload_bits,
//                                            get_axi_rsvd_bits
//   FlooNoC  hw/include/floo_noc/typedef.svh FLOO_TYPEDEF_AXI_FROM_CFG,
//                                            FLOO_TYPEDEF_AXI_CHAN_ALL
//   axi 0.39.9  src/axi_pkg.sv               aw_width, w_width, b_width,
//                                            ar_width, r_width and the field
//                                            width constants
//
// The sizing functions are cross-checked against the RTL functions by
// `rtl_crosscheck/run_axi_sizing_crosscheck.sh`, which evaluates both sides
// over the same configuration list. Two details there are easy to get wrong
// and are the reason that cross-check exists:
//
//   * `get_max_axi_payload_bits` adds one spare bit, so a physical channel is
//     always at least one bit wider than its widest AXI payload;
//   * the AXI channel widths use `cfg.InIdWidth`, never `OutIdWidth`.

#pragma once

#include "floo_noc_model/floo_types.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace floo::model {

/// Field widths from `axi_pkg`. Values verified against
/// `axi/src/axi_pkg.sv` at the locked revision `a256a3b`.
namespace axi_pkg {

inline constexpr unsigned burst_width = 2;
inline constexpr unsigned resp_width = 2;
inline constexpr unsigned cache_width = 4;
inline constexpr unsigned prot_width = 3;
inline constexpr unsigned qos_width = 4;
inline constexpr unsigned region_width = 4;
inline constexpr unsigned len_width = 8;
inline constexpr unsigned size_width = 3;
inline constexpr unsigned lock_width = 1;
inline constexpr unsigned atop_width = 6;

constexpr unsigned aw_width(
    unsigned addr_width, unsigned id_width, unsigned user_width)
{
    return id_width + addr_width + len_width + size_width + burst_width
        + lock_width + cache_width + prot_width + qos_width + region_width
        + atop_width + user_width;
}

constexpr unsigned w_width(unsigned data_width, unsigned user_width)
{
    // data + strobe + last + user
    return data_width + data_width / 8 + 1 + user_width;
}

constexpr unsigned b_width(unsigned id_width, unsigned user_width)
{
    return id_width + resp_width + user_width;
}

constexpr unsigned ar_width(
    unsigned addr_width, unsigned id_width, unsigned user_width)
{
    return id_width + addr_width + len_width + size_width + burst_width
        + lock_width + cache_width + prot_width + qos_width + region_width
        + user_width;
}

constexpr unsigned r_width(
    unsigned data_width, unsigned id_width, unsigned user_width)
{
    return id_width + data_width + resp_width + 1 + user_width;
}

} // namespace axi_pkg

/// Mirrors `floo_pkg::axi_cfg_t`.
struct axi_cfg {
    unsigned addr_width{};
    unsigned data_width{};
    unsigned user_width{};
    unsigned in_id_width{};
    unsigned out_id_width{};
};

/// Mirrors `floo_pkg::axi_chan_mapping`: AW, W, and AR ride the `req` physical
/// channel; B and R ride `rsp`.
constexpr physical_channel axi_chan_mapping(axi_channel channel)
{
    return channel == axi_channel::aw || channel == axi_channel::w
            || channel == axi_channel::ar
        ? physical_channel::req
        : physical_channel::rsp;
}

/// Mirrors `floo_pkg::get_axi_chan_width`. Note the use of `InIdWidth`.
constexpr unsigned get_axi_chan_width(const axi_cfg& cfg, axi_channel channel)
{
    switch (channel) {
    case axi_channel::aw:
        return axi_pkg::aw_width(
            cfg.addr_width, cfg.in_id_width, cfg.user_width);
    case axi_channel::w:
        return axi_pkg::w_width(cfg.data_width, cfg.user_width);
    case axi_channel::b:
        return axi_pkg::b_width(cfg.in_id_width, cfg.user_width);
    case axi_channel::ar:
        return axi_pkg::ar_width(
            cfg.addr_width, cfg.in_id_width, cfg.user_width);
    case axi_channel::r:
        return axi_pkg::r_width(
            cfg.data_width, cfg.in_id_width, cfg.user_width);
    }
    throw std::out_of_range("invalid AXI channel");
}

/// Mirrors `floo_pkg::get_max_axi_payload_bits`, including its trailing `+ 1`:
/// a physical channel always carries at least one reserved bit.
constexpr unsigned get_max_axi_payload_bits(
    const axi_cfg& cfg, physical_channel channel)
{
    unsigned widest = 0;
    for (unsigned index = 0; index <= static_cast<unsigned>(axi_channel::r);
         ++index) {
        const auto axi_ch = static_cast<axi_channel>(index);
        if (axi_chan_mapping(axi_ch) != channel) {
            continue;
        }
        widest = std::max(widest, get_axi_chan_width(cfg, axi_ch));
    }
    return widest + 1;
}

/// Mirrors `floo_pkg::get_axi_rsvd_bits`: the padding a given AXI channel needs
/// to fill its physical channel.
constexpr unsigned get_axi_rsvd_bits(const axi_cfg& cfg, axi_channel channel)
{
    return get_max_axi_payload_bits(cfg, axi_chan_mapping(channel))
        - get_axi_chan_width(cfg, channel);
}

/// AXI channel payloads. Field sets follow `AXI_TYPEDEF_ALL_CT` in
/// `axi/include/axi/typedef.svh`; widths are carried as plain integers because
/// the model does not need bit-accurate packing yet, only correct sizing and
/// correct field content.
struct axi_aw_chan {
    std::uint64_t id{};
    std::uint64_t addr{};
    std::uint8_t len{};
    std::uint8_t size{};
    std::uint8_t burst{};
    bool lock{};
    std::uint8_t cache{};
    std::uint8_t prot{};
    std::uint8_t qos{};
    std::uint8_t region{};
    std::uint8_t atop{};
    std::uint64_t user{};

    bool operator==(const axi_aw_chan& other) const
    {
        return id == other.id && addr == other.addr && len == other.len
            && size == other.size && burst == other.burst
            && lock == other.lock && cache == other.cache
            && prot == other.prot && qos == other.qos
            && region == other.region && atop == other.atop
            && user == other.user;
    }
};

struct axi_w_chan {
    std::uint64_t data{};
    std::uint64_t strb{};
    bool last{};
    std::uint64_t user{};

    bool operator==(const axi_w_chan& other) const
    {
        return data == other.data && strb == other.strb && last == other.last
            && user == other.user;
    }
};

struct axi_ar_chan {
    std::uint64_t id{};
    std::uint64_t addr{};
    std::uint8_t len{};
    std::uint8_t size{};
    std::uint8_t burst{};
    bool lock{};
    std::uint8_t cache{};
    std::uint8_t prot{};
    std::uint8_t qos{};
    std::uint8_t region{};
    std::uint64_t user{};

    bool operator==(const axi_ar_chan& other) const
    {
        return id == other.id && addr == other.addr && len == other.len
            && size == other.size && burst == other.burst
            && lock == other.lock && cache == other.cache
            && prot == other.prot && qos == other.qos
            && region == other.region && user == other.user;
    }
};

struct axi_b_chan {
    std::uint64_t id{};
    std::uint8_t resp{};
    std::uint64_t user{};

    bool operator==(const axi_b_chan& other) const
    {
        return id == other.id && resp == other.resp && user == other.user;
    }
};

struct axi_r_chan {
    std::uint64_t id{};
    std::uint64_t data{};
    std::uint8_t resp{};
    bool last{};
    std::uint64_t user{};

    bool operator==(const axi_r_chan& other) const
    {
        return id == other.id && data == other.data && resp == other.resp
            && last == other.last && user == other.user;
    }
};


// `sc_signal` requires streaming as well as equality. These print the fields a
// cross-check trace reads back, not the full payload.

inline std::ostream& operator<<(std::ostream& os, const axi_aw_chan& value)
{
    return os << "aw{id=" << value.id << ",addr=" << value.addr
              << ",len=" << unsigned{value.len} << '}';
}

inline std::ostream& operator<<(std::ostream& os, const axi_w_chan& value)
{
    return os << "w{data=" << value.data << ",last=" << value.last << '}';
}

inline std::ostream& operator<<(std::ostream& os, const axi_ar_chan& value)
{
    return os << "ar{id=" << value.id << ",addr=" << value.addr
              << ",len=" << unsigned{value.len} << '}';
}

inline std::ostream& operator<<(std::ostream& os, const axi_b_chan& value)
{
    return os << "b{id=" << value.id << ",resp=" << unsigned{value.resp} << '}';
}

inline std::ostream& operator<<(std::ostream& os, const axi_r_chan& value)
{
    return os << "r{id=" << value.id << ",data=" << value.data
              << ",last=" << value.last << '}';
}


// `sc_signal`/`sc_in` of these types instantiate `sc_trace`, so it must exist
// even when nothing is traced. Only the fields a cross-check reads back are
// emitted.

inline void sc_trace(
    sc_core::sc_trace_file* tf, const axi_aw_chan& value, const std::string& name)
{
    sc_core::sc_trace(tf, value.id, name + ".id");
    sc_core::sc_trace(tf, value.addr, name + ".addr");
    sc_core::sc_trace(tf, value.len, name + ".len");
    sc_core::sc_trace(tf, value.atop, name + ".atop");
}

inline void sc_trace(
    sc_core::sc_trace_file* tf, const axi_w_chan& value, const std::string& name)
{
    sc_core::sc_trace(tf, value.data, name + ".data");
    sc_core::sc_trace(tf, value.strb, name + ".strb");
    sc_core::sc_trace(tf, value.last, name + ".last");
}

inline void sc_trace(
    sc_core::sc_trace_file* tf, const axi_ar_chan& value, const std::string& name)
{
    sc_core::sc_trace(tf, value.id, name + ".id");
    sc_core::sc_trace(tf, value.addr, name + ".addr");
    sc_core::sc_trace(tf, value.len, name + ".len");
}

inline void sc_trace(
    sc_core::sc_trace_file* tf, const axi_b_chan& value, const std::string& name)
{
    sc_core::sc_trace(tf, value.id, name + ".id");
    sc_core::sc_trace(tf, value.resp, name + ".resp");
}

inline void sc_trace(
    sc_core::sc_trace_file* tf, const axi_r_chan& value, const std::string& name)
{
    sc_core::sc_trace(tf, value.id, name + ".id");
    sc_core::sc_trace(tf, value.data, name + ".data");
    sc_core::sc_trace(tf, value.last, name + ".last");
}

} // namespace floo::model
