# CDC-VP SystemC ISP Implementation Plan

## 1. Project Overview

### 1.1 Objective
Implement a cycle-accurate SystemC TLM model of the Infinite-ISP pipeline, matching the RTL architecture from `/home/hoangquan/workspace/ISP_stuff/Infinite-ISP_RTL`. Each block must:
- Be functionally equivalent to its RTL counterpart
- Include cycle-accurate timing (pipeline latency matching RTL)
- Support comprehensive metrics collection for performance analysis
- Maintain TLM compatibility with TLM-2.0 sockets for integration

### 1.2 Reference Architecture
- **RTL Source**: `/home/hoangquan/workspace/ISP_stuff/Infinite-ISP_RTL/hw/rtl/isp/`
- **Top Module**: `isp_top.v` - instantiates all ISP blocks in order
- **AXI Wrapper**: `infinite_isp_AXI_wrapper.v` - AXI4-Lite register interface

### 1.3 Architecture Documentation

**Documentation Location**: `/home/hoangquan/workspace/ISP_stuff/Infinite-ISP_RTL/doc/`

| Document | Description |
|----------|-------------|
| `Infinite-ISP_RTLArchitecture_v1.0.pdf` | Architecture specification v1.0 (179KB) |
| `Infinite-ISP_RTLArchitecture_v1.1.pdf` | Architecture specification v1.1 (227KB) - **Latest** |
| `user/rtl_user_guide.md` | RTL user guide for simulation setup |
| `CONTRIBUTIONS.md` | Contribution guidelines |
| `assets/Infinite-ISP_v1.0-pipeline.png` | Pipeline diagram v1.0 |
| `assets/Infinite-ISP_v1.1-pipeline.png` | Pipeline diagram v1.1 |
| `assets/Infinite-ISP_Repo_Flow.png` | Repository flow diagram |

**Pipeline Diagram v1.1:**
![Pipeline v1.1](docs/Infinite-ISP_v1.1-pipeline.png)

**Repository Flow:**
![Repo Flow](docs/Infinite-ISP_Repo_Flow.png)

### 1.4 ISP Pipeline Overview

The Infinite-ISP is a comprehensive image signal processing pipeline that converts RAW Bayer sensor data to processed YUV output. The pipeline consists of three main sections:

#### Section 1: RAW Processing (Bayer Domain)
- **Input**: RAW Bayer pattern (10-bit, RGGB/GRBG/GBRG/BGGR)
- **Blocks**: CROP → DPC → BLC → OECF → DGain → LSC → BNR
- **Characteristics**: Single-channel processing, pixel-level operations

#### Section 2: Color Processing (RGB Domain)
- **Input**: Corrected RAW
- **Blocks**: WB → DEMOSAIC (CFA) → CCM → GAMMA
- **Characteristics**: Multi-channel (R/G/B), color space conversion

#### Section 3: YUV Processing & Output
- **Input**: RGB
- **Blocks**: CSC → LDCI → SHARP → 2DNR
- **Output**: YUV422 format
- **Statistics**: AWB (statistics), AE (statistics) - feedback loops

### 1.5 Key Design Principles (from Architecture Documentation)

1. **Modular Architecture**: Each block is independent and can be enabled/disabled
2. **Configurable Parameters**: All block parameters are tunable via AXI register interface
3. **Fixed-Point Arithmetic**: All algorithms use quantized fixed-point math matching RTL
4. **Pipeline Processing**: Continuous streaming with configurable delays
5. **Statistics Gathering**: AWB and AE use frame-based statistics for feedback

### 1.6 RTL Coding Standards (from CONTRIBUTIONS.md)

- Module naming: `isp_<block_name>.v` for ISP blocks
- File naming: **lowercase** for better compatibility
- Port/parameter patterns must match existing modules
- Helper modules in `isp_utils.v`
- Variables: lowercase with descriptive names (e.g., `start_index`, `red_channel`)
- Comments: English only, complete sentences with period
- Box diagram and microarchitecture diagram required for new modules

### 1.7 Pipeline Order (as per RTL)

```
Input RAW (Bayer)
    │
    ├── CROP ────────────────────────────────────┐
    │                                             │
    ├── DPC ──────────────────────────┐          │
    │                                 │          │
    ├── BLC ─────────────────────┐    │          │
    │                             │    │          │
    ├── OECF ───────────┐        │    │          │
    │                   │        │    │          │
    ├── DGain ──┐       │        │    │          │
    │           │       │        │    │          │
    ├── LSC ────┘       │        │    │          │
    │                   │        │    │          │
    ├── BNR ──────────────────────────│──────────┤
    │                                 │          │
    ├── AWB (statistics only) ─────────┼──────────┤
    │                                 │          │
    ├── WB ────────────────────────────┼──────────┤
    │                                 │          │
    ├── DEMOSAIC (CFA) ───────────────┼──────────┤
    │                                 │          │
    ├── CCM ───────────────────────────┼──────────┤
    │                                 │          │
    ├── GAMMA ─────────────────────────┼──────────┤
    │                                 │          │
    ├── AE (statistics only) ──────────┼──────────┤
    │                                 │          │
    ├── CSC (RGB→YUV) ────────────────┼──────────┤
    │                                 │          │
    ├── LDCI (passthrough) ────────────┼──────────┤
    │                                 │          │
    ├── SHARP ──────────────────────────┼──────────┤
    │                                 │          │
    └── 2DNR ──────────────────────────┼──────────┘
                                       │
                                Output YUV
```

---

## 2. Current Implementation Status

### 2.1 Completed Blocks ✓

| Block | Status | RTL Reference | Latency |
|-------|--------|---------------|---------|
| **BLC** | ✅ Done | `isp_blc.v` | 0 cycles (combinational) |
| **CROP** | ✅ Done | `isp_crop.v` | 0 cycles (combinational) |
| **WB** | ✅ Done | `isp_wb.v` | 3 cycles |
| **CCM** | ✅ Done | `isp_ccm.v` | 4 cycles |
| **CSC** | ✅ Done | `isp_csc.v` | 9 cycles |
| **DEMOSAIC** | ✅ Done | `isp_demosaic.v` | 9 cycles |
| **GAMMA** | ✅ Done | `isp_gamma.v` | 2 cycles |
| **SHARP** | ✅ Done | `isp_sharpen.v` | 15 cycles |
| **DPC** | ✅ Done | `isp_dpc.v` | 10 cycles |
| **BNR** | ✅ Done | `isp_bnr.v` | ~50 cycles |
| **2DNR** | ✅ Done | `isp_2dnr.v` | ~30 cycles |
| **AWB** | ✅ Done | `isp_awb.v` | Statistics only |
| **AE** | ✅ Done | `isp_ae.v` | Statistics only |
| **OECF** | ✅ Done | `isp_oecf.v` | 1 cycle |
| **DGAIN** | ✅ Done | `isp_dgain.v` | 2 cycles |
| **LSC** | ✅ Done | `isp_lsc.v` | 0 cycles (passthrough) |
| **LDCI** | ✅ Done | `isp_ldci.v` | 0 cycles (passthrough) |

