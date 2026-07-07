# Color Correction Matrix (CCM)

Applies a 3×3 color correction matrix to the RGB image. The matrix transforms the camera RGB colorspace to a more accurate/desired colorspace. A default identity matrix is provided.

**Pipeline position:** After WB

**Input:**  White-balanced RGB image (uint16_t per pixel, 3 channels)
**Output:** Color-corrected RGB image (uint16_t per pixel, 3 channels)

## Parameters

| Name | Type | Description |
|------|------|-------------|
| `is_enable` | bool | Enable/disable CCM |
| `auto_compute` | bool | Compute CCM from input image automatically on each run (overrides `corrected_*`) |
| `bit_depth` | uint8_t | Bit depth for normalization |
| `corrected_red[3]` | float[3] | CCM row for red channel |
| `corrected_green[3]` | float[3] | CCM row for green channel |
| `corrected_blue[3]` | float[3] | CCM row for blue channel |

## Default matrix (identity)

```
| 1.0  0.0  0.0 |
| 0.0  1.0  0.0 |
| 0.0  0.0  1.0 |
```

## Auto-compute mode

When `auto_compute = true`, the block derives the CCM from the input image at runtime using the **gray-world assumption**:

1. Accumulate per-channel mean (R̄,Ḡ,B̄) over all non-saturated pixels.
2. Compute per-channel scale factors `sr = gray / R̄`, `sg = gray / Ḡ`, `sb = gray / B̄` where `gray = (R̄ + Ḡ + B̄) / 3`.
3. Build a 3×3 matrix with these diagonal scale factors and modest cross-channel corrections (strength `k = 0.20`).

The resulting matrix has:
- Row sums ≈ 1 — luminance is preserved.
- All off-diagonal terms ≤ 0.15 — no clipping regardless of input.
- Determinant ≈ 1 — full-rank, invertible.

Saturated pixels (all channels at `max_value`) are excluded from the statistics to prevent highlight clipping from biasing the correction.
