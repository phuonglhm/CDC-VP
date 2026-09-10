# SAURIA V4.4 CDC-VP Integration

`npu_tlm` connects the SAURIA MP1 V1.1 V4.4 SystemC model to the
CDC-VP RISC-V full SoC.

The default SAURIA model is bundled under:

```text
CDC-VP/components/npu_tlm/models/v4.4_model_25Aug
```

Set `SAURIA_NPU_ROOT` at CMake configure time only when an external model tree
should override the bundled version.

## CDC-VP Mapping

The full SoC maps one optional NPU instance:

```text
NPU physical range:  0x10200000..0x102FFFFF
CDC_NPU0_BASE:       0x10200000
NPU aperture size:   1 MiB
External RAM:        0x80000000..0x8FFFFFFF
PLIC interrupt:      source 17
```

Unless an absolute address is shown, every address below is an offset from
`CDC_NPU0_BASE`.

```text
physical_address = CDC_NPU0_BASE + offset
```

For example, native profile offset `0x0004` is physical address
`0x10200004`, while the CDC 64x64 GEMM control register at offset
`0x30000` is physical address `0x10230000`.

The NPU aperture is present only when `CDC_ENABLE_SAURIA_NPU_V4=ON`.
Otherwise, the address range remains reserved and PLIC source 17 is tied low.

## Interface Flow

```text
RISC-V software
  -> 32-bit MMIO at 0x10200000 + offset
    -> CDC-VP bus_router
      -> npu_tlm::b_transport
        -> native V4.4 host interface, or
        -> CDC 64x64 GEMM controller
          -> RAM master socket
            -> system RAM at 0x80000000..0x8FFFFFFF
```

Native MMIO requests are executed by the NPU worker thread so that one
SystemC process owns the V4.4 host-interface signals.

The V4.4 rich executor uses its own byte-addressed DRAM vector. The bridge
translates system physical buffer addresses to RAM-relative model addresses,
copies rich-operation inputs from system RAM before a queue push, keeps the
gated NPU clock running while either lane is active, and copies completed
outputs back to system RAM. Firmware must therefore program full physical RAM
addresses in all rich address registers.

## Built Instance

The wrapper instantiates:

```cpp
sauria::NpuTop<64, 64, int8_t, int8_t, int32_t,
               16, 128, 1>
```

| Item | Value |
| --- | ---: |
| Array X | `64` |
| Array Y | `64` |
| Activation type | `int8_t` |
| Weight type | `int8_t` |
| Partial-sum type | `int32_t` |
| FIFO depth | `16` |
| PE latency parameter | `128` |
| SRAM A depth | `1024` |
| SRAM B depth | `1024` |
| SRAM C depth | `2048` |
| Dilation-pattern width | `64` |
| NPU clock | `800 MHz` (`1.25 ns` period) |

These build-time values do not have runtime MMIO registers.

## Access Classes

All accesses must be aligned 32-bit little-endian words. Byte enables and
other transfer lengths are rejected. Buffer-address registers contain system
physical addresses; firmware must not subtract `CDC_RAM0_BASE`.

| Class | Meaning |
| --- | --- |
| Native config RW | V4.4 `config_regs.h` decodes host writes and reads |
| SRAM RW | V4.4 `sram_top.h` decodes host writes and reads |
| Rich WO | V4.4 `instruction_decoder.h` decodes writes; it defines no register readback |
| OBP RW | V4.4 decodes writes and `NpuTop` routes host reads |
| OBP WO | V4.4 decodes writes but `NpuTop` does not route that region on host reads |
| RCE RW | V4.4 decodes writes and `NpuTop` routes host reads |
| Counter RO | CDC-VP returns a raw field updated by V4.4 instrumentation |
| Software GEMM RW/RO/W1C | CDC-VP implements the software-facing 64x64 GEMM controller |

## Aperture Summary

| VP offset/range | Physical address/range | Interface |
| ---: | ---: | --- |
| `0x00000..0x00AFF` | `0x10200000..0x10200AFF` | Native V4.4 control and configuration |
| selected `0x01200..0x01328` | `0x10201200..0x10201328` | Raw V4.4 performance counters |
| `0x10000..0x10FFF` | `0x10210000..0x10210FFF` | Compact rich-instruction alias |
| `0x20000..0x2EFFF` | `0x10220000..0x1022EFFF` | Compact OBP aliases |
| `0x32000..0x37FFF` | `0x10232000..0x10237FFF` | Compact RCE aliases |
| `0x30000..0x31FFF` | `0x10230000..0x10231FFF` | CDC 64x64 GEMM bank |
| `0x40000` window | starts at `0x10240000` | Native SRAM A |
| `0x80000` window | starts at `0x10280000` | Native SRAM B |
| `0xC0000` window | starts at `0x102C0000` | Native SRAM C |

## Native Common Registers

| Offset | Physical address | Access | Firmware symbol | Description |
| ---: | ---: | --- | --- | --- |
| `0x0000` | `0x10200000` | RW/pulse | `CDC_NPU_NATIVE_CONTROL` | Native start, done and soft reset |
| `0x0004` | `0x10200004` | RW | `CDC_NPU_NATIVE_CFG_PROFILE` | Select configuration profile |

At native control offset `0x0000`, V4.4 implements:

| Value or bit | Access | Meaning |
| ---: | --- | --- |
| Lane 0 nonzero | W | Start pulse |
| Lane 0 zero | W | Clear done |
| Lane 0 bit 1 | R | Done |
| Lane 0 bit 2 | R | Idle (`!done`) |
| Lane 0 bit 3 | R | Ready (`!done`) |
| Bits `16..23` | W | Soft-reset byte |

Profile values are:

