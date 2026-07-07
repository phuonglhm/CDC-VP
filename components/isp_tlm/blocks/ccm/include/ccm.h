#pragma once

#include <cstdint>
#include <vector>

struct ccm_config {
    bool is_enable   = false;
    bool auto_compute = false;   // true → compute CCM from input image on each run
    float corrected_red[3]   = {1.0f, 0.0f, 0.0f};
    float corrected_green[3] = {0.0f, 1.0f, 0.0f};
    float corrected_blue[3]  = {0.0f, 0.0f, 1.0f};
};

class ccm_block {
public:
    // Compute a CCM from the supplied white-balanced RGB image and fill cfg with it.
    // Uses the gray-world assumption: average R≈G≈B → drive cross-channel coefficients
    // to equalize the means.  Produces a well-conditioned matrix with all rows
    // summing to ~1 so luminance is preserved.
    void compute_ccm(const std::uint16_t* in,
                     std::uint32_t        width,
                     std::uint32_t        height,
                     ccm_config&          cfg,
                     std::uint8_t         bit_depth);

    // Apply the 3×3 CCM to every pixel.
    void process(const std::uint16_t* in,
                 std::uint16_t*       out,
                 std::uint32_t        width,
                 std::uint32_t        height,
                 const ccm_config&    cfg,
                 std::uint8_t         bit_depth) const;

    void process(const std::vector<std::uint16_t>& in,
                 std::vector<std::uint16_t>&       out,
                 std::uint32_t                     width,
                 std::uint32_t                     height,
                 const ccm_config&                 cfg,
                 std::uint8_t                      bit_depth) const;
};
