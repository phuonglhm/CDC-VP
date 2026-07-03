# 2D Noise Reduction (2DNR)

Applies spatial noise reduction to the luma channel using a bilateral-filter-like approach with a search window and patch size. It smooths noise while preserving edges. U and V channels pass through unchanged.

**Pipeline position:** After Sharpening

**Input:**  Sharpened YUV420 image (uint8_t per pixel)
**Output:** Noise-reduced YUV420 image (uint8_t per pixel)

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `is_enable` | bool | Enable/disable 2DNR |
| `window_size` | uint8_t | Search window size (must be >= 3) |
| `patch_size` | uint8_t | Patch size (must be >= 3) |
| `wts` | uint16_t | Weight parameter |

**Note:** Weight LUT formula: weight_lut[d] = exp(-d / wts). Only the Y (luma) channel is filtered. U and V pass through unchanged.
