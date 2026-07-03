# Defective Pixel Correction (DPC)

Detects and corrects defective/hot/dead pixels in the RAW Bayer image by comparing each pixel with its neighbors. If a pixel deviates significantly from its surroundings, it is replaced with an interpolated value.

**Pipeline position:** Second block (after BLC)

**Input:**  Black-level-corrected RAW Bayer data (uint16_t per pixel)
**Output:** Defect-corrected RAW Bayer data (uint16_t per pixel)

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `is_enable` | bool | Enable/disable DPC |
| `dp_threshold` | uint16_t | Threshold for detecting defective pixels |
