# Image Scaling (Scale)

Resizes the YUV image to a different output resolution. Uses nearest-neighbor downscaling or upscaling. Only processes the Y (luma) channel; U and V are scaled by the same factor.

**Pipeline position:** After 2DNR (optional)

**Input:**  Noise-reduced YUV420 image (uint8_t per pixel)
**Output:** Scaled YUV420 image (uint8_t per pixel)

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `is_enable` | bool | Enable/disable scaling |
| `in_width` | uint16_t | Input image width |
| `in_height` | uint16_t | Input image height |
| `out_width` | uint16_t | Output image width |
| `out_height` | uint16_t | Output image height |
