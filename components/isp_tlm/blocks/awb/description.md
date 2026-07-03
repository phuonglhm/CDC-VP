# Auto White Balance (AWB)

Automatically computes white balance gains by analyzing the RAW Bayer image. Uses the Gray World algorithm to estimate the average color of the scene and compute R/B gains relative to G.

**Pipeline position:** After demosaic (optional, feeds WB block)

**Input:**  Noise-reduced RAW Bayer data (uint16_t per pixel)
**Output:** Computed R and B gains (r_gain_out, b_gain_out)

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `is_enable` | bool | Enable/disable AWB |
| `algorithm` | uint8_t | Algorithm selection (0 = Gray World) |
| `underexposed_percentage` | float | Threshold for underexposed pixels |
| `overexposed_percentage` | float | Threshold for overexposed pixels |
| `percentage` | float | Percentage of pixels to use |
| `r_gain_out` | float | Computed red gain (output) |
| `b_gain_out` | float | Computed blue gain (output) |

**Note:** When AWB is enabled, the computed gains feed directly into the WB block, multiplying with the WB gains.
