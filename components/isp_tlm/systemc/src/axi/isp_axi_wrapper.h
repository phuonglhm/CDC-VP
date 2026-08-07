#ifndef ISP_AXI_WRAPPER_H
#define ISP_AXI_WRAPPER_H

// Compatibility include for code that used the former filename. The old
// private register implementation and its public tuning outputs were removed;
// both AXI and TLM control must now share an externally owned register bank.

#include "axi/isp_axi_lite_adapter.h"

using isp_axi_wrapper = isp_tlm::axi::IspAxiLiteAdapter;

#endif  // ISP_AXI_WRAPPER_H