| Value | V4.4 profile |
| ---: | --- |
| `0` | `PROFILE_V1_SAURIA` |
| `1` | `PROFILE_V4_LINEAR` |

The profiles also select the activation-feeder addressing behavior:

- `PROFILE_V1_SAURIA` uses SAURIA's implicit im2col, dilation-aware address
  generation, and tiling. Software provides the original convolution tensor
  and programs the convolution/feeder configuration; the activation feeder
  dynamically generates the im2col-style stream without materializing a full
  im2col matrix in memory.
- `PROFILE_V4_LINEAR` uses linear feeder addressing
  (`address += incntstep`) and does not generate an im2col stream. Software or
  the compiler must provide data in the linear/GEMM layout required by this
  profile.

Write the profile before writing profile-dependent registers.

## PROFILE_V1_SAURIA Register Map

### Implicit im2col Programming

`PROFILE_V1_SAURIA` does not require software to materialize an expanded
im2col matrix in memory. Software writes the convolution activation tensor to
SRAM A and programs the activation-feeder limits and steps. The V4.4 IFMAP
feeder then generates the logical im2col stream on demand.

The feeder selects this SAURIA address-generation mode when all six inner-loop
values are nonzero: `ACT_XLIM`, `ACT_XSTEP`, `ACT_YLIM`, `ACT_YSTEP`,
`ACT_CHLIM`, and `ACT_CHSTEP`. Its nested counter order is:

```text
kernel X -> kernel Y -> input channel -> output/tile X -> output/tile Y
```

The following V1 MMIO registers form the implicit im2col programming
interface. The detailed field names and access classes are listed again in the
V1 register map below.

| Offset | Firmware symbol | im2col/address-generation role |
| ---: | --- | --- |
| `0x0404` | `CDC_NPU_NATIVE_ACT_INCNTLIM` | Activation input-count limit; contributes to effective K |
| `0x0408` | `CDC_NPU_NATIVE_ACT_INCNTSTEP` | Activation input-count step |
| `0x0414` | `CDC_NPU_NATIVE_ACT_XLIM` | Kernel-X address limit |
| `0x0418` | `CDC_NPU_NATIVE_ACT_XSTEP` | Kernel-X address step |
| `0x041C` | `CDC_NPU_NATIVE_ACT_YLIM` | Kernel-Y address limit |
| `0x0420` | `CDC_NPU_NATIVE_ACT_YSTEP` | Kernel-Y address step; native dilation is encoded in this step |
| `0x0424` | `CDC_NPU_NATIVE_ACT_CHLIM` | Input-channel address limit |
| `0x0428` | `CDC_NPU_NATIVE_ACT_DIL_PAT` | Dilation bitmap; the current feeder applies bitmap gating in linear mode |
| `0x042C` | `CDC_NPU_NATIVE_ACT_CHSTEP` | Input-channel address step |
| `0x0430` | `CDC_NPU_NATIVE_ACT_TIL_XLIM` | Output/tile-X limit |
| `0x0434` | `CDC_NPU_NATIVE_ACT_TIL_XSTEP` | Output/tile-X step; carries horizontal stride |
| `0x0438` | `CDC_NPU_NATIVE_ACT_TIL_YLIM` | Output/tile-Y limit |
| `0x043C` | `CDC_NPU_NATIVE_ACT_TIL_YSTEP` | Output/tile-Y step; carries vertical stride |
| `0x0480` | `CDC_NPU_NATIVE_CFG_ACT_BASE_ADDR` | Activation SRAM base address |

The model's `driver/libsauria_cfg.h` computes these fields from a layer and
tile description. Using its names, the implemented relationships include:

```text
effective_kernel_width  = 1 + (B_w - 1) * dilation
effective_kernel_height = 1 + (B_h - 1) * dilation
effective_tile_width    = 1 + (w_tile - 1) * stride
effective_tile_height   = 1 + (h_tile - 1) * stride
A_w_tile = effective_tile_width  + effective_kernel_width  - 1
A_h_tile = effective_tile_height + effective_kernel_height - 1

CON_INCNTLIM = B_w * B_h * c_tile - 1
ACT_YSTEP    = A_w_tile * dilation
ACT_CHSTEP   = A_w_tile * A_h_tile
ACT_CHLIM    = ACT_CHSTEP * c_tile
ACT_TIL_XSTEP = Y_used * stride
ACT_TIL_YSTEP = A_w_tile * stride
```

The layer-description registers `KERNEL_H`, `KERNEL_W`, `STRIDE`, `PADDING`,
and `DILATION` do not automatically derive or program these feeder limits and
steps in the current V4.4 datapath. 

### Controller

| Offset | Access | Firmware symbol | V4.4 field |
| ---: | --- | --- | --- |
| `0x0200` | RW | `CDC_NPU_NATIVE_CON_INCNTLIM` | `F_INCNTLIM` |
| `0x0204` | RW | `CDC_NPU_NATIVE_CON_ACT_REPS` | `F_ACT_REPS` |
| `0x0208` | RW | `CDC_NPU_NATIVE_CON_WEI_REPS` | `F_WEI_REPS` |
| `0x0214` | RW | `CDC_NPU_NATIVE_CON_NSPLIT` | `F_NSPLIT` |

### Activation Feeder

