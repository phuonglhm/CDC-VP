# SPDX-License-Identifier: Apache-2.0
#
# The Phase 9 endpoint gate. It needs no mesh: the endpoint's contract is about
# how a transfer is split and where it is allowed to go, and both are visible
# with plain TLM stubs on either side. Attaching a real `noc_interconnect` is
# the platform's gate, not this one's.

add_executable(test_tpu_v3_noc_endpoint test_chip_noc_endpoint.cpp)
target_link_libraries(test_tpu_v3_noc_endpoint PRIVATE
    cdc::components::tpu_v3_noc_endpoint)
add_test(NAME tpu_v3_noc_endpoint COMMAND test_tpu_v3_noc_endpoint)
set_tests_properties(tpu_v3_noc_endpoint PROPERTIES
    LABELS "tpu_v3;unit;noc"
    TIMEOUT 300)

# Decision record D25. The one property the stub gate above cannot establish,
# because the stub is not what enforces it: a chip aperture is registered
# `target_kind::mmio`, and `noc_interconnect` refuses a full-width read against
# an mmio target whose address or length is not bus-aligned -- before
# injection, with the target never called. Whether the endpoint's read shaping
# is *sufficient* is therefore a question only the real interconnect can
# answer.
#
# This is the single place `tpu_v3_noc_endpoint` touches `noc_interconnect`,
# and it is a test rather than the library: the component still declares no
# dependency on the mesh, which is what keeps its own gate mesh-free and stops
# it reaching into the interconnect for something it should have been told.
add_executable(test_tpu_v3_endpoint_on_real_noc test_endpoint_on_real_noc.cpp)
target_link_libraries(test_tpu_v3_endpoint_on_real_noc PRIVATE
    cdc::components::tpu_v3_noc_endpoint
    cdc::components::noc_interconnect)
add_test(NAME tpu_v3_endpoint_on_real_noc
         COMMAND test_tpu_v3_endpoint_on_real_noc)
set_tests_properties(tpu_v3_endpoint_on_real_noc PROPERTIES
    LABELS "tpu_v3;integration;noc"
    TIMEOUT 600)
