# White Balance (WB)

Applies manual white balance gains to the RGB image. Multiplies the R and B channels by their respective gains while passing G unchanged. When AWB is enabled, the WB gains are multiplied with the AWB-computed gains.

**Pipeline position:** After demosaic + AWB

**Input:**  Full RGB image from demosaic (uint16_t per pixel, 3 channels)
**Output:** White-balanced RGB image (uint16_t per pixel, 3 channels)

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `is_enable` | bool | Enable/disable WB |
| `r_gain` | float | Red channel gain multiplier |
| `b_gain` | float | Blue channel gain multiplier |

**Note:** G channel is passed through unchanged.
