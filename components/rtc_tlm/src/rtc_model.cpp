#include "rtc_model.h"

namespace {

constexpr uint32_t REG_DR = 0x00;    // current counter (RO)
constexpr uint32_t REG_MR = 0x04;    // match/alarm value (R/W)
constexpr uint32_t REG_LR = 0x08;    // load value (R/W)
constexpr uint32_t REG_CR = 0x0C;    // control (R/W)
constexpr uint32_t REG_IMSC = 0x10;  // interrupt mask set/clear (R/W)
constexpr uint32_t REG_RIS = 0x14;   // raw interrupt status (RO)
constexpr uint32_t REG_MIS = 0x18;   // masked interrupt status (RO)
constexpr uint32_t REG_ICR = 0x1C;   // interrupt clear (W1C)

constexpr uint32_t CR_EN = 1u << 0;    // counter enable
constexpr uint32_t INT_ALARM = 1u << 0; // bit0 of IMSC/RIS/MIS/ICR

} // namespace

RTC_Model::RTC_Model()
{
    reset();
}

void RTC_Model::reset()
{
    reg_data = 0;
    reg_match = 0;
    reg_load = 0;
    reg_control = 0;
    reg_imsc = 0;
    reg_ris = 0;
}

bool RTC_Model::tick()
{
    if ((reg_control & CR_EN) == 0u) {
        return false;
    }

    reg_data += 1u; // 32-bit free-running wrap is intentional

    if (reg_data == reg_match) {
        reg_ris |= INT_ALARM;
        return true;
    }
    return false;
}

uint32_t RTC_Model::readReg(uint32_t offset)
{
    switch (offset) {
        case REG_DR:
            return reg_data;
        case REG_MR:
            return reg_match;
        case REG_LR:
            return reg_load;
        case REG_CR:
            return reg_control;
        case REG_IMSC:
            return reg_imsc;
        case REG_RIS:
            return reg_ris & INT_ALARM;
        case REG_MIS:
            return reg_ris & reg_imsc & INT_ALARM;
        case REG_ICR:
            return 0; // write-only
        default:
            return 0;
    }
}

void RTC_Model::writeReg(uint32_t offset, uint32_t data)
{
    switch (offset) {
        case REG_DR:
            break; // counter is read-only; use LR to set it
        case REG_MR:
            reg_match = data;
            break;
        case REG_LR:
            reg_load = data;
            reg_data = data; // load immediately seeds the counter
            break;
        case REG_CR:
            // PL031 RTCEN is write-once: software can enable the RTC but cannot
            // disable it again. Only a reset clears it.
            if ((data & CR_EN) != 0u) {
                reg_control |= CR_EN;
            }
            break;
        case REG_IMSC:
            reg_imsc = data & INT_ALARM;
            break;
        case REG_RIS:
        case REG_MIS:
            break; // read-only
        case REG_ICR:
            if ((data & INT_ALARM) != 0u) {
                reg_ris &= ~INT_ALARM; // W1C
            }
            break;
        default:
            break;
    }
}

bool RTC_Model::hasInterrupt() const
{
    return ((reg_ris & reg_imsc & INT_ALARM) != 0u);
}

uint32_t RTC_Model::debugReadReg(uint32_t offset) const
{
    switch (offset) {
        case REG_DR:
            return reg_data;
        case REG_MR:
            return reg_match;
        case REG_LR:
            return reg_load;
        case REG_CR:
            return reg_control;
        case REG_IMSC:
            return reg_imsc;
        case REG_RIS:
            return reg_ris & INT_ALARM;
        case REG_MIS:
            return reg_ris & reg_imsc & INT_ALARM;
        case REG_ICR:
            return 0;
        default:
            return 0;
    }
}

void RTC_Model::debugWriteReg(uint32_t offset, uint32_t data)
{
    switch (offset) {
        case REG_DR:
            reg_data = data;
            break;
        case REG_MR:
            reg_match = data;
            break;
        case REG_LR:
            reg_load = data;
            break;
        case REG_CR:
            reg_control = data & CR_EN;
            break;
        case REG_IMSC:
            reg_imsc = data & INT_ALARM;
            break;
        case REG_RIS:
            reg_ris = data & INT_ALARM;
            break;
        case REG_MIS:
        case REG_ICR:
            break;
        default:
            break;
    }
}