### 2.2 Block Architecture Pattern

Each block follows this template (see `src/blocks/blc/isp_blc.h`):

```cpp
template<unsigned BITS>
class isp_block_name : public sc_module {
public:
    // Clock and reset
    sc_in<bool> pclk{"pclk"};
    sc_in<bool> rst_n{"rst_n"};
    sc_in<bool> enable{"enable"};
    
    // Video input
    sc_in<bool> i_href{"i_href"};
    sc_in<bool> i_vsync{"i_vsync"};
    sc_in<uint16_t> i_data{"i_data"};
    
    // Video output
    sc_out<bool> o_href{"o_href"};
    sc_out<bool> o_vsync{"o_vsync"};
    sc_out<uint16_t> o_data{"o_data"};
    
    // Block-specific parameters (matching RTL ports)
    // ...
    
    // Constructor
    isp_block_name(const sc_module_name& name);
    
    // Metrics access
    BlockMetricsCollector& get_metrics();
    void set_image_size(unsigned w, unsigned h);
};
```

---

## 3. Block Implementation Specifications

### 3.1 CROP (`isp_crop`)

**RTL Reference**: `/hw/rtl/isp/crop/isp_crop.v`

**Parameters**:
- `BITS`: pixel bit width (default: 10)
- `WIDTH`: sensor width (default: 2048)
- `HEIGHT`: sensor height (default: 1536)

**Ports** (matching RTL):
| Port | Direction | Width | Description |
|------|-----------|-------|-------------|
| `crop_w` | in | 12-bit | Output width |
| `crop_h` | in | 12-bit | Output height |
| `in_href` | in | 1-bit | Input horizontal valid |
| `in_vsync` | in | 1-bit | Input vertical sync |
| `in_data` | in | BITS | Input pixel data |
| `out_href` | out | 1-bit | Output horizontal valid |
| `out_vsync` | out | 1-bit | Output vertical sync |
| `out_data` | out | BITS | Output pixel data |

**Algorithm**:
1. Track pixel count within line using `href` transitions
2. Track line count within frame using `vsync` fall edges
3. Compute crop boundaries: `crop_x = (WIDTH - crop_w) / 2`, `crop_y = (HEIGHT - crop_h) / 2`
4. Output valid when: `pix_cnt >= crop_x && pix_cnt < crop_x + crop_w && line_cnt >= crop_y && line_cnt < crop_y + crop_h`
5. Bayer pattern integrity: width/height adjustments must be multiples of 4

**Pipeline Latency**: 0 cycles (passthrough with gating)

**Metrics to Collect**:
- `input_pixels`, `output_pixels`, `cropped_pixels`
- `active_cycles`, `stall_cycles`

---

### 3.2 DPC (`isp_dpc`)

**RTL Reference**: `/hw/rtl/isp/dpc/isp_dpc.v`

**Parameters**:
- `BITS`: pixel bit width (default: 10)
- `WIDTH`: image width
- `HEIGHT`: image height
- `BAYER`: Bayer pattern (0=RGGB, 1=GRBG, 2=GBRG, 3=BGGR)

**Ports** (matching RTL):
| Port | Direction | Width | Description |
|------|-----------|-------|-------------|
| `threshold` | in | BITS | DPC threshold |
| `in_href` | in | 1-bit | Input horizontal valid |
| `in_vsync` | in | 1-bit | Input vertical sync |
| `in_raw` | in | BITS | Input raw pixel |
| `out_href` | out | 1-bit | Output horizontal valid |
| `out_vsync` | out | 1-bit | Output vertical sync |
| `out_raw` | out | BITS | Output corrected pixel |

**Algorithm** (Gradient-Based DPC):
1. Build 5×5 window using line buffer (shift register)
2. Compute 4 directional gradients:
   - `vertical_grad = 2*p5 - p2 - p8`
   - `horizontal_grad = 2*p5 - p4 - p6`
   - `left_diag_grad = 2*p5 - p1 - p9`
   - `right_diag_grad = 2*p5 - p3 - p7`
3. Select minimum gradient direction
4. Defect detection:
   - `below_min = p5 < all(neighbors)`
   - `above_max = p5 > all(neighbors)`
   - `diff > threshold` for all 8 neighbors
5. Replace with gradient-based interpolation

**Pipeline Latency**: 10 cycles (RTL `DLY_CLK = 10`)

**Metrics to Collect**:
- `corrected_pixels`, `hot_pixels`, `dead_pixels`
- `gradient_directions` (v/h/l/r distribution)

---

### 3.3 OECF (`isp_oecf`)

**RTL Reference**: `/hw/rtl/isp/oecf/isp_oecf.v`

**Parameters**:
- `BITS`: pixel bit width
- `WIDTH`, `HEIGHT`: dimensions
- `BAYER`: Bayer pattern
- `R_LUT_INIT`, `GR_LUT_INIT`, `GB_LUT_INIT`, `B_LUT_INIT`: LUT files

**Ports** (matching RTL):
| Port | Direction | Width | Description |
|------|-----------|-------|-------------|
| `r_table_*` | in | various | R channel LUT interface |
| `gr_table_*` | in | various | Gr channel LUT interface |
| `gb_table_*` | in | various | Gb channel LUT interface |
| `b_table_*` | in | various | B channel LUT interface |
| `in_href` | in | 1-bit | Input horizontal valid |
| `in_vsync` | in | 1-bit | Input vertical sync |
| `in_raw` | in | BITS | Input raw pixel |
| `out_href` | out | 1-bit | Output horizontal valid |
| `out_vsync` | out | 1-bit | Output vertical sync |
| `out_raw` | out | BITS | Output corrected pixel |

**Algorithm**:
1. Detect Bayer channel (R/Gr/Gb/B) from pixel position
2. Index LUT with input value
3. Output corrected value

**Pipeline Latency**: 1 cycle (RAM access)

**Metrics to Collect**:
- `lut_reads`, `channel_distribution`

---

### 3.4 DGAIN (`isp_dgain`)

