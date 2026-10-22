/*
 * FX1 bare-metal driver for the VP camera sensor (CSI-2 M5b): the CCS
 * register subset of the CcsSensor model
 * (components/FX1_Components/mipi-csi2/sensor, spec_decisions P12) over I2C.
 *
 * Registers: 16-bit index, 8-bit data, big-endian multi-byte values.
 * Sequence the model supports and the camera smokes use:
 *   probe -> set_mode (in standby) -> stream(1) ... stream(0)
 * A stop (stream 0 or soft_reset) ends the stream at the end of the frame in
 * progress; the sensor has no status register for that, so firmware observes
 * the end of the frame at the CSI-2 receiver (BUF_DONE / FRAME_COUNT).
 */
#ifndef FX1_CCS_SENSOR_H
#define FX1_CCS_SENSOR_H

#include <stdint.h>

#include "fx1_i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FX1_CCS_ADDR      0x10u   /* 7-bit address on I2C0 */
#define FX1_CCS_MODEL_ID  0xF1C5u /* VP value, not a commercial sensor */

#define FX1_CCS_OK      0
#define FX1_CCS_EINVAL -10
#define FX1_CCS_ENODEV -11 /* model_id does not match */
#define FX1_CCS_ESTATE -12 /* set_mode while streaming */
/* other negative values: FX1_I2C_* from the transfer */

typedef struct fx1_ccs_mode {
    uint16_t width, height;      /* x_output_size, y_output_size */
    uint16_t line_length;        /* line_length_pck: line period in pixel clocks (>= width) */
    uint16_t frame_length;       /* frame_length_lines: frame period in lines (>= height) */
    uint8_t bits;                /* 10 (RAW10) or 12 (RAW12) */
    uint8_t lanes;               /* 1, 2 or 4 */
} fx1_ccs_mode;

typedef struct fx1_ccs {
    fx1_i2c_t* bus;
    uint8_t addr7;
    uint32_t timeout_us;          /* per register transfer */
} fx1_ccs;

/* Reads model_id; FX1_CCS_ENODEV if it is not FX1_CCS_MODEL_ID. */
int fx1_ccs_probe(fx1_ccs* s, fx1_i2c_t* bus, uint8_t addr7, uint32_t timeout_us);
int fx1_ccs_read8(fx1_ccs* s, uint16_t reg, uint8_t* v);
int fx1_ccs_read16(fx1_ccs* s, uint16_t reg, uint16_t* v);
int fx1_ccs_write8(fx1_ccs* s, uint16_t reg, uint8_t v);
int fx1_ccs_write16(fx1_ccs* s, uint16_t reg, uint16_t v);
/* Programs format, lanes, size and timing; requires mode_select = 0. Reads
 * the size back. */
int fx1_ccs_set_mode(fx1_ccs* s, const fx1_ccs_mode* m);
int fx1_ccs_stream(fx1_ccs* s, int on);
/* mode_select read back: *on = 1 streaming requested, 0 standby requested. */
int fx1_ccs_stream_state(fx1_ccs* s, int* on);
int fx1_ccs_soft_reset(fx1_ccs* s);
/* CCS frame_count (0x0005): frames sent completely since the last reset, 8 bits. */
int fx1_ccs_frame_count(fx1_ccs* s, uint8_t* n);

/* Pixel of the model's built-in image: frame number f (1 after a reset),
 * column x, line y, bit depth bits (CcsSensor::pattern). */
static inline uint16_t fx1_ccs_pattern(uint32_t f, uint32_t x, uint32_t y, uint32_t bits)
{
    return (uint16_t)((x * 37u + y * 101u + f * 53u + 5u) & ((1u << bits) - 1u));
}

#ifdef __cplusplus
}
#endif

#endif /* FX1_CCS_SENSOR_H */
