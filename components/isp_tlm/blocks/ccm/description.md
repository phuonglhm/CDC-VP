# Color Correction Matrix (CCM)

Applies a 3×3 color correction matrix to the RGB image. The matrix transforms the camera RGB colorspace to a more accurate/desired colorspace. A default identity matrix is provided.

**Pipeline position:** After WB

**Input:**  White-balanced RGB image (uint16_t per pixel, 3 channels)
**Output:** Color-corrected RGB image (uint16_t per pixel, 3 channels)

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `is_enable` | bool | Enable/disable CCM |
| `bit_depth` | uint8_t | Bit depth for normalization |
| `corrected_red[3]` | float[3] | CCM row for red channel |
| `corrected_green[3]` | float[3] | CCM row for green channel |
| `corrected_blue[3]` | float[3] | CCM row for blue channel |

**Default matrix (identity):**
```
| 1.0  0.0  0.0 |
| 0.0  1.0  0.0 |
| 0.0  0.0  1.0 |
```