**RTL Reference**: `/hw/rtl/isp/dg/isp_dgain.v`

**Parameters**:
- `BITS`: pixel bit width
- `WIDTH`, `HEIGHT`: dimensions
- `DGAIN_ARRAY_SIZE`: number of gain entries (100)
- `DGAIN_ARRAY_BITS`: bits for index (7)

**Ports** (matching RTL):
| Port | Direction | Width | Description |
|------|-----------|-------|-------------|
| `isManual` | in | 1-bit | Manual/Auto mode select |
| `manual_index` | in | 7-bit | Manual gain index |
| `ae_feedback_index` | in | 7-bit | AE feedback index |
| `dgain_array` | in | 800-bit | Array of 100 × 8-bit gains |
| `applied_index` | out | 7-bit | Currently applied index |
| `in_href` | in | 1-bit | Input valid |
| `in_vsync` | in | 1-bit | Input vsync |
| `in_raw` | in | BITS | Input pixel |
| `out_href` | out | 1-bit | Output valid |
| `out_vsync` | out | 1-bit | Output vsync |
| `out_raw` | out | BITS | Output pixel |

**Algorithm**:
1. Select gain index based on mode (manual/auto)
2. Look up gain value from array
3. Multiply pixel: `out = in * gain`
4. Clip to BITS range

**Pipeline Latency**: 2 cycles (RTL `DLY_CLK = 2`)

**Metrics to Collect**:
- `gain_index_distribution`, `multiplied_pixels`, `saturation_clips`

---

### 3.5 LSC (`isp_lsc`)

**RTL Reference**: `/hw/rtl/isp/` (not yet implemented in RTL - placeholder)

**Status**: Placeholder/pass-through only (as per RTL)

**Pipeline Latency**: 0 cycles

---

### 3.6 BNR (`isp_bnr`)

**RTL Reference**: `/hw/rtl/isp/bnr/isp_bnr.v`

**Parameters**:
- `BITS`: pixel bit width
- `WIDTH`, `HEIGHT`: dimensions
- `BAYER`: Bayer pattern
- `WEIGHT_BITS`: kernel weight bits (default: 8)

**Ports** (matching RTL):
| Port | Direction | Width | Description |
|------|-----------|-------|-------------|
| `space_kernel_r/gr/gb` | in | 5×5×WEIGHT_BITS | Spatial kernel per channel |
| `color_curve_x_r/g/b` | in | 9×BITS | Color curve X coordinates |
| `color_curve_y_r/g/b` | in | 9×WEIGHT_BITS | Color curve Y coordinates |
| `in_href` | in | 1-bit | Input valid |
| `in_vsync` | in | 1-bit | Input vsync |
| `in_raw` | in | BITS | Input pixel |
| `out_href` | out | 1-bit | Output valid |
| `out_vsync` | out | 1-bit | Output vsync |
| `out_raw` | out | BITS | Output pixel |

**Algorithm** (Joint Bilateral Filter):
1. Green interpolation first (see `isp_greenIntrp`)
2. Apply JBF on interpolated green
3. 5×5 spatial kernel + range kernel from color curve

**Pipeline Latency**: ~50 cycles (complex filter)

**Sub-modules Required**:
- `isp_greenIntrp`: Green channel interpolation
- `isp_jbf`: Joint Bilateral Filter

**Metrics to Collect**:
- `filtered_pixels`, `kernel_applications`, `interpolated_green_pixels`

---

### 3.7 AWB (`isp_awb`)

**RTL Reference**: `/hw/rtl/isp/awb/isp_awb.v`

**Parameters**:
- `BITS`: pixel bit width
- `WIDTH`, `HEIGHT`: dimensions
- `BAYER`: Bayer pattern
- `CROP_LEFT/RIGHT/TOP/BOTTOM`: crop margins

**Ports** (matching RTL):
| Port | Direction | Width | Description |
|------|-----------|-------|-------------|
| `underexposed_limit` | in | BITS | Min value for valid pixels |
| `overexposed_limit` | in | BITS | Max value for valid pixels |
| `frames` | in | BITS | Number of frames to average |
| `out_r_gain` | out | 12-bit | Computed R gain |
| `out_b_gain` | out | 12-bit | Computed B gain |
| `in_href` | in | 1-bit | Input valid |
| `in_vsync` | in | 1-bit | Input vsync |
| `in_raw` | in | BITS | Input pixel |

**Algorithm**:
1. Crop valid region (excluding edges)
2. Filter by exposure limits
3. Compute mean R, G, B per frame
4. Calculate gains: `R_gain = mean_G / mean_R`, `B_gain = mean_G / mean_B`
5. Average across configured number of frames

**Output**: Statistics only (no pixel processing)

**Metrics to Collect**:
- `valid_pixels`, ` overexposed_count`, `underexposed_count`, `computed_gains`

---

### 3.8 WB (`isp_wb`)

**RTL Reference**: `/hw/rtl/isp/wb/isp_wb.v`

**Parameters**:
- `BITS`: pixel bit width
- `WIDTH`, `HEIGHT`: dimensions
- `BAYER`: Bayer pattern

**Ports** (matching RTL):
| Port | Direction | Width | Description |
|------|-----------|-------|-------------|
| `gain_r` | in | 12-bit | R channel gain |
| `gain_b` | in | 12-bit | B channel gain |
| `in_href` | in | 1-bit | Input valid |
| `in_vsync` | in | 1-bit | Input vsync |
| `in_raw` | in | BITS | Input pixel |
| `out_href` | out | 1-bit | Output valid |
| `out_vsync` | out | 1-bit | Output vsync |
| `out_raw` | out | BITS | Output corrected pixel |

**Algorithm**:
1. Detect Bayer channel from position
2. Apply gain:
   - R channel: `out = in * gain_r`
   - Gr/Gb channels: pass through (gain = 1.0)
   - B channel: `out = in * gain_b`
3. Clip to BITS range

**Pipeline Latency**: 3 cycles (RTL `DLY_CLK = 3`)

**Metrics to Collect**:
- `r_gain_applications`, `b_gain_applications`, `clipped_pixels`

---

### 3.9 DEMOSAIC (`isp_demosaic`)

**RTL Reference**: `/hw/rtl/isp/cfa/isp_demosaic.v`

**Parameters**:
- `BITS`: pixel bit width
- `WIDTH`, `HEIGHT`: dimensions
- `BAYER`: Bayer pattern

