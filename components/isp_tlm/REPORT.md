# ISP TLM Component - Development Report

## Date: July 3, 2026

---

## Summary

This report documents the work completed to fix the ISP TLM pipeline output issues and create proper tooling for running and visualizing the pipeline.

---

## Problem 1: Black/Solid Color Output

### Symptoms
- The Y plane of the output YUV420 file was all zeros or all 255
- The U and V planes showed flat values (constant 128)
- The visualization script showed a black or solid-color image

### Root Causes Identified

#### 1.1 Incorrect YUV Buffer Size

**File:** `src/isp_tlm.cpp`

**Problem:** The `yuv_buffer_` was allocated with the wrong size:
```cpp
// WRONG - RGB interleaved size
const std::size_t yuv_size = static_cast<std::size_t>(width_) * height_ * 3;
```

**Fix:** Changed to YUV420 correct size:
```cpp
// CORRECT - YUV420 size (Y + U + V = 1.5x)
const std::size_t yuv_size = static_cast<std::size_t>(width_) * height_ * 3 / 2;
```

#### 1.2 CSC Input Bit Depth Mismatch

**File:** `src/blocks/rgb_to_yuv.cpp`

**Problem:** The CSC (RGB to YUV conversion) expected 8-bit input but received 12-bit input from the previous pipeline stages. With 12-bit values (0-4095) passed through BT.709 coefficients:
```
Y = (54 * R + 183 * G + 18 * B) >> 8
```

For typical 12-bit values (~700-1400), this produces:
- Y values that saturate to 255 (clipping)
- U/V values that are also incorrect

**Fix:** Added 12-bit to 8-bit normalization before applying CSC coefficients:
```cpp
// Normalize 12-bit input to 8-bit range [0, 255]
// Shift right by 4 bits (4096 -> 256)
const std::int32_t r = r_raw >> 4;
const std::int32_t g = g_raw >> 4;
const std::int32_t b = b_raw >> 4;
```

---

## Problem 2: Buffer Allocation Before File Read

### Symptoms
- File read returned 0 bytes
- Output file was all zeros

### Root Cause

**File:** `tools/isp_run.cpp` and `tests/test_isp_tlm.cpp`

**Problem:** The `raw_buffer_` in `isp_tlm` is only allocated when `trigger_processing()` is called. However, the file read was attempted before triggering, and `get_raw_buffer()` returned a pointer to an empty vector.

**Fix:** Added `allocate_buffers()` method to `isp_tlm` class:

```cpp
// In isp_tlm.h
void allocate_buffers();

// In isp_tlm.cpp
void isp_tlm::allocate_buffers()
{
    const std::size_t raw_size = static_cast<std::size_t>(width_) * height_;
    const std::size_t yuv_size = static_cast<std::size_t>(width_) * height_ * 3 / 2;
    raw_buffer_.resize(raw_size);
    yuv_buffer_.resize(yuv_size);
}
```

---

## Feature Additions

### 1. Standalone Pipeline Runner (`isp_run`)

**File:** `tools/isp_run.cpp`

Created a new standalone tool that:
- Accepts CLI arguments for input file, output file, dimensions, bit depth, bayer pattern
- Enables all ISP processing blocks with sensible defaults
- Writes proper YUV420 output
- Generates metadata JSON sidecar file
- Prints `ffplay` command for easy viewing

**CMake Integration:** Added `tools/CMakeLists.txt` and updated main `CMakeLists.txt`

### 2. Metadata Sidecar

**Files:** `tools/isp_run.cpp` (writer), `visualize.py` (reader)

Each run generates a JSON file with:
```json
{
  "width": 2592,
  "height": 1536,
  "format": "yuv420p",
  "bit_depth": 8,
  "plane_order": ["Y", "U", "V"],
  "chroma_subsampling": "4:2:0",
  "conv_standard": "BT.709",
  "source_raw": "ColorChecker_2592x1536_12bits_RGGB.raw"
}
```

### 3. Updated Visualization Script

**File:** `visualize.py`

Rewrote to:
- Read metadata JSON for dimensions (no hardcoded values)
- Look for `output.yuv` in the canonical location
- Provide helpful error messages when files are missing
- Show output statistics
- Generate both standalone JPEG and comparison image

---

## Files Modified

### Source Files

| File | Change |
|------|--------|
| `src/isp_tlm.cpp` | Fixed YUV buffer size; added `allocate_buffers()` method |
| `src/blocks/rgb_to_yuv.cpp` | Added 12-bit to 8-bit normalization in CSC |
| `src/isp_pipeline.cpp` | Added `#include <iostream>` for debug output (later removed) |

### Header Files

| File | Change |
|------|--------|
| `include/isp_tlm.h` | Added `allocate_buffers()` declaration; added getter methods |

### Test Files

| File | Change |
|------|--------|
| `tests/test_isp_tlm.cpp` | Added `allocate_buffers()` call before file read |

### Tooling Files

| File | Change |
|------|--------|
| `tools/isp_run.cpp` | New standalone runner tool |
| `tools/CMakeLists.txt` | New CMake file for isp_run |
| `visualize.py` | Rewritten to use metadata JSON |

### Build Files

| File | Change |
|------|--------|
| `CMakeLists.txt` | Added `add_subdirectory(tools)` |

### Documentation

| File | Change |
|------|--------|
| `README.md` | Added detailed build/run instructions |

---

## Verification

After fixes, the output shows proper values:
```
Y: min=15, max=131, mean=60.5
U: min=100, max=141, mean=117.7
V: min=88, max=139, mean=116.7
```

This indicates a properly functioning pipeline with meaningful image content.

---

## Usage

### Quick Start

```bash
# Configure
cmake -S . -B build/bremen -G Ninja \
  -DCMAKE_C_COMPILER=/usr/bin/gcc \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DCDC_BUILD_TESTS=ON \
  -DSYSTEMC_INCLUDE_DIR=/opt/systemc-2.3.4/include \
  -DSYSTEMC_LIBRARY=/opt/systemc-2.3.4/lib/libsystemc.so

# Build
cmake --build build/bremen --target isp_run

# Run pipeline
./build/bremen/components/isp_tlm/tests/isp_run \
  -i ./components/isp_tlm/input/ColorChecker_2592x1536_12bits_RGGB.raw \
  -o output.yuv -w 2592 --height 1536 -b 16 -p 2

# View
ffplay -f rawvideo -pixel_format yuv420p -video_size 2592x1536 output.yuv

# Visualize
python3 visualize.py
```

---

## Notes

- OSD (On-Screen Display) block exists in the codebase but is NOT integrated into the pipeline
- RGBC block was previously removed from the codebase
- All unit tests pass after the fixes
