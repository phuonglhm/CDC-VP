#pragma once

#include "hevc/yuv420.hpp"

#include <cstdint>
#include <vector>

namespace hevc {

enum class CodingMode : std::uint32_t {
    Pcm = 0,
    IntraDc = 1,
    HybridDc = 2,
    IntraDcTq = 3,
    IntraFullTq = 4,
    IntraFullTq16 = 5,
    IntraAdaptiveTq = 6,
    IntraDirectionalTq = 7,
};

struct EncoderConfig {
    unsigned width = 0;
    unsigned height = 0;
    int qp = 26;
    CodingMode mode = CodingMode::Pcm;
};

class HevcPcmEncoder {
public:
    explicit HevcPcmEncoder(EncoderConfig config);

    [[nodiscard]] unsigned padded_width() const { return padded_width_; }
    [[nodiscard]] unsigned padded_height() const { return padded_height_; }
    [[nodiscard]] unsigned ctu_count() const;

    [[nodiscard]] std::vector<std::uint8_t> parameter_sets_annex_b() const;
    [[nodiscard]] std::vector<std::uint8_t>
    encode_idr_frame_annex_b(const Yuv420Frame& frame, std::uint64_t frame_index) const;
    [[nodiscard]] Yuv420Frame reconstruct_idr_frame(const Yuv420Frame& frame) const;

private:
    struct DcLevels {
        int y = 0;
        int cb = 0;
        int cr = 0;
    };

    [[nodiscard]] std::vector<std::uint8_t> make_vps_rbsp() const;
    [[nodiscard]] std::vector<std::uint8_t> make_sps_rbsp() const;
    [[nodiscard]] std::vector<std::uint8_t> make_pps_rbsp() const;
    [[nodiscard]] std::vector<std::uint8_t>
    make_pcm_slice_rbsp(const Yuv420Frame& padded, unsigned ctu_address,
                        bool first_slice) const;
    [[nodiscard]] std::vector<std::uint8_t>
    make_intra_dc_slice_rbsp(unsigned ctu_address, bool first_slice) const;
    [[nodiscard]] std::vector<std::uint8_t>
    make_intra_dc_tq_slice_rbsp(const Yuv420Frame& padded,
                                unsigned ctu_address, bool first_slice) const;
    [[nodiscard]] std::vector<std::uint8_t>
    make_intra_full_tq_slice_rbsp(const Yuv420Frame& padded,
                                  unsigned ctu_address, bool first_slice) const;
    [[nodiscard]] std::vector<std::uint8_t>
    make_intra_full_tq16_slice_rbsp(const Yuv420Frame& padded,
                                    unsigned ctu_address, bool first_slice) const;
    [[nodiscard]] std::vector<std::uint8_t>
    make_intra_adaptive_tq_slice_rbsp(const Yuv420Frame& padded,
                                      unsigned ctu_address, bool first_slice) const;
    [[nodiscard]] std::vector<std::uint8_t>
    make_intra_directional_tq_slice_rbsp(const Yuv420Frame& padded,
                                         unsigned ctu_address,
                                         bool first_slice) const;
    [[nodiscard]] DcLevels dc_levels(const Yuv420Frame& padded,
                                     unsigned ctu_address) const;
    [[nodiscard]] bool use_intra_dc(const Yuv420Frame& padded,
                                    unsigned ctu_address) const;

    EncoderConfig config_;
    unsigned padded_width_ = 0;
    unsigned padded_height_ = 0;
};

} // namespace hevc
