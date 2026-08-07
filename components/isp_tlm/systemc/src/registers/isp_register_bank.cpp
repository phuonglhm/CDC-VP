#include "isp_register_bank.h"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace isp_tlm::registers {
namespace {

constexpr std::uint32_t low_mask(std::uint32_t bits) {
    return bits >= 32u ? 0xFFFFFFFFu : ((1u << bits) - 1u);
}

constexpr std::uint32_t kIspTopEnableMask = 0x0003FFFFu;
constexpr std::uint32_t kIspTopEnableReset = 0x0003EFDFu;
constexpr std::uint32_t kVipTopEnableMask = 0x0000001Fu;
constexpr std::uint32_t kVipTopEnableReset = 0x0000001Fu;

constexpr std::array<std::uint32_t, 81u> kSharpenKernelReset = {
    764u, 1833u, 3424u, 4982u, 5646u, 4982u, 3424u, 1833u, 764u,
    1833u, 4397u, 8215u, 11953u, 13544u, 11953u, 8215u, 4397u, 1833u,
    3424u, 8215u, 15348u, 22331u, 25305u, 22331u, 15348u, 8215u, 3424u,
    4982u, 11953u, 22331u, 32492u, 36819u, 32492u, 22331u, 11953u, 4982u,
    5646u, 13544u, 25305u, 36819u, 41721u, 36819u, 25305u, 13544u, 5646u,
    4982u, 11953u, 22331u, 32492u, 36819u, 32492u, 22331u, 11953u, 4982u,
    3424u, 8215u, 15348u, 22331u, 25305u, 22331u, 15348u, 8215u, 3424u,
    1833u, 4397u, 8215u, 11953u, 13544u, 11953u, 8215u, 4397u, 1833u,
    764u, 1833u, 3424u, 4982u, 5646u, 4982u, 3424u, 1833u, 764u,
};

constexpr std::array<std::uint8_t, 25u> kBnrSpatialReset = {
    0u, 3u, 7u, 3u, 0u,
    3u, 43u, 105u, 43u, 3u,
    7u, 105u, 255u, 105u, 7u,
    3u, 43u, 105u, 43u, 3u,
    0u, 3u, 7u, 3u, 0u,
};

constexpr std::array<std::uint16_t, 9u> kBnrColorXRedBlue = {
    28u, 57u, 85u, 114u, 143u, 171u, 200u, 229u, 257u,
};
constexpr std::array<std::uint16_t, 9u> kBnrColorXGreen = {
    10u, 20u, 30u, 40u, 51u, 61u, 71u, 81u, 92u,
};
constexpr std::array<std::uint8_t, 9u> kBnrColorYRedBlue = {
    250u, 236u, 214u, 186u, 155u, 125u, 96u, 71u, 51u,
};
constexpr std::array<std::uint8_t, 9u> kBnrColorYGreen = {
    250u, 236u, 215u, 188u, 155u, 125u, 97u, 73u, 51u,
};

}  // namespace

const char* to_string(AccessResult result) {
    switch (result) {
        case AccessResult::Ok:
            return "ok";
        case AccessResult::Misaligned:
            return "misaligned";
        case AccessResult::OutOfRange:
            return "out_of_range";
        case AccessResult::Unmapped:
            return "unmapped";
        case AccessResult::ReadOnly:
            return "read_only";
        case AccessResult::InvalidByteStrobe:
            return "invalid_byte_strobe";
        case AccessResult::InvalidValue:
            return "invalid_value";
    }
    return "unknown";
}

