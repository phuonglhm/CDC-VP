# Black Level Correction (BLC)

Corrects the black level offset introduced by the sensor. Each color channel (R, Gr, Gb, B) has its own offset value. The correction is applied per Bayer channel:

```
output = input - offset[channel]
```

If the result would exceed the saturation value, it is clamped.

**Pipeline position:** First block (after raw input)

**Input:**  RAW Bayer data (uint16_t per pixel)
**Output:** Black-level-corrected RAW Bayer data (uint16_t per pixel)

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `is_enable` | bool | Enable/disable BLC |
| `is_linear` | bool | Linear mode flag |
| `r_offset` | uint16_t | Red channel offset |
| `gr_offset` | uint16_t | Green-Red channel offset |
| `gb_offset` | uint16_t | Green-Blue channel offset |
| `b_offset` | uint16_t | Blue channel offset |
| `r_sat` | uint16_t | Red saturation value |
| `gr_sat` | uint16_t | Green-Red saturation value |
| `gb_sat` | uint16_t | Green-Blue saturation value |
| `b_sat` | uint16_t | Blue saturation value |
