#include "vp_abi_regs.h"

namespace cdc::platforms::fu3_soc {

vp_abi_regs::vp_abi_regs(sc_core::sc_module_name name, fu3_plic::Intmap& intmap)
    : sc_core::sc_module(name), intmap_(intmap)
{
    socket.register_b_transport(this, &vp_abi_regs::b_transport);
    socket.register_transport_dbg(this, &vp_abi_regs::transport_dbg);
    SC_METHOD(on_reset);
    sensitive << reset_n;
    SC_METHOD(drive);
    sensitive << changed_;
}

void vp_abi_regs::on_reset()
{
    if (!reset_n.read()) {
        intmap_.enable_all(false);
        swirq_ = 0;
        valid_ = false;
    } else if (!valid_) {
        // Capture at the release of the SYSCSR reset (after its release
        // synchronization, CRM 3.1.2); later pin changes are not captured.
        captured_ = (boot.read() ? FU3_VP_BOOT_BOOT : 0) | (mode_0.read() ? FU3_VP_BOOT_MODE_0 : 0) |
                    (mode_1.read() ? FU3_VP_BOOT_MODE_1 : 0) | (test_mode.read() ? FU3_VP_BOOT_TEST_MODE : 0) |
                    (test_select.read() ? FU3_VP_BOOT_TEST_MODE_SEL : 0);
        valid_ = true;
    }
    changed_.notify(sc_core::SC_ZERO_TIME);
}

void vp_abi_regs::drive()
{
    cva6_software.write(swirq_ & FU3_VP_SWIRQ_CVA6);
    ibex_software.write(swirq_ & FU3_VP_SWIRQ_IBEX);
}

std::uint32_t vp_abi_regs::boot_status() const
{
    return (valid_ ? captured_ | FU3_VP_BOOT_STRAPS_VALID : 0) | (debug_en.read() ? FU3_VP_BOOT_DEBUG_EN : 0);
}

std::uint32_t vp_abi_regs::read(std::uint32_t offset) const
{
    switch (offset) {
    case FU3_VP_ABI_VERSION_OFFSET:
        return FU3_VP_ABI_VERSION_VALUE;
    case FU3_VP_BOOT_STATUS_OFFSET:
        return boot_status();
    case FU3_VP_RESET_CAUSE_OFFSET:
        return cause_;
    case FU3_VP_INTMAP_EN0_OFFSET:
    case FU3_VP_INTMAP_EN1_OFFSET: {
        const unsigned first = offset == FU3_VP_INTMAP_EN0_OFFSET ? 0 : 32;
        std::uint32_t v = 0;
        for (unsigned b = 0; b < 32 && first + b < FU3_VP_INTMAP_SOURCES; ++b)
            v |= std::uint32_t(intmap_.enabled(first + b)) << b;
        return v;
    }
    case FU3_VP_SWIRQ_PENDING_OFFSET:
        return swirq_;
    default:
        return 0;   // reserved, SET/CLEAR read 0
    }
}

void vp_abi_regs::write(std::uint32_t offset, std::uint32_t value, std::uint32_t lanes)
{
    const std::uint32_t v = value & lanes;
    switch (offset) {
    case FU3_VP_RESET_CAUSE_OFFSET:
        cause_ &= ~(v & FU3_VP_RESET_MASK);
        break;
    case FU3_VP_INTMAP_EN0_OFFSET:
    case FU3_VP_INTMAP_EN1_OFFSET: {
        const unsigned first = offset == FU3_VP_INTMAP_EN0_OFFSET ? 0 : 32;
        const std::uint32_t merged = (read(offset) & ~lanes) | v;
        for (unsigned b = 0; b < 32 && first + b < FU3_VP_INTMAP_SOURCES; ++b)
            if (lanes & (1u << b))
                intmap_.set_enable(first + b, (merged >> b) & 1);
        break;
    }
    case FU3_VP_SWIRQ_SET_OFFSET:
        swirq_ |= v & (FU3_VP_SWIRQ_CVA6 | FU3_VP_SWIRQ_IBEX);
        changed_.notify(sc_core::SC_ZERO_TIME);
        break;
    case FU3_VP_SWIRQ_CLEAR_OFFSET:
        swirq_ &= ~(v & (FU3_VP_SWIRQ_CVA6 | FU3_VP_SWIRQ_IBEX));
        changed_.notify(sc_core::SC_ZERO_TIME);
        break;
    default:
        break;   // RO and reserved: write ignored
    }
}

bool vp_abi_regs::access(tlm::tlm_generic_payload& tx, bool backdoor)
{
    const auto address = tx.get_address();
    const unsigned length = tx.get_data_length();
    const unsigned lane = unsigned(address % 4);
    if (length == 0 || length == 3 || length > 4 || lane % length) {
        tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return false;
    }
    std::uint32_t lanes = 0;
    const unsigned char* be = tx.get_byte_enable_ptr();
    const unsigned be_len = tx.get_byte_enable_length();
    for (unsigned i = 0; i < length; ++i)
        if (!be || !be_len || be[i % be_len] != TLM_BYTE_DISABLED)
            lanes |= 0xffu << (8 * (lane + i));
    const auto offset = static_cast<std::uint32_t>(address - lane);
    unsigned char* data = tx.get_data_ptr();
    if (tx.is_read()) {
        const std::uint32_t v = read(offset);   // reads have no side effect
        for (unsigned i = 0; i < length; ++i)
            data[i] = (v >> (8 * (lane + i))) & 0xff;
    } else if (tx.is_write()) {
        if (backdoor)
            return false;   // the debug path does not trigger W1S/W1C side effects
        std::uint32_t v = 0;
        for (unsigned i = 0; i < length; ++i)
            v |= std::uint32_t(data[i]) << (8 * (lane + i));
        if (lanes && reset_n.read())
            write(offset, v, lanes);
    } else {
        tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return false;
    }
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
    return true;
}

void vp_abi_regs::b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay)
{
    const auto offset = tx.get_address() & 0xffffu;
    if (offset >= FU3_VP_ABI_VERSION_OFFSET && offset < FU3_VP_ABI_END_OFFSET)
        access(tx, false);
    else
        out->b_transport(tx, delay);
}

unsigned vp_abi_regs::transport_dbg(tlm::tlm_generic_payload& tx)
{
    const auto offset = tx.get_address() & 0xffffu;
    if (offset >= FU3_VP_ABI_VERSION_OFFSET && offset < FU3_VP_ABI_END_OFFSET)
        return access(tx, true) ? tx.get_data_length() : 0;
    return out->transport_dbg(tx);
}

} // namespace cdc::platforms::fu3_soc
