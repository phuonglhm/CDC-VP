#include "fx1_ccs_sensor.h"

/* CCS register indexes of the subset (sensor/include/fx1_csi2_sensor/ccs_sensor.h). */
#define CCS_MODEL_ID         0x0000u
#define CCS_FRAME_COUNT      0x0005u
#define CCS_MODE_SELECT      0x0100u
#define CCS_SOFTWARE_RESET   0x0103u
#define CCS_CSI_DATA_FORMAT  0x0112u
#define CCS_CSI_LANE_MODE    0x0114u
#define CCS_FRAME_LENGTH     0x0340u
#define CCS_LINE_LENGTH      0x0342u
#define CCS_X_OUTPUT_SIZE    0x034Cu
#define CCS_Y_OUTPUT_SIZE    0x034Eu

int fx1_ccs_read8(fx1_ccs* s, uint16_t reg, uint8_t* v)
{
    if (!s || !v) return FX1_CCS_EINVAL;
    const uint8_t idx[2] = {(uint8_t)(reg >> 8), (uint8_t)reg};
    return fx1_i2c_write_read(s->bus, s->addr7, idx, 2, v, 1, s->timeout_us);
}

int fx1_ccs_read16(fx1_ccs* s, uint16_t reg, uint16_t* v)
{
    if (!s || !v) return FX1_CCS_EINVAL;
    const uint8_t idx[2] = {(uint8_t)(reg >> 8), (uint8_t)reg};
    uint8_t b[2] = {0, 0};
    const int rc = fx1_i2c_write_read(s->bus, s->addr7, idx, 2, b, 2, s->timeout_us);
    *v = (uint16_t)(b[0] << 8 | b[1]);
    return rc;
}

int fx1_ccs_write8(fx1_ccs* s, uint16_t reg, uint8_t v)
{
    if (!s) return FX1_CCS_EINVAL;
    const uint8_t b[3] = {(uint8_t)(reg >> 8), (uint8_t)reg, v};
    return fx1_i2c_write(s->bus, s->addr7, b, 3, s->timeout_us);
}

int fx1_ccs_write16(fx1_ccs* s, uint16_t reg, uint16_t v)
{
    if (!s) return FX1_CCS_EINVAL;
    const uint8_t b[4] = {(uint8_t)(reg >> 8), (uint8_t)reg, (uint8_t)(v >> 8), (uint8_t)v};
    return fx1_i2c_write(s->bus, s->addr7, b, 4, s->timeout_us);
}

int fx1_ccs_probe(fx1_ccs* s, fx1_i2c_t* bus, uint8_t addr7, uint32_t timeout_us)
{
    if (!s || !bus) return FX1_CCS_EINVAL;
    s->bus = bus;
    s->addr7 = addr7;
    s->timeout_us = timeout_us;
    uint16_t id = 0;
    const int rc = fx1_ccs_read16(s, CCS_MODEL_ID, &id);
    if (rc) return rc;
    return id == FX1_CCS_MODEL_ID ? FX1_CCS_OK : FX1_CCS_ENODEV;
}

int fx1_ccs_set_mode(fx1_ccs* s, const fx1_ccs_mode* m)
{
    if (!s || !m || (m->bits != 10 && m->bits != 12) || (m->lanes != 1 && m->lanes != 2 && m->lanes != 4) ||
        !m->width || !m->height || m->line_length < m->width || m->frame_length < m->height)
        return FX1_CCS_EINVAL;
    uint8_t mode = 0;
    int rc = fx1_ccs_read8(s, CCS_MODE_SELECT, &mode);
    if (rc) return rc;
    if (mode & 1u) return FX1_CCS_ESTATE;
    const uint16_t fmt = m->bits == 10 ? 0x0A0Au : 0x0C0Cu;
    if ((rc = fx1_ccs_write16(s, CCS_CSI_DATA_FORMAT, fmt))) return rc;
    if ((rc = fx1_ccs_write8(s, CCS_CSI_LANE_MODE, (uint8_t)(m->lanes - 1u)))) return rc;
    /* x and y output size in one auto-increment write */
    const uint8_t size[6] = {(uint8_t)(CCS_X_OUTPUT_SIZE >> 8), (uint8_t)CCS_X_OUTPUT_SIZE,
                             (uint8_t)(m->width >> 8),          (uint8_t)m->width,
                             (uint8_t)(m->height >> 8),         (uint8_t)m->height};
    if ((rc = fx1_i2c_write(s->bus, s->addr7, size, 6, s->timeout_us))) return rc;
    if ((rc = fx1_ccs_write16(s, CCS_LINE_LENGTH, m->line_length))) return rc;
    if ((rc = fx1_ccs_write16(s, CCS_FRAME_LENGTH, m->frame_length))) return rc;
    uint16_t w = 0, h = 0;
    if ((rc = fx1_ccs_read16(s, CCS_X_OUTPUT_SIZE, &w))) return rc;
    if ((rc = fx1_ccs_read16(s, CCS_Y_OUTPUT_SIZE, &h))) return rc;
    return w == m->width && h == m->height ? FX1_CCS_OK : FX1_CCS_EINVAL;
}

int fx1_ccs_stream(fx1_ccs* s, int on) { return fx1_ccs_write8(s, CCS_MODE_SELECT, on ? 1u : 0u); }

int fx1_ccs_stream_state(fx1_ccs* s, int* on)
{
    if (!on) return FX1_CCS_EINVAL;
    uint8_t mode = 0;
    const int rc = fx1_ccs_read8(s, CCS_MODE_SELECT, &mode);
    if (rc) return rc;
    *on = mode & 1u;
    return FX1_CCS_OK;
}

int fx1_ccs_soft_reset(fx1_ccs* s) { return fx1_ccs_write8(s, CCS_SOFTWARE_RESET, 1u); }

int fx1_ccs_frame_count(fx1_ccs* s, uint8_t* n) { return fx1_ccs_read8(s, CCS_FRAME_COUNT, n); }
