// SPDX-License-Identifier: SHL-0.51
//
// Behavioral SystemC representation derived from the public FlooNoC interface
// and routing definitions. See docs/P0_SCOPE.md for the frozen source revision.

#pragma once

#include <cstdint>
#include <ostream>
#include <stdexcept>
#include <string>

#include <systemc>

namespace floo::model {

enum class direction : std::uint8_t {
    north = 0,
    east = 1,
    south = 2,
    west = 3,
    eject = 4,
};

enum class axi_channel : std::uint8_t {
    aw = 0,
    w = 1,
    ar = 2,
    b = 3,
    r = 4,
};

enum class physical_channel : std::uint8_t {
    req = 0,
    rsp = 1,
};

inline constexpr unsigned to_port(direction value)
{
    return static_cast<unsigned>(value);
}

inline direction direction_from_port(unsigned value)
{
    if (value > to_port(direction::eject)) {
        throw std::out_of_range("invalid FlooNoC direction");
    }
    return static_cast<direction>(value);
}

inline const char* to_string(direction value)
{
    switch (value) {
    case direction::north: return "North";
    case direction::east:  return "East";
    case direction::south: return "South";
    case direction::west:  return "West";
    case direction::eject: return "Eject";
    }
    return "Invalid";
}

struct coordinate {
    sc_dt::sc_uint<16> x{0};
    sc_dt::sc_uint<16> y{0};
    sc_dt::sc_uint<8> port_id{0};

    coordinate() = default;

    coordinate(unsigned x_value, unsigned y_value, unsigned port = 0)
        : x(x_value)
        , y(y_value)
        , port_id(port)
    {
    }

    bool operator==(const coordinate& other) const
    {
        return x == other.x && y == other.y && port_id == other.port_id;
    }

    bool operator!=(const coordinate& other) const
    {
        return !(*this == other);
    }
};

inline std::ostream& operator<<(std::ostream& os, const coordinate& value)
{
    return os << '(' << value.x.to_uint() << ',' << value.y.to_uint()
              << ",p" << value.port_id.to_uint() << ')';
}

inline void sc_trace(
    sc_core::sc_trace_file* tf,
    const coordinate& value,
    const std::string& name)
{
    sc_core::sc_trace(tf, value.x, name + ".x");
    sc_core::sc_trace(tf, value.y, name + ".y");
    sc_core::sc_trace(tf, value.port_id, name + ".port_id");
}

struct flit_header {
    coordinate dst_id{};
    coordinate src_id{};
    bool last{true};
    sc_dt::sc_uint<3> axi_ch{static_cast<unsigned>(axi_channel::aw)};
    bool rob_req{false};
    sc_dt::sc_uint<16> rob_idx{0};
    bool atop{false};

    bool operator==(const flit_header& other) const
    {
        return dst_id == other.dst_id
            && src_id == other.src_id
            && last == other.last
            && axi_ch == other.axi_ch
            && rob_req == other.rob_req
            && rob_idx == other.rob_idx
            && atop == other.atop;
    }

    bool operator!=(const flit_header& other) const
    {
        return !(*this == other);
    }
};

inline std::ostream& operator<<(std::ostream& os, const flit_header& value)
{
    return os << "{dst=" << value.dst_id
              << ",src=" << value.src_id
              << ",last=" << value.last
              << ",axi_ch=" << value.axi_ch.to_uint()
              << ",rob_req=" << value.rob_req
              << ",rob_idx=" << value.rob_idx.to_uint()
              << ",atop=" << value.atop << '}';
}

inline void sc_trace(
    sc_core::sc_trace_file* tf,
    const flit_header& value,
    const std::string& name)
{
    sc_trace(tf, value.dst_id, name + ".dst_id");
    sc_trace(tf, value.src_id, name + ".src_id");
    sc_core::sc_trace(tf, value.last, name + ".last");
    sc_core::sc_trace(tf, value.axi_ch, name + ".axi_ch");
    sc_core::sc_trace(tf, value.rob_req, name + ".rob_req");
    sc_core::sc_trace(tf, value.rob_idx, name + ".rob_idx");
    sc_core::sc_trace(tf, value.atop, name + ".atop");
}

template <unsigned PayloadBits>
struct basic_flit {
    static_assert(PayloadBits > 0, "a FlooNoC flit payload must not be empty");

    flit_header hdr{};
    sc_dt::sc_bv<PayloadBits> payload{};

    bool operator==(const basic_flit& other) const
    {
        return hdr == other.hdr && payload == other.payload;
    }

    bool operator!=(const basic_flit& other) const
    {
        return !(*this == other);
    }
};

template <unsigned PayloadBits>
inline std::ostream& operator<<(
    std::ostream& os,
    const basic_flit<PayloadBits>& value)
{
    return os << "{hdr=" << value.hdr << ",payload=0x"
              << value.payload.to_string(sc_dt::SC_HEX, false) << '}';
}

template <unsigned PayloadBits>
inline void sc_trace(
    sc_core::sc_trace_file* tf,
    const basic_flit<PayloadBits>& value,
    const std::string& name)
{
    sc_trace(tf, value.hdr, name + ".hdr");
    sc_core::sc_trace(tf, value.payload, name + ".payload");
}

using test_flit = basic_flit<64>;

} // namespace floo::model