**Ports** (matching RTL):
| Port | Direction | Width | Description |
|------|-----------|-------|-------------|
| `in_href` | in | 1-bit | Input valid |
| `in_vsync` | in | 1-bit | Input vsync |
| `in_raw` | in | BITS | Input RAW pixel |
| `out_href` | out | 1-bit | Output valid |
| `out_vsync` | out | 1-bit | Output vsync |
| `out_r` | out | BITS | Output R |
| `out_g` | out | BITS | Output G |
| `out_b` | out | BITS | Output B |

**Algorithm** (Bilinear CFA Interpolation):
1. Build 5×5 window with line buffer
2. Red extraction: filter based on pattern
3. Green extraction: bilinear interpolation
4. Blue extraction: filter based on pattern
5. Handle all 4 Bayer patterns (RGGB, GRBG, GBRG, BGGR)

**Pipeline Latency**: 9 cycles (RTL `DLY_CLK = 9`)

**Metrics to Collect**:
- `r_pixels`, `g_pixels`, `b_pixels`, `edge_pixels`

---

### 3.10 CCM (`isp_ccm`)

**RTL Reference**: `/hw/rtl/isp/ccm/isp_ccm.v`

**Parameters**:
- `BITS`: pixel bit width
- `WIDTH`, `HEIGHT`: dimensions

**Ports** (matching RTL):
| Port | Direction | Width | Description |
|------|-----------|-------|-------------|
| `m_rr/m_rg/m_rb` | in | 16-bit (S8.8) | CCM row 1 |
| `m_gr/m_gg/m_gb` | in | 16-bit (S8.8) | CCM row 2 |
| `m_br/m_bg/m_bb` | in | 16-bit (S8.8) | CCM row 3 |
| `in_href` | in | 1-bit | Input valid |
| `in_vsync` | in | 1-bit | Input vsync |
| `in_r/g/b` | in | BITS | Input RGB |
| `out_href` | out | 1-bit | Output valid |
| `out_vsync` | out | 1-bit | Output vsync |
| `out_r/g/b` | out | BITS | Output RGB |

**Algorithm**:
```
Rout = (Mrr*Rin + Mrg*Gin + Mrb*Bin) >> 10
Gout = (Mgr*Rin + Mgg*Gin + Mgb*Bin) >> 10
Bout = (Mbr*Rin + Mbg*Gin + Mbb*Bin) >> 10
```
**Pipeline Latency**: 4 cycles (RTL `DLY_CLK = 4`)

**Metrics to Collect**:
- `ccm_multiplications`, `clipped_pixels`, `saturation_rate`

---

### 3.11 GAMMA (`isp_gamma`)

**RTL Reference**: `/hw/rtl/isp/gamma/isp_gamma.v`

**Parameters**:
- `BITS`: pixel bit width
- `WIDTH`, `HEIGHT`: dimensions
- `R_LUT_INIT`, `G_LUT_INIT`, `B_LUT_INIT`: LUT files

**Ports** (matching RTL):
| Port | Direction | Width | Description |
|------|-----------|-------|-------------|
| `table_*_clk/wen/ren/addr/wdata/rdata` | in/out | various | LUT interface |
| `in_href` | in | 1-bit | Input valid |
| `in_vsync` | in | 1-bit | Input vsync |
| `in_data_r/g/b` | in | BITS | Input channels |
| `out_href` | out | 1-bit | Output valid |
| `out_vsync` | out | 1-bit | Output vsync |
| `out_data_r/g/b` | out | BITS | Output channels |

**Algorithm**:
1. LUT lookup for each channel
2. Dual-port RAM: config port + processing port

**Pipeline Latency**: 2 cycles (RTL `href_dly[1]`)

**Metrics to Collect**:
- `lut_lookups`, `input_distribution`, `output_distribution`

---

### 3.12 AE (`isp_ae`)

**RTL Reference**: `/hw/rtl/isp/ae/isp_ae.v`

**Parameters**:
- `BITS`: pixel bit width
- `WIDTH`, `HEIGHT`: dimensions

**Ports** (matching RTL):
| Port | Direction | Width | Description |
|------|-----------|-------|-------------|
| `center_illuminance` | in | 8-bit | Target brightness |
| `skewness` | in | 16-bit | Target skewness |
| `ae_crop_*` | in | 12-bit | Crop window |
| `ae_response` | out | 2-bit | Exposure adjustment |
| `ae_result_skewness` | out | 16-bit | Computed skewness |
| `ae_done` | out | 1-bit | Calculation done |

**Algorithm**:
1. Crop input region
2. Compute statistics (sum, sum², sum³)
3. Calculate moments
4. Compute skewness
5. Adjust exposure response

**Output**: Statistics only

**Metrics to Collect**:
- `ae_iterations`, `exposure_adjustments`, `skewness_values`

---

### 3.13 CSC (`isp_csc`)

**RTL Reference**: `/hw/rtl/isp/csc/isp_csc.v`

**Parameters**:
- `BITS`: pixel bit width
- `WIDTH`, `HEIGHT`: dimensions

**Ports** (matching RTL):
| Port | Direction | Width | Description |
|------|-----------|-------|-------------|
| `in_conv_standard` | in | 2-bit | Conversion standard (BT.601/BT.709) |
| `in_href` | in | 1-bit | Input valid |
| `in_vsync` | in | 1-bit | Input vsync |
| `in_r/g/b` | in | BITS | Input RGB |
| `out_href` | out | 1-bit | Output valid |
| `out_vsync` | out | 1-bit | Output vsync |
| `out_y/u/v` | out | 8-bit | Output YUV |

**Algorithm** (BT.601):
```
Y  = (77*R + 150*G + 29*B) >> 8
U  = (-43*R - 85*G + 128*B + 32768) >> 8
V  = (128*R - 107*G - 21*B + 32768) >> 8
```

**Pipeline Latency**: 9 cycles (RTL `DLY_CLK = 9`)

**Metrics to Collect**:
- `rgb_to_yuv_conversions`, `y_u_v_pixels`

---

### 3.14 LDCI (`isp_ldci`)

**RTL Reference**: `/hw/rtl/isp/` (not implemented - passthrough)

**Status**: Placeholder/pass-through only

**Pipeline Latency**: 0 cycles

---

### 3.15 SHARP (`isp_sharpen`)

**RTL Reference**: `/hw/rtl/isp/sharpen/isp_sharpen.v`

**Parameters**:
- `BITS`: pixel bit width (8 for YUV)
- `WIDTH`, `HEIGHT`: dimensions
- `SHARP_WEIGHT_BITS`: kernel weight bits (20)

