// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "floo_noc_model/floo_types.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace floo::model {

struct endpoint_region {
    std::uint64_t base{0};
    std::uint64_t size{0};
    coordinate destination{};
};

class reference_address_map {
public:
    explicit reference_address_map(std::vector<endpoint_region> regions)
        : regions_(std::move(regions))
    {
        validate();
    }

    std::optional<coordinate> decode(std::uint64_t address) const
    {
        for (const auto& region : regions_) {
            if (address >= region.base && address - region.base < region.size) {
                return region.destination;
            }
        }
        return std::nullopt;
    }

    const std::vector<endpoint_region>& regions() const
    {
        return regions_;
    }

private:
    static bool overlaps(const endpoint_region& lhs, const endpoint_region& rhs)
    {
        const auto lhs_last = lhs.base + (lhs.size - 1);
        const auto rhs_last = rhs.base + (rhs.size - 1);
        return lhs.base <= rhs_last && rhs.base <= lhs_last;
    }

    void validate() const
    {
        for (std::size_t i = 0; i < regions_.size(); ++i) {
            const auto& region = regions_[i];
            if (region.size == 0) {
                throw std::invalid_argument("FlooNoC address region has zero size");
            }
            if (region.base > UINT64_MAX - (region.size - 1)) {
                throw std::invalid_argument("FlooNoC address region wraps");
            }
            for (std::size_t j = i + 1; j < regions_.size(); ++j) {
                if (overlaps(region, regions_[j])) {
                    throw std::invalid_argument("FlooNoC address regions overlap");
                }
            }
        }
    }

    std::vector<endpoint_region> regions_;
};

inline direction xy_next_hop(
    const coordinate& current,
    const coordinate& destination)
{
    if (current.x == destination.x && current.y == destination.y) {
        const unsigned local_port =
            to_port(direction::eject) + destination.port_id.to_uint();
        if (local_port != to_port(direction::eject)) {
            throw std::out_of_range(
                "vertical slice v0 supports one eject port per router");
        }
        return direction::eject;
    }
    if (current.x < destination.x) {
        return direction::east;
    }
    if (current.x > destination.x) {
        return direction::west;
    }
    if (current.y < destination.y) {
        return direction::north;
    }
    return direction::south;
}

inline std::vector<direction> xy_path(
    coordinate source,
    const coordinate& destination,
    unsigned mesh_width,
    unsigned mesh_height)
{
    if (mesh_width == 0 || mesh_height == 0) {
        throw std::invalid_argument("FlooNoC mesh dimensions must be non-zero");
    }

    const auto in_mesh = [mesh_width, mesh_height](const coordinate& id) {
        return id.x.to_uint() < mesh_width && id.y.to_uint() < mesh_height;
    };
    if (!in_mesh(source) || !in_mesh(destination)) {
        throw std::out_of_range("FlooNoC endpoint lies outside the mesh");
    }

    std::vector<direction> path;
    const std::size_t max_hops =
        static_cast<std::size_t>(mesh_width) + mesh_height;

    while (source.x != destination.x || source.y != destination.y) {
        if (path.size() > max_hops) {
            throw std::logic_error("XY routing failed to converge");
        }
        const direction hop = xy_next_hop(source, destination);
        path.push_back(hop);
        switch (hop) {
        case direction::east:  source.x = source.x + 1; break;
        case direction::west:  source.x = source.x - 1; break;
        case direction::north: source.y = source.y + 1; break;
        case direction::south: source.y = source.y - 1; break;
        case direction::eject:
            throw std::logic_error("early ejection in XY reference path");
        }
    }

    path.push_back(direction::eject);
    return path;
}

} // namespace floo::model