IspRegisterBank::IspRegisterBank(BuildConfiguration configuration)
    : configuration_(configuration) {
    if (configuration_.sensor_width == 0u ||
        configuration_.sensor_height == 0u ||
        configuration_.crop_width == 0u ||
        configuration_.crop_height == 0u ||
        configuration_.output_width == 0u ||
        configuration_.output_height == 0u) {
        throw std::invalid_argument("ISP dimensions must be non-zero");
    }
    if (configuration_.bits < 8u || configuration_.bits > 12u) {
        throw std::invalid_argument(
            "ISP bits must be in [8, 12] for the preserved Gamma window");
    }
    if (configuration_.vip_bits == 0u || configuration_.vip_bits > 10u) {
        throw std::invalid_argument("VIP bits must be in [1, 10]");
    }
    if (configuration_.bayer > 3u) {
        throw std::invalid_argument("Bayer identification must be in [0, 3]");
    }
    if (configuration_.output_width > 0xFFFu ||
        configuration_.output_height > 0xFFFu) {
        throw std::invalid_argument(
            "VIP scale dimensions exceed the preserved 12-bit fields");
    }

    const std::size_t lut_size =
        static_cast<std::size_t>(1u << configuration_.bits);
    gamma_pending_.resize(lut_size);
    gamma_active_.resize(lut_size);
    for (auto& channel : oecf_pending_) {
        channel.resize(lut_size);
    }
    for (auto& channel : oecf_active_) {
        channel.resize(lut_size);
    }
    vip_osd_pending_.resize(kVipOsdWords, 0u);
    vip_osd_active_.resize(kVipOsdWords, 0u);

    const std::uint32_t lut_value_mask = low_mask(configuration_.bits);
    for (std::size_t index = 0u; index < lut_size; ++index) {
        const std::uint32_t identity =
            static_cast<std::uint32_t>(index) & lut_value_mask;
        gamma_pending_[index] = identity;
        gamma_active_[index] = identity;
        for (std::size_t channel = 0u; channel < oecf_pending_.size();
             ++channel) {
            oecf_pending_[channel][index] = identity;
            oecf_active_[channel][index] = identity;
        }
    }

    initialize_registers();
    reset();
}

std::uint32_t IspRegisterBank::byte_mask(std::uint8_t byte_strobe) {
    std::uint32_t mask = 0u;
    for (std::uint32_t byte = 0u; byte < 4u; ++byte) {
        if ((byte_strobe & (1u << byte)) != 0u) {
            mask |= 0xFFu << (byte * 8u);
        }
    }
    return mask;
}

AccessResult IspRegisterBank::validate_address(std::uint32_t address) const {
    if (address >= kAddressSpaceBytes) {
        return AccessResult::OutOfRange;
    }
    if ((address & (kRegisterBytes - 1u)) != 0u) {
        return AccessResult::Misaligned;
    }
    return AccessResult::Ok;
}

bool IspRegisterBank::is_gamma_window(std::uint32_t address) const {
    return address >= kGammaWindowBase && address <= kGammaWindowEnd;
}

bool IspRegisterBank::is_vip_osd_window(std::uint32_t address) const {
    return address >= kVipOsdWindowBase && address <= kVipOsdWindowEnd;
}

std::size_t IspRegisterBank::gamma_index(std::uint32_t address) const {
    const std::size_t word =
        static_cast<std::size_t>((address - kGammaWindowBase) / kRegisterBytes);
    return word % gamma_pending_.size();
}

std::size_t IspRegisterBank::vip_osd_index(std::uint32_t address) const {
    const std::uint32_t canonical_offset =
        (address - kVipOsdWindowBase) % kVipOsdCanonicalBytes;
    return static_cast<std::size_t>(canonical_offset / kRegisterBytes);
}

std::size_t IspRegisterBank::selected_oecf_channel() const {
    return static_cast<std::size_t>(
        internal_value(kOecfLutControl) & kOecfChannelMask);
}

std::size_t IspRegisterBank::selected_oecf_index() const {
    return static_cast<std::size_t>(internal_value(kOecfLutIndex));
}

bool IspRegisterBank::oecf_auto_increment() const {
    return (internal_value(kOecfLutControl) & kOecfAutoIncrement) != 0u;
}

void IspRegisterBank::increment_oecf_index() {
    const std::size_t next =
        (selected_oecf_index() + 1u) % gamma_pending_.size();
    set_internal(kOecfLutIndex, static_cast<std::uint32_t>(next));
}

AccessResult IspRegisterBank::read(std::uint32_t address,
                                   std::uint32_t& data) {
    const AccessResult validation = validate_address(address);
    if (validation != AccessResult::Ok) {
        return validation;
    }

    if (is_gamma_window(address)) {
        data = gamma_pending_[gamma_index(address)];
        return AccessResult::Ok;
    }
    if (is_vip_osd_window(address)) {
        data = vip_osd_pending_[vip_osd_index(address)];
        return AccessResult::Ok;
    }
    if (address == kOecfLutData) {
        const std::size_t index = selected_oecf_index();
        if (index >= gamma_pending_.size()) {
            return AccessResult::InvalidValue;
        }
        data = oecf_pending_[selected_oecf_channel()][index];
        if (oecf_auto_increment()) {
            increment_oecf_index();
        }
        return AccessResult::Ok;
    }

    const auto iterator = registers_.find(address);
    if (iterator == registers_.end()) {
        return AccessResult::Unmapped;
    }
    data = iterator->second.pending_value;
    return AccessResult::Ok;
}