**Ports** (matching RTL):
| Port | Direction | Width | Description |
|------|-----------|-------|-------------|
| `luma_kernel` | in | 9×9×20 bits | 9×9 sharpening kernel |
| `sharpen_strength` | in | 12-bit | Sharpen strength |
| `in_href` | in | 1-bit | Input valid |
| `in_vsync` | in | 1-bit | Input vsync |
| `in_data_y/u/v` | in | 8-bit | Input YUV |
| `out_href` | out | 1-bit | Output valid |
| `out_vsync` | out | 1-bit | Output vsync |
| `out_data_y/u/v` | out | 8-bit | Output YUV |

**Algorithm** (Unsharp Masking):
1. 9×9 convolution on Y channel
2. Subtract smoothed from original
3. Scale by strength
4. Add back to original
5. Clip to 0-255

**Pipeline Latency**: 15 cycles (RTL `DLY_CLK = 15`)

**Metrics to Collect**:
- `sharpened_pixels`, `kernel_applications`, `edge_enhancement`

---

### 3.16 2DNR (`isp_2dnr`)

**RTL Reference**: `/hw/rtl/isp/2dnr/isp_2dnr.v`

**Parameters**:
- `BITS`: pixel bit width (8 for YUV)
- `WIDTH`, `HEIGHT`: dimensions
- `WEIGHT_BITS`: weight bits (5)
- `LUT_SIZE`: LUT size (15)

**Ports** (matching RTL):
| Port | Direction | Width | Description |
|------|-----------|-------|-------------|
| `diff` | in | 32×8 bits | Difference LUT |
| `weight` | in | 32×WEIGHT_BITS | Weight LUT |
| `in_href` | in | 1-bit | Input valid |
| `in_vsync` | in | 1-bit | Input vsync |
| `in_y/u/v` | in | 8-bit | Input YUV |
| `out_href` | out | 1-bit | Output valid |
| `out_vsync` | out | 1-bit | Output vsync |
| `out_y/u/v` | out | 8-bit | Output YUV |

**Algorithm**:
1. 9×9 window on Y channel
2. Calculate difference with center
3. Assign weights based on difference LUT
4. Weighted average filter
5. Pass U/V through with delay

**Pipeline Latency**: 14+BITS+WEIGHT_BITS+7 cycles

**Metrics to Collect**:
- `filtered_pixels`, `weight_distributions`, `noise_reduction`

---

## 4. Metrics Framework

### 4.1 Base Metrics Interface

```cpp
// Located at: src/metrics/isp_metrics_base.h
class IspMetricsBase {
public:
    virtual void reset() = 0;
    virtual void collect_from(const sc_module& module) = 0;
    
    // Timing metrics
    virtual uint64_t get_total_cycles() const = 0;
    virtual uint64_t get_active_cycles() const = 0;
    virtual uint64_t get_stall_cycles() const = 0;
    virtual double get_utilization() const = 0;
    
    // Throughput metrics
    virtual double get_throughput_fps(double clock_hz) const = 0;
    virtual double get_max_throughput_fps(double clock_hz, unsigned pixels) const = 0;
};
```

### 4.2 Block-Specific Metrics

Each block has specialized metrics inheriting from `IspMetricsBase`:

```cpp
class BlcMetricsCollector : public IspMetricsBase {
    // BLC-specific counters
    uint64_t m_r_corrections, m_gr_corrections, m_gb_corrections, m_b_corrections;
    uint64_t m_saturation_clips;
    uint64_t m_linear_corrections, m_nonlinear_corrections;
    
    // Accessors
    unsigned get_channel_corrections(unsigned ch) const;
    double get_saturation_clip_rate() const;
    uint16_t get_max_value_seen() const;
};
```

---

## 5. Pipeline Integration

### 5.1 Top Module Structure

```cpp
// src/pipeline/isp_top.h
class isp_top : public sc_module {
public:
    // Input/Output ports matching RTL
    sc_in<bool> pclk, rst_n;
    
    // RAW input
    sc_in<bool> i_href, i_vsync;
    sc_in<uint16_t> i_data;
    
    // Processed output (YUV)
    sc_out<bool> o_href, o_vsync;
    sc_out<uint8_t> o_y, o_u, o_v;
    
    // Configuration
    sc_in<IspConfig> i_config;
    
    // Statistics outputs
    sc_out<uint16_t> o_awb_r_gain, o_awb_b_gain;
    sc_out<uint16_t> o_ae_response;
    
    // Methods
    void set_image_size(unsigned w, unsigned h);
    void get_all_metrics(IspPipelineMetrics& metrics);
};
```

### 5.2 Configuration Structure

```cpp
// src/pipeline/isp_config.h
struct IspBlockConfig {
    bool enable;
    // Block-specific parameters...
};

struct IspConfig {
    // Image dimensions
    unsigned sensor_width, sensor_height;
    unsigned crop_width, crop_height;
    BayerPattern bayer;
    
    // Block enables
    bool crop_en, dpc_en, blc_en, oecf_en, dgain_en, lsc_en,
         bnr_en, wb_en, demosic_en, ccm_en, gamma_en, csc_en,
         sharpen_en, ldci_en, nr2d_en, awb_en, ae_en;
    
    // Block parameters
    DpcConfig dpc;
    BlcConfig blc;
    // ... other blocks
};
```

---

## 6. Testbench Structure

### 6.1 Testbench Template

```cpp
// tb/<block>_tb.cpp
class BlockTestbench : public sc_module {
public:
    sc_port<blocking_in_if<PixelPacket>> in_port;
    sc_port<blocking_out_if<PixelPacket>> out_port;
    
    void run_test(const BlockConfig& config);
    
private:
    void send_frame();
    void receive_and_verify();
    void print_metrics(const BlockMetrics& metrics);
};
```

### 6.2 Pipeline Testbench

```cpp
// tb/isp_pipeline_tb.cpp
class IspPipelineTestbench : public sc_module {
public:
    sc_port<tlm_initiator_socket<>> initiator;
    
    // Test methods
    void test_single_frame(const IspConfig& config);
    void test_multi_frame(const IspConfig& config, unsigned num_frames);
    void test_config_changes();
    void benchmark_throughput();
    
    // Metrics collection
    void print_pipeline_metrics();
};
```

---

## 7. Implementation Order

### Phase 1: Basic Pipeline Blocks (Pure pixel processing)

| Priority | Block | Estimated LOC | Dependencies |
|----------|-------|--------------|--------------|
| 1 | CROP | ~150 | None |
| 2 | WB | ~200 | None |
| 3 | CCM | ~300 | None |
| 4 | CSC | ~400 | None |

