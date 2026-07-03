# Bayer Noise Reduction (BNR)

Applies adaptive noise reduction to the RAW Bayer image. Uses a bilateral-filter-like approach with separate standard deviation parameters for spatial (s) and range (r) dimensions, per color channel.

**Pipeline position:** Fifth block (after DG)

**Input:**  Gain-adjusted RAW Bayer data (uint16_t per pixel)
**Output:** Noise-reduced RAW Bayer data (uint16_t per pixel)

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `is_enable` | bool | Enable/disable BNR |
| `filter_window` | uint8_t | Filter window size |
| `r_std_dev_s` | float | Red channel spatial std dev |
| `r_std_dev_r` | float | Red channel range std dev |
| `g_std_dev_s` | float | Green channel spatial std dev |
| `g_std_dev_r` | float | Green channel range std dev |
| `b_std_dev_s` | float | Blue channel spatial std dev |
| `b_std_dev_r` | float | Blue channel range std dev |
