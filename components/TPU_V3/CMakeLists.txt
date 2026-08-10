# SPDX-License-Identifier: Apache-2.0
#
# TPU_V3 component tree. See docs/TPU_V3_IMPLEMENTATION_PLAN.md.
#
# Subdirectories appear here as their phase gate is reached, not before. An
# empty placeholder library would export a target name that links and does
# nothing, which is worse than a missing target: a platform could depend on it
# and appear to work.
#
#   Phase 1  common                 <- here
#   Phase 3  shared_vector_memory
#   Phase 4  mxu
#   Phase 5  tpu_core
#   Phase 6  tpu_chip
#   Phase 7  noc_endpoint

add_subdirectory(common)