### Phase 2: Window-Based Blocks (Require line buffer)

| Priority | Block | Estimated LOC | Dependencies |
|----------|-------|--------------|--------------|
| 5 | DEMOSAIC | ~800 | CROP |
| 6 | GAMMA | ~250 | CROP |
| 7 | SHARP | ~600 | CROP |

### Phase 3: Complex Filter Blocks

| Priority | Block | Estimated LOC | Dependencies |
|----------|-------|--------------|--------------|
| 8 | DPC | ~500 | CROP |
| 9 | BNR | ~1000 | DEMOSAIC |
| 10 | 2DNR | ~800 | SHARP |

### Phase 4: Statistical Blocks

| Priority | Block | Estimated LOC | Dependencies |
|----------|-------|--------------|--------------|
| 11 | AWB | ~400 | BNR |
| 12 | AE | ~600 | GAMMA |

### Phase 5: Integration

| Priority | Task | Estimated LOC |
|----------|------|--------------|
| 13 | OECF | ~200 |
| 14 | DGAIN | ~200 |
| 15 | LSC/LDCI placeholders | ~50 |
| 16 | Top module integration | ~500 |
| 17 | AXI wrapper | ~1000 |

---

## 8. File Structure

```
src/
├── common/
│   ├── common_defs.h         # SC_MODULE, timing constants
│   ├── isp_types.h           # Types, BayerPattern enum
│   └── isp_params.h         # Default parameters, enables
│
├── utils/
│   ├── line_buffer.h         # Shift register line buffer
│   ├── window_buffer.h       # 2D sliding window
│   └── timing_adapter.h      # href/vsync edge detection
│
├── metrics/
│   ├── isp_metrics_base.h    # Base metrics interface
│   ├── timing_metrics.h      # Timing collector
│   └── operation_metrics.h   # Operation counters
│
├── blocks/
│   ├── crop/
│   │   ├── isp_crop.h
│   │   └── crop_metrics.h
│   ├── dpc/
│   │   ├── isp_dpc.h
│   │   └── dpc_metrics.h
│   ├── blc/
│   │   ├── isp_blc.h         # ✅ DONE
│   │   └── blc_metrics.h     # ✅ DONE
│   ├── oecf/
│   ├── dgain/
│   ├── bnr/
│   ├── wb/
│   ├── demosaic/
│   ├── ccm/
│   ├── gamma/
│   ├── csc/
│   ├── sharpen/
│   ├── ldci/
│   ├── 2dnr/
│   ├── awb/
│   └── ae/
│
├── pipeline/
│   ├── isp_config.h          # Configuration structures
│   └── isp_top.h             # Pipeline integration
│
└── testbench/
    ├── isp_testbench.h       # Base testbench
    └── isp_verification.h    # Verification utilities

tb/
├── blc_tb.cpp                # ✅ DONE
├── crop_tb.cpp
├── dpc_tb.cpp
├── wb_tb.cpp
├── demosaic_tb.cpp
├── ccm_tb.cpp
├── csc_tb.cpp
├── gamma_tb.cpp
├── sharpen_tb.cpp
├── 2dnr_tb.cpp
├── bnr_tb.cpp
└── isp_pipeline_tb.cpp
```

---

## 9. Key Implementation Notes

### 9.1 RTL Timing Accuracy

- **Critical**: Match RTL pipeline latency exactly
- Use `DLY_CLK` parameter from RTL to delay href/vsync
- Delay output data by same amount

### 9.2 Bayer Pattern Handling

```cpp
// Bayer pattern detection (matches RTL)
unsigned get_bayer_channel(unsigned x, unsigned y) {
    bool odd_y = (y & 1);
    bool odd_x = (x & 1);
    switch (bayer) {
        case BayerPattern::RGGB: return odd_y ? (odd_x ? 3 : 2) : (odd_x ? 1 : 0);
        case BayerPattern::GRBG: return odd_y ? (odd_x ? 3 : 0) : (odd_x ? 1 : 2);
        case BayerPattern::GBRG: return odd_y ? (odd_x ? 0 : 3) : (odd_x ? 1 : 2);
        case BayerPattern::BGGR: return odd_y ? (odd_x ? 1 : 0) : (odd_x ? 3 : 2);
    }
}
```

### 9.3 Line Buffer Implementation

```cpp
// Shift register line buffer (matches RTL shift_register module)
template<unsigned BITS, unsigned WIDTH, unsigned LINES>
class shift_register : sc_module {
    sc_in<bool> clk, href;
    sc_in<uint16_t> data_in;
    sc_out<uint16_t> shiftout;
    sc_out<std::array<uint16_t, LINES>> taps;
    
    // Internal shift registers
    std::array<std::array<uint16_t, WIDTH>, LINES> buffer;
};
```

### 9.4 Fixed-Point Arithmetic

RTL uses S8.8 format for CCM coefficients:
```cpp
// S8.8 fixed-point multiplication with rounding
inline int16_t multiply_s8_8(int16_t a, int16_t b) {
    int32_t result = (int32_t)a * (int32_t)b;
    return (int16_t)((result + 512) >> 10);  // Round and scale
}
```

---

## 10. Verification Strategy

### 10.1 Unit Testing

Each block testbench:
1. Generate known input pattern
2. Calculate expected output
3. Compare with DUT output
4. Report pass/fail with detailed mismatch info

### 10.2 Reference Model

For complex blocks (BNR, 2DNR, SHARP):
- Implement reference model in Python/MATLAB
- Generate golden output
- Compare SystemC output with golden

### 10.3 Regression Testing

```bash
# Test all blocks
make test_all

# Test specific block
make run_<block>

# Generate coverage report
make coverage
```

---

## 11. AXI Register Map Reference

From `infinite_isp_AXI_wrapper.v`:

| Module | Address Range | Registers |
|--------|--------------|-----------|
| CONFIG | 0x0000-0x00FF | RESET, SNS_WIDTH, SNS_HEIGHT, CROP_*, BITS, BAYER, EN |
| DPC | 0x0100-0x01FF | THRESHOLD |
| BLC | 0x0200-0x02FF | BLC_R/GR/GB/B, LINEAR_R/GR/GB/B |
| AE | 0x0300-0x03FF | CENTER_ILLUM, SKEWNESS, CROP_*, RESPONSE |
| DGAIN | 0x0400-0x04FF | ISMANUAL, MAN_INDEX, ARRAY[100] |
| AWB | 0x0600-0x06FF | UNDER/OVER_EXPOSED, FRAMES, GAINS |
| WB | 0x0700-0x07FF | RGAIN, BGAIN |
| CCM | 0x0900-0x09FF | RR, RG, RB, GR, GG, GB, BR, BG, BB |
| CSC | 0x0A00-0x0AFF | CONV_STD |
| SHARP | 0x0E00-0x0EFF | STRENGTH, KERNEL[81] |
| BNR | 0x1000-0x10FF | SPACE_KERNEL, COLOR_CURVE |
| 2DNR | 0x1500-0x15FF | DIFF[], WEIGHT[] |

