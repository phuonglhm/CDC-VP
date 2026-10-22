#include "ddr3_subsystem.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include <fx1/fx1_memory_map.h>
#include <fx1_ddr3/functional_phy.h>
#include <fx1_ddr3/fx1_ddr3_regs.h>
#include <fx1_ddr3/speed_bin.h>

namespace cdc::platforms::fx1_soc {

namespace {

using fx1::ddr3::Ddr3Controller;

static_assert(FX1_DDR3_CSR_APERTURE_SIZE == FX1_DDR3_CSR_SIZE, "DDR3_CSR bus window != controller CSR space");

// Mission readiness must come within this simulated time after GLOBAL_CTRL.enable. The
// ECC startup scrub of the 4 GiB baseline takes about 0.67 s (DEC-10).
sc_core::sc_time kPreinitLimit() { return sc_core::sc_time(10, sc_core::SC_SEC); }

// Baseline physical model (plan §5.1): NATIVE_R2, DDR3-1600, 4 Gb x8 devices on the x32
// bus, 2 ranks = 4 GiB physical, ECC on; one memory port (MEMCTL_DDR).
fx1::ddr3::BuildConfig make_config(bool ecc)
{
    auto cfg = fx1::ddr3::BuildConfig::native_r2();
    cfg.n_ports = 1;
    cfg.ecc_enable = ecc;
    cfg.speed_bin = fx1::ddr3::SpeedBin::DDR3_1600;
    cfg.device_width = fx1::ddr3::DeviceWidth::X8;
    cfg.density = fx1::ddr3::Density::Gb4;
    return cfg;
}

std::string hex(std::uint64_t v)
{
    std::ostringstream s;
    s << "0x" << std::hex << v;
    return s.str();
}

bool parse_u64(const std::string& text, std::uint64_t& value)
{
    try {
        std::size_t used = 0;
        value = std::stoull(text, &used, 0);
        return used == text.size() && !text.empty();
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace

// The controller's CSR port has one AXI4-Lite slave; the bus and the platform's preinit
// initiator share it (the preinit accesses end before any CPU runs).
struct Ddr3Subsystem::CsrMux : sc_core::sc_module {
    tlm_utils::simple_target_socket<CsrMux> bus{"bus"};
    tlm_utils::simple_target_socket<CsrMux> host{"host"};
    tlm_utils::simple_initiator_socket<CsrMux> out{"out"};

    explicit CsrMux(sc_core::sc_module_name name) : sc_module(name)
    {
        bus.register_b_transport(this, &CsrMux::forward);
        host.register_b_transport(this, &CsrMux::forward);
        bus.register_transport_dbg(this, &CsrMux::forward_dbg);
        host.register_transport_dbg(this, &CsrMux::forward_dbg);
    }
    void forward(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) { out->b_transport(tx, delay); }
    unsigned forward_dbg(tlm::tlm_generic_payload& tx) { return out->transport_dbg(tx); }
};

const char* ddr3_fault_usage()
{
    return "  --ddr3-fault T_US:KIND[:ARG]  host fault injection into the DDR3 controller,\n"
           "                T_US microseconds of simulated time after CPU release; may be repeated.\n"
           "                KIND: ce:ADDR ue:ADDR check:ADDR (stored-bit corruption at global\n"
           "                DDR address ADDR), read-timeout[:N] read-malformed[:N]\n"
           "                read-context[:N] read-generation[:N] read-unexpected,\n"
           "                write-result[:N], fatal:dfi|wdata|unmatched|stale|scrub,\n"
           "                refresh-block:RANK refresh-unblock:RANK phy-fail phy-recover\n";
}

bool parse_ddr3_fault(const std::string& text, Ddr3Fault& fault, std::string& why)
{
    fault = Ddr3Fault{};
    fault.text = text;
    const auto c1 = text.find(':');
    std::uint64_t us = 0;
    if (c1 == std::string::npos || !parse_u64(text.substr(0, c1), us)) {
        why = "expected T_US:KIND[:ARG]";
        return false;
    }
    fault.at = sc_core::sc_time(static_cast<double>(us), sc_core::SC_US);
    const auto c2 = text.find(':', c1 + 1);
    fault.kind = text.substr(c1 + 1, c2 == std::string::npos ? std::string::npos : c2 - c1 - 1);
    fault.arg = c2 == std::string::npos ? std::string() : text.substr(c2 + 1);
    const std::string& k = fault.kind;
    const std::string& a = fault.arg;
    if (k == "ce" || k == "ue" || k == "check") {
        if (!parse_u64(a, fault.address) || fault.address < FX1_DDR_BASE ||
            fault.address >= std::uint64_t{FX1_DDR_BASE} + FX1_DDR_SIZE) {
            why = k + " needs a global address inside the DDR aperture";
            return false;
        }
        return true;
    }
    if (k == "read-timeout" || k == "read-malformed" || k == "read-context" || k == "read-generation" ||
        k == "write-result") {
        std::uint64_t n = 1;
        if (!a.empty() && (!parse_u64(a, n) || n == 0 || n > 1000000)) {
            why = k + ": count must be 1..1000000";
            return false;
        }
        fault.count = static_cast<unsigned>(n);
        return true;
    }
    if (k == "read-unexpected" || k == "phy-fail" || k == "phy-recover") {
        if (!a.empty()) {
            why = k + " takes no argument";
            return false;
        }
        return true;
    }
    if (k == "fatal") {
        if (a == "dfi" || a == "wdata" || a == "unmatched" || a == "stale" || a == "scrub") return true;
        why = "fatal:dfi|wdata|unmatched|stale|scrub";
        return false;
    }
    if (k == "refresh-block" || k == "refresh-unblock") {
        std::uint64_t r = 0;
        if (!parse_u64(a, r) || r > 1) {
            why = k + ": rank must be 0 or 1";
            return false;
        }
        fault.count = static_cast<unsigned>(r);
        return true;
    }
    why = "unknown fault kind '" + k + "'";
    return false;
}

Ddr3Subsystem::Ddr3Subsystem(sc_core::sc_module_name name, const Options& options)
    : sc_module(name), options_(options), cfg_(make_config(options.ecc))
{
    ctrl_ = std::make_unique<Ddr3Controller>("ddr3", cfg_);
    // The FX1 bus removes the target base (MEMCTL_DDR): the bridge adds it back so the
    // controller sees global AXI addresses (DEC-07).
    bridge_ = std::make_unique<fx1::ddr3::GenericAxiBridge>("ddr3_bridge", cfg_.axi_beat_bytes(), FX1_DDR_BASE);
    csr_mux_ = std::make_unique<CsrMux>("ddr3_csr_mux");
    sysctrl_ = std::make_unique<DdrSysctrl>("ddr_sysctrl", *ctrl_, options_.boot == DdrBoot::preinit);

    bridge_->initiator.bind(ctrl_->mem_socket[0]);
    csr_mux_->out.bind(ctrl_->csr_socket);
    host_csr_.bind(csr_mux_->host);
    ctrl_->irq.bind(irq_);

    // Sideband at power-up (programming guide §2): port 0 decodes the whole CPU aperture
    // until firmware commits a region map, and its QoS table is configured (all NORMAL).
    ctrl_->set_bootstrap_region(0, 0, {FX1_DDR_BASE, FX1_DDR_SIZE});
    ctrl_->qos_set_configured(0, true);

    if (options_.boot == DdrBoot::preinit) {
        SC_THREAD(preinit_thread);
    } else {
        released_ = true;
    }
    if (!options_.faults.empty()) SC_THREAD(fault_thread);
}

Ddr3Subsystem::~Ddr3Subsystem() = default;

tlm::tlm_target_socket<>& Ddr3Subsystem::mem_target() { return bridge_->target; }
tlm::tlm_target_socket<>& Ddr3Subsystem::csr_target() { return csr_mux_->bus; }
tlm::tlm_target_socket<>& Ddr3Subsystem::sysctrl_target() { return sysctrl_->socket; }
sc_core::sc_out<bool>& Ddr3Subsystem::irq() { return irq_; }

void Ddr3Subsystem::stage(std::uint64_t address, const std::uint8_t* data, std::size_t length, const std::string& what)
{
    if (options_.boot == DdrBoot::firmware)
        throw std::runtime_error("fx1_soc: " + what + " targets DDR at " + hex(address) +
                                 ", but with --ddr-boot firmware the DDR is not initialized before the "
                                 "firmware runs (the ECC startup scrub would erase it). Place the image in "
                                 "BootROM/SRAM, or use --ddr-boot preinit.");
    if (sc_core::sc_is_running())
        throw std::logic_error("fx1_soc: DDR images must be staged before the simulation starts");
    if (address < FX1_DDR_BASE || length > FX1_DDR_SIZE || address - FX1_DDR_BASE > FX1_DDR_SIZE - length)
        throw std::out_of_range("fx1_soc: " + what + " does not fit in DDR at " + hex(address));
    staged_.push_back({address - FX1_DDR_BASE, std::vector<std::uint8_t>(data, data + length), what});
}

void Ddr3Subsystem::canonical_ranges(std::uint64_t lo, std::uint64_t hi,
                                     std::vector<std::pair<std::uint64_t, std::uint64_t>>& out) const
{
    const std::uint64_t ap_lo = FX1_DDR_BASE, ap_hi = ap_lo + FX1_DDR_SIZE;
    if (hi <= ap_lo || lo >= ap_hi) { // not DDR: the bus address is the identity
        out.push_back({lo, hi});
        return;
    }
    const fx1::ddr3::RegionMap map = ctrl_->decode_map(0);
    while (lo < hi) {
        bool decoded = false;
        for (const auto& r : map) {
            if (r.size == 0 || lo < r.base || lo - r.base >= r.size) continue;
            const std::uint64_t end = std::min<std::uint64_t>(hi, std::uint64_t{r.base} + r.size);
            out.push_back({kCanonicalBase + (lo - r.base), kCanonicalBase + (end - r.base)});
            lo = end;
            decoded = true;
            break;
        }
        if (decoded) continue;
        // Not decoded (DECERR): its own name, up to the next region.
        std::uint64_t end = hi;
        for (const auto& r : map)
            if (r.size != 0 && r.base > lo && r.base < end) end = r.base;
        out.push_back({lo, end});
        lo = end;
    }
}

void Ddr3Subsystem::dump(std::uint64_t address, std::uint8_t* data, std::size_t length) const
{
    if (address < FX1_DDR_BASE || length > FX1_DDR_SIZE || address - FX1_DDR_BASE > FX1_DDR_SIZE - length)
        throw std::out_of_range("fx1_soc: dump range is outside DDR");
    // User offset = global address - FX1_DDR_BASE: the bootstrap map (and any firmware
    // region 0 based at FX1_DDR_BASE) maps the aperture start to user offset 0.
    if (!ctrl_->debug_read(address - FX1_DDR_BASE, data, length))
        throw std::runtime_error("fx1_soc: DDR dump at " + hex(address) +
                                 " failed: the controller has no committed topology covering it");
}

bool Ddr3Subsystem::host_csr_write(std::uint32_t offset, std::uint32_t value)
{
    unsigned char buf[4];
    for (unsigned i = 0; i < 4; ++i) buf[i] = static_cast<unsigned char>(value >> (8 * i));
    tlm::tlm_generic_payload tx;
    tx.set_command(tlm::TLM_WRITE_COMMAND);
    tx.set_address(offset);
    tx.set_data_ptr(buf);
    tx.set_data_length(4);
    tx.set_streaming_width(4);
    tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    host_csr_->b_transport(tx, delay);
    return tx.get_response_status() == tlm::TLM_OK_RESPONSE;
}

void Ddr3Subsystem::preinit_thread()
{
    // The firmware sequence of programming guide §4, issued by the platform (plan §5.3:
    // the same validation and lifecycle as a firmware boot, not a forced READY bit).
    using namespace fx1::ddr3;
    std::string failed;
    auto w = [&](std::uint32_t offset, std::uint32_t value) {
        if (failed.empty() && !host_csr_write(offset, value))
            failed = "CSR write " + hex(offset) + " <- " + hex(value) + " rejected";
    };
    const TimingImage t = timing_image(cfg_);
    for (unsigned i = 0; i < kTimingWords; ++i) w(FX1_DDR3_TIMING_SHADOW(i), t[i]);
    const DfiImage dfi = dfi_image(cfg_);
    w(FX1_DDR3_DFI_WR_ENABLE_DELAY, dfi.wr_enable_delay);
    w(FX1_DDR3_DFI_RD_ENABLE_DELAY, dfi.rd_enable_delay);
    w(FX1_DDR3_PHY_MAX_READ_RETURN, dfi.max_read_return);
    w(FX1_DDR3_DFI_ASSEMBLY_MARGIN, dfi.assembly_margin);
    const InitWaitImage iw = init_wait_image(cfg_);
    w(FX1_DDR3_INIT_RESET_RELEASE_WAIT, iw.reset_release);
    w(FX1_DDR3_INIT_CKE_LOW_WAIT, iw.cke_low);
    w(FX1_DDR3_INIT_CKE_HIGH_WAIT, iw.cke_high);
    w(FX1_DDR3_INIT_DLL_WAIT, iw.dll);
    w(FX1_DDR3_INIT_ZQ_WAIT, iw.zq);
    DeviceGeometry g{};
    if (!device_geometry(cfg_.device_width, cfg_.density, g)) failed = "unsupported device organisation";
    const unsigned page_code = g.page_words == 256 ? FX1_DDR3_TOPO_PAGE_256
                             : g.page_words == 512 ? FX1_DDR3_TOPO_PAGE_512
                             : g.page_words == 1024 ? FX1_DDR3_TOPO_PAGE_1024
                                                    : FX1_DDR3_TOPO_PAGE_2048;
    w(FX1_DDR3_TOPO_SHADOW, FX1_DDR3_TOPO_VAL(cfg_.max_ranks(), g.row_bits, page_code, FX1_DDR3_TOPO_MAP_LINEAR));
    w(FX1_DDR3_TOPO_COMMIT, 1);
    w(FX1_DDR3_TIMING_COMMIT, 1);
    w(FX1_DDR3_DFI_TIMING_COMMIT, 1);
    w(FX1_DDR3_GLOBAL_CTRL, FX1_DDR3_GLOBAL_CTRL_ENABLE);

    const sc_core::sc_time end = sc_core::sc_time_stamp() + kPreinitLimit();
    while (failed.empty() && !ctrl_->mission_ready() && !ctrl_->init_failed() && !ctrl_->fatal()) {
        const sc_core::sc_time now = sc_core::sc_time_stamp();
        if (now >= end) break;
        wait(end - now, ctrl_->lifecycle_event());
    }
    if (failed.empty() && !ctrl_->mission_ready())
        failed = ctrl_->init_failed() ? "initialization failed"
               : ctrl_->fatal()       ? "fatal event during initialization"
                                      : "no mission readiness within " + kPreinitLimit().to_string();

    // Preload strictly after the scrub: debug_write regenerates the ECC records.
    for (const auto& s : staged_) {
        if (!failed.empty()) break;
        if (!ctrl_->debug_write(s.offset, s.bytes.data(), s.bytes.size()))
            failed = s.what + ": DDR preload at " + hex(FX1_DDR_BASE + s.offset) + " failed";
    }
    staged_.clear();
    staged_.shrink_to_fit();

    if (!failed.empty()) {
        startup_error_ = "DDR3 preinit: " + failed;
        sc_core::sc_stop();
        return;
    }
    released_ = true;
    release_time_ = sc_core::sc_time_stamp();
    release_event_.notify(sc_core::SC_ZERO_TIME);
    // Hand control back to sc_main, which starts the run's time budget now.
    sc_core::sc_pause();
}

void Ddr3Subsystem::apply_fault(const Ddr3Fault& f)
{
    bool ok = true;
    const std::uint64_t offset = f.address - FX1_DDR_BASE;
    const std::uint64_t granule = offset / 32;
    const unsigned byte = static_cast<unsigned>(offset % 32);
    const std::string& k = f.kind;
    if (k == "ce") {
        ok = ctrl_->fault_flip_data_bit(granule, byte * 8);
    } else if (k == "ue") {
        const unsigned lane = byte / 8;
        ok = ctrl_->fault_flip_data_bit(granule, lane * 64) && ctrl_->fault_flip_data_bit(granule, lane * 64 + 1);
    } else if (k == "check") {
        ok = ctrl_->fault_flip_check_bit(granule, byte / 8, 0);
    } else if (k == "read-timeout") {
        ctrl_->fault_read_return(Ddr3Controller::ReturnFault::Timeout, f.count);
    } else if (k == "read-malformed") {
        ctrl_->fault_read_return(Ddr3Controller::ReturnFault::Malformed, f.count);
    } else if (k == "read-context") {
        ctrl_->fault_read_return(Ddr3Controller::ReturnFault::ContextMismatch, f.count);
    } else if (k == "read-generation") {
        ctrl_->fault_read_return(Ddr3Controller::ReturnFault::GenerationMismatch, f.count);
    } else if (k == "read-unexpected") {
        ctrl_->fault_read_return(Ddr3Controller::ReturnFault::Unexpected, 1);
    } else if (k == "write-result") {
        ctrl_->fault_write_result(f.count);
    } else if (k == "fatal") {
        using F = Ddr3Controller::FatalFault;
        const F src = f.arg == "dfi"       ? F::DfiProtocol
                    : f.arg == "wdata"     ? F::WriteDataProtocol
                    : f.arg == "unmatched" ? F::WriteResultUnmatched
                    : f.arg == "stale"     ? F::StaleLockRelease
                                           : F::ScrubMapping;
        ctrl_->fault_fatal(src);
    } else if (k == "refresh-block" || k == "refresh-unblock") {
        ctrl_->fault_block_refresh(f.count, k == "refresh-block");
    } else if (k == "phy-fail" || k == "phy-recover") {
        auto* phy = ctrl_->functional_phy();
        ok = phy != nullptr;
        if (phy) phy->force_failure(k == "phy-fail");
    }
    std::cout << "[fx1_soc] ddr3 fault at " << sc_core::sc_time_stamp() << ": " << f.text
              << (ok ? "" : " (not applied: address not mapped by the committed topology)") << '\n';
}

void Ddr3Subsystem::fault_thread()
{
    if (!released_) wait(release_event_);
    auto faults = options_.faults;
    std::stable_sort(faults.begin(), faults.end(), [](const Ddr3Fault& a, const Ddr3Fault& b) { return a.at < b.at; });
    for (const auto& f : faults) {
        const sc_core::sc_time due = release_time_ + f.at;
        if (due > sc_core::sc_time_stamp()) wait(due - sc_core::sc_time_stamp());
        apply_fault(f);
    }
}

std::string Ddr3Subsystem::banner() const
{
    std::ostringstream s;
    s << "[fx1_soc] DDR3: boot " << (options_.boot == DdrBoot::preinit ? "preinit" : "firmware") << ", "
      << fx1::ddr3::to_string(cfg_.profile) << " DDR3-1600, " << cfg_.max_ranks()
      << " ranks of 4Gb x8 on x32 (4096 MiB physical), ECC " << (cfg_.ecc_enable ? "on" : "off")
      << ", port 0 aperture " << hex(FX1_DDR_BASE) << "+" << (FX1_DDR_SIZE >> 20) << " MiB, CSR "
      << hex(FX1_DDR3_CSR_BASE) << ", DDR_SYSCTRL " << hex(FX1_DDR_SYSCTRL_BASE) << ", IRQ " << FX1_IRQ_DDR;
    return s.str();
}

std::string Ddr3Subsystem::report() const
{
    std::ostringstream s;
    s << "AXI bursts " << bridge_->transactions() << ", row hit/miss/conflict "
      << ctrl_->bank_timer().row_hits() << '/' << ctrl_->bank_timer().row_misses() << '/'
      << ctrl_->bank_timer().row_conflicts() << ", CE " << ctrl_->ce_count() << ", UE " << ctrl_->ue_count()
      << ", quarantine " << (ctrl_->quarantined() ? "on" : "off") << ", fatal " << hex(ctrl_->fatal_status())
      << ", power entries " << sysctrl_->entries() << " auto-wakes " << sysctrl_->auto_wakes() << ", resets cold "
      << sysctrl_->cold_resets() << " core " << sysctrl_->core_resets() << ", host pages "
      << ctrl_->resident_pages();
    return s.str();
}

} // namespace cdc::platforms::fx1_soc
