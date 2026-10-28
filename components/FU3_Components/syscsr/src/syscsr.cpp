#include "fu3_syscsr/syscsr.h"

#include <cstring>

namespace fu3_syscsr {
namespace {

struct RegDef {
    std::uint32_t offset;
    std::uint32_t reset;
    std::uint32_t wmask;   // bit rw 
    // còn lại ro (đọc theo giá trị lưu, ghi bỏ qua)
};

constexpr std::uint32_t kPad24 = 0x00FF'FFFF;
constexpr std::uint32_t kPad16 = 0x0000'FFFF;

// Table Register Definition
constexpr RegDef kRegs[] = {
    {kVersion, 0x0, 0x0},         // major/minor từ input RO, FU3 top buộc 0
    {0x0010, 0x0, 0xFFFF'FFFF}, 
    {0x0014, 0x0, 0xFFFF'FFFF},   // left/bottom_io_cfg
    {0x0018, 0x0, 0xFFFF'FFFF}, 
    {0x001C, 0x0, 0xFFFF'FFFF},   // right/top_io_cfg

    // left_pad_ctrl_0..4 (IE, PU, PD, SR, CS)
    {0x0020, 0x00F8'6C3F, kPad24}, 
    {0x0024, 0x00FF'FFFF, kPad24}, 
    {0x0028, 0x0, kPad24},
    {0x002C, 0x0, kPad24}, 
    {0x0030, 0x0, kPad24},

    // bottom_pad_ctrl_0..4
    {0x0034, 0x00C5'007F, kPad24},
    {0x0038, 0x00FF'FFFF, kPad24}, 
    {0x003C, 0x0, kPad24},
    {0x0040, 0x0, kPad24}, 
    {0x0044, 0x0, kPad24},

    // right_pad_ctrl_0..4
    {0x0048, 0x00F8'0580, kPad24}, 
    {0x004C, 0x00FF'FFFF, kPad24}, 
    {0x0050, 0x0, kPad24},
    {0x0054, 0x0, kPad24},
    {0x0058, 0x0, kPad24},
    
    // top_pad_ctrl_0..4
    {0x005C, 0x00E1'6E0F, kPad24}, 
    {0x0060, 0x00FF'FFFF, kPad24}, 
    {0x0064, 0x0, kPad24},
    {0x0068, 0x0, kPad24}, 
    {0x006C, 0x0, kPad24},

    {kSyscsrStatus, 0x0, 0x0},                  // đọc live từ port
    {kSysCtrl, 0x0, kSwResetReq | kSwGlobalRstReq | kJtagmuxSel | kJtagmuxBlock},
    {kCpuCtrl, 0x0, 0x0}, 
    {kCpuSystick, 0x0, 0x0},                    // Reserved trong FU3
    {kOscPadCtrl, 0x13, 0x3F},
    {kRomCtrl, 0x0, 0x1},                       // 0x0084

    // left/bottom/right/top_pad_ctrl_5..7 (DR0, OS, OD)
    {0x008C, 0x00F8'041F, kPad24}, 
    {0x0090, 0x0, kPad24}, 
    {0x0094, 0x0, kPad24},
    {0x0098, 0x00C7'007F, kPad24}, 
    {0x009C, 0x0, kPad24}, 
    {0x00A0, 0x0, kPad24},
    {0x00A4, 0x00F8'0180, kPad24}, 
    {0x00A8, 0x0, kPad24}, 
    {0x00AC, 0x0, kPad24},
    {0x00B0, 0x00E1'E007, kPad24}, 
    {0x00B4, 0x0, kPad24}, 
    {0x00B8, 0x0, kPad24},

    // left/bottom/right/top_io_cfg_ext (PAD 16..23)
    {0x00BC, 0x0, kPad16}, 
    {0x00C0, 0x0, kPad16}, 
    {0x00C4, 0x0, kPad16}, 
    {0x00C8, 0x0, kPad16},

    // left/bottom/right/top_pad_ctrl_8 (DR1)
    {0x00CC, 0x0007'FBE0, kPad24}, 
    {0x00D0, 0x0038'FF80, kPad24},
    {0x00D4, 0x0007'FE7F, kPad24}, 
    {0x00D8, 0x001E'1FF8, kPad24},
};

const RegDef* find(std::uint32_t offset) {
    for (const auto& r : kRegs) {
        if (r.offset == offset) return &r;
    }
    return nullptr;
}

constexpr std::uint32_t kApertureSize = 0x1'0000;   // Address Width = 16

}  // namespace

Syscsr::Syscsr(sc_core::sc_module_name name) : sc_core::sc_module(name) {
    socket.register_b_transport(this, &Syscsr::b_transport);
    socket.register_transport_dbg(this, &Syscsr::transport_dbg);
    reset_registers();
    SC_METHOD(on_reset);
    sensitive << reset_n << sys_ctrl_changed_;
}

void Syscsr::reset_registers() {
    for (const auto& r : kRegs) 
        regs_[r.offset / 4] = r.reset;
}

void Syscsr::on_reset() {
    if (!reset_n.read())
         reset_registers();
    sw_reset_req.write(regs_[kSysCtrl / 4] & kSwResetReq);
    sw_global_rst_req.write(regs_[kSysCtrl / 4] & kSwGlobalRstReq);
}

std::uint32_t Syscsr::read_register(std::uint32_t offset) const {
    if (offset == kSyscsrStatus) {
        // FU3_SYSCSR_v1: live pin values, no capture in SYSCSR (a captured
        // copy for the Boot ROM is a platform extension, FU3_VP_ABI_1).
        return (std::uint32_t(test_mode_sel.read()) << 4) | (std::uint32_t(test_mode.read()) << 3) |
               (std::uint32_t(mode_1.read()) << 2) | (std::uint32_t(mode_0.read()) << 1) | std::uint32_t(boot.read());
    }
    return (offset % 4 == 0 && find(offset)) ? regs_[offset / 4] : 0;
}

void Syscsr::b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time&) {
    access(tx, false);
}

unsigned Syscsr::transport_dbg(tlm::tlm_generic_payload& tx) {
    access(tx, true);
    return tx.is_response_ok() ? tx.get_data_length() : 0;
}

void Syscsr::access(tlm::tlm_generic_payload& tx, bool backdoor) {
    const std::uint64_t addr = tx.get_address();
    const unsigned len = tx.get_data_length();
    // APB 32 bit
    // 1 beat 1/2/4 byte căn hàng, không vượt qua word (bus FU3 đã tách burst).
    if (len != 1 && len != 2 && len != 4) {
        tx.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if (addr % len != 0 || addr >= kApertureSize) {
        tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    const std::uint32_t word_offset = static_cast<std::uint32_t>(addr) & ~3u;
    const unsigned lane = static_cast<unsigned>(addr & 3);
    const RegDef* def = find(word_offset);
    unsigned char* data = tx.get_data_ptr();
    const unsigned char* be = tx.get_byte_enable_ptr();
    const unsigned be_len = tx.get_byte_enable_length();

    if (tx.is_read()) {
        const std::uint32_t value = read_register(word_offset);
        for (unsigned i = 0; i < len; ++i) data[i] = (value >> (8 * (lane + i))) & 0xFF;
    } else if (tx.is_write()) {
        // Offset không map (như 0x0084) đọc 0 / write bỏ qua, chưa rõ RTL có trả PSLVERR không?
        // => Đổi sang TLM_ADDRESS_ERROR_RESPONSE khi RTL chốt
        // Đang giữ reset thì ghi không có tác dụng (trừ backdoor)
        if (def && (backdoor || reset_n.read())) {
            std::uint32_t lanes = 0, value = 0;
            for (unsigned i = 0; i < len; ++i) {
                if (be && be_len && be[i % be_len] == TLM_BYTE_DISABLED) continue;   // PSTRB = 0
                lanes |= 0xFFu << (8 * (lane + i));
                value |= std::uint32_t(data[i]) << (8 * (lane + i));
            }

            const std::uint32_t mask = lanes & def->wmask;
            std::uint32_t& reg = regs_[word_offset / 4];
            reg = (reg & ~mask) | (value & mask);

            // sc_out chỉ được 1 process lái => báo on_reset() cập nhật output.
            if (word_offset == kSysCtrl /* && sc_core::sc_is_running() */) {
                /* sw_reset_req.write(reg & kSwResetReq);
                sw_global_rst_req.write(reg & kSwGlobalRstReq); */
                sys_ctrl_changed_.notify(sc_core::SC_ZERO_TIME);
            }
        }
    } else {
        tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
}

}  // namespace fu3_syscsr
