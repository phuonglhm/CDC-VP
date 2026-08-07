#ifndef ISP_TLM_REGISTERS_ISP_REGISTER_BANK_H
#define ISP_TLM_REGISTERS_ISP_REGISTER_BANK_H

#include "isp_register_map.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

namespace isp_tlm::registers {

struct BuildConfiguration {
    std::uint32_t sensor_width = 2592u;
    std::uint32_t sensor_height = 1536u;
    std::uint32_t crop_width = 2592u;
    std::uint32_t crop_height = 1536u;
    std::uint32_t bits = 12u;
    std::uint32_t bayer = 0u;  // 0 is RGGB.
    std::uint32_t output_width = 2592u;
    std::uint32_t output_height = 1536u;
    std::uint32_t vip_bits = 8u;
};

struct JobStatistics {
    std::uint32_t read_bytes = 0u;
    std::uint32_t write_bytes = 0u;
    std::uint32_t read_transactions = 0u;
    std::uint32_t write_transactions = 0u;
};

enum class AccessResult {
    Ok,
    Misaligned,
    OutOfRange,
    Unmapped,
    ReadOnly,
    InvalidByteStrobe,
    InvalidValue,
};

enum class RegisterAccess {
    ReadOnly,
    ReadWrite,
    WriteOneToClear,
    WriteOneToSet,
    Mixed,
    Unmapped,
};

const char* to_string(AccessResult result);

// Pure C++ register storage shared by the AXI and TLM front ends. Bus
// reads return software-pending configuration. read_active() exposes the
// frame-stable copy consumed by the image pipeline.
class IspRegisterBank {
public:
    explicit IspRegisterBank(BuildConfiguration configuration = {});

    AccessResult read(std::uint32_t address, std::uint32_t& data);
    AccessResult read_active(std::uint32_t address, std::uint32_t& data) const;
    AccessResult write(std::uint32_t address,
                       std::uint32_t data,
                       std::uint8_t byte_strobe = 0x0Fu);
    AccessResult access_policy(std::uint32_t address,
                               RegisterAccess& policy) const;

    // Atomically exposes all pending frame configuration and array data to the
    // processing pipeline. Status, counters, reset, and interrupt handling are
    // immediate and are not changed by a frame commit.
    void commit_frame();
    void reset();

    bool consume_start_request();
    bool irq_level() const;
    bool vip_irq_level() const;

    void raise_interrupt(std::uint32_t interrupt_bits);
    void raise_vip_interrupt(std::uint32_t interrupt_bits);
    void set_job_busy(bool busy);
    void complete_job(const JobStatistics& statistics,
                      bool error = false,
                      std::uint32_t error_code = 0u);
    void increment_frame_count();
    void record_tlm_error();

    // Updates only values produced by model hardware (AE/AWB results, the
    // DGain output index, job diagnostics, and counters). Identification RO
    // registers cannot be changed through this hook.
    AccessResult set_hardware_value(std::uint32_t address,
                                    std::uint32_t value);

    AccessResult read_active_oecf(std::size_t channel,
                                  std::size_t index,
                                  std::uint32_t& value) const;
    AccessResult read_active_gamma(std::size_t index,
                                   std::uint32_t& value) const;

    const BuildConfiguration& configuration() const { return configuration_; }
    std::size_t oecf_lut_size() const { return gamma_pending_.size(); }

private:
    enum class StoragePolicy {
        ShadowedReadWrite,
        ImmediateReadWrite,
        IdentificationReadOnly,
        HardwareReadOnly,
        WriteOneToClear,
        WriteOneToSet,
        JobControlMixed,
        JobStatusMixed,
    };

    struct RegisterState {
        std::uint32_t reset_value = 0u;
        std::uint32_t pending_value = 0u;
        std::uint32_t active_value = 0u;
        std::uint32_t value_mask = 0xFFFFFFFFu;
        StoragePolicy policy = StoragePolicy::ShadowedReadWrite;
    };

    static std::uint32_t byte_mask(std::uint8_t byte_strobe);
    AccessResult validate_address(std::uint32_t address) const;
    bool is_gamma_window(std::uint32_t address) const;
    bool is_vip_osd_window(std::uint32_t address) const;
    std::size_t gamma_index(std::uint32_t address) const;
    std::size_t vip_osd_index(std::uint32_t address) const;
    std::size_t selected_oecf_channel() const;
    std::size_t selected_oecf_index() const;
    bool oecf_auto_increment() const;
    void increment_oecf_index();
    void reset_vip();

    void initialize_registers();
    void add_register(std::uint32_t address,
                      std::uint32_t reset_value,
                      std::uint32_t value_mask,
                      StoragePolicy policy);
    void add_shadowed(std::uint32_t address,
                      std::uint32_t reset_value,
                      std::uint32_t value_mask = 0xFFFFFFFFu);
    void add_immediate(std::uint32_t address,
                       std::uint32_t reset_value,
                       std::uint32_t value_mask = 0xFFFFFFFFu);
    void add_identification(std::uint32_t address,
                            std::uint32_t value,
                            std::uint32_t value_mask = 0xFFFFFFFFu);
    void add_hardware_value(std::uint32_t address,
                            std::uint32_t reset_value,
                            std::uint32_t value_mask = 0xFFFFFFFFu);
    void set_internal(std::uint32_t address, std::uint32_t value);
    std::uint32_t internal_value(std::uint32_t address) const;

    BuildConfiguration configuration_;
    std::map<std::uint32_t, RegisterState> registers_;
    std::array<std::vector<std::uint32_t>, 4u> oecf_pending_;
    std::array<std::vector<std::uint32_t>, 4u> oecf_active_;
    std::vector<std::uint32_t> gamma_pending_;
    std::vector<std::uint32_t> gamma_active_;
    std::vector<std::uint32_t> vip_osd_pending_;
    std::vector<std::uint32_t> vip_osd_active_;
    bool start_requested_ = false;
};

}  // namespace isp_tlm::registers

#endif  // ISP_TLM_REGISTERS_ISP_REGISTER_BANK_H
