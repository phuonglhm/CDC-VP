# Sharpening (Sharpen)

Applies unsharp masking to the luma channel to enhance edge contrast. Uses a Gaussian kernel to create a blurred version of the image, then subtracts it from the original to enhance edges. U and V channels pass through unchanged.

**Pipeline position:** After CSE

**Input:**  Saturation-enhanced YUV420 image (uint8_t per pixel)
**Output:** Sharpened YUV420 image (uint8_t per pixel)

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `is_enable` | bool | Enable/disable sharpening |
| `sharpen_sigma` | uint8_t | Gaussian kernel sigma |
| `sharpen_strength` | uint16_t | Sharpening strength |

**Note:** Gaussian kernel radius = ceil(3 * sigma). Only the Y (luma) channel is sharpened. U and V pass through unchanged.