---

## 12. Summary Checklist

- [x] **BLC** - Done
- [x] **CROP** - Done
- [x] **DPC** - Done
- [x] **OECF** - Done
- [x] **DGAIN** - Done
- [x] **LSC** - Done (passthrough)
- [x] **BNR** - Done
- [x] **AWB** - Done (statistics)
- [x] **WB** - Done
- [x] **DEMOSAIC** - Done
- [x] **CCM** - Done
- [x] **GAMMA** - Done
- [x] **AE** - Done (statistics)
- [x] **CSC** - Done
- [x] **LDCI** - Done (passthrough)
- [x] **SHARP** - Done
- [x] **2DNR** - Done
- [x] **Pipeline Top** - Done
- [x] **AXI Wrapper** - Done
- [x] **Full Integration Test** - Done
- [ ] **Pipeline Top** - Pending
- [ ] **AXI Wrapper** - Pending
- [ ] **Full Integration Test** - Pending

---

## 13. Architecture Documentation Reference

### 13.1 Infinite-ISP Repository Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         Infinite-ISP Repository Flow                         │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
            ┌───────────────────────┼───────────────────────┐
            │                       │                       │
            ▼                       ▼                       ▼
    ┌───────────────┐     ┌───────────────┐     ┌───────────────┐
    │    Infinite   │     │    Infinite   │     │    Infinite   │
    │   ISP Algo    │────▶│    ISP Ref     │────▶│     ISP       │
    │    Design     │     │     Model      │     │     RTL       │
    │  (Software)    │     │   (Python)     │     │   (Verilog)   │
    └───────────────┘     └───────────────┘     └───────────────┘
                                                          │
                                    ┌───────────────────────┼───────────────────────┐
                                    │                       │                       │
                                    ▼                       ▼                       ▼
                            ┌───────────────┐     ┌───────────────┐     ┌───────────────┐
                            │    FPGA       │     │    ASIC       │     │    Custom     │
                            │  Implementation│     │  Implementation│     │  Integration  │
                            └───────────────┘     └───────────────┘     └───────────────┘
```

### 13.2 Pipeline Architecture v1.1

**Key Features in v1.1:**
- Improved BNR (Bayer Noise Reduction) with Joint Bilateral Filter
- Enhanced 2DNR with adaptive filtering
- Better AWB (Auto White Balance) with improved statistics
- Optimized AE (Auto Exposure) response

**Block Dependencies:**
```
RAW Input
   │
   ▼
┌─────────┐    ┌─────────┐    ┌─────────┐
│  CROP   │───▶│   DPC   │───▶│   BLC   │    (RAW Processing)
└─────────┘    └─────────┘    └─────────┘
                                           │
   ┌─────────┐    ┌─────────┐    ┌─────────┐
   │  DGain  │◀───│  OECF   │◀───│  LSC    │    (Tone Mapping)
   └─────────┘    └─────────┘    └─────────┘
                   │
   ┌─────────┐    │
   │   BNR   │◀───┘                              (Noise Reduction)
   └─────────┘
        │
        │  ◀── AWB Statistics ──────────────┐
        ▼                                   │
┌─────────┐    ┌─────────┐    ┌─────────┐ │
│    WB   │───▶│DEMOSAIC │───▶│   CCM   │ │  (Color Processing)
└─────────┘    └─────────┘    └─────────┘ │
                                        │   │
                                        ▼   ▼
                               ┌─────────────┐
                               │   GAMMA     │
                               └─────────────┘
                                        │   │
                                        ▼   │  ◀── AE Statistics
                               ┌─────────────┐    │
                               │    CSC      │◀───┘    (YUV Conversion)
                               └─────────────┘
                                        │
                               ┌─────────────┐
                               │   LDCI      │                   (Contrast)
                               └─────────────┘
                                        │
                               ┌─────────────┐
                               │   SHARP    │                   (Sharpening)
                               └─────────────┘
                                        │
                               ┌─────────────┐
                               │    2DNR     │
                               └─────────────┘
                                        │
                                        ▼
                               ┌─────────────┐
                               │    Output   │                   (YUV422)
                               │   YUV422    │
                               └─────────────┘
```

### 13.3 RTL File Organization

```
Infinite-ISP_RTL/
├── hw/
│   └── rtl/
│       ├── isp/                    # ISP blocks
│       │   ├── top/               # Top module (isp_top.v)
│       │   ├── blc/              # Black Level Correction
│       │   ├── dpc/              # Defective Pixel Correction
│       │   ├── wb/               # White Balance
│       │   ├── ccm/              # Color Correction Matrix
│       │   ├── csc/              # Color Space Conversion
│       │   ├── demosaic/         # Demosaicing (CFA)
│       │   ├── gamma/            # Gamma Correction
│       │   ├── sharpen/           # Sharpening
│       │   ├── bnr/              # Bayer Noise Reduction
│       │   ├── 2dnr/             # 2D Noise Reduction
│       │   ├── awb/              # Auto White Balance
│       │   ├── ae/                # Auto Exposure
│       │   ├── crop/              # Cropping
│       │   ├── dg/               # Digital Gain
│       │   ├── oecf/             # OECF (Opto-Electronic Conversion Function)
│       │   ├── lsc/              # Lens Shading Correction (placeholder)
│       │   └── ldci/             # Local Dynamic Contrast (placeholder)
│       ├── axi_wrapper/           # AXI4-Lite register interface
│       ├── vip/                   # Video Processing (post-ISP)
│       │   ├── top/
│       │   ├── yuv2rgb/
│       │   ├── dscale/
│       │   ├── osd/
│       │   └── ...
│       └── top/                   # Full ISP + VIP top (infinite_isp.v)
├── fpga/
│   └── vivado/
│       └── isp_rtl/              # Vivado project files
├── sim/
│   └── tb/                       # Testbenches
└── doc/
    ├── Infinite-ISP_RTLArchitecture_v1.1.pdf
    └── user/
        └── rtl_user_guide.md