| Offset | Access | Firmware symbol | V4.4 field |
| ---: | --- | --- | --- |
| `0x0400` | RW | `CDC_NPU_NATIVE_ACT_ROWS_ACTIVE` | `F_ROWS_ACTIVE` |
| `0x0404` | RW | `CDC_NPU_NATIVE_ACT_INCNTLIM` | `F_ACT_INCNTLIM` |
| `0x0408` | RW | `CDC_NPU_NATIVE_ACT_INCNTSTEP` | `F_ACT_INCNTSTEP` |
| `0x040C` | RW | `CDC_NPU_NATIVE_ACT_OUTCNTLIM` | `F_ACT_OUTCNTLIM` |
| `0x0410` | RW | `CDC_NPU_NATIVE_ACT_OUTCNTSTEP` | `F_ACT_OUTCNTSTEP` |
| `0x0414` | RW | `CDC_NPU_NATIVE_ACT_XLIM` | `F_ACT_XLIM` |
| `0x0418` | RW | `CDC_NPU_NATIVE_ACT_XSTEP` | `F_ACT_XSTEP` |
| `0x041C` | RW | `CDC_NPU_NATIVE_ACT_YLIM` | `F_ACT_YLIM` |
| `0x0420` | RW | `CDC_NPU_NATIVE_ACT_YSTEP` | `F_ACT_YSTEP` |
| `0x0424` | RW | `CDC_NPU_NATIVE_ACT_CHLIM` | `F_ACT_CHLIM` |
| `0x0428` | RW | `CDC_NPU_NATIVE_ACT_DIL_PAT` | `F_DIL_PAT` |
| `0x042C` | RW | `CDC_NPU_NATIVE_ACT_CHSTEP` | `F_ACT_CHSTEP` |
| `0x0430` | RW | `CDC_NPU_NATIVE_ACT_TIL_XLIM` | `F_ACT_TIL_XLIM` |
| `0x0434` | RW | `CDC_NPU_NATIVE_ACT_TIL_XSTEP` | `F_ACT_TIL_XSTEP` |
| `0x0438` | RW | `CDC_NPU_NATIVE_ACT_TIL_YLIM` | `F_ACT_TIL_YLIM` |
| `0x043C` | RW | `CDC_NPU_NATIVE_ACT_TIL_YSTEP` | `F_ACT_TIL_YSTEP` |
| `0x0480` | RW | `CDC_NPU_NATIVE_CFG_ACT_BASE_ADDR` | `F_ACT_BASE_ADDR` |

### Weight Feeder

| Offset | Access | Firmware symbol | V4.4 field |
| ---: | --- | --- | --- |
| `0x0604` | RW | `CDC_NPU_NATIVE_WEI_INCNTLIM` | `F_WEI_INCNTLIM` |
| `0x0608` | RW | `CDC_NPU_NATIVE_WEI_INCNTSTEP` | `F_WEI_INCNTSTEP` |
| `0x0610` | RW | `CDC_NPU_NATIVE_WEI_WLIM` | `F_WEI_WLIM` |
| `0x0614` | RW | `CDC_NPU_NATIVE_WEI_WSTEP` | `F_WEI_WSTEP` |
| `0x0618` | RW | `CDC_NPU_NATIVE_WEI_KLIM` | `F_WEI_KLIM` |
| `0x061C` | RW | `CDC_NPU_NATIVE_WEI_KSTEP` | `F_WEI_KSTEP` |
| `0x0620` | RW | `CDC_NPU_NATIVE_WEI_TIL_XLIM` | `F_WEI_TIL_KLIM` |
| `0x0624` | RW | `CDC_NPU_NATIVE_WEI_TIL_XSTEP` | `F_WEI_TIL_KSTEP` |
| `0x0628` | RW | `CDC_NPU_NATIVE_WEI_COLS_ACTIVE` | `F_WEI_COLS_ACTIVE` |
| `0x062C` | RW | `CDC_NPU_NATIVE_WEI_WALIGNED` | `F_WEI_WALIGNED` |
| `0x0680` | RW | `CDC_NPU_NATIVE_CFG_WEI_BASE_ADDR` | `F_WEI_BASE_ADDR` |

### Output, PSM And OBP

| Offset | Access | Firmware symbol | V4.4 field |
| ---: | --- | --- | --- |
| `0x0800` | RW | `CDC_NPU_NATIVE_NCONTEXTS` | `F_NCONTEXTS` |
| `0x0804` | RW | `CDC_NPU_NATIVE_OUT_CXLIM` | `F_CXLIM` |
| `0x0808` | RW | `CDC_NPU_NATIVE_OUT_CXSTEP` | `F_CXSTEP` |
| `0x080C` | RW | `CDC_NPU_NATIVE_OUT_CKLIM` | `F_CKLIM` |
| `0x0810` | RW | `CDC_NPU_NATIVE_OUT_CKSTEP` | `F_CKSTEP` |
| `0x0814` | RW | `CDC_NPU_NATIVE_TIL_CYLIM` | `F_TIL_CYLIM` |
| `0x0818` | RW | `CDC_NPU_NATIVE_TIL_CYSTEP` | `F_TIL_CYSTEP` |
| `0x081C` | RW | `CDC_NPU_NATIVE_TIL_CKLIM` | `F_TIL_CKLIM` |
| `0x0820` | RW | `CDC_NPU_NATIVE_OUT_OBP_CFG_A` | `F_OBP_CFG_A` |
| `0x0824` | RW | `CDC_NPU_NATIVE_OUT_REQUANT_SCALE_A` | `F_REQUANT_SCALE_A` |
| `0x0828` | RW | `CDC_NPU_NATIVE_OUT_REQUANT_SHIFT_A` | `F_REQUANT_SHIFT_A` |
| `0x0830` | RW | `CDC_NPU_NATIVE_OUT_OBP_CFG_B` | `F_OBP_CFG_B` |
| `0x0834` | RW | `CDC_NPU_NATIVE_OUT_REQUANT_SCALE_B` | `F_REQUANT_SCALE_B` |
| `0x0838` | RW | `CDC_NPU_NATIVE_OUT_REQUANT_SHIFT_B` | `F_REQUANT_SHIFT_B` |
| `0x0880` | RW | `CDC_NPU_NATIVE_CFG_OUT_BASE_ADDR` | `F_OUT_BASE_ADDR` |

