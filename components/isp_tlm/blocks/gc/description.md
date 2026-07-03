# Gamma Correction (GC)

Applies gamma correction to the RGB image using a lookup table (LUT). Supports multiple bit depths (8, 10, 12, 14-bit). The LUT maps input values to gamma-corrected output values.

**Pipeline position:** After CCM

**Input:**  Color-corrected RGB image (uint16_t per pixel, 3 channels)
**Output:** Gamma-corrected RGB image (uint16_t per pixel, 3 channels)

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `is_enable` | bool | Enable/disable GC |
| `bit_depth` | uint8_t | Bit depth selecting which LUT to use |
| `gamma_lut_8` | vector<uint16_t> | 256-entry LUT for 8-bit |
| `gamma_lut_10` | vector<uint16_t> | 1024-entry LUT for 10-bit |
| `gamma_lut_12` | vector<uint16_t> | 4096-entry LUT for 12-bit |
| `gamma_lut_14` | vector<uint16_t> | 16384-entry LUT for 14-bit |
