# YUV420 Downsampling (YUV420)

Converts YUV444 format (full resolution U and V planes) to YUV420 format (quarter-resolution chroma planes). This reduces the data rate by subsampling the chroma channels by 2× in both dimensions.

**Pipeline position:** Last block in pipeline (after Scale if enabled)

**Input:**  Full-resolution YUV image (uint8_t per pixel)
**Output:** YUV420 image (Y at full resolution, U/V at 1/4 resolution)

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `is_enable` | bool | Enable/disable YUV420 conversion |

**Output size formula:**
```
output_size = width * height * 3 / 2
= (Y plane) + (U plane: width/2 * height/2) + (V plane: width/2 * height/2)
```
