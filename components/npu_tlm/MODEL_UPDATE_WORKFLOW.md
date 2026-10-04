# Integrate A New NPU Model

Example: replace the current model with V4.7.

## 1. Take And Check The New Model

Put the model-team delivery under:

```text
components/npu_tlm/models/v4.7_model/
```

Do not edit files inside the model directory. Compare it with the previously
integrated model and read its integration guide:

```bash
diff -qr components/npu_tlm/models/v4.6_model \
         components/npu_tlm/models/v4.7_model
```

Check which top level must be used. V4.7 uses:

```text
components/npu_tlm/models/v4.7_model/core_rtl
```

## 2. Integrate It Into CDC-VP

Update only the integration-owned files that are affected:

```text
components/npu_tlm/CMakeLists.txt
components/npu_tlm/include/npu_tlm.h
components/npu_tlm/include/npu_tlm_regmap.h
components/npu_tlm/src/npu_tlm.cpp
components/npu_tlm/tests/test_npu_tlm.cpp
components/npu_tlm/README.md
fw/common/include/soc/regs/soc_regs_npu_v4.h
```

Update the selected model root, MMIO registers, memory/address translation,
completion/IRQ behavior, counters, tests, and firmware definitions according
to the new model source. Do not add registers or features that are not
implemented by the model.

Edit the SoC platform files only if the model needs a different NPU address
range, clock, interrupt, or RAM connection.

## 3. Build And Package

```bash
CDC_VP=/path/to/CDC-VP
cd "$CDC_VP"

cmake -S . -B build-soc \
  -DCDC_ENABLE_SAURIA_NPU_V4=ON \
  -DSAURIA_NPU_ROOT= \
  -DSYSTEMC_INCLUDE_DIR=/opt/systemc-2.3.4/include \
  -DSYSTEMC_LIBRARY=/opt/systemc-2.3.4/lib-linux64/libsystemc.so

cmake --build build-soc \
  --target vp_fx1_full_soc \
  --clean-first \
  -j"$(nproc)"

cmake --build build-soc \
  --target vp_fx1_full_soc_package \
  -j"$(nproc)"
```

## 4. Copy To fx1 And Test

The fx1 repository is:

```text
https://github.com/duyphan95z/fx1.git
```

Clone it first if needed:

```bash
git clone https://github.com/duyphan95z/fx1.git
```

```bash
CDC_VP=/path/to/CDC-VP
fx1=/path/to/fx1
fx1_bin="$fx1/vp/bin/VP_FX1_V2.0"
fx1_fw="$fx1/sw/bootloader/sources/VP_FX1_V2.0"

cp "$CDC_VP/out/vp_fx1_full_soc/vp_fx1_full_soc" "$fx1_bin/"
cp "$CDC_VP/out/vp_fx1_full_soc/libsystemc.so" "$fx1_bin/"
cp "$CDC_VP/out/vp_fx1_full_soc/configs/default.yaml" \
   "$fx1_bin/default.yaml"
cp "$CDC_VP/fw/common/include/soc/regs/soc_regs_npu_v4.h" \
   "$fx1_fw/common/include/soc/regs/soc_regs_npu_v4.h"

cd "$fx1"
make -C sw/bootloader/sources/VP_FX1_V2.0 clean
make -C sw/bootloader/sources/VP_FX1_V2.0 NPU=1 TFLM=1
sw/bootloader/test/VP_FX1_V2.0/run_vp.sh smoke
```

A passing smoke test confirms that the packaged VP boots and the existing fx1
NPU/TFLM software flow still works with the new model integration.
