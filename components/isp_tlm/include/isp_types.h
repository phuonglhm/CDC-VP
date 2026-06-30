#ifndef ISP_TYPES_H
#define ISP_TYPES_H

enum class cfa_types {
    RGGB,
    GRBG,
    BGGR,
    GBRG
};

enum class bayer_channel {
    R = 0,
    GR = 1,
    GB = 2,
    B = 3
};

#endif // ISP_TYPES_H
