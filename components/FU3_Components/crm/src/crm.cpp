#include "fu3_crm/crm.h"

namespace fu3_crm {
namespace {

struct RegDef {
    std::uint32_t offset;
    std::uint32_t reset;
    std::uint32_t wmask;   // Bit RW, còn lại RO
};

// FU3 CRM Specification V1.0
constexpr RegDef kRegs[] = {
    {FU3_CRM_VERSION_OFFSET,       0x0001'0000, 0x0},
    {FU3_CRM_PLL_OFFSET,           0x0000'0231, 0x3FFF},   // lock [14] tính khi đọc
    {FU3_CRM_PLL_CTRL_OFFSET,      0x0000'5949, 0x7FFF},
    {FU3_CRM_CLK_0_OFFSET,         0x0,         0x1},
    {FU3_CRM_CLK_1_OFFSET,         0x0,         0x1},
    {FU3_CRM_CLK_OFFSET,           0x0,         0x3FF},
    {FU3_CRM_CLK_GATE_0_OFFSET,    0x0000'1FFF, 0x1FFF},
    {FU3_CRM_CLK_GATE_1_OFFSET,    0x0000'7FFF, 0x7FFF},
    {FU3_CRM_FCLK_SW_OFFSET,       0x0,         0x1},
    {FU3_CRM_ACLK_SW_OFFSET,       0x1,         0x1},
    {FU3_CRM_PCLK_0_SW_OFFSET,     0x1,         0x1},
    {FU3_CRM_PCLK_1_SW_OFFSET,     0x1,         0x1},
    {FU3_CRM_SW_RST_N_0_OFFSET,    0x0000'00FF, 0xFF},
    {FU3_CRM_SW_RST_N_1_OFFSET,    0x0000'000F, 0xF},
    {FU3_CRM_SW_RST_N_2_OFFSET,    0x0000'3FFF, 0x3FFF},
    {FU3_CRM_CPU_DEBUG_OFFSET,     0x0,         0x1},
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

Crm::Crm(sc_core::sc_module_name name, sc_core::sc_time hold, sc_core::sc_time startup_delay)
    : sc_core::sc_module(name), hold_(hold), startup_delay_(startup_delay) {
    socket.register_b_transport(this, &Crm::b_transport);
    socket.register_transport_dbg(this, &Crm::transport_dbg);
    reset_registers();

    SC_METHOD(on_reset_inputs);
    sensitive << prst_n << sw_global_rst_req;

    SC_METHOD(on_request);
    sensitive << wdt_reset_req << cpu_reset_req << sw_reset_req;
    dont_initialize();

    SC_THREAD(firmware);

    // Output chỉ được lái từ đây
    SC_METHOD(update);
    sensitive << changed_ << prst_n << sw_global_rst_req;
}

void Crm::reset_registers() {
    for (const auto& r : kRegs) 
        regs_[r.offset / 4] = r.reset;
    done_ = pending_ = false;
}

void Crm::on_reset_inputs() {
    // PRSTN và global request: giữ CRM reset, register về mặc định
    if (in_reset()) 
        reset_registers();
    restart_.notify(sc_core::SC_ZERO_TIME);
    changed_.notify(sc_core::SC_ZERO_TIME);
}

void Crm::on_request() {
    const bool now[3] = {wdt_reset_req.read(), cpu_reset_req.read(), sw_reset_req.read()};
    bool rise = false;
    for (unsigned i = 0; i < 3; ++i) {
        rise |= now[i] && !last_req_[i];
        last_req_[i] = now[i];
    }
    // Vẫn theo dõi mức khi CRM đang reset: mức giữ qua lúc nhả reset không tính là sườn
    if (rise && !in_reset()) {
        pending_ = true;   // Đường ngắt chỉ giữ 1 sự kiện 
        request_.notify(sc_core::SC_ZERO_TIME);
    }
}

bool Crm::delay(sc_core::sc_time t) {
    sc_core::wait(t, restart_);
    return !in_reset();
}

void Crm::firmware() {
    auto set = [this](std::uint32_t offset, std::uint32_t v) {
        reg(offset) = v;
        changed_.notify(sc_core::SC_ZERO_TIME);
    };
    while (true) {
        while (in_reset()) 
            sc_core::wait(restart_);

        // Khởi động: Nhả miền hệ thống, chờ ổn định, DONE, rồi nhả CVA6
        set(FU3_CRM_SYSTEM_CLK_SW_OFFSET, 1);
        if (!delay(startup_delay_)) 
            continue;
        done_ = true;
        set(FU3_CRM_FCLK_SW_OFFSET, 1);

        // Xét mức live, chỉ yêu cầu ưu tiên cao nhất
        while (!in_reset()) {
            if (!pending_ || !(reg(FU3_CRM_CLK_GATE_0_OFFSET) & FU3_CRM_GATE0_MRV)) {
                sc_core::wait(request_ | restart_ | changed_);   // MRV bị gate: firmware dừng
                continue;
            }
            pending_ = false;
            if (wdt_reset_req.read() || cpu_reset_req.read()) {
                set(FU3_CRM_SYSTEM_CLK_SW_OFFSET, 0);
                set(FU3_CRM_FCLK_SW_OFFSET, 0);
                if (!delay(hold_)) 
                    break;
                set(FU3_CRM_SYSTEM_CLK_SW_OFFSET, 1);
                if (!delay(startup_delay_))
                    break;
                done_ = true;
                set(FU3_CRM_FCLK_SW_OFFSET, 1);
            } else if (sw_reset_req.read()) {
                set(FU3_CRM_SYSTEM_CLK_SW_OFFSET, 0);
                if (!delay(hold_))
                    break;
                set(FU3_CRM_SYSTEM_CLK_SW_OFFSET, 1);
                done_ = true;
            }
        }
    }
}

void Crm::update() {
    const bool on = !in_reset();
    cva6_rst_n.write(on && (regs_[FU3_CRM_FCLK_SW_OFFSET / 4] & 1));
    sys_rst_n.write(on && (regs_[FU3_CRM_SYSTEM_CLK_SW_OFFSET / 4] & 1));
    dbg_rst_n.write(on && (regs_[FU3_CRM_CPU_DEBUG_OFFSET / 4] & 1));
    irq.write(on && (regs_[FU3_CRM_CRM_OFFSET / 4] & 1));
    clk_gate_0.write(regs_[FU3_CRM_CLK_GATE_0_OFFSET / 4]);
    clk_gate_1.write(regs_[FU3_CRM_CLK_GATE_1_OFFSET / 4]);
    sw_rst_n_0.write(regs_[FU3_CRM_SW_RST_N_0_OFFSET / 4]);
    sw_rst_n_1.write(regs_[FU3_CRM_SW_RST_N_1_OFFSET / 4]);
    sw_rst_n_2.write(regs_[FU3_CRM_SW_RST_N_2_OFFSET / 4]);
}

std::uint32_t Crm::read_register(std::uint32_t offset) const {
    if (offset % 4 || !find(offset)) 
        return 0;
    const std::uint32_t ctrl = regs_[FU3_CRM_PLL_CTRL_OFFSET / 4];
    switch (offset) {
        case FU3_CRM_PLL_OFFSET: {
            // Wrapper không có PLL: lock chỉ từ lken hoặc bộ đếm lock khi bật
            // Bộ đếm lock xong ngay, đổi khi có PLL thật
            const bool lock = (ctrl & FU3_CRM_PLL_LKEN) ||
                              ((ctrl & FU3_CRM_PLL_LOCK_TYPE) && !(ctrl & FU3_CRM_PLL_POWERDOWN));
            return regs_[offset / 4] | (lock ? FU3_CRM_PLL_LOCK : 0);
        }
        case FU3_CRM_SW_RESET_REQ_OFFSET:
            return (sw_global_rst_req.read() ? FU3_CRM_REQ_GLOBAL : 0) |
                   (sw_reset_req.read() ? FU3_CRM_REQ_SYSTEM : 0) |
                   (wdt_reset_req.read() ? FU3_CRM_REQ_WATCHDOG : 0) |
                   (cpu_reset_req.read() ? FU3_CRM_REQ_SYSRESETREQ : 0);
        default:
            return regs_[offset / 4];
    }
}

std::uint64_t Crm::bus_clock_hz() const {
    // Nnhánh chia lấy từ OSC, chỉ có tác dụng khi cả 2 bộ chọn chọn nhánh
    const std::uint64_t osc = FU3_CRM_OSC_HZ;
    const std::uint32_t div = regs_[FU3_CRM_CLK_OFFSET / 4] & 0x3FF;
    const bool branch = (regs_[FU3_CRM_CLK_1_OFFSET / 4] & 1) && (regs_[FU3_CRM_CLK_0_OFFSET / 4] & 1);
    return branch && div ? osc / (2u * div) : osc;
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
        // Offset không map: bỏ qua
        // Đang reset: ghi không có tác dụng (trừ backdoor)
        const RegDef* def = find(offset);
        if (def && (backdoor || !in_reset())) {
            std::uint32_t value = 0;
            for (unsigned i = 0; i < 4; ++i) value |= std::uint32_t(data[i]) << (8 * i);
            reg(offset) = (reg(offset) & ~def->wmask) | (value & def->wmask);
            changed_.notify(sc_core::SC_ZERO_TIME);
        }
    } else {
        tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
}

}  // namespace fu3_crm
