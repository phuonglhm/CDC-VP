// SPDX-License-Identifier: Apache-2.0

#include "tpu_v3/sauria/sauria_matrix_if.h"

namespace cdc::components::tpu_v3::sauria {

const char* to_string(submit_status status) noexcept
{
    switch (status) {
    case submit_status::accepted:
        return "accepted";
    case submit_status::busy:
        return "refused: the engine is already running a job";
    case submit_status::invalid_dimension:
        return "refused: M, N or K is zero or larger than the engine can "
               "sequence";
    case submit_status::dimension_exceeds_array:
        return "refused: the problem is larger than one pass of the array. "
               "Phase 5 does not tile, so a GEMM whose N exceeds the array's "
               "columns or whose M exceeds its rows must be split by the "
               "caller. A silently truncated result is the one outcome this "
               "phase exists to make impossible";
    case submit_status::invalid_address:
        return "refused: an operand or result region leaves the core-SRAM "
               "window";
    case submit_status::invalid_stride:
        return "refused: a row stride is smaller than the row it describes, so "
               "rows would overlap";
    case submit_status::region_overlap:
        return "refused: A, B and C overlap in a way that makes the result "
               "depend on access order";
    case submit_status::datatype_unsupported:
        return "refused: this engine does not implement the requested "
               "datatype; read the capability register for what it does";
    case submit_status::accumulation_unsupported:
        return "refused: accumulation and C preload are not available in Phase "
               "5 (decision record D17). Writeback is not atomic, so "
               "accumulating into a region a failed job partially wrote has no "
               "settled meaning";
    case submit_status::staging_capacity_exceeded:
        return "refused: the GEMM fits one array pass but its K dimension "
               "exceeds this engine's private operand-staging capacity";
    }
    return "unknown submit status";
}

} // namespace cdc::components::tpu_v3::sauria