V4.4 also declares `F_TIL_CKSTEP`, `F_INACTIVE_COLS`, and `F_PRELOAD_EN` at
`0x0820`, `0x0824`, and `0x0828`. The earlier OBP-A decode branches at those
same addresses take precedence in the current model.

### Layer Fields

| Offset | Access | Firmware symbol | V4.4 field |
| ---: | --- | --- | --- |
| `0x0A00` | RW | `CDC_NPU_NATIVE_IN_H` | `F_IN_H` |
| `0x0A04` | RW | `CDC_NPU_NATIVE_IN_W` | `F_IN_W` |
| `0x0A08` | RW | `CDC_NPU_NATIVE_IN_C` | `F_IN_C` |
| `0x0A18` | RW | `CDC_NPU_NATIVE_KERNEL_H` | `F_KERNEL_H` |
| `0x0A1C` | RW | `CDC_NPU_NATIVE_KERNEL_W` | `F_KERNEL_W` |
| `0x0A20` | RW | `CDC_NPU_NATIVE_STRIDE` | `F_STRIDE` |
| `0x0A24` | RW | `CDC_NPU_NATIVE_PADDING` | `F_PADDING` |
| `0x0A28` | RW | `CDC_NPU_NATIVE_DILATION` | `F_DILATION` |
| `0x0A30` | RW | `CDC_NPU_NATIVE_TILE_X` | `F_TILE_X` |
| `0x0A34` | RW | `CDC_NPU_NATIVE_TILE_Y` | `F_TILE_Y` |
| `0x0A38` | RW | `CDC_NPU_NATIVE_TILE_K` | `F_TILE_K` |
| `0x0A3C` | RW | `CDC_NPU_NATIVE_TILE_C` | `F_TILE_C` |
| `0x0A40` | RW | `CDC_NPU_NATIVE_X_USED` | `F_X_USED` |
| `0x0A44` | RW | `CDC_NPU_NATIVE_Y_USED` | `F_Y_USED` |

The V4.4 constants `OUT_H`, `OUT_W`, `OUT_C`, and layer `DIL_PAT` exist at
`0x0A0C`, `0x0A10`, `0x0A14`, and `0x0A2C`, but the current V1/V4 maps do not
decode them. They must not be treated as working runtime registers.

## PROFILE_V4_LINEAR Register Map

The V4 profile reuses some addresses with different meanings.

### Controller

| Offset | Access | Firmware symbol | V4.4 field |
| ---: | --- | --- | --- |
| `0x0200` | RW | `CDC_NPU_NATIVE_CON_INCNTLIM` | `F_INCNTLIM` |
| `0x0204` | RW | `CDC_NPU_NATIVE_CON_ACT_REPS` | `F_ACT_REPS` |
| `0x0208` | RW | `CDC_NPU_NATIVE_CON_WEI_REPS` | `F_WEI_REPS` |
| `0x020C` | RW | `CDC_NPU_NATIVE_V4_CON_NCONTEXTS` | `F_NCONTEXTS` |
| `0x0210` | RW | `CDC_NPU_NATIVE_V4_CON_PRELOAD_EN` | `F_PRELOAD_EN` |
| `0x0214` | RW | `CDC_NPU_NATIVE_CON_NSPLIT` | `F_NSPLIT` |

### Feeder And Output

| Offset | Access | Firmware symbol | V4.4 field |
| ---: | --- | --- | --- |
| `0x0400` | RW | `CDC_NPU_NATIVE_ACT_ROWS_ACTIVE` | `F_ROWS_ACTIVE` |
| `0x0404` | RW | `CDC_NPU_NATIVE_ACT_INCNTLIM` | `F_ACT_INCNTLIM` |
| `0x0408` | RW | `CDC_NPU_NATIVE_ACT_INCNTSTEP` | `F_ACT_INCNTSTEP` |
| `0x040C` | RW | `CDC_NPU_NATIVE_ACT_OUTCNTLIM` | `F_ACT_OUTCNTLIM` |
| `0x0410` | RW | `CDC_NPU_NATIVE_ACT_OUTCNTSTEP` | `F_ACT_OUTCNTSTEP` |
| `0x0428` | RW | `CDC_NPU_NATIVE_ACT_DIL_PAT` | `F_DIL_PAT` |
| `0x0600` | RW | `CDC_NPU_NATIVE_V4_WEI_INCNTLIM` | `F_WEI_INCNTLIM` |
| `0x0604` | RW | `CDC_NPU_NATIVE_V4_WEI_INCNTSTEP` | `F_WEI_INCNTSTEP` |
| `0x0800` | RW | `CDC_NPU_NATIVE_V4_OUT_CXLIM` | `F_CXLIM` |
| `0x0804` | RW | `CDC_NPU_NATIVE_V4_OUT_CXSTEP` | `F_CXSTEP` |
| `0x0808` | RW | `CDC_NPU_NATIVE_V4_OUT_CKLIM` | `F_CKLIM` |
| `0x080C` | RW | `CDC_NPU_NATIVE_V4_OUT_CKSTEP` | `F_CKSTEP` |
| `0x0810` | RW | `CDC_NPU_NATIVE_V4_OUT_TIL_CYLIM` | `F_TIL_CYLIM` |
| `0x0814` | RW | `CDC_NPU_NATIVE_V4_OUT_TIL_CYSTEP` | `F_TIL_CYSTEP` |
| `0x0818` | RW | `CDC_NPU_NATIVE_V4_OUT_TIL_CKLIM` | `F_TIL_CKLIM` |
| `0x081C` | RW | `CDC_NPU_NATIVE_V4_OUT_TIL_CKSTEP` | `F_TIL_CKSTEP` |
| `0x0820` | RW | `CDC_NPU_NATIVE_OUT_OBP_CFG_A` | `F_OBP_CFG_A` |
| `0x0824` | RW | `CDC_NPU_NATIVE_OUT_REQUANT_SCALE_A` | `F_REQUANT_SCALE_A` |
| `0x0828` | RW | `CDC_NPU_NATIVE_OUT_REQUANT_SHIFT_A` | `F_REQUANT_SHIFT_A` |
| `0x0830` | RW | `CDC_NPU_NATIVE_OUT_OBP_CFG_B` | `F_OBP_CFG_B` |
| `0x0834` | RW | `CDC_NPU_NATIVE_OUT_REQUANT_SCALE_B` | `F_REQUANT_SCALE_B` |
| `0x0838` | RW | `CDC_NPU_NATIVE_OUT_REQUANT_SHIFT_B` | `F_REQUANT_SHIFT_B` |

