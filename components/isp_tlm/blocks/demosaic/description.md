# Demosaicing (Demosaic)

Converts the RAW Bayer CFA (Color Filter Array) image into a full RGB image using adaptive homogeneous area-based demosaicing. For each pixel, it uses a 5×5 window around the pixel to interpolate the missing color channels.

**Pipeline position:** After BNR (first step into RGB domain)

**Input:**  Noise-reduced RAW Bayer data (uint16_t per pixel)
**Output:** Full RGB image (uint16_t per pixel, 3 channels interleaved: R0, G0, B0, R1, G1, B1, ...)

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `is_enable` | bool | Enable/disable demosaicing (bypass copies raw to all channels) |

**Supported Bayer patterns:** RGGB, GRBG, BGGR, GBRG
