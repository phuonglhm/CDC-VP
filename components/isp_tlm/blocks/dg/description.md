# Digital Gain (DG)

Applies digital gain to the RAW image, either in manual or auto mode. In auto mode, the gain is controlled by AE feedback. This is a per-pixel multiplication that amplifies or attenuates the signal.

**Pipeline position:** Fourth block (after LSC)

**Input:**  LSC-corrected RAW Bayer data (uint16_t per pixel)
**Output:** Gain-adjusted RAW Bayer data (uint16_t per pixel)

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `is_enable` | bool | Enable/disable DG |
| `is_auto` | bool | Auto gain mode |
| `current_gain` | uint16_t | Manual gain value |
| `ae_feedback` | int32_t | AE feedback value (auto mode) |