AccessResult IspRegisterBank::read_active(std::uint32_t address,
                                          std::uint32_t& data) const {
    const AccessResult validation = validate_address(address);
    if (validation != AccessResult::Ok) {
        return validation;
    }

    if (is_gamma_window(address)) {
        data = gamma_active_[gamma_index(address)];
        return AccessResult::Ok;
    }
    if (is_vip_osd_window(address)) {
        data = vip_osd_active_[vip_osd_index(address)];
        return AccessResult::Ok;
    }
    if (address == kOecfLutData) {
        const std::size_t index = selected_oecf_index();
        if (index >= gamma_active_.size()) {
            return AccessResult::InvalidValue;
        }
        data = oecf_active_[selected_oecf_channel()][index];
        return AccessResult::Ok;
    }

    const auto iterator = registers_.find(address);
    if (iterator == registers_.end()) {
        return AccessResult::Unmapped;
    }
    data = iterator->second.active_value;
    return AccessResult::Ok;
}

AccessResult IspRegisterBank::write(std::uint32_t address,
                                    std::uint32_t data,
                                    std::uint8_t byte_strobe) {
    const AccessResult validation = validate_address(address);
    if (validation != AccessResult::Ok) {
        return validation;
    }
    if ((byte_strobe & 0xF0u) != 0u) {
        return AccessResult::InvalidByteStrobe;
    }

    const std::uint32_t selected_bytes = byte_mask(byte_strobe);
    if (is_gamma_window(address)) {
        const std::size_t index = gamma_index(address);
        const std::uint32_t writable = selected_bytes & low_mask(configuration_.bits);
        gamma_pending_[index] =
            (gamma_pending_[index] & ~writable) | (data & writable);
        return AccessResult::Ok;
    }
    if (is_vip_osd_window(address)) {
        const std::size_t index = vip_osd_index(address);
        vip_osd_pending_[index] =
            (vip_osd_pending_[index] & ~selected_bytes) |
            (data & selected_bytes);
        return AccessResult::Ok;
    }
    if (address == kOecfLutData) {
        const std::size_t index = selected_oecf_index();
        if (index >= gamma_pending_.size()) {
            return AccessResult::InvalidValue;
        }
        const std::uint32_t writable =
            selected_bytes & low_mask(configuration_.bits);
        std::uint32_t& entry =
            oecf_pending_[selected_oecf_channel()][index];
        entry = (entry & ~writable) | (data & writable);
        if (oecf_auto_increment()) {
            increment_oecf_index();
        }
        return AccessResult::Ok;
    }

    auto iterator = registers_.find(address);
    if (iterator == registers_.end()) {
        return AccessResult::Unmapped;
    }
    RegisterState& state = iterator->second;
    const std::uint32_t writable = selected_bytes & state.value_mask;

    switch (state.policy) {
        case StoragePolicy::ShadowedReadWrite:
            state.pending_value =
                (state.pending_value & ~writable) | (data & writable);
            return AccessResult::Ok;

        case StoragePolicy::ImmediateReadWrite: {
            const std::uint32_t candidate =
                (state.pending_value & ~writable) | (data & writable);
            if (address == kOecfLutIndex &&
                candidate >= gamma_pending_.size()) {
                return AccessResult::InvalidValue;
            }
            state.pending_value = candidate;
            state.active_value = candidate;
            return AccessResult::Ok;
        }

        case StoragePolicy::IdentificationReadOnly:
        case StoragePolicy::HardwareReadOnly:
            return AccessResult::ReadOnly;

        case StoragePolicy::WriteOneToClear: {
            const std::uint32_t clear_bits = data & writable;
            state.pending_value &= ~clear_bits;
            state.active_value = state.pending_value;
            return AccessResult::Ok;
        }

        case StoragePolicy::WriteOneToSet:
            if ((data & writable & 0x1u) != 0u) {
                if (address == kVipReset) {
                    reset_vip();
                } else {
                    reset();
                }
            }
            return AccessResult::Ok;

        case StoragePolicy::JobControlMixed: {
            constexpr std::uint32_t persistent =
                kJobControlSourceMode | kJobControlDirectRgbInput;
            const bool start_write =
                (data & writable & kJobControlStart) != 0u;
            if (start_write &&
                (internal_value(kIspJobStatus) & kJobStatusBusy) != 0u) {
                set_internal(kIspJobStatus,
                             internal_value(kIspJobStatus) |
                                 kJobStatusError);
                set_internal(kIspJobErrorCode,
                             kJobErrorCommandWhileBusy);
                raise_interrupt(kInterruptJobError);
                return AccessResult::InvalidValue;
            }
            const std::uint32_t persistent_writable = writable & persistent;
            state.pending_value =
                (state.pending_value & ~persistent_writable) |
                (data & persistent_writable);
            if (start_write) {
                start_requested_ = true;
                set_internal(kIspJobStatus,
                             internal_value(kIspJobStatus) |
                                 kJobStatusBusy);
            }
            state.pending_value &= ~kJobControlStart;
            return AccessResult::Ok;
        }

        case StoragePolicy::JobStatusMixed: {
            constexpr std::uint32_t clearable =
                kJobStatusDone | kJobStatusError;
            state.pending_value &= ~(data & writable & clearable);
            state.active_value = state.pending_value;
            return AccessResult::Ok;
        }
    }
    return AccessResult::Unmapped;
}

