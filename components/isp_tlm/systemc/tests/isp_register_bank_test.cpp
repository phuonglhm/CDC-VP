#include "registers/isp_register_bank.h"

#include <cstdint>
#include <iostream>
#include <string>

namespace reg = isp_tlm::registers;

namespace {

class TestContext {
public:
    void check(bool condition, const std::string& message) {
        if (!condition) {
            ++failures_;
            std::cerr << "FAIL: " << message << '\n';
        }
    }

    void expect_result(reg::AccessResult actual,
                       reg::AccessResult expected,
                       const std::string& message) {
        if (actual != expected) {
            ++failures_;
            std::cerr << "FAIL: " << message << " (expected "
                      << reg::to_string(expected) << ", got "
                      << reg::to_string(actual) << ")\n";
        }
    }

    std::uint32_t read(reg::IspRegisterBank& bank,
                       std::uint32_t address,
                       const std::string& message) {
        std::uint32_t value = 0xDEADBEEFu;
        expect_result(bank.read(address, value), reg::AccessResult::Ok,
                      message);
        return value;
    }

    std::uint32_t read_active(const reg::IspRegisterBank& bank,
                              std::uint32_t address,
                              const std::string& message) {
        std::uint32_t value = 0xDEADBEEFu;
        expect_result(bank.read_active(address, value), reg::AccessResult::Ok,
                      message);
        return value;
    }

