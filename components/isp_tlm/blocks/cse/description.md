# Color Saturation Enhancement (CSE)

Enhances the color saturation of the YUV image by amplifying the chroma deviation from neutral. U and V channels are scaled relative to the chroma offset (128).

**Pipeline position:** First YUV-domain block (after CSC)

**Input:**  YUV420 image (uint8_t per pixel)
**Output:** Saturation-enhanced YUV420 image (uint8_t per pixel)

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `is_enable` | bool | Enable/disable CSE |
| `saturation_gain` | float | Gain applied to chroma deviation |