AccessResult IspRegisterBank::access_policy(
    std::uint32_t address,
    RegisterAccess& policy) const {
    const AccessResult validation = validate_address(address);
    if (validation != AccessResult::Ok) {
        return validation;
    }
    if (is_gamma_window(address) || is_vip_osd_window(address) ||
        address == kOecfLutData) {
        policy = RegisterAccess::ReadWrite;
        return AccessResult::Ok;
    }

    const auto iterator = registers_.find(address);
    if (iterator == registers_.end()) {
        policy = RegisterAccess::Unmapped;
        return AccessResult::Unmapped;
    }
    switch (iterator->second.policy) {
        case StoragePolicy::ShadowedReadWrite:
        case StoragePolicy::ImmediateReadWrite:
            policy = RegisterAccess::ReadWrite;
            break;
        case StoragePolicy::IdentificationReadOnly:
        case StoragePolicy::HardwareReadOnly:
            policy = RegisterAccess::ReadOnly;
            break;
        case StoragePolicy::WriteOneToClear:
            policy = RegisterAccess::WriteOneToClear;
            break;
        case StoragePolicy::WriteOneToSet:
            policy = RegisterAccess::WriteOneToSet;
            break;
        case StoragePolicy::JobControlMixed:
        case StoragePolicy::JobStatusMixed:
            policy = RegisterAccess::Mixed;
            break;
    }
    return AccessResult::Ok;
}

void IspRegisterBank::commit_frame() {
    for (auto& [address, state] : registers_) {
        (void)address;
        if (state.policy == StoragePolicy::ShadowedReadWrite ||
            state.policy == StoragePolicy::JobControlMixed) {
            state.active_value = state.pending_value;
        }
    }
    oecf_active_ = oecf_pending_;
    gamma_active_ = gamma_pending_;
    vip_osd_active_ = vip_osd_pending_;
}

void IspRegisterBank::reset() {
    for (auto& [address, state] : registers_) {
        (void)address;
        state.pending_value = state.reset_value & state.value_mask;
        state.active_value = state.pending_value;
    }
    start_requested_ = false;
}

void IspRegisterBank::reset_vip() {
    for (auto& [address, state] : registers_) {
        if (address >= kVipReset && address <= kVipOutputFormat) {
            state.pending_value = state.reset_value & state.value_mask;
            state.active_value = state.pending_value;
        }
    }
}

bool IspRegisterBank::consume_start_request() {
    const bool requested = start_requested_;
    start_requested_ = false;
    return requested;
}

bool IspRegisterBank::irq_level() const {
    const std::uint32_t status = internal_value(kIspInterruptStatus);
    const std::uint32_t mask = internal_value(kIspInterruptMask);
    return (status & ~mask & kInterruptMaskAll) != 0u;
}

bool IspRegisterBank::vip_irq_level() const {
    const std::uint32_t status = internal_value(kVipInterruptStatus);
    const std::uint32_t mask = internal_value(kVipInterruptMask);
    return (status & ~mask & kVipInterruptMaskAll) != 0u;
}

