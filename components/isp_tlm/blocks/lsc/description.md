# Lens Shading Correction (LSC)

Compensates for lens falloff (vignetting) which causes image brightness to decrease toward the edges. Uses a grid-based correction mesh stored in external LSC memory.

**Pipeline position:** Third block (after DPC)

**Input:**  Defect-corrected RAW Bayer data (uint16_t per pixel)
**Output:** LSC-corrected RAW Bayer data (uint16_t per pixel)

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `is_enable` | bool | Enable/disable LSC |
| `grid_width` | uint16_t | Grid mesh width |
| `grid_height` | uint16_t | Grid mesh height |

**External dependency:** LSC correction mesh stored in `lsc_mem_ptr`
