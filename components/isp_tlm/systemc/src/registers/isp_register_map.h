#ifndef ISP_TLM_REGISTERS_ISP_REGISTER_MAP_H
#define ISP_TLM_REGISTERS_ISP_REGISTER_MAP_H

#include <cstdint>

namespace isp_tlm::registers {

// Canonical software-visible address space. All addresses are byte addresses.
inline constexpr std::uint32_t kAddressSpaceBytes = 0x00010000u;
inline constexpr std::uint32_t kRegisterBytes = 4u;

constexpr std::uint32_t module_register(std::uint32_t module_id,
                                        std::uint32_t register_id) {
    return (module_id << 9u) | (register_id << 2u);
}

// ISP identification, control, interrupt, job, and descriptor registers.
inline constexpr std::uint32_t kIspReset = 0x0000u;
inline constexpr std::uint32_t kIspSensorWidth = 0x0004u;
inline constexpr std::uint32_t kIspSensorHeight = 0x0008u;
inline constexpr std::uint32_t kIspCropWidth = 0x000Cu;
inline constexpr std::uint32_t kIspCropHeight = 0x0010u;
inline constexpr std::uint32_t kIspBits = 0x0014u;
inline constexpr std::uint32_t kIspBayer = 0x0018u;
inline constexpr std::uint32_t kIspTopEnable = 0x0040u;
inline constexpr std::uint32_t kIspInterruptStatus = 0x0044u;
inline constexpr std::uint32_t kIspInterruptMask = 0x0048u;

inline constexpr std::uint32_t kIspJobControl = 0x0080u;
inline constexpr std::uint32_t kIspJobStatus = 0x0084u;
inline constexpr std::uint32_t kIspJobErrorCode = 0x0088u;
inline constexpr std::uint32_t kIspFrameCount = 0x008Cu;

inline constexpr std::uint32_t kIspSourceAddress = 0x0100u;
inline constexpr std::uint32_t kIspSourceStrideBytes = 0x0104u;
inline constexpr std::uint32_t kIspSourceSizeBytes = 0x0108u;
inline constexpr std::uint32_t kIspDestinationYAddress = 0x010Cu;
inline constexpr std::uint32_t kIspDestinationUAddress = 0x0110u;
inline constexpr std::uint32_t kIspDestinationVAddress = 0x0114u;
inline constexpr std::uint32_t kIspDestinationYStrideBytes = 0x0118u;
inline constexpr std::uint32_t kIspDestinationUvStrideBytes = 0x011Cu;
inline constexpr std::uint32_t kIspDestinationYSizeBytes = 0x0120u;
inline constexpr std::uint32_t kIspDestinationUSizeBytes = 0x0124u;
inline constexpr std::uint32_t kIspDestinationVSizeBytes = 0x0128u;
inline constexpr std::uint32_t kIspOutputWidth = 0x012Cu;
inline constexpr std::uint32_t kIspOutputHeight = 0x0130u;
inline constexpr std::uint32_t kIspOutputFormat = 0x0134u;
inline constexpr std::uint32_t kIspLastJobReadBytes = 0x0140u;
inline constexpr std::uint32_t kIspLastJobWriteBytes = 0x0144u;
inline constexpr std::uint32_t kIspLastJobReadTransactions = 0x0148u;
inline constexpr std::uint32_t kIspLastJobWriteTransactions = 0x014Cu;
inline constexpr std::uint32_t kIspTlmErrorCount = 0x0150u;

inline constexpr std::uint32_t kJobControlSourceMode = 1u << 0u;
inline constexpr std::uint32_t kJobControlStart = 1u << 1u;
inline constexpr std::uint32_t kJobControlDirectRgbInput = 1u << 2u;
inline constexpr std::uint32_t kJobStatusBusy = 1u << 0u;
inline constexpr std::uint32_t kJobStatusDone = 1u << 1u;
inline constexpr std::uint32_t kJobStatusError = 1u << 2u;
inline constexpr std::uint32_t kJobErrorNone = 0u;
inline constexpr std::uint32_t kJobErrorCommandWhileBusy = 1u;
inline constexpr std::uint32_t kJobErrorUnsupportedConfiguration = 2u;
inline constexpr std::uint32_t kJobErrorSourceDescriptor = 3u;
inline constexpr std::uint32_t kJobErrorMemoryRead = 4u;
inline constexpr std::uint32_t kJobErrorOutputFrame = 5u;
inline constexpr std::uint32_t kJobErrorDestinationDescriptor = 6u;
inline constexpr std::uint32_t kJobErrorMemoryWrite = 7u;

inline constexpr std::uint32_t kInterruptFrameStart = 1u << 0u;
inline constexpr std::uint32_t kInterruptFrameDone = 1u << 1u;
inline constexpr std::uint32_t kInterruptAeDone = 1u << 2u;
inline constexpr std::uint32_t kInterruptAwbDone = 1u << 3u;
inline constexpr std::uint32_t kInterruptJobDone = 1u << 4u;
inline constexpr std::uint32_t kInterruptJobError = 1u << 5u;
inline constexpr std::uint32_t kInterruptMaskAll = 0x3Fu;

// Existing mode-0 ISP scalar registers and packed arrays.
inline constexpr std::uint32_t kDpcThreshold = 0x0200u;

inline constexpr std::uint32_t kBlcR = 0x0400u;
inline constexpr std::uint32_t kBlcGr = 0x0404u;
inline constexpr std::uint32_t kBlcGb = 0x0408u;
inline constexpr std::uint32_t kBlcB = 0x040Cu;
inline constexpr std::uint32_t kLinearR = 0x0410u;
inline constexpr std::uint32_t kLinearGr = 0x0414u;
inline constexpr std::uint32_t kLinearGb = 0x0418u;
inline constexpr std::uint32_t kLinearB = 0x041Cu;

inline constexpr std::uint32_t kAeCenterIlluminance = 0x0600u;
inline constexpr std::uint32_t kAeSkewness = 0x0604u;
inline constexpr std::uint32_t kAeCropLeft = 0x0608u;
inline constexpr std::uint32_t kAeCropRight = 0x060Cu;
inline constexpr std::uint32_t kAeCropTop = 0x0610u;
inline constexpr std::uint32_t kAeCropBottom = 0x0614u;
inline constexpr std::uint32_t kAeResponse = 0x0618u;
inline constexpr std::uint32_t kAeResultSkewness = 0x061Cu;
inline constexpr std::uint32_t kAeResponseDebug = 0x0620u;
inline constexpr std::uint32_t kAeDone = 0x0624u;

inline constexpr std::uint32_t kDgainIsManual = 0x0800u;
inline constexpr std::uint32_t kDgainManualIndex = 0x0804u;
inline constexpr std::uint32_t kDgainIndexOut = 0x0808u;
inline constexpr std::uint32_t kDgainArrayBase = 0x0840u;
inline constexpr std::uint32_t kDgainArrayWords = 100u;

inline constexpr std::uint32_t kAwbUnderexposedLimit = 0x0C00u;
inline constexpr std::uint32_t kAwbOverexposedLimit = 0x0C04u;
inline constexpr std::uint32_t kAwbFrames = 0x0C08u;
inline constexpr std::uint32_t kAwbFinalRGain = 0x0C0Cu;
inline constexpr std::uint32_t kAwbFinalBGain = 0x0C10u;

inline constexpr std::uint32_t kWbRGain = 0x0E00u;
inline constexpr std::uint32_t kWbBGain = 0x0E04u;

inline constexpr std::uint32_t kCcmRr = 0x1200u;
inline constexpr std::uint32_t kCcmRg = 0x1204u;
inline constexpr std::uint32_t kCcmRb = 0x1208u;
inline constexpr std::uint32_t kCcmGr = 0x120Cu;
inline constexpr std::uint32_t kCcmGg = 0x1210u;
inline constexpr std::uint32_t kCcmGb = 0x1214u;
inline constexpr std::uint32_t kCcmBr = 0x1218u;
inline constexpr std::uint32_t kCcmBg = 0x121Cu;
inline constexpr std::uint32_t kCcmBb = 0x1220u;

inline constexpr std::uint32_t kCscConversionStandard = 0x1400u;

inline constexpr std::uint32_t kSharpenStrength = 0x1C00u;
inline constexpr std::uint32_t kSharpenKernelBase = 0x1C40u;
inline constexpr std::uint32_t kSharpenKernelWords = 81u;

inline constexpr std::uint32_t kBnrSpatialRBase = 0x2000u;
inline constexpr std::uint32_t kBnrSpatialGBase = 0x2040u;
inline constexpr std::uint32_t kBnrSpatialBBase = 0x2080u;
inline constexpr std::uint32_t kBnrSpatialWordsPerChannel = 10u;
inline constexpr std::uint32_t kBnrColorRBase = 0x2100u;
inline constexpr std::uint32_t kBnrColorGBase = 0x2140u;
inline constexpr std::uint32_t kBnrColorBBase = 0x2180u;
inline constexpr std::uint32_t kBnrColorWordsPerChannel = 9u;

inline constexpr std::uint32_t kNr2dDifferenceBase = 0x2A00u;
inline constexpr std::uint32_t kNr2dDifferenceWords = 8u;
inline constexpr std::uint32_t kNr2dWeightBase = 0x2A40u;
inline constexpr std::uint32_t kNr2dWeightWords = 8u;

// Indexed OECF programming interface. Direct legacy mode-2/mode-3 LUT
// windows are deliberately absent from the canonical 64 KiB map.
inline constexpr std::uint32_t kOecfLutControl = 0x2C00u;
inline constexpr std::uint32_t kOecfLutIndex = 0x2C04u;
inline constexpr std::uint32_t kOecfLutData = 0x2C08u;
inline constexpr std::uint32_t kOecfChannelMask = 0x3u;
inline constexpr std::uint32_t kOecfAutoIncrement = 1u << 8u;

// Sole VIP. These preserve the former first-VIP offsets without retaining a
// numbered VIP in the public ABI.
inline constexpr std::uint32_t kVipReset = 0x4000u;
inline constexpr std::uint32_t kVipWidth = 0x4004u;
inline constexpr std::uint32_t kVipHeight = 0x4008u;
inline constexpr std::uint32_t kVipBits = 0x400Cu;
inline constexpr std::uint32_t kVipTopEnable = 0x4040u;
inline constexpr std::uint32_t kVipInterruptStatus = 0x4044u;
inline constexpr std::uint32_t kVipInterruptMask = 0x4048u;
inline constexpr std::uint32_t kVipRgbConversionStandard = 0x4200u;
inline constexpr std::uint32_t kVipIrcX = 0x4400u;
inline constexpr std::uint32_t kVipIrcY = 0x4404u;
inline constexpr std::uint32_t kVipIrcOutput = 0x4408u;
inline constexpr std::uint32_t kVipScaleInputCropWidth = 0x4600u;
inline constexpr std::uint32_t kVipScaleInputCropHeight = 0x4604u;
inline constexpr std::uint32_t kVipScaleOutputCropWidth = 0x4608u;
inline constexpr std::uint32_t kVipScaleOutputCropHeight = 0x460Cu;
inline constexpr std::uint32_t kVipScaleDownscaleWidth = 0x4610u;
inline constexpr std::uint32_t kVipScaleDownscaleHeight = 0x4614u;
inline constexpr std::uint32_t kVipOsdX = 0x4800u;
inline constexpr std::uint32_t kVipOsdY = 0x4804u;
inline constexpr std::uint32_t kVipOsdWidth = 0x4808u;
inline constexpr std::uint32_t kVipOsdHeight = 0x480Cu;
inline constexpr std::uint32_t kVipOsdForegroundColor = 0x4810u;
inline constexpr std::uint32_t kVipOsdBackgroundColor = 0x4814u;
inline constexpr std::uint32_t kVipOsdAlpha = 0x4818u;
inline constexpr std::uint32_t kVipOutputFormat = 0x4A00u;

inline constexpr std::uint32_t kVipInterruptFrameStart = 1u << 0u;
inline constexpr std::uint32_t kVipInterruptFrameDone = 1u << 1u;
inline constexpr std::uint32_t kVipInterruptMaskAll = 0x3u;

// Fixed architectural output format value.
inline constexpr std::uint32_t kOutputFormatI420 = 0u;

// Preserved RTL array-window decoding in the canonical address space.
inline constexpr std::uint32_t kGammaWindowBase = 0x8000u;
inline constexpr std::uint32_t kGammaWindowEnd = 0xBFFFu;
inline constexpr std::uint32_t kVipOsdWindowBase = 0xC000u;
inline constexpr std::uint32_t kVipOsdWindowEnd = 0xDFFFu;
inline constexpr std::uint32_t kVipOsdCanonicalBytes = 0x0800u;
inline constexpr std::uint32_t kVipOsdWords = 512u;

}  // namespace isp_tlm::registers

#endif  // ISP_TLM_REGISTERS_ISP_REGISTER_MAP_H
