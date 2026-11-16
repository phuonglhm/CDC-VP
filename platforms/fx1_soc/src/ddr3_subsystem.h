#pragma once
// FX1 DDR3 subsystem (DDR3 VP plan subtask 5): the DDR3 controller IP with its platform
// glue. It owns
//  - the controller (NATIVE_R2, DDR3-1600, 2 ranks of 4 Gb x8 on a x32 bus, ECC, port 0),
//  - the generic-payload -> AXI bridge behind the MEMCTL_DDR bus target,
//  - a CSR multiplexer (the bus target DDR3_CSR and a host initiator for preinit),
//  - DDR_SYSCTRL (P-channel, readiness mirror, QoS sideband, resets),
//  - the boot-mode sequencing and the host image/fault interface.
//
// Boot modes (plan §5, DEC-09):
//  preinit   The platform programs and commits a legal image, enables the controller,
//            waits for mission readiness (the ECC startup scrub included), then writes the
//            staged DDR image (ELF segments, --load files) with debug_write (consistent
//            ECC) and only then releases the CPUs: scrub -> preload -> CPU execution.
//  firmware  Nothing is programmed. The CPUs are released at once and boot from
//            BootROM/SRAM; firmware brings the controller up (programming guide §4). No
//            DDR image may be staged: the scrub would erase it.

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <fx1_ddr3/ddr3_controller.h>
#include <fx1_ddr3/generic_axi_bridge.h>

#include "ddr_sysctrl.h"

namespace cdc::platforms::fx1_soc {

enum class DdrBoot { preinit, firmware };

// One --ddr3-fault request: KIND at `at` after CPU release (see parse_ddr3_fault).
struct Ddr3Fault {
    sc_core::sc_time at;
    std::string kind;
    std::string arg;
    std::uint64_t address = 0;
    unsigned count = 1;
    std::string text;  // the option as given, for the log
};

// T_US:KIND[:ARG]. KIND is one of
//   ce:ADDR  ue:ADDR  check:ADDR        stored-bit corruption at global DDR address ADDR
//                                       (single data bit / two bits of one 8-byte lane /
//                                       check bit 0 of the lane), without an ECC update
//   read-timeout[:N] read-malformed[:N] read-context[:N] read-generation[:N] read-unexpected
//                                       read-return faults of the next N AXI reads
//   write-result[:N]                    DFI write-result error of the next N granule writes
//   fatal:dfi|wdata|unmatched|stale|scrub
//   refresh-block:RANK  refresh-unblock:RANK  phy-fail  phy-recover
// False (with `why`) on a malformed request.
bool parse_ddr3_fault(const std::string& text, Ddr3Fault& fault, std::string& why);

// Usage text for the --ddr3-fault option.
const char* ddr3_fault_usage();

class Ddr3Subsystem : public sc_core::sc_module {
public:
    struct Options {
        DdrBoot boot = DdrBoot::preinit;
        bool ecc = true;  // DDR_SYS_ECC_ENABLE of the build (baseline: on)
        std::vector<Ddr3Fault> faults;
    };

    SC_HAS_PROCESS(Ddr3Subsystem);
    Ddr3Subsystem(sc_core::sc_module_name name, const Options& options);
    ~Ddr3Subsystem() override;

    // Bus-facing targets: MEMCTL_DDR, DDR3_CSR, DDR_SYSCTRL.
    tlm::tlm_target_socket<>& mem_target();
    tlm::tlm_target_socket<>& csr_target();
    tlm::tlm_target_socket<>& sysctrl_target();
    sc_core::sc_out<bool>& irq();

    // Host image staging (elaboration only). preinit: written after readiness, before
    // the CPUs run. firmware: throws (DDR content cannot exist before firmware init).
    // `address` is a global address inside the DDR aperture.
    void stage(std::uint64_t address, const std::uint8_t* data, std::size_t length, const std::string& what);
    // Host dump of the DDR aperture (global address), stored bytes, no ECC correction.
    // Throws when the range is outside the aperture or no topology is committed.
    void dump(std::uint64_t address, std::uint8_t* data, std::size_t length) const;
    // Host debug write into the DDR aperture at run time (global address): the stored
    // bytes with consistent ECC, no bus transaction, interrupt or exclusive-state change
    // (VP-only host agents such as the ISP stream bridge; never on the firmware path).
    // Throws like dump().
    void poke(std::uint64_t address, const std::uint8_t* data, std::size_t length);

    // CPU release (preinit: after the preload; firmware: at time 0).
    bool released() const noexcept { return released_; }
    const sc_core::sc_event& release_event() const noexcept { return release_event_; }
    sc_core::sc_time release_time() const noexcept { return release_time_; }
    // Non-empty when the preinit sequence failed (the simulation was stopped).
    const std::string& startup_error() const noexcept { return startup_error_; }

    DdrBoot boot() const noexcept { return options_.boot; }
    // One line each: the configuration (start of run) and the counters (end of run).
    std::string banner() const;
    std::string report() const;

    fx1::ddr3::Ddr3Controller& controller() noexcept { return *ctrl_; }
    const DdrSysctrl& sysctrl() const noexcept { return *sysctrl_; }

    // Storage identity for the platform exclusive monitor (review S5-R3). A DDR address
    // that port 0 decodes is named by its user offset, at kCanonicalBase + offset, so every
    // region alias of one location gets one name (L-13). Other addresses are their own name.
    // Appends the canonical pieces of [lo, hi) to `out`, split at region boundaries.
    static constexpr std::uint64_t kCanonicalBase = std::uint64_t{1} << 40;
    static constexpr std::uint64_t kCanonicalSize = std::uint64_t{1} << 36;
    void canonical_ranges(std::uint64_t lo, std::uint64_t hi,
                          std::vector<std::pair<std::uint64_t, std::uint64_t>>& out) const;

private:
    struct CsrMux;
    struct Staged {
        std::uint64_t offset;
        std::vector<std::uint8_t> bytes;
        std::string what;
    };

    void preinit_thread();
    void fault_thread();
    bool host_csr_write(std::uint32_t offset, std::uint32_t value);
    void apply_fault(const Ddr3Fault& fault);

    Options options_;
    fx1::ddr3::BuildConfig cfg_;
    std::unique_ptr<fx1::ddr3::Ddr3Controller> ctrl_;
    std::unique_ptr<fx1::ddr3::GenericAxiBridge> bridge_;
    std::unique_ptr<CsrMux> csr_mux_;
    std::unique_ptr<DdrSysctrl> sysctrl_;
    tlm_utils::simple_initiator_socket<Ddr3Subsystem> host_csr_{"host_csr"};
    sc_core::sc_out<bool> irq_{"irq"};
    std::vector<Staged> staged_;
    bool released_ = false;
    sc_core::sc_event release_event_;
    sc_core::sc_time release_time_ = sc_core::SC_ZERO_TIME;
    std::string startup_error_;
};

} // namespace cdc::platforms::fx1_soc
