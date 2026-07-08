# Gamma Correction (GC) Block

## Overview

The Gamma Correction block applies gamma correction to input pixel values using a pre-loaded Look-Up Table (LUT). The LUT maps input values to gamma-corrected output values.

## Configuration

| Parameter | Type | Description |
|----------|------|-------------|
| `is_enable` | bool | Enable/disable GC |
| `bit_depth` | uint8_t | Bit depth selecting which LUT to use (8, 10, 12, 14, 16) |

## LUT Tables

The GC block uses pre-loaded gamma correction LUT tables stored in `blocks/gc/gc_lut/lut.cpp`. Each bit depth has its own LUT:

| Bit Depth | LUT Size | Max Value |
|-----------|----------|-----------|
| 8 | 256 entries | 255 |
| 10 | 1024 entries | 1023 |
| 12 | 4096 entries | 4095 |
| 14 | 16384 entries | 16383 |
| 16 | 65536 entries | 65535 |

## Usage

```cpp
gc_config cfg;
cfg.is_enable = true;
cfg.bit_depth = 12;  // Use 12-bit LUT

gc_block gc;
gc.process(in.data(), out.data(), width, height, cfg);
```

## Notes

- The LUT table is automatically selected based on the `bit_depth` setting
- Input values are clamped to the valid range for the selected bit depth before LUT lookup
- When disabled, the block passes through input data unchanged