## Native SRAM Windows

| Offset | Physical base | Access | Firmware symbol |
| ---: | ---: | --- | --- |
| `0x40000` | `0x10240000` | RW | `CDC_NPU_NATIVE_SRAMA_OFFSET` |
| `0x80000` | `0x10280000` | RW | `CDC_NPU_NATIVE_SRAMB_OFFSET` |
| `0xC0000` | `0x102C0000` | RW | `CDC_NPU_NATIVE_SRAMC_OFFSET` |

These expose the model's private host SRAM interface. The address after each
base uses V4.4 physical-row and subword encoding; it is not a normal linear
CPU byte array. For this 64x64 instance, each host word carries four lanes,
so each SRAM row has 16 subwords. The encoding used by the bridge is:

```text
host_offset = (physical_row << 4) | subword
subword     = 0..15
```

## Compact Aliases

Some V4.4 model host addresses are outside the 1 MiB SoC aperture. CDC-VP
translates compact VP offsets to those sparse model addresses.

| VP offset/range | Physical range | Access | V4.4 model range | Region |
| ---: | ---: | --- | ---: | --- |
| selected `0x10300..0x10468` | `0x10210300..0x10210468` | WO | `0x40000300..0x40000468` | Rich instruction decoder |
| `0x20000..0x23FFF` | `0x10220000..0x10223FFF` | RW | `0x00140000..0x00143FFF` | OBP A LUT, all 64 x 256 byte entries |
| `0x24000..0x240FF` | `0x10224000..0x102240FF` | RW | `0x00150000..0x001500FF` | OBP A bias, 64 x 32-bit entries |
| `0x25000..0x250FF` | `0x10225000..0x102250FF` | WO | `0x00180000..0x001800FF` | OBP A scale, 64 x 32-bit entries |
| `0x26000..0x260FF` | `0x10226000..0x102260FF` | WO | `0x00190000..0x001900FF` | OBP A shift, 64 x 32-bit entries |
| `0x28000..0x2BFFF` | `0x10228000..0x1022BFFF` | RW | `0x00160000..0x00163FFF` | OBP B LUT, all 64 x 256 byte entries |
| `0x2C000..0x2C0FF` | `0x1022C000..0x1022C0FF` | RW | `0x00170000..0x001700FF` | OBP B bias, 64 x 32-bit entries |
| `0x2D000..0x2D0FF` | `0x1022D000..0x1022D0FF` | WO | `0x001A0000..0x001A00FF` | OBP B scale, 64 x 32-bit entries |
| `0x2E000..0x2E0FF` | `0x1022E000..0x1022E0FF` | WO | `0x001B0000..0x001B00FF` | OBP B shift, 64 x 32-bit entries |
| `0x32000..0x320FF` | `0x10232000..0x102320FF` | RW | `0x00200000..0x002000FF` | RCE A exponential |
| `0x33000..0x331FF` | `0x10233000..0x102331FF` | RW | `0x00210000..0x002101FF` | RCE A reciprocal |
| `0x34000..0x347FF` | `0x10234000..0x102347FF` | RW | `0x00220000..0x002207FF` | RCE A reciprocal square root |
| `0x35000..0x350FF` | `0x10235000..0x102350FF` | RW | `0x00230000..0x002300FF` | RCE B exponential |
| `0x36000..0x361FF` | `0x10236000..0x102361FF` | RW | `0x00240000..0x002401FF` | RCE B reciprocal |
| `0x37000..0x377FF` | `0x10237000..0x102377FF` | RW | `0x00250000..0x002507FF` | RCE B reciprocal square root |

The compact offsets are CDC-VP integration definitions. The sparse target
addresses and their behavior belong to the V4.4 model.

## Rich Instruction Write Registers

The rich page is write-only because V4.4 does not define host readback for
these instruction-decoder registers.

