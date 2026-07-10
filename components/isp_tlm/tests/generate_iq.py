import struct

# The packing format matches the isp_iq_config struct in isp_config.h exactly
# '<' = Little Endian
# 'I' = uint32 (4 bytes)
# 'H' = uint16 (2 bytes)
# 'B' = uint8  (1 byte)
# 'f' = float  (4 bytes)

fmt = "<" + (
    "I "  # magic_word
    "I I I I I I I "  # Global params (width, height, stride, format, op_mode, bit_depth, bayer_pattern)
    "B B H H H H H H H H "  # BLC
    "B H "  # DPC
    "B I I "  # LSC
    "B I I "  # DG
    "B I f f f f f f "  # BNR
    "B "  # Demosaic
    "B I f f f f f "  # AWB
    "B f f "  # WB
    "B f f f f f f f f f "  # CCM
    "B I "  # GC
    "B I I f "  # AEC
    "B I "  # CSC
    "B f "  # CSE
    "B I I "  # Sharpen
    "B I I I "  # 2DNR
    "B I I "  # Scale
    "B"  # YUV420
)

# Populate the values (these map 1-to-1 with the C++ struct variables)
params = [
    0x49535021,  # magic_word
    # Global
    2688,
    1520,
    2688,
    0,
    0,
    16,
    2,  # width, height, stride, format, op_mode, bit_depth, bayer_pattern
    # BLC
    1,
    1,
    256,
    256,
    256,
    256,
    4095,
    4095,
    4095,
    4095,  # enable, linear, r/gr/gb/b offset, r/gr/gb/b sat
    # DPC
    1,
    30,  # enable, threshold
    # LSC
    0,
    16,
    12,  # enable, grid_w, grid_h
    # DG
    1,
    5,
    1,  # enable, gain, auto
    # BNR
    1,
    3,
    1.5,
    0.1,
    1.5,
    0.1,
    1.5,
    0.1,  # enable, window, r_std_dev_s, r_std_dev_r, g_std_dev_s, g_std_dev_r, b_std_dev_s, b_std_dev_r
    # Demosaic
    1,  # enable
    # AWB
    1,
    0,
    1.0,
    1.0,
    0.01,
    0.01,
    0.5,  # enable, algorithm, r_gain, b_gain, under_pct, over_pct, percent
    # WB
    1,
    1.0,
    1.0,  # enable, r_gain, b_gain
    # CCM
    1,
    1.0,
    0.0,
    0.0,
    0.0,
    1.0,
    0.0,
    0.0,
    0.0,
    1.0,  # enable, matrix
    # GC
    1,
    1,  # enable, gamma
    # AEC
    1,
    0,
    128,
    0.5,  # enable, feedback, center_illum, skewness
    # CSC
    1,
    0,  # enable, standard
    # CSE
    1,
    1.0,  # enable, sat_gain
    # Sharpen
    1,
    1,
    1,  # enable, sigma, strength
    # 2DNR
    1,
    5,
    3,
    100,  # enable, window, patch, wts
    # Scale
    0,
    2688,
    1520,  # enable, out_w, out_h
    # YUV420
    1,  # enable
]

import os

try:
    binary_data = struct.pack(fmt, *params)
    script_dir = os.path.dirname(os.path.abspath(__file__))
    out_path = os.path.join(script_dir, "tuning.bin")
    with open(out_path, "wb") as f:
        f.write(binary_data)
    print(f"Successfully generated {out_path}")
except struct.error as e:
    print(f"Failed to pack binary data: {e}")