void IspRegisterBank::raise_interrupt(std::uint32_t interrupt_bits) {
    set_internal(kIspInterruptStatus,
                 internal_value(kIspInterruptStatus) |
                     (interrupt_bits & kInterruptMaskAll));
}

void IspRegisterBank::raise_vip_interrupt(std::uint32_t interrupt_bits) {
    set_internal(kVipInterruptStatus,
                 internal_value(kVipInterruptStatus) |
                     (interrupt_bits & kVipInterruptMaskAll));
}

void IspRegisterBank::set_job_busy(bool busy) {
    std::uint32_t status = internal_value(kIspJobStatus);
    if (busy) {
        status |= kJobStatusBusy;
    } else {
        status &= ~kJobStatusBusy;
    }
    set_internal(kIspJobStatus, status);
}

void IspRegisterBank::complete_job(const JobStatistics& statistics,
                                   bool error,
                                   std::uint32_t error_code) {
    std::uint32_t status = internal_value(kIspJobStatus);
    status &= ~kJobStatusBusy;
    status |= kJobStatusDone;
    if (error) {
        status |= kJobStatusError;
    }
    set_internal(kIspJobStatus, status);
    set_internal(kIspJobErrorCode, error ? error_code : 0u);
    set_internal(kIspLastJobReadBytes, statistics.read_bytes);
    set_internal(kIspLastJobWriteBytes, statistics.write_bytes);
    set_internal(kIspLastJobReadTransactions, statistics.read_transactions);
    set_internal(kIspLastJobWriteTransactions, statistics.write_transactions);
    increment_frame_count();
    raise_interrupt(kInterruptJobDone |
                    (error ? kInterruptJobError : 0u));
}

void IspRegisterBank::increment_frame_count() {
    set_internal(kIspFrameCount, internal_value(kIspFrameCount) + 1u);
}

void IspRegisterBank::record_tlm_error() {
    set_internal(kIspTlmErrorCount,
                 internal_value(kIspTlmErrorCount) + 1u);
}

AccessResult IspRegisterBank::set_hardware_value(std::uint32_t address,
                                                 std::uint32_t value) {
    const AccessResult validation = validate_address(address);
    if (validation != AccessResult::Ok) {
        return validation;
    }
    auto iterator = registers_.find(address);
    if (iterator == registers_.end()) {
        return AccessResult::Unmapped;
    }
    RegisterState& state = iterator->second;
    if (state.policy != StoragePolicy::HardwareReadOnly) {
        return AccessResult::ReadOnly;
    }
    state.pending_value = value & state.value_mask;
    state.active_value = state.pending_value;
    return AccessResult::Ok;
}

AccessResult IspRegisterBank::read_active_oecf(
    std::size_t channel,
    std::size_t index,
    std::uint32_t& value) const {
    if (channel >= oecf_active_.size() ||
        index >= oecf_active_[channel].size()) {
        return AccessResult::InvalidValue;
    }
    value = oecf_active_[channel][index];
    return AccessResult::Ok;
}

AccessResult IspRegisterBank::read_active_gamma(
    std::size_t index,
    std::uint32_t& value) const {
    if (index >= gamma_active_.size()) {
        return AccessResult::InvalidValue;
    }
    value = gamma_active_[index];
    return AccessResult::Ok;
}

void IspRegisterBank::add_register(std::uint32_t address,
                                   std::uint32_t reset_value,
                                   std::uint32_t value_mask,
                                   StoragePolicy policy) {
    RegisterState state;
    state.reset_value = reset_value & value_mask;
    state.pending_value = state.reset_value;
    state.active_value = state.reset_value;
    state.value_mask = value_mask;
    state.policy = policy;
    const bool inserted = registers_.emplace(address, state).second;
    if (!inserted) {
        throw std::logic_error("duplicate ISP register address");
    }
}

void IspRegisterBank::add_shadowed(std::uint32_t address,
                                   std::uint32_t reset_value,
                                   std::uint32_t value_mask) {
    add_register(address, reset_value, value_mask,
                 StoragePolicy::ShadowedReadWrite);
}

void IspRegisterBank::add_immediate(std::uint32_t address,
                                    std::uint32_t reset_value,
                                    std::uint32_t value_mask) {
    add_register(address, reset_value, value_mask,
                 StoragePolicy::ImmediateReadWrite);
}

