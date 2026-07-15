#include "hevc/bit_writer.hpp"
#include "hevc/cabac.hpp"
#include "hevc/transform.hpp"
#include "hevc/yuv420.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <sstream>

int main() {
    hevc::BitWriter bits;
    bits.put_ue(0); // 1
    bits.put_ue(1); // 010
    bits.put_se(-1); // ue(2) = 011
    bits.rbsp_trailing_bits();
    assert(bits.bytes().size() == 1);
    assert(bits.bytes()[0] == 0xa7); // 1 010 011 1

    const std::vector<std::uint8_t> rbsp{0x00, 0x00, 0x01, 0x00, 0x00, 0x03, 0x04};
    const auto ebsp = hevc::rbsp_to_ebsp(rbsp);
    const std::vector<std::uint8_t> expected{0x00,0x00,0x03,0x01,0x00,0x00,0x03,0x03,0x04};
    assert(ebsp == expected);

    auto context = hevc::CabacContext::from_init_value(139, 26);
    assert(context.state() == 0 && context.mps() == 0);

    assert(hevc::yuv420_frame_bytes(64, 64) == 6144);

    hevc::Plane plane{32, 32, std::vector<std::uint8_t>(32 * 32, 140)};
    const auto dc = hevc::transform_quantize_block(plane, 0, 0, 32, 26);
    assert(dc.has_nonzero());
    assert(dc.coefficients[0] != 0);
    for (std::size_t i = 1; i < dc.coefficients.size(); ++i) {
        assert(dc.coefficients[i] == 0);
    }
    hevc::Plane reconstruction{32, 32,
                               std::vector<std::uint8_t>(32 * 32, 0)};
    hevc::inverse_reconstruct_block(dc, 26, reconstruction, 0, 0);
    assert(std::all_of(reconstruction.samples.begin(),
                       reconstruction.samples.end(),
                       [&](std::uint8_t value) {
                           return value == reconstruction.samples.front();
                       }));

    for (unsigned y = 0; y < 32; ++y) {
        for (unsigned x = 0; x < 32; ++x) {
            plane.samples[static_cast<std::size_t>(y) * plane.width + x] =
                static_cast<std::uint8_t>((x * 5 + y * 3) & 255U);
        }
    }
    const auto ac = hevc::transform_quantize_block(plane, 0, 0, 32, 20);
    assert(std::any_of(ac.coefficients.begin() + 1, ac.coefficients.end(),
                       [](int value) { return value != 0; }));

    hevc::Plane small_plane{8, 8, std::vector<std::uint8_t>(8 * 8)};
    std::vector<std::uint8_t> spatial_prediction(8 * 8);
    for (unsigned y = 0; y < 8; ++y) {
        for (unsigned x = 0; x < 8; ++x) {
            small_plane.samples[static_cast<std::size_t>(y) * 8 + x] =
                static_cast<std::uint8_t>(32 + x * 9 + y * 5);
            spatial_prediction[static_cast<std::size_t>(y) * 8 + x] =
                static_cast<std::uint8_t>(40 + x * 3);
        }
    }
    const auto small = hevc::transform_quantize_predicted_block(
        small_plane, 0, 0, 8, 26, spatial_prediction);
    assert(small.size == 8 && small.has_nonzero());
    hevc::Plane small_reconstruction{
        8, 8, std::vector<std::uint8_t>(8 * 8)};
    hevc::inverse_reconstruct_predicted_block(
        small, 26, small_reconstruction, 0, 0, spatial_prediction);
    assert(std::any_of(small_reconstruction.samples.begin(),
                       small_reconstruction.samples.end(),
                       [](std::uint8_t value) { return value != 0; }));
    std::cout << "core tests passed\n";
    return 0;
}