```

### 13.4 AXI Register Map Summary

From `infinite_isp_AXI_wrapper.v`:

| Base Address | Module | Key Registers |
|--------------|--------|--------------|
| 0x0000 | CONFIG | SNS_WIDTH, SNS_HEIGHT, CROP_W, CROP_H, BITS, BAYER, EN |
| 0x0100 | DPC | THRESHOLD |
| 0x0200 | BLC | BLC_R, BLC_GR, BLC_GB, BLC_B, LINEAR_R, LINEAR_GR, LINEAR_GB, LINEAR_B |
| 0x0300 | AE | CENTER_ILLUM, SKEWNESS, CROP_L/R/T/B, RESPONSE, RESULT_SKEWNESS |
| 0x0400 | DGAIN | ISMANUAL, MAN_INDEX, INDEX_OUT, ARRAY[100] |
| 0x0500 | LSC | (placeholder) |
| 0x0600 | AWB | UNDER_EXPOSED, OVER_EXPOSED, FRAMES, FINAL_R_GAIN, FINAL_B_GAIN |
| 0x0700 | WB | R_GAIN, B_GAIN |
| 0x0800 | CFA | (Demosaic - no config) |
| 0x0900 | CCM | RR, RG, RB, GR, GG, GB, BR, BG, BB (3×3 matrix) |
| 0x0A00 | CSC | CONV_STD (BT.601/BT.709) |
| 0x0B00 | OECF | LUT interface |
| 0x0E00 | SHARP | STRENGTH, KERNEL[81] (9×9) |
| 0x1000 | BNR | SPACE_KERNEL[75], COLOR_CURVE[27] |
| 0x1500 | 2DNR | DIFF[32], WEIGHT[32] |

### 13.5 Input/Output Formats

**Input:**
- Single-channel RAW Bayer: 10-bit per pixel, stored as binary file
- 3-channel RGB: Separate R/G/B files, 10-bit per pixel
- Pixel order: Row by row, sequential

**Output:**
- YUV422 format: Y (8-bit), U (8-bit), V (8-bit)
- Can output RGB after CSC conversion
- Gamma-corrected RGB also available (pre-CSC)

### 13.6 Simulation Setup (from rtl_user_guide.md)

1. Place input file in `tv/input/` directory
2. Name: `filename_0.bin` for RAW, `R_filename_0.bin`, `G_filename_0.bin`, `B_filename_0.bin` for RGB
3. Update testbench (`tb_seq_simulation.sv`) with filename and parameters
4. Configure module enables via parameters (e.g., `CROP_EN = 1`)
5. Set sensor dimensions: `SNS_WIDTH`, `SNS_HEIGHT`

---

## 14. Algorithm Reference Summary

### 14.1 BLC (Black Level Correction)
- **Purpose**: Subtract black level offset from each channel
- **Formula**: `out = (in > offset) ? (in - offset) : 0`
- **Linearization**: Optional gain per channel: `out = in * gain >> BITS`

### 14.2 DPC (Defective Pixel Correction)
- **Algorithm**: Gradient-based detection
- **Window**: 5×5 pixel neighborhood
- **Detection**: Compare center pixel with 8 neighbors
- **Correction**: Replace with gradient direction interpolation

### 14.3 WB (White Balance)
- **Algorithm**: Channel gain multiplication
- **Formula**: `R_out = R_in * gain_r`, `B_out = B_in * gain_b`
- **Green**: Pass through (gain = 1.0)

### 14.4 CCM (Color Correction Matrix)
- **Formula**: 3×3 matrix multiplication with S8.8 fixed-point
- **Normalization**: Shift right by 10 bits
- **Clipping**: Saturation handling

### 14.5 CSC (Color Space Conversion)
- **BT.601**:
  - Y = (77R + 150G + 29B) >> 8
  - U = (-43R - 85G + 128B + 32768) >> 8
  - V = (128R - 107G - 21B + 32768) >> 8
- **BT.709**: Different coefficients

### 14.6 DEMOSAIC (Bayer CFA Interpolation)
- **Algorithm**: Bilinear interpolation per channel
- **Pattern-aware**: Different filters for R/G/B positions
- **Edge handling**: Special cases at boundaries

### 14.7 GAMMA
- **Implementation**: LUT (Look-Up Table)
- **Dual-port RAM**: Config port + Processing port
- **Format**: 10-bit input, 10-bit output

### 14.8 SHARP (Unsharp Masking)
- **Kernel**: 9×9 convolution
- **Formula**: `out = Y + strength * (Y - smoothed_Y)`
- **Separable**: Row/column optimization possible

### 14.9 BNR (Bayer Noise Reduction)
- **Algorithm**: Joint Bilateral Filter (JBF)
- **Components**:
  - Green interpolation first
  - 5×5 spatial kernel
  - 9-point color/range kernel

### 14.10 2DNR (2D Noise Reduction)
- **Algorithm**: Weighted averaging with difference-based weights
- **LUT**: 32-level difference LUT
- **Window**: 9×9

### 14.11 AWB (Auto White Balance)
- **Algorithm**: Gray world assumption
- **Statistics**: Mean R, G, B per frame
- **Gain**: `gain_R = mean_G / mean_R`, `gain_B = mean_G / mean_B`

### 14.12 AE (Auto Exposure)
- **Algorithm**: Moment-based exposure control
- **Metrics**: Sum of pixels, sum of squares, sum of cubes
- **Feedback**: Exposure response index → DGain

---

## 15. Testing & Verification

### 15.1 RTL Testbench Structure

```systemverilog
// tb_seq_simulation.sv - Main sequential testbench
module tb_seq_simulation;
    parameter FRAMES = 1;
    parameter IN_FILE = "input_raw";
    // ...
    
    initial begin
        // Load input file
        // Configure ISP parameters
        // Run simulation
        // Compare with golden output
    end
endmodule
```

### 15.2 Test Patterns

| Pattern | Description | Use Case |
|---------|------------|----------|
| Uniform | Single value | Basic functionality |
| Gradient | Linear ramp | Linearity check |
| Checkerboard | Alternating | Edge detection |
| Random | Pseudorandom | Statistical analysis |
| Real image | Camera capture | Visual verification |

### 15.3 Verification Metrics

- **Functional**: Pixel-by-pixel comparison
- **Timing**: Cycle count matching
- **Resource**: LUT/FF/DSP utilization
- **Power**: Dynamic/static power estimation

---

*Last Updated: 2026-07-22*
*Reference RTL: Infinite-ISP_RTL (10xEngineers)*
*Architecture Docs: `/home/hoangquan/workspace/ISP_stuff/Infinite-ISP_RTL/doc/`*