void IspRegisterBank::add_identification(std::uint32_t address,
                                         std::uint32_t value,
                                         std::uint32_t value_mask) {
    add_register(address, value, value_mask,
                 StoragePolicy::IdentificationReadOnly);
}

void IspRegisterBank::add_hardware_value(std::uint32_t address,
                                         std::uint32_t reset_value,
                                         std::uint32_t value_mask) {
    add_register(address, reset_value, value_mask,
                 StoragePolicy::HardwareReadOnly);
}

void IspRegisterBank::set_internal(std::uint32_t address,
                                   std::uint32_t value) {
    RegisterState& state = registers_.at(address);
    state.pending_value = value & state.value_mask;
    state.active_value = state.pending_value;
}

std::uint32_t IspRegisterBank::internal_value(std::uint32_t address) const {
    return registers_.at(address).pending_value;
}

void IspRegisterBank::initialize_registers() {
    const std::uint32_t pixel_mask = low_mask(configuration_.bits);
    const std::uint32_t vip_color_mask =
        low_mask(3u * configuration_.vip_bits);

    add_register(kIspReset, 0u, 0x1u, StoragePolicy::WriteOneToSet);
    add_identification(kIspSensorWidth, configuration_.sensor_width);
    add_identification(kIspSensorHeight, configuration_.sensor_height);
    add_identification(kIspCropWidth, configuration_.crop_width);
    add_identification(kIspCropHeight, configuration_.crop_height);
    add_identification(kIspBits, configuration_.bits);
    add_identification(kIspBayer, configuration_.bayer, 0x3u);
    add_shadowed(kIspTopEnable, kIspTopEnableReset, kIspTopEnableMask);
    add_register(kIspInterruptStatus, 0u, kInterruptMaskAll,
                 StoragePolicy::WriteOneToClear);
    add_immediate(kIspInterruptMask, kInterruptMaskAll, kInterruptMaskAll);

    add_register(kIspJobControl, 0u,
                 kJobControlSourceMode | kJobControlStart |
                     kJobControlDirectRgbInput,
                 StoragePolicy::JobControlMixed);
    add_register(kIspJobStatus, 0u,
                 kJobStatusBusy | kJobStatusDone | kJobStatusError,
                 StoragePolicy::JobStatusMixed);
    add_hardware_value(kIspJobErrorCode, 0u);
    add_hardware_value(kIspFrameCount, 0u);

    add_shadowed(kIspSourceAddress, 0u);
    add_shadowed(kIspSourceStrideBytes, 0u);
    add_shadowed(kIspSourceSizeBytes, 0u);
    add_shadowed(kIspDestinationYAddress, 0u);
    add_shadowed(kIspDestinationUAddress, 0u);
    add_shadowed(kIspDestinationVAddress, 0u);
    add_shadowed(kIspDestinationYStrideBytes, 0u);
    add_shadowed(kIspDestinationUvStrideBytes, 0u);
    add_shadowed(kIspDestinationYSizeBytes, 0u);
    add_shadowed(kIspDestinationUSizeBytes, 0u);
    add_shadowed(kIspDestinationVSizeBytes, 0u);
    add_identification(kIspOutputWidth, configuration_.output_width);
    add_identification(kIspOutputHeight, configuration_.output_height);
    add_identification(kIspOutputFormat, kOutputFormatI420);
    add_hardware_value(kIspLastJobReadBytes, 0u);
    add_hardware_value(kIspLastJobWriteBytes, 0u);
    add_hardware_value(kIspLastJobReadTransactions, 0u);
    add_hardware_value(kIspLastJobWriteTransactions, 0u);
    add_hardware_value(kIspTlmErrorCount, 0u);

    add_shadowed(kDpcThreshold, 2u, pixel_mask);
    const std::uint32_t blc_reset =
        (16u << (configuration_.bits - 8u)) & pixel_mask;
    add_shadowed(kBlcR, blc_reset, pixel_mask);
    add_shadowed(kBlcGr, blc_reset, pixel_mask);
    add_shadowed(kBlcGb, blc_reset, pixel_mask);
    add_shadowed(kBlcB, blc_reset, pixel_mask);
    add_shadowed(kLinearR, 0x4445u, 0xFFFFu);
    add_shadowed(kLinearGr, 0x4445u, 0xFFFFu);
    add_shadowed(kLinearGb, 0x4445u, 0xFFFFu);
    add_shadowed(kLinearB, 0x4445u, 0xFFFFu);

    add_shadowed(kAeCenterIlluminance, 110u, 0xFFu);
    add_shadowed(kAeSkewness, 275u, 0xFFFFu);
    add_shadowed(kAeCropLeft, 12u, 0xFFFu);
    add_shadowed(kAeCropRight, 12u, 0xFFFu);
    add_shadowed(kAeCropTop, 22u, 0xFFFu);
    add_shadowed(kAeCropBottom, 2u, 0xFFFu);
    add_hardware_value(kAeResponse, 0u, 0x3u);
    add_hardware_value(kAeResultSkewness, 0u, 0xFFFFu);
    add_hardware_value(kAeResponseDebug, 0u, 0x3u);
    add_hardware_value(kAeDone, 0u, 0x1u);

    add_shadowed(kDgainIsManual, 0u, 0x1u);
    add_shadowed(kDgainManualIndex, 0u, 0x7Fu);
    add_hardware_value(kDgainIndexOut, 0u, 0x7Fu);
    for (std::uint32_t index = 0u; index < kDgainArrayWords; ++index) {
        add_shadowed(kDgainArrayBase + index * kRegisterBytes,
                     index + 1u, 0xFFu);
    }

    add_shadowed(kAwbUnderexposedLimit, 51u, pixel_mask);
    add_shadowed(kAwbOverexposedLimit, 972u, pixel_mask);
    add_shadowed(kAwbFrames, 1u, pixel_mask);
    add_hardware_value(kAwbFinalRGain, 0u, 0xFFFu);
    add_hardware_value(kAwbFinalBGain, 0u, 0xFFFu);

    add_shadowed(kWbRGain, 0x13Fu, 0xFFFu);
    add_shadowed(kWbBGain, 0x2CFu, 0xFFFu);

    add_shadowed(kCcmRr, static_cast<std::uint16_t>(2966), 0xFFFFu);
    add_shadowed(kCcmRg, static_cast<std::uint16_t>(-1687), 0xFFFFu);
    add_shadowed(kCcmRb, static_cast<std::uint16_t>(-255), 0xFFFFu);
    add_shadowed(kCcmGr, static_cast<std::uint16_t>(-663), 0xFFFFu);
    add_shadowed(kCcmGg, static_cast<std::uint16_t>(2312), 0xFFFFu);
    add_shadowed(kCcmGb, static_cast<std::uint16_t>(-625), 0xFFFFu);
    add_shadowed(kCcmBr, static_cast<std::uint16_t>(-104), 0xFFFFu);
    add_shadowed(kCcmBg, static_cast<std::uint16_t>(-1049), 0xFFFFu);
    add_shadowed(kCcmBb, static_cast<std::uint16_t>(2177), 0xFFFFu);

    add_shadowed(kCscConversionStandard, 2u, 0x3u);
    add_shadowed(kSharpenStrength, 0x399u, 0xFFFu);
    for (std::uint32_t index = 0u; index < kSharpenKernelWords; ++index) {
        add_shadowed(kSharpenKernelBase + index * kRegisterBytes,
                     kSharpenKernelReset[index], 0xFFFFFu);
    }

    const auto add_spatial_channel = [this](std::uint32_t base) {
        for (std::uint32_t row = 0u; row < 5u; ++row) {
            std::uint32_t packed = 0u;
            for (std::uint32_t column = 0u; column < 4u; ++column) {
                packed |=
                    static_cast<std::uint32_t>(
                        kBnrSpatialReset[row * 5u + column])
                    << (column * 8u);
            }
            add_shadowed(base + row * 2u * kRegisterBytes, packed);
            add_shadowed(base + (row * 2u + 1u) * kRegisterBytes,
                         kBnrSpatialReset[row * 5u + 4u], 0xFFu);
        }
    };
    add_spatial_channel(kBnrSpatialRBase);
    add_spatial_channel(kBnrSpatialGBase);
    add_spatial_channel(kBnrSpatialBBase);

    const std::uint32_t bnr_color_mask = (0xFFu << 16u) | pixel_mask;
    const auto add_color_channel =
        [this, bnr_color_mask](std::uint32_t base,
                               const std::array<std::uint16_t, 9u>& x,
                               const std::array<std::uint8_t, 9u>& y) {
            for (std::uint32_t index = 0u;
                 index < kBnrColorWordsPerChannel; ++index) {
                const std::uint32_t packed =
                    static_cast<std::uint32_t>(x[index]) |
                    (static_cast<std::uint32_t>(y[index]) << 16u);
                add_shadowed(base + index * kRegisterBytes,
                             packed, bnr_color_mask);
            }
        };
    add_color_channel(kBnrColorRBase, kBnrColorXRedBlue,
                      kBnrColorYRedBlue);
    add_color_channel(kBnrColorGBase, kBnrColorXGreen, kBnrColorYGreen);
    add_color_channel(kBnrColorBBase, kBnrColorXRedBlue,
                      kBnrColorYRedBlue);

    for (std::uint32_t word = 0u; word < kNr2dDifferenceWords; ++word) {
        std::uint32_t packed = 0u;
        for (std::uint32_t byte = 0u; byte < 4u; ++byte) {
            const std::uint32_t sample = word * 4u + byte;
            const std::uint32_t value =
                sample == 31u ? 255u : sample * 8u;
            packed |= value << (byte * 8u);
        }
        add_shadowed(kNr2dDifferenceBase + word * kRegisterBytes, packed);
    }
    constexpr std::array<std::uint8_t, 4u> first_weights = {
        31u, 22u, 9u, 2u,
    };
    std::uint32_t packed_weights = 0u;
    for (std::uint32_t byte = 0u; byte < first_weights.size(); ++byte) {
        packed_weights |= static_cast<std::uint32_t>(first_weights[byte])
                          << (byte * 8u);
    }
    for (std::uint32_t word = 0u; word < kNr2dWeightWords; ++word) {
        add_shadowed(kNr2dWeightBase + word * kRegisterBytes,
                     word == 0u ? packed_weights : 0u,
                     0x1F1F1F1Fu);
    }

    add_immediate(kOecfLutControl, 0u,
                  kOecfChannelMask | kOecfAutoIncrement);
    add_immediate(kOecfLutIndex, 0u);

    add_register(kVipReset, 0u, 0x1u, StoragePolicy::WriteOneToSet);
    add_identification(kVipWidth, configuration_.output_width);
    add_identification(kVipHeight, configuration_.output_height);
    add_identification(kVipBits, configuration_.vip_bits);
    add_shadowed(kVipTopEnable, kVipTopEnableReset, kVipTopEnableMask);
    add_register(kVipInterruptStatus, 0u, kVipInterruptMaskAll,
                 StoragePolicy::WriteOneToClear);
    add_immediate(kVipInterruptMask, kVipInterruptMaskAll,
                  kVipInterruptMaskAll);
    add_shadowed(kVipRgbConversionStandard, 2u, 0x3u);
    add_shadowed(kVipIrcX, 16u, 0xFFFFu);
    add_shadowed(kVipIrcY, 30u, 0xFFFFu);
    add_shadowed(kVipIrcOutput, 1u, 0x3u);
    add_shadowed(kVipScaleInputCropWidth, configuration_.output_width,
                 0xFFFu);
    add_shadowed(kVipScaleInputCropHeight, configuration_.output_height,
                 0xFFFu);
    add_shadowed(kVipScaleOutputCropWidth, configuration_.output_width,
                 0xFFFu);
    add_shadowed(kVipScaleOutputCropHeight, configuration_.output_height,
                 0xFFFu);
    add_shadowed(kVipScaleDownscaleWidth, 1u, 0x7u);
    add_shadowed(kVipScaleDownscaleHeight, 1u, 0x7u);
    add_shadowed(kVipOsdX, 16u, 0xFFFFu);
    add_shadowed(kVipOsdY, 16u, 0xFFFFu);
    add_shadowed(kVipOsdWidth, 128u, 0xFFFFu);
    add_shadowed(kVipOsdHeight, 64u, 0xFFFFu);
    add_shadowed(kVipOsdForegroundColor, 0x0000FFu, vip_color_mask);
    add_shadowed(kVipOsdBackgroundColor, 0xFFFFFFu, vip_color_mask);
    add_shadowed(kVipOsdAlpha, 50u, 0xFFu);
    add_identification(kVipOutputFormat, kOutputFormatI420);
}

}  // namespace isp_tlm::registers