    int failures() const { return failures_; }

private:
    int failures_ = 0;
};

void test_identification_and_decode(TestContext& test) {
    reg::IspRegisterBank bank;

    test.check(test.read(bank, reg::kIspSensorWidth, "read sensor width") ==
                   2592u,
               "default sensor width is the golden 2592 pixels");
    test.check(test.read(bank, reg::kIspSensorHeight, "read sensor height") ==
                   1536u,
               "default sensor height is the golden 1536 pixels");
    test.check(test.read(bank, reg::kIspBits, "read ISP bits") == 12u,
               "default input precision is 12 bits");
    test.check(test.read(bank, reg::kIspBayer, "read Bayer ID") == 0u,
               "default Bayer identification is RGGB");
    test.check(test.read(bank, reg::kIspOutputFormat,
                         "read ISP output format") ==
                   reg::kOutputFormatI420,
               "ISP output format is fixed to I420");
    test.check(test.read(bank, reg::kVipOutputFormat,
                         "read VIP output format") ==
                   reg::kOutputFormatI420,
               "sole VIP output format is fixed to I420");

    test.expect_result(bank.write(reg::kIspSensorWidth, 640u),
                       reg::AccessResult::ReadOnly,
                       "structural identification rejects software writes");
    test.expect_result(bank.write(reg::kVipOutputFormat, 1u),
                       reg::AccessResult::ReadOnly,
                       "fixed VIP output format rejects software writes");

    std::uint32_t unused = 0u;
    test.expect_result(bank.read(0x0002u, unused),
                       reg::AccessResult::Misaligned,
                       "misaligned read reports an error");
    test.expect_result(bank.write(reg::kDpcThreshold, 0u, 0x10u),
                       reg::AccessResult::InvalidByteStrobe,
                       "invalid byte strobe reports an error");
    test.expect_result(bank.read(reg::kAddressSpaceBytes, unused),
                       reg::AccessResult::OutOfRange,
                       "address above canonical 64 KiB reports an error");
    test.expect_result(bank.read(0x0020u, unused),
                       reg::AccessResult::Unmapped,
                       "reserved scalar hole is inaccessible");
    test.expect_result(bank.read(0x6000u, unused),
                       reg::AccessResult::Unmapped,
                       "former second-VIP scalar space is inaccessible");
    test.expect_result(bank.read(0xE000u, unused),
                       reg::AccessResult::Unmapped,
                       "former second-VIP OSD space is inaccessible");

    reg::RegisterAccess policy = reg::RegisterAccess::Unmapped;
    test.expect_result(bank.access_policy(reg::kIspInterruptStatus, policy),
                       reg::AccessResult::Ok,
                       "interrupt-status policy is discoverable");
    test.check(policy == reg::RegisterAccess::WriteOneToClear,
               "interrupt status is W1C");
    test.expect_result(bank.access_policy(reg::kIspJobControl, policy),
                       reg::AccessResult::Ok,
                       "mixed job-control policy is discoverable");
    test.check(policy == reg::RegisterAccess::Mixed,
               "job control exposes mixed RW and W1S behavior");
    test.expect_result(bank.access_policy(reg::kGammaWindowBase, policy),
                       reg::AccessResult::Ok,
                       "Gamma window policy is discoverable");
    test.check(policy == reg::RegisterAccess::ReadWrite,
               "Gamma array window is read/write");
    test.expect_result(bank.access_policy(0xE000u, policy),
                       reg::AccessResult::Unmapped,
                       "removed OSD window has no access policy");
    test.check(policy == reg::RegisterAccess::Unmapped,
               "removed OSD window is explicitly unmapped");
}

void test_shadowing_and_byte_strobes(TestContext& test) {
    reg::IspRegisterBank bank;

    test.expect_result(bank.write(reg::kDpcThreshold, 0xABCu),
                       reg::AccessResult::Ok,
                       "write pending DPC threshold");
    test.check(test.read(bank, reg::kDpcThreshold,
                         "read pending DPC threshold") == 0xABCu,
               "bus reads see pending frame configuration");
    test.check(test.read_active(bank, reg::kDpcThreshold,
                                "read active DPC threshold") == 2u,
               "pipeline keeps the old DPC value before frame commit");

    test.expect_result(bank.write(reg::kIspSourceAddress, 0x11223344u),
                       reg::AccessResult::Ok,
                       "write source descriptor");
    test.expect_result(bank.write(reg::kIspSourceAddress, 0x0000AA00u, 0x2u),
                       reg::AccessResult::Ok,
                       "partially update source descriptor");
    test.check(test.read(bank, reg::kIspSourceAddress,
                         "read byte-merged descriptor") == 0x1122AA44u,
               "byte strobes merge only selected descriptor bytes");
    test.expect_result(bank.write(reg::kIspSourceAddress, 0xFFFFFFFFu, 0x0u),
                       reg::AccessResult::Ok,
                       "zero-strobe write is accepted as a no-op");
    test.check(test.read(bank, reg::kIspSourceAddress,
                         "read descriptor after no-op") == 0x1122AA44u,
               "zero byte strobe does not alter data");

    bank.commit_frame();
    test.check(test.read_active(bank, reg::kDpcThreshold,
                                "read committed DPC threshold") == 0xABCu,
               "frame commit publishes scalar configuration atomically");
    test.check(test.read_active(bank, reg::kIspSourceAddress,
                                "read committed source descriptor") ==
                   0x1122AA44u,
               "frame commit publishes job descriptors");
}

void test_interrupts_jobs_and_hardware_updates(TestContext& test) {
    reg::IspRegisterBank bank;

    test.expect_result(
        bank.write(reg::kIspJobControl,
                   reg::kJobControlSourceMode | reg::kJobControlStart |
                       reg::kJobControlDirectRgbInput),
        reg::AccessResult::Ok, "program and start a memory job");
    test.check(test.read(bank, reg::kIspJobControl,
                         "read self-cleared job control") ==
                   (reg::kJobControlSourceMode |
                    reg::kJobControlDirectRgbInput),
               "START self-clears while persistent job bits remain");
    test.check(bank.consume_start_request(),
               "START produces one consumable model event");
    test.check(!bank.consume_start_request(),
               "START event is consumed exactly once");
    test.check((test.read(bank, reg::kIspJobStatus,
                          "read status after accepted START") &
                reg::kJobStatusBusy) != 0u,
               "accepted START raises BUSY immediately");
    test.check(test.read_active(bank, reg::kIspJobControl,
                                "read active job control before commit") == 0u,
               "persistent job control is frame-shadowed");
    bank.commit_frame();
    test.check(test.read_active(bank, reg::kIspJobControl,
                                "read active job control after commit") == 5u,
               "frame commit activates persistent job-control bits");

    bank.set_job_busy(true);
    test.check(test.read(bank, reg::kIspJobStatus, "read BUSY status") ==
                   reg::kJobStatusBusy,
               "hardware can assert BUSY");
    test.expect_result(
        bank.write(reg::kIspJobControl, reg::kJobControlStart),
        reg::AccessResult::InvalidValue,
        "START while BUSY rejects the second command");
    const std::uint32_t rejected_start_status =
        test.read(bank, reg::kIspJobStatus,
                  "read status after rejected START");
    test.check((rejected_start_status & reg::kJobStatusBusy) != 0u,
               "rejected START does not disturb the running job");
    test.check((rejected_start_status & reg::kJobStatusError) != 0u,
               "START while BUSY latches sticky ERROR");
    test.check(test.read(bank, reg::kIspJobErrorCode,
                         "read rejected-START error code") ==
                   reg::kJobErrorCommandWhileBusy,
               "START while BUSY records COMMAND_WHILE_BUSY");
    test.check(!bank.consume_start_request(),
               "rejected START does not queue a second request");
    test.check((test.read(bank, reg::kIspInterruptStatus,
                          "read rejected-START interrupt") &
                reg::kInterruptJobError) != 0u,
               "START while BUSY raises JOB_ERROR interrupt status");

    reg::JobStatistics statistics;
    statistics.read_bytes = 4096u;
    statistics.write_bytes = 6144u;
    statistics.read_transactions = 16u;
    statistics.write_transactions = 24u;
    bank.complete_job(statistics, true, 0x42u);

    const std::uint32_t completed_status =
        test.read(bank, reg::kIspJobStatus, "read completed job status");
    test.check((completed_status &
                (reg::kJobStatusDone | reg::kJobStatusError)) ==
                   (reg::kJobStatusDone | reg::kJobStatusError),
               "failed completion sets sticky DONE and ERROR");
    test.check((completed_status & reg::kJobStatusBusy) == 0u,
               "job completion clears BUSY");
    test.check(test.read(bank, reg::kIspJobErrorCode,
                         "read job error code") == 0x42u,
               "job error code is hardware-owned");
    test.check(test.read(bank, reg::kIspFrameCount, "read frame count") == 1u,
               "completed job increments the frame counter");
    test.check(test.read(bank, reg::kIspLastJobReadBytes,
                         "read job byte counter") == 4096u,
               "job completion publishes functional counters");

    test.expect_result(
        bank.write(reg::kIspJobStatus,
                   reg::kJobStatusDone | reg::kJobStatusError),
        reg::AccessResult::Ok, "clear sticky job status");
    test.check(test.read(bank, reg::kIspJobStatus,
                         "read cleared job status") == 0u,
               "DONE and ERROR implement W1C");

    test.check(!bank.irq_level(),
               "interrupt reset mask disables all core sources");
    test.expect_result(bank.write(reg::kIspInterruptMask, 0u),
                       reg::AccessResult::Ok,
                       "unmask core interrupt sources");
    test.check(bank.irq_level(),
               "latched completion sources assert IRQ after unmasking");
    test.expect_result(
        bank.write(reg::kIspInterruptStatus,
                   reg::kInterruptJobDone | reg::kInterruptJobError),
        reg::AccessResult::Ok, "clear job interrupt sources");
    test.check(!bank.irq_level(), "W1C deasserts the core IRQ");

    test.expect_result(bank.write(reg::kVipInterruptMask, 0u),
                       reg::AccessResult::Ok,
                       "unmask sole-VIP interrupts");
    bank.raise_vip_interrupt(reg::kVipInterruptFrameDone);
    test.check(bank.vip_irq_level(), "sole VIP raises its interrupt");
    test.expect_result(
        bank.write(reg::kVipInterruptStatus,
                   reg::kVipInterruptFrameDone),
        reg::AccessResult::Ok, "clear sole-VIP interrupt status");
    test.check(!bank.vip_irq_level(), "VIP W1C deasserts its interrupt");

    test.expect_result(bank.set_hardware_value(reg::kAeResponse, 3u),
                       reg::AccessResult::Ok,
                       "hardware updates AE response");
    test.check(test.read(bank, reg::kAeResponse, "read AE response") == 3u,
               "hardware-owned AE response is software-readable");
    test.expect_result(
        bank.set_hardware_value(reg::kIspSensorWidth, 1920u),
        reg::AccessResult::ReadOnly,
        "hardware hook cannot mutate structural identification");
    bank.record_tlm_error();
    test.check(test.read(bank, reg::kIspTlmErrorCount,
                         "read TLM error counter") == 1u,
               "TLM transport failures have a dedicated counter");
}

void test_oecf_indexed_interface(TestContext& test) {
    reg::IspRegisterBank bank;

    test.expect_result(
        bank.write(reg::kOecfLutControl,
                   2u | reg::kOecfAutoIncrement),
        reg::AccessResult::Ok, "select Gb OECF channel with auto increment");
    test.expect_result(bank.write(reg::kOecfLutIndex, 5u),
                       reg::AccessResult::Ok, "select OECF identity entry");
    test.check(test.read(bank, reg::kOecfLutData,
                         "read identity OECF entry") == 5u,
               "OECF starts as a deterministic identity LUT");
    test.check(test.read(bank, reg::kOecfLutIndex,
                         "read incremented OECF index") == 6u,
               "successful OECF DATA read auto-increments INDEX");

    test.expect_result(bank.write(reg::kOecfLutIndex, 7u),
                       reg::AccessResult::Ok, "select OECF write entry");
    test.expect_result(bank.write(reg::kOecfLutData, 0x1ABCu),
                       reg::AccessResult::Ok, "write OECF data");
    test.check(test.read(bank, reg::kOecfLutIndex,
                         "read post-write OECF index") == 8u,
               "successful OECF DATA write auto-increments INDEX");

    test.expect_result(bank.write(reg::kOecfLutControl, 2u),
                       reg::AccessResult::Ok,
                       "disable OECF auto increment");
    test.expect_result(bank.write(reg::kOecfLutIndex, 7u),
                       reg::AccessResult::Ok, "reselect OECF write entry");
    test.check(test.read(bank, reg::kOecfLutData,
                         "read pending OECF data") == 0xABCu,
               "OECF DATA is masked to the configured ISP bit depth");
    test.check(test.read_active(bank, reg::kOecfLutData,
                                "read active OECF data before commit") == 7u,
               "OECF writes remain pending before frame commit");
    bank.commit_frame();
    test.check(test.read_active(bank, reg::kOecfLutData,
                                "read active OECF data after commit") ==
                   0xABCu,
               "frame commit atomically activates OECF contents");

    test.expect_result(bank.write(reg::kOecfLutIndex, 4096u),
                       reg::AccessResult::InvalidValue,
                       "OECF INDEX rejects entries outside 2^BITS");
    test.check(test.read(bank, reg::kOecfLutIndex,
                         "read index after rejected write") == 7u,
               "invalid OECF INDEX write leaves the prior index intact");

    test.expect_result(bank.write(reg::kIspReset, 1u),
                       reg::AccessResult::Ok,
                       "issue immediate ISP software reset");
    test.expect_result(bank.write(reg::kOecfLutControl, 2u),
                       reg::AccessResult::Ok,
                       "reselect OECF channel after reset");
    test.expect_result(bank.write(reg::kOecfLutIndex, 7u),
                       reg::AccessResult::Ok,
                       "reselect OECF entry after reset");
    test.check(test.read(bank, reg::kOecfLutData,
                         "read OECF SRAM after reset") == 0xABCu,
               "OECF LUT SRAM is intentionally not reset-cleared");
}

void test_aliased_windows(TestContext& test) {
    reg::BuildConfiguration configuration;
    configuration.bits = 10u;
    reg::IspRegisterBank bank(configuration);

    constexpr std::uint32_t gamma_entry = reg::kGammaWindowBase + 3u * 4u;
    constexpr std::uint32_t gamma_alias = gamma_entry + 0x1000u;
    test.expect_result(bank.write(gamma_entry, 0x155u),
                       reg::AccessResult::Ok, "write canonical Gamma entry");
    test.check(test.read(bank, gamma_alias, "read mirrored Gamma entry") ==
                   0x155u,
               "Gamma decoder mirrors its canonical 2^BITS words");
    test.check(test.read_active(bank, gamma_alias,
                                "read active Gamma entry before commit") == 3u,
               "Gamma LUT writes are frame-shadowed");

    constexpr std::uint32_t osd_entry = reg::kVipOsdWindowBase + 5u * 4u;
    constexpr std::uint32_t osd_alias =
        osd_entry + reg::kVipOsdCanonicalBytes;
    test.expect_result(bank.write(osd_entry, 0xA1B2C3D4u),
                       reg::AccessResult::Ok, "write sole-VIP OSD RAM");
    test.check(test.read(bank, osd_alias, "read mirrored OSD RAM") ==
                   0xA1B2C3D4u,
               "sole-VIP OSD RAM mirrors every 0x800 bytes");
    test.check(test.read_active(bank, osd_alias,
                                "read active OSD RAM before commit") == 0u,
               "OSD RAM writes are frame-shadowed");

    bank.commit_frame();
    test.check(test.read_active(bank, gamma_alias,
                                "read committed Gamma alias") == 0x155u,
               "frame commit activates Gamma LUT contents");
    test.check(test.read_active(bank, osd_alias,
                                "read committed OSD alias") == 0xA1B2C3D4u,
               "frame commit activates sole-VIP OSD RAM");

    std::uint32_t unused = 0u;
    test.expect_result(bank.read(0xE000u, unused),
                       reg::AccessResult::Unmapped,
                       "removed second-VIP OSD decoder stays inaccessible");
}

}  // namespace

int main() {
    static_assert(reg::kVipReset == 0x4000u,
                  "sole VIP must preserve the first RTL VIP offsets");
    static_assert(reg::kVipOutputFormat == 0x4A00u,
                  "fixed I420 identification offset changed");
    static_assert(reg::kOecfLutControl == 0x2C00u,
                  "indexed OECF control offset changed");
    static_assert(reg::kIspJobControl == 0x0080u,
                  "job-control extension offset changed");

    TestContext test;
    test_identification_and_decode(test);
    test_shadowing_and_byte_strobes(test);
    test_interrupts_jobs_and_hardware_updates(test);
    test_oecf_indexed_interface(test);
    test_aliased_windows(test);

    if (test.failures() != 0) {
        std::cerr << test.failures() << " register-bank test(s) failed\n";
        return 1;
    }
    std::cout << "ISP register-bank tests passed\n";
    return 0;
}