| VP offset | Physical address | Firmware symbol | V4.4 field or action |
| ---: | ---: | --- | --- |
| `0x10300` | `0x10210300` | `CDC_NPU_VP_RICH_INST_LO_A` | Instruction low A |
| `0x10304` | `0x10210304` | `CDC_NPU_VP_RICH_INST_HI_A` | Instruction high A |
| `0x10308` | `0x10210308` | `CDC_NPU_VP_RICH_INST_LO_B` | Instruction low B |
| `0x1030C` | `0x1021030C` | `CDC_NPU_VP_RICH_INST_HI_B` | Instruction high B |
| `0x10310` | `0x10210310` | `CDC_NPU_VP_RICH_PUSH_A` | Push queue A |
| `0x10314` | `0x10210314` | `CDC_NPU_VP_RICH_PUSH_B` | Push queue B |
| `0x10400` | `0x10210400` | `CDC_NPU_VP_RICH_IN_ADDR` | `in_addr` |
| `0x10404` | `0x10210404` | `CDC_NPU_VP_RICH_WEIGHT_ADDR` | `w_addr` |
| `0x10408` | `0x10210408` | `CDC_NPU_VP_RICH_OUT_ADDR` | `out_addr` |
| `0x1040C` | `0x1021040C` | `CDC_NPU_VP_RICH_BIAS_ADDR` | `bias_addr` |
| `0x10410` | `0x10210410` | `CDC_NPU_VP_RICH_M` | `m` |
| `0x10414` | `0x10210414` | `CDC_NPU_VP_RICH_K` | `k` |
| `0x10418` | `0x10210418` | `CDC_NPU_VP_RICH_N` | `n` |
| `0x1041C` | `0x1021041C` | `CDC_NPU_VP_RICH_KERNEL_H` | `kh` |
| `0x10420` | `0x10210420` | `CDC_NPU_VP_RICH_KERNEL_W` | `kw` |
| `0x10424` | `0x10210424` | `CDC_NPU_VP_RICH_STRIDE` | `stride` |
| `0x10428` | `0x10210428` | `CDC_NPU_VP_RICH_PADDING` | `pad` |
| `0x1042C` | `0x1021042C` | `CDC_NPU_VP_RICH_ACT_TYPE` | `act_type` |
| `0x10430` | `0x10210430` | `CDC_NPU_VP_RICH_HAS_SKIP` | `has_skip` |
| `0x10434` | `0x10210434` | `CDC_NPU_VP_RICH_SKIP_ADDR` | `skip_addr` |
| `0x10438` | `0x10210438` | `CDC_NPU_VP_RICH_IN_SCALE` | `in_scale` |
| `0x1043C` | `0x1021043C` | `CDC_NPU_VP_RICH_WEIGHT_SCALE` | `w_scale` |
| `0x10440` | `0x10210440` | `CDC_NPU_VP_RICH_OUT_SCALE` | `out_scale` |
| `0x10444` | `0x10210444` | `CDC_NPU_VP_RICH_Q_GAMMA_A_ADDR` | `q_addr`, `gamma_addr`, `a_addr` |
| `0x10448` | `0x10210448` | `CDC_NPU_VP_RICH_K_B_ADDR` | `k_addr`, `b_addr` |
| `0x1044C` | `0x1021044C` | `CDC_NPU_VP_RICH_V_BETA_ADDR` | `v_addr`, `beta_addr` |
| `0x10450` | `0x10210450` | `CDC_NPU_VP_RICH_SEQ_LEN` | `seq_len`, `len` |
| `0x10454` | `0x10210454` | `CDC_NPU_VP_RICH_HEADS_DIM_MODE` | `num_heads`, `dim`, `mode` |
| `0x10458` | `0x10210458` | `CDC_NPU_VP_RICH_HEAD_DIM_EPS` | `head_dim`, `eps_shift`, `scale_a` |
| `0x1045C` | `0x1021045C` | `CDC_NPU_VP_RICH_ATTN_SCALE` | `attn_scale`, `scale_b` |
| `0x10460` | `0x10210460` | `CDC_NPU_VP_RICH_SCALE_OUT` | `scale_out` |
| `0x10464` | `0x10210464` | `CDC_NPU_VP_RICH_A_LEN` | `a_len` |
| `0x10468` | `0x10210468` | `CDC_NPU_VP_RICH_B_LEN` | `b_len` |

The scale fields use IEEE-754 binary32 MMIO bit patterns.

For `ELEM_WISE`, write the total output element count to
`CDC_NPU_VP_RICH_SEQ_LEN`, and write the valid source element counts to
`CDC_NPU_VP_RICH_A_LEN` and `CDC_NPU_VP_RICH_B_LEN` before pushing the
instruction. V4.4 uses these lengths for operand broadcasting. A zero source
length selects the fallback behavior implemented by the model.

Rich instruction values decoded by V4.4 are:

| Type | Value | V4.4 name |
| --- | ---: | --- |
| Opcode | `0x05` | `SET_NSPLIT` |
| Opcode | `0x12` | `GEMM_FUSED` |
| Opcode | `0x13` | `FUSED_ATTN` |
| Opcode | `0x14` | `LAYERNORM` |
| Opcode | `0x15` | `ELEM_WISE` |
| Activation | `0` | None |
| Activation | `1` | ReLU |
| Activation | `2` | SiLU |
| Activation | `3` | GELU |
| Element-wise mode | `0` | Add |
| Element-wise mode | `1` | Max pool |
| Element-wise mode | `2` | Multiply |
| Element-wise mode | `3` | Subtract |
| Element-wise mode | `4` | Divide |

For packed V4.4 writes to `CDC_NPU_VP_RICH_HEADS_DIM_MODE`, bits `15..0`
hold `num_heads`, bits `23..16` hold `dim`, and bits `31..24` hold `mode`.
If bits `31..16` are zero, V4.4 applies the complete value to all three
legacy interpretations.

## Raw V4.4 Performance Counters

The wrapper exposes the raw `fx1::PerfCounters` fields that V4.4 modules
directly update. Each 64-bit counter is a read-only low/high pair. Derived
getter results and override/placeholder fields are not assigned MMIO offsets.

