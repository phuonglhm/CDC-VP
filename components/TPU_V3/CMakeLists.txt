# SPDX-License-Identifier: Apache-2.0
#
# TPU_V3 component tree. See docs/TPU_V3_IMPLEMENTATION_PLAN.md.
#
# Subdirectories appear here as their phase gate is reached, not before. An
# empty placeholder library would export a target name that links and does
# nothing, which is worse than a missing target: a platform could depend on it
# and appear to work.
#
#   Phase 1  common                 <- configuration and address map
#   Phase 3  native_port            <- the native local-data interface alone,
#                                    so a requester can depend on it without
#                                    depending on the storage behind it
#   Phase 3  core_sram              <- shared SRAM, sparse page-backed storage
#   Phase 3  tpu_core               <- NEO control / local-SRAM / external
#                                      fabrics (the D15 split). `tpu_core.h`
#                                      itself is Phase 7; the directory exists
#                                      now because the fabrics live in it.
#   Phase 4  neo_dma               <- the independent DMA. Deliberately not the
#                                    shared PL330-style `components/dma_tlm`,
#                                    which three other platforms depend on and
#                                    which this phase does not touch.
#   Phase 5  sauria_matrix        <- the pinned v4.2 int8_64x64 engine,
#                                    behind CDC_BUILD_TPU_V3_SAURIA_MATRIX
#   Phase 6  image_transform
#   Phase 8  tpu_chip
#   Phase 9  noc_endpoint

add_subdirectory(common)
add_subdirectory(native_port)
add_subdirectory(core_sram)
add_subdirectory(tpu_core)
add_subdirectory(neo_dma)

# Phase 5. Returns immediately unless CDC_BUILD_TPU_V3_SAURIA_MATRIX is on, so
# the default TPU_V3 build neither needs the pinned Sauria source nor acquires a
# dependency on it.
add_subdirectory(sauria_matrix)

# Phase 6. The subdirectory returns immediately unless the transform option is
# on. Im2Col is implemented from the pinned v4.2 semantics; Col2Im remains an
# explicit unavailable capability until the NPU team supplies it.
add_subdirectory(image_transform)
