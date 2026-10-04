// Build-time "core" adapter interface. NOTE: not used for backend selection -- the real switch is the
// LaneACoreBlockA alias in control/native_lane_a_core.h (-DSAURIA_CORE_BACKEND_RTL_REF).
//
// Describes the "core" boundary towards the OBP: any core that produces these 4 signals can be combined with the
// existing OBP (and obp_inst_a / re_inst_a / rce_inst_a / dma_inst stay unchanged):
//   o_sramc_wdata : psum_vector_t<Y_DIM, T_PSUM>
//   o_sramc_addr  : uint32_t
//   o_sramc_wmask : sramc_mask_t<Y_DIM>
//   o_sramc_wren  : bool
//
// Plain C++ helper classes holding pointers to NpuTop's existing public modules (no new sc_module / sc_signal).
#ifndef SAURIA_CORE_ADAPTER_H
#define SAURIA_CORE_ADAPTER_H

#include <cstdint>
#include <string>

namespace sauria
{
    // NativeCoreAdapter: wraps this checkout's existing ctrl_inst_a / psm_inst_a. No new wiring.
    template <typename NpuT>
    class NativeCoreAdapter
    {
    public:
        explicit NativeCoreAdapter(NpuT *npu) : npu_(npu) {}

        std::string state_name() const
        {
            return npu_->ctrl_inst_a->state_name(npu_->ctrl_inst_a->get_state());
        }
        bool is_active() const { return npu_->ctrl_inst_a->o_active.read(); }

        // Raw-PSUM stream this core hands to the OBP.
        bool psum_wren() const { return npu_->psm_inst_a->o_sramc_wren.read(); }
        uint32_t psum_addr() const { return npu_->psm_inst_a->o_sramc_addr.read(); }
        auto psum_wdata() const { return npu_->psm_inst_a->o_sramc_wdata.read(); }

    private:
        NpuT *npu_;
    };

    // RtlRefCoreAdapter: deliberate fail-to-compile stub (never returns fake data). The RTL-accurate core itself is
    // RtlRefLaneACoreA in control/native_lane_a_core.h.
    template <typename NpuT>
    class RtlRefCoreAdapter
    {
        static_assert(sizeof(NpuT) == 0,
            "RtlRefCoreAdapter is a stub and cannot be instantiated. "
            "Use RtlRefLaneACoreA (control/native_lane_a_core.h, -DSAURIA_CORE_BACKEND_RTL_REF) for the RTL-accurate core "
            "(context_fsm.h/feeders_fsm.h/psm_shift_fsm.h/psm_idxcnt.h/psm_rdata_manager.h/"
            "psm_wdata_manager.h). It must not be replaced by fake data.");
    public:
        explicit RtlRefCoreAdapter(NpuT *) {}
    };

#if defined(SAURIA_CORE_BACKEND_RTL_REF)
    template <typename NpuT>
    using CoreAdapter = RtlRefCoreAdapter<NpuT>;
#else
    template <typename NpuT>
    using CoreAdapter = NativeCoreAdapter<NpuT>;
#endif

} // namespace sauria

#endif // SAURIA_CORE_ADAPTER_H