| Low offset | High offset | Low physical | High physical | Counter |
| ---: | ---: | ---: | ---: | --- |
| `0x1200` | `0x1240` | `0x10201200` | `0x10201240` | Execution cycles |
| `0x1204` | `0x1244` | `0x10201204` | `0x10201244` | Stall cycles |
| `0x1208` | `0x1248` | `0x10201208` | `0x10201248` | MAC operations |
| `0x120C` | `0x124C` | `0x1020120C` | `0x1020124C` | Active PE cycles |
| `0x1210` | `0x1250` | `0x10201210` | `0x10201250` | Total PE cycles |
| `0x1214` | `0x1254` | `0x10201214` | `0x10201254` | Total cycles |
| `0x1218` | `0x1258` | `0x10201218` | `0x10201258` | Accumulated rich-operation M dimension |
| `0x121C` | `0x125C` | `0x1020121C` | `0x1020125C` | Accumulated rich-operation K dimension |
| `0x1220` | `0x1260` | `0x10201220` | `0x10201260` | Accumulated rich-operation N dimension |
| `0x1224` | `0x1264` | `0x10201224` | `0x10201264` | Systolic-array active cycles |
| `0x1228` | `0x1268` | `0x10201228` | `0x10201268` | OBP active cycles |
| `0x1280` | `0x1300` | `0x10201280` | `0x10201300` | Processing cycles |
| `0x1284` | `0x1304` | `0x10201284` | `0x10201304` | Transfer cycles |
| `0x1288` | `0x1308` | `0x10201288` | `0x10201308` | MAC-engine cycles |
| `0x128C` | `0x130C` | `0x1020128C` | `0x1020130C` | DMA-engine cycles |
| `0x1290` | `0x1310` | `0x10201290` | `0x10201310` | Activation-engine cycles |
| `0x1294` | `0x1314` | `0x10201294` | `0x10201314` | Pooling-engine cycles |
| `0x1298` | `0x1318` | `0x10201298` | `0x10201318` | Reduction-engine cycles |
| `0x129C` | `0x131C` | `0x1020129C` | `0x1020131C` | DDR read bytes |
| `0x12A0` | `0x1320` | `0x102012A0` | `0x10201320` | DDR write bytes |
| `0x12A4` | `0x1324` | `0x102012A4` | `0x10201324` | Raw DMA read cycles |
| `0x12A8` | `0x1328` | `0x102012A8` | `0x10201328` | Raw DMA write cycles |

Read a counter as:

```c
uint64_t value =
    ((uint64_t)mmio_read32(CDC_NPU0_BASE + high_offset) << 32) |
    mmio_read32(CDC_NPU0_BASE + low_offset);
```

PE utilization and stall fraction are software calculations. The wrapper does
not create counter values that are absent from V4.4 instrumentation hooks.

## CDC 64x64 GEMM Bank

The software-facing 64x64 GEMM bank starts at:

```text
VP offset:        0x30000
Physical base:    0x10230000
Firmware symbol:  CDC_NPU_WRAPPER_BASE
```

The table's local offset is relative to `0x10230000`. Firmware definitions
such as `CDC_NPU_CTRL` already include `CDC_NPU_WRAPPER_BASE`; use them as
`CDC_NPU0_BASE + CDC_NPU_CTRL`.

| Local offset | Physical address | Register | Access | Description |
| ---: | ---: | --- | --- | --- |
| `0x0000` | `0x10230000` | `CTRL` | RW/pulse | `ENABLE[0]`, `START[1]`, `SOFT_RESET[2]`, `IRQ_EN[3]` |
| `0x0004` | `0x10230004` | `STATUS` | RO/W1C | `BUSY[0]`, `DONE[1]`, `ERROR[2]`, `IDLE[3]` |
| `0x0008` | `0x10230008` | `IRQ_ENABLE` | RW | Per-cause `DONE[0]`, `ERROR[1]` |
| `0x000C` | `0x1023000C` | `IRQ_STATUS` | RO/W1C | Sticky done/error causes |
| `0x0010` | `0x10230010` | `SRC_ADDR` | RW | Physical address of row-major `A[64][K]` |
| `0x0014` | `0x10230014` | `DST_ADDR` | RW | Physical address of row-major `C[64][64]` |
| `0x0018` | `0x10230018` | `SCRATCH_ADDR` | RW | Reserved |
| `0x001C` | `0x1023001C` | `SRC_SIZE_BYTES` | RW | Source span including row stride |
| `0x0020` | `0x10230020` | `DST_SIZE_BYTES` | RW | Must be at least `16384` |
| `0x0024` | `0x10230024` | `WIDTH` | RW | Must be `64` |
| `0x0028` | `0x10230028` | `HEIGHT` | RW | Must be `64` |
| `0x002C` | `0x1023002C` | `SRC_STRIDE_BYTES` | RW | Bytes between A rows; zero means `K` |
| `0x0030` | `0x10230030` | `FORMAT` | RW | `1` means INT8 x INT8 to INT32 |
| `0x0034` | `0x10230034` | `OP_MODE` | RW | `0` means GEMM |
| `0x0038` | `0x10230038` | `WEIGHTS_ADDR` | RW | Physical address of row-major `B[K][64]` |
| `0x003C` | `0x1023003C` | `PARAM_ADDR` | RW | Reserved |
| `0x0040` | `0x10230040` | `WEIGHTS_SIZE_BYTES` | RW | Must be at least `K * 64` |
| `0x1000` | `0x10231000` | `K_DIMENSION` | RW | Reduction dimension `1..960` |
| `0x1004` | `0x10231004` | `ZERO_THRESHOLD_FP32` | RW | IEEE-754 binary32 bits |
| `0x1008` | `0x10231008` | `ROWS_ACTIVE` | RW | Active-row mask |
| `0x100C` | `0x1023100C` | `DILATION_PATTERN` | RW | Feeder dilation pattern |
| `0x1010` | `0x10231010` | `CYCLE_COUNT` | RO | Core cycles for the last job |
| `0x1014` | `0x10231014` | `BYTES_READ` | RO | RAM bytes read for the last job |
| `0x1018` | `0x10231018` | `BYTES_WRITTEN` | RO | RAM bytes written for the last job |
| `0x101C` | `0x1023101C` | `LAST_ERROR` | RO | Software GEMM error code |
| `0x1020` | `0x10231020` | `CORE_ID` | RO | `0x53413432` (`"SA42"`) |
| `0x1100` | `0x10231100` | `INPUT_OFFSET` | RW | Quantized input offset |
| `0x1104` | `0x10231104` | `WEIGHT_OFFSET` | RW | Quantized weight offset |
| `0x1108` | `0x10231108` | `OUTPUT_OFFSET` | RW | Quantized output offset |
| `0x110C` | `0x1023110C` | `ACTIVATION_MIN` | RW | Output clamp minimum |
| `0x1110` | `0x10231110` | `ACTIVATION_MAX` | RW | Output clamp maximum |
| `0x1114` | `0x10231114` | `BIAS_ADDR` | RW | Physical bias-buffer address |
| `0x1118` | `0x10231118` | `BIAS_SIZE_BYTES` | RW | Bias-buffer size |
| `0x111C` | `0x1023111C` | `MULTIPLIER_ADDR` | RW | Physical multiplier-buffer address |
| `0x1120` | `0x10231120` | `MULTIPLIER_SIZE_BYTES` | RW | Multiplier-buffer size |
| `0x1124` | `0x10231124` | `SHIFT_ADDR` | RW | Physical shift-buffer address |
| `0x1128` | `0x10231128` | `SHIFT_SIZE_BYTES` | RW | Shift-buffer size |

