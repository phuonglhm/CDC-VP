#include "fu3_crm/crm.h"

namespace fu3_crm {
namespace {

struct RegDef {
    std::uint32_t offset;
    std::uint32_t reset;
    std::uint32_t wmask;   // Bit RW, còn lại RO
};

// CSR_CRM_fu3_v2: 
// Reset của CLK_GATE_1/SW_RST_N_1 lấy theo field (25 bit = 1), 0x03FF_FFFF
// FU3_VP_ABI_1 (2026-10-26): bits 25-29 = UART3-7 (Q05-d, VP extension of CSR v2)
// -> 30 field bits, reset 0x3FFF_FFFF
// CLK_GATE_0 lấy theo field (9 bit = 1),  0x3FF
constexpr RegDef kRegs[] = {
    {FU3_CRM_VERSION_OFFSET,       0x0001'0000, 0x0},
    {FU3_CRM_PLL_OFFSET,           0x0000'0231, 0x3FFF},   // lock [14] tính khi đọc
    {FU3_CRM_PLL_CTRL_OFFSET,      0x0000'5949, 0x7FFF},
    {FU3_CRM_CLK_0_OFFSET,         0x0,         0x1},
    {FU3_CRM_CLK_1_OFFSET,         0x0,         0x1},
    {FU3_CRM_CLK_OFFSET,           0x0,         0x3FF},
    {FU3_CRM_CLK_GATE_0_OFFSET,    0x0000'01FF, 0x1FF},
    {FU3_CRM_CLK_GATE_1_OFFSET,    0x3FFF'FFFF, 0x3FFF'FFFF},
    {FU3_CRM_IBEX_SW_OFFSET,       0x0,         0x1},
    {FU3_CRM_FCLK_SW_OFFSET,       0x0,         0x1},
    {FU3_CRM_ACLK_SW_OFFSET,       0x1,         0x1},
    {FU3_CRM_PCLK_0_SW_OFFSET,     0x1,         0x1},
    {FU3_CRM_PCLK_1_SW_OFFSET,     0x1,         0x1},
    {FU3_CRM_SW_RST_N_0_OFFSET,    0x0000'00FF, 0xFF},
    {FU3_CRM_SW_RST_N_1_OFFSET,    0x3FFF'FFFF, 0x3FFF'FFFF},
    {FU3_CRM_SYSTEM_CLK_SW_OFFSET, 0x0,         0x1},
    {FU3_CRM_SW_RESET_REQ_OFFSET,  0x0,         0x0},      // Mức live, tính khi đọc
    {FU3_CRM_CRM_OFFSET,           0x0,         0x1},
};

const RegDef* find(std::uint32_t offset) {
    for (const auto& r : kRegs) {
        if (r.offset == offset)
            return &r;
    }
    return nullptr;
}

constexpr std::uint32_t kDecodedWindow = 0x1000;   // Ngoài 4 KiB trả lỗi
constexpr std::uint32_t kApertureSize = 0x1'0000;

// Thứ tự last_req_
enum Request { kWdt, kCpu, kSystem };

}  // namespace

Crm::Crm(sc_core::sc_module_name name, sc_core::sc_time hold, sc_core::sc_time startup_delay,
         sc_core::sc_time por_filter, unsigned readiness_cycles)
    : sc_core::sc_module(name), hold_(hold), startup_delay_(startup_delay), por_filter_(por_filter),
      readiness_cycles_(readiness_cycles) {
    socket.register_b_transport(this, &Crm::b_transport);
    socket.register_transport_dbg(this, &Crm::transport_dbg);
    reset_registers();

    SC_THREAD(filter_por);

    SC_METHOD(on_reset_inputs);
    sensitive << por_changed_ << sw_global_rst_req;

    SC_METHOD(on_request);
    sensitive << wdt_reset_req << cpu_reset_req << sw_reset_req;
    dont_initialize();

    SC_THREAD(sequencer);

    // Output chỉ được lái từ đây
    SC_METHOD(update);
    sensitive << changed_ << por_changed_ << sw_global_rst_req;
}

void Crm::filter_por() {
    // Spec 3.2: a PRSTN transition takes effect once the pin has kept its new
    // level for por_filter_; a pulse shorter than that is rejected. Asserted at
    // time 0 (power-on).
    while (true) {
        if (prst_n.read() == por_released_)
            sc_core::wait(prst_n.value_changed_event());
        while (true) {
            const sc_core::sc_time start = sc_core::sc_time_stamp();
            sc_core::wait(por_filter_, prst_n.value_changed_event());
            if (sc_core::sc_time_stamp() - start >= por_filter_)
                break;   // stable for the whole filter span
        }
        if (prst_n.read() != por_released_) {
            por_released_ = prst_n.read();
            if (!por_released_)
                ready_ = false;   // spec 3.1.1: readiness stays locked only until PRSTN
            por_changed_.notify(sc_core::SC_ZERO_TIME);
        }
    }
}

void Crm::reset_registers() {
    for (const auto& r : kRegs)
        regs_[r.offset / 4] = r.reset;
    pending_ = keep_dbg_ = false;
    debug_only_ = true;
}

void Crm::on_reset_inputs() {
    // PRSTN và Global request: giữ CRM reset, register về mặc định
    if (in_reset())
        reset_registers();
    restart_.notify(sc_core::SC_ZERO_TIME);
    changed_.notify(sc_core::SC_ZERO_TIME);
}

void Crm::on_request() {
    const bool now[3] = {wdt_reset_req.read(), cpu_reset_req.read(), sw_reset_req.read()};
    bool rise[3];
    for (unsigned i = 0; i < 3; ++i) {
        rise[i] = now[i] && !last_req_[i];
        last_req_[i] = now[i];
    }
    // Vẫn theo dõi mức khi CRM đang reset: mức giữ qua lúc nhả reset không tính là sườn
    if ((rise[kWdt] || rise[kCpu] || rise[kSystem]) && !in_reset()) {
        if (rise[kWdt] || rise[kSystem])
            debug_only_ = false;   // Có yêu cầu reset cả DBG
        pending_ = true;
        request_.notify(sc_core::SC_ZERO_TIME);
    }
}

void Crm::set(std::uint32_t offset, std::uint32_t value) {
    reg(offset) = value;
    changed_.notify(sc_core::SC_ZERO_TIME);
}

bool Crm::delay(sc_core::sc_time t) {
    sc_core::wait(t, restart_);
    return !in_reset();
}

bool Crm::startup() {
    // Spec 4.1: CVA6 giữ reset (FCLK_SW = 0), nhả hệ thống, chờ, nhả Ibex
    set(FU3_CRM_SYSTEM_CLK_SW_OFFSET, 1);
    keep_dbg_ = false;
    if (!delay(startup_delay_))
        return false;
    set(FU3_CRM_IBEX_SW_OFFSET, 1);
    return true;
}

void Crm::sequencer() {
    while (true) {
        while (in_reset())
            sc_core::wait(restart_);
        // Spec 3.1.1: after PRSTN the controller starts once readiness has been
        // qualified for readiness_cycles_ OSC cycles (bypassed inputs still count).
        if (!ready_) {
            if (!delay(sc_core::sc_time(1.0 / FU3_CRM_OSC_HZ, sc_core::SC_SEC) * readiness_cycles_))
                continue;
            ready_ = true;
        }
        if (!startup())
            continue;

        // HAS 6.5: WDT, SYSCSR và debug reset => reset hệ thống (trừ CRM), debug reset giữ DBG
        // Boot chạy lại từ đầu: Ibex vào ROM, CVA6 chờ Ibex nhả (HAS 5.2)
        while (!in_reset()) {
            if (!pending_) {
                sc_core::wait(request_ | restart_);
                continue;
            }
            pending_ = false;
            keep_dbg_ = debug_only_;
            debug_only_ = true;
            set(FU3_CRM_FCLK_SW_OFFSET, 0);
            set(FU3_CRM_IBEX_SW_OFFSET, 0);
            set(FU3_CRM_SYSTEM_CLK_SW_OFFSET, 0);
            if (!delay(hold_) || !startup())
                break;
        }
    }
}

void Crm::update() {
    const bool on = !in_reset();
    const bool sys = on && (regs_[FU3_CRM_SYSTEM_CLK_SW_OFFSET / 4] & 1);
    sys_rst_n.write(sys);
    ibex_rst_n.write(sys && (regs_[FU3_CRM_IBEX_SW_OFFSET / 4] & 1));
    cva6_rst_n.write(sys && (regs_[FU3_CRM_FCLK_SW_OFFSET / 4] & 1));
    dbg_rst_n.write(sys || (on && keep_dbg_));
    irq.write(on && (regs_[FU3_CRM_CRM_OFFSET / 4] & 1));
    clk_gate_0.write(regs_[FU3_CRM_CLK_GATE_0_OFFSET / 4]);
    clk_gate_1.write(regs_[FU3_CRM_CLK_GATE_1_OFFSET / 4]);
    sw_rst_n_0.write(regs_[FU3_CRM_SW_RST_N_0_OFFSET / 4]);
    sw_rst_n_1.write(regs_[FU3_CRM_SW_RST_N_1_OFFSET / 4]);
}

std::uint32_t Crm::read_register(std::uint32_t offset) const {
    if (offset % 4 || !find(offset))
        return 0;
    switch (offset) {
        case FU3_CRM_PLL_OFFSET:
            return regs_[offset / 4] | (pll_locked() ? FU3_CRM_PLL_LOCK : 0);
        case FU3_CRM_SW_RESET_REQ_OFFSET:
            return (sw_global_rst_req.read() ? FU3_CRM_REQ_GLOBAL : 0) |
                   (sw_reset_req.read() ? FU3_CRM_REQ_SYSTEM : 0) |
                   (wdt_reset_req.read() ? FU3_CRM_REQ_WATCHDOG : 0) |
                   (cpu_reset_req.read() ? FU3_CRM_REQ_SYSRESETREQ : 0);
        default:
            return regs_[offset / 4];
    }
}

bool Crm::pll_locked() const {
    // CSR v2 pll_ctrl: lken forces lock; lock_type = 1 (reset) locks after
    // `count` OSC cycles of the programmable lock counter (reset 404 = 20.2 us).
    // The counter restarts at power-up and when the dividers are written while
    // running (VP choice). lock_type = 0 selects the PLL LOCK signal: there is
    // no analog PLL in the VP, so it is taken as locked while powered up.
    const std::uint32_t ctrl = regs_[FU3_CRM_PLL_CTRL_OFFSET / 4];
    if (ctrl & FU3_CRM_PLL_LKEN)
        return true;
    if (ctrl & FU3_CRM_PLL_POWERDOWN)
        return false;
    if (!(ctrl & FU3_CRM_PLL_LOCK_TYPE))
        return true;
    const unsigned count = (ctrl >> 4) & 0x3FF;
    return sc_core::sc_time_stamp() - lock_start_ >= sc_core::sc_time(1.0 / FU3_CRM_OSC_HZ, sc_core::SC_SEC) * count;
}

std::uint64_t Crm::pll_clock_hz() const {
    // CSR v2: Fout = Fin * M / N / OD, bypass: Fin / N
    const std::uint32_t pll = regs_[FU3_CRM_PLL_OFFSET / 4];
    const std::uint32_t ctrl = regs_[FU3_CRM_PLL_CTRL_OFFSET / 4];
    const std::uint64_t n = (pll >> 8) & 0x3F, m = (pll >> 2) & 0x3F, od = 1u << (pll & 0x3);
    if ((ctrl & FU3_CRM_PLL_POWERDOWN) || !(ctrl & FU3_CRM_PLL_OE) || n == 0)
        return 0;
    return ctrl & FU3_CRM_PLL_BYPASS ? FU3_CRM_OSC_HZ / n : FU3_CRM_OSC_HZ * m / (n * od);
}

std::uint64_t Crm::branch_hz() const {
    // Spec 3.7.1: nhánh chỉ có tác dụng khi cả 2 bộ chọn chọn nó, còn lại là OSC
    const bool branch = (regs_[FU3_CRM_CLK_1_OFFSET / 4] & 1) && (regs_[FU3_CRM_CLK_0_OFFSET / 4] & 1);
    return branch ? pll_clock_hz() : FU3_CRM_OSC_HZ;
}

std::uint64_t Crm::cpu_clock_hz() const {
    return branch_hz();
}

std::uint64_t Crm::bus_clock_hz() const {
    const bool branch = (regs_[FU3_CRM_CLK_1_OFFSET / 4] & 1) && (regs_[FU3_CRM_CLK_0_OFFSET / 4] & 1);
    const std::uint32_t div = regs_[FU3_CRM_CLK_OFFSET / 4] & 0x3FF;
    return branch && div ? branch_hz() / (2u * div) : branch_hz();
}

void Crm::b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time&) {
    access(tx, false);
}

unsigned Crm::transport_dbg(tlm::tlm_generic_payload& tx) {
    access(tx, true);
    return tx.is_response_ok() ? tx.get_data_length() : 0;
}

void Crm::access(tlm::tlm_generic_payload& tx, bool backdoor) {
    const std::uint64_t addr = tx.get_address();
    // Chỉ truy cập word 32-bit thẳng hàng
    if (tx.get_data_length() != 4) {
        tx.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if (addr % 4 || addr >= kApertureSize) {
        tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }
    unsigned char* data = tx.get_data_ptr();
    const auto offset = static_cast<std::uint32_t>(addr);
    if (offset >= kDecodedWindow) {   // Ngoài vùng decode: lỗi, dữ liệu đọc = 0
        if (tx.is_read()) for (unsigned i = 0; i < 4; ++i) data[i] = 0;
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }

    if (tx.is_read()) {
        const std::uint32_t value = read_register(offset);
        for (unsigned i = 0; i < 4; ++i)
            data[i] = (value >> (8 * i)) & 0xFF;
    } else if (tx.is_write()) {
        // PSTRB không qua bridge bất đồng bộ: luôn ghi cả word
        // Offset không map: bỏ qua. Đang reset: ghi không có tác dụng (trừ backdoor)
        const RegDef* def = find(offset);
        if (def && (backdoor || !in_reset())) {
            std::uint32_t value = 0;
            for (unsigned i = 0; i < 4; ++i) value |= std::uint32_t(data[i]) << (8 * i);
            const std::uint32_t next = (reg(offset) & ~def->wmask) | (value & def->wmask);
            // Spec 3.7.4: change the bus divider only with both selectors on the
            // oscillator; anything else is outside the supported sequence.
            if (!backdoor && offset == FU3_CRM_CLK_OFFSET && next != reg(offset) &&
                ((reg(FU3_CRM_CLK_0_OFFSET) & 1) || (reg(FU3_CRM_CLK_1_OFFSET) & 1)))
                SC_REPORT_WARNING("fu3_crm/clock_switch",
                                  "CLK.div changed while a selector is on the branch: outside the supported "
                                  "switching sequence (CRM spec 3.7.4: select the oscillator first)");
            const bool running = !(reg(FU3_CRM_PLL_CTRL_OFFSET) & FU3_CRM_PLL_POWERDOWN);
            reg(offset) = next;
            const bool now_running = !(reg(FU3_CRM_PLL_CTRL_OFFSET) & FU3_CRM_PLL_POWERDOWN);
            if ((now_running && !running) || (offset == FU3_CRM_PLL_OFFSET && now_running))
                lock_start_ = sc_core::sc_time_stamp();   // PLL (re)locks
            changed_.notify(sc_core::SC_ZERO_TIME);
        }
    } else {
        tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
}

}  // namespace fu3_crm
