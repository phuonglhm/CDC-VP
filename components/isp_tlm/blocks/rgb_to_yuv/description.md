# RGB to YUV Color Space Conversion (CSC / rgb_to_yuv)

Converts RGB image data to YUV420 color space using BT.601 or BT.709 conversion standards. This is the point where the image transitions from RGB domain to YUV domain.

**Pipeline position:** After GC

**Input:**  Gamma-corrected RGB image (uint16_t per pixel, 3 channels)
**Output:** YUV420 image (uint8_t per pixel, Y + U + V planes)

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `conv_standard` | uint8_t | Conversion standard: 0=BT.601, 1=BT.709 |
| `bit_depth` | uint8_t | Input bit depth for normalization |

**BT.709 Coefficients (default):**
```
Y = (54 * R + 183 * G + 18 * B) >> 8
U = (-29 * R - 99 * G + 128 * B) >> 8
V = (128 * R - 116 * G - 12 * B) >> 8
```

**BT.601 Coefficients:**
```
Y = (77 * R + 150 * G + 29 * B) >> 8
U = (-43 * R - 84 * G + 128 * B) >> 8
V = (128 * R - 107 * G - 21 * B) >> 8
```

Luma offset = 0, Chroma offset = 128.