The software-facing operation is:

```text
C[64][64] = A[64][K] x B[K][64]
```

`A` and `B` are signed INT8, `C` is signed INT32, and `K` is `1..960`.

### Start And Interrupt Sequence

1. Program buffers, sizes, dimensions, format, and operation while
   `STATUS.BUSY=0`.
2. Enable desired causes in `IRQ_ENABLE`.
3. Write `CTRL.ENABLE=1`; also set `CTRL.IRQ_EN=1` when using interrupts.
4. Write the same value with `CTRL.START=1`.
5. Poll `STATUS`, or handle PLIC source 17.
6. Clear the cause in `IRQ_STATUS` before completing the PLIC claim.
7. Clear sticky `STATUS.DONE` or `STATUS.ERROR` by writing one to that bit.

```text
irq_out = CTRL.IRQ_EN && ((IRQ_ENABLE & IRQ_STATUS) != 0)
```

### Error Codes

| Value | Firmware symbol | Meaning |
| ---: | --- | --- |
| `0` | `CDC_NPU_ERROR_NONE` | No error |
| `1` | `CDC_NPU_ERROR_DISABLED` | Start requested while disabled |
| `2` | `CDC_NPU_ERROR_BUSY` | Request made while busy |
| `3` | `CDC_NPU_ERROR_INVALID_DIMENSIONS` | Unsupported dimensions |
| `4` | `CDC_NPU_ERROR_INVALID_FORMAT` | Unsupported data format |
| `5` | `CDC_NPU_ERROR_INVALID_OPERATION` | Unsupported operation |
| `6` | `CDC_NPU_ERROR_INVALID_ADDRESS` | Buffer outside system RAM |
| `7` | `CDC_NPU_ERROR_INVALID_SIZE` | Buffer is too small |
| `8` | `CDC_NPU_ERROR_DMA_READ` | RAM read failed |
| `9` | `CDC_NPU_ERROR_DMA_WRITE` | RAM write failed |
| `10` | `CDC_NPU_ERROR_CORE_DEADLOCK` | V4.4 feeder deadlock |
| `11` | `CDC_NPU_ERROR_CORE_TIMEOUT` | V4.4 core timed out |
| `12` | `CDC_NPU_ERROR_RESET_ABORTED` | External reset aborted work |

## Model Files That Are Not MMIO Specifications

The following V4.4 files define driver packing, testbench, target, or stimulus
formats. They do not create additional CDC-VP MMIO offsets:

```text
sauria_cfg_layout.h
sauria_targets.h
driver/libsauria_cfg.h
driver/libsauria_mem.h
driver/sauria_run.h
driver/sauria_stim.h
driver/sauria_golden.h
```

## Authoritative MMIO Sources

| File | MMIO responsibility |
| --- | --- |
| `platforms/VP_FX1_Full_SoC/configs/default.yaml` | NPU physical aperture |
| `platforms/VP_FX1_Full_SoC/src/vp_fx1_full_soc_top.cpp` | Bus range and PLIC source 17 |
| `components/npu_tlm/include/npu_tlm_regmap.h` | C++ MMIO constants |
| `components/npu_tlm/src/npu_tlm.cpp` | Address decode, aliases and access behavior |
| `fw/common/include/soc/soc_memory_map.h` | Firmware NPU physical base |
| `fw/common/include/soc/regs/soc_regs_npu_v4.h` | Firmware-visible offsets and fields |
| V4.4 `config_regs.h` and `config_map.h` | Native profile-dependent decode |
| V4.4 `sram/sram_top.h` | Native SRAM host decode |
| V4.4 `control/instruction_decoder.h` | Rich write-register decode |
| V4.4 `instrumentation/perf_counters.h` | Raw counter storage and derived metric helpers |

## MMIO Contract Limits

- Rich instruction registers are write-only because V4.4 defines no host
  readback path for them.
- Native and rich completion update the software-bank `STATUS` and `IRQ_STATUS`;
  enable its IRQ controls to receive completion on PLIC source 17.
- Native SRAM windows use V4.4 row/subword host encoding, not linear byte
  addressing.
- Only raw fields directly updated by V4.4 modules have counter offsets.
- The 64x64 GEMM bank is a CDC-VP extension. It is not part of the native
  V4.4 register map; native and rich interfaces remain available separately.
