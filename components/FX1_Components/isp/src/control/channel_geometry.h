// SPDX-License-Identifier: Apache-2.0
// Output channel geometry (MAS v1.07 §6.20.1-6.20.4, §8.3): per-channel
// post-IQ YUV444 crop, Resizer preset and output limit, validated into the
// complete set that GEOMETRY_STATUS, ACTIVE_CROP_* and RESIZER_OUT_W/H report
// and that a frame captures at its accepted SOF. Shared by the control unit
// (publication), the pipeline (datapath) and the tests.
#pragma once

#include <cstdint>

namespace cdc::components::fx1_isp {

struct geometry {
   std::uint32_t width;
   std::uint32_t height;
};

constexpr unsigned num_channels = 3;  // CH0, CH1 NV12; CH2 RGB888

// GEOMETRY_STATUS.ERROR priority codes (CSR v1.07): the lowest applicable wins.
enum geometry_error : unsigned {
   geo_ok = 0,
   geo_input = 1,          // input format / active size
   geo_crop_bounds = 2,    // crop empty or out of bounds
   geo_crop_odd = 3,       // odd crop origin or size
   geo_mode = 4,           // reserved resize mode (for this channel)
   geo_limit = 5,          // target exceeds the channel limit
   geo_exceeds_crop = 6,   // target exceeds the crop (no upscaling)
};

// CHc_OUTPUT_LIMIT (Table 6-115).
geometry channel_limit(unsigned ch);
// The preset of RESIZER_CTRL.SCALE on channel `ch` (Table 6-122; CH2 accepts
// 0, 8, 14 and 16..21 only). Code 0 is FullRes ({0, 0}). False: reserved.
bool resizer_target(unsigned ch, std::uint32_t scale, geometry &target);

// Bilinear and decimation plan of one channel (MAS §6.20.3.4-6.20.3.6).
struct resizer_plan {
   bool pass = true;       // no scaling: FullRes, disabled scaler or target = crop
   unsigned stages = 0;    // k decimation stages
   std::uint32_t out_w = 0, out_h = 0;
   std::uint32_t dec_w = 0, dec_h = 0;  // bilinear input size
   std::uint32_t step_x = 0, step_y = 0, init_x = 0, init_y = 0;  // UQ16.16
};
// From an in_w x in_h crop to an out_w x out_h target (out <= in).
resizer_plan plan_resizer(std::uint32_t in_w, std::uint32_t in_h, std::uint32_t out_w, std::uint32_t out_h);

// The configuration key of one channel (MAS §6.20.4.3): active input size and
// format, crop enable/origin/size, scaler enable and mode; the limit is fixed
// by the channel.
struct channel_key {
   bool valid = false;  // false: the reset set, derived from no key
   unsigned ch = 0;
   std::uint32_t width = 0, height = 0;  // active image
   bool input_ok = true;                 // COMMON_INPUT_FORMAT.DEPTH not reserved
   bool crop_en = false;
   std::uint32_t crop_x = 0, crop_y = 0, crop_w = 0, crop_h = 0;
   bool en = false;
   std::uint32_t scale = 0;
   bool operator==(const channel_key &o) const {
      return valid == o.valid && ch == o.ch && width == o.width && height == o.height && input_ok == o.input_ok &&
             crop_en == o.crop_en && crop_x == o.crop_x && crop_y == o.crop_y && crop_w == o.crop_w &&
             crop_h == o.crop_h && en == o.en && scale == o.scale;
   }
   bool operator!=(const channel_key &o) const { return !(*this == o); }
};

// The complete set validated for a key. An invalid set carries its error;
// its readback words (crop, out) are those of the last valid set (MAS §8.3:
// the last readback may remain), zero after reset.
struct channel_set {
   channel_key key;
   bool valid = false;
   unsigned error = geo_ok;
   bool scaling = false;
   std::uint32_t crop_x = 0, crop_y = 0, crop_w = 0, crop_h = 0;  // resolved crop (ACTIVE_CROP_*)
   geometry out{0, 0};                                            // RESIZER_OUT_W/H
   resizer_plan plan{};
};

// Validation and derivation of a key (pure). `previous` supplies the
// readback an invalid result keeps.
channel_set derive_channel(const channel_key &k, const channel_set &previous = channel_set{});

}  // namespace cdc::components::fx1_isp
