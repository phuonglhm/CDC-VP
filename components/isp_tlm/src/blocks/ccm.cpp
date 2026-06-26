#include "adc_model.h"

#include <cstdlib>

namespace {

constexpr uint32_t REG_CONTROL = 0x00;
constexpr uint32_t REG_STATUS = 0x04;
constexpr uint32_t REG_DATA = 0x08;
constexpr uint32_t REG_INTR_ENABLE = 0x0C;

constexpr uint32_t CTRL_START = 1u << 0;
constexpr uint32_t CTRL_ADC_EN = 1u << 1;
constexpr uint32_t CTRL_MASK = CTRL_START | CTRL_ADC_EN;

constexpr uint32_t STATUS_EOC = 1u << 0;
constexpr uint32_t INTR_EOC = 1u << 0;

} // namespace

ADC_Model::ADC_Model()
{
    reset();
}

void ADC_Model::reset()
{
    reg_control = 0;
    reg_status = 0;
    reg_data = 0;
    reg_intr_enable = 0;
}

uint32_t ADC_Model::readReg(uint32_t offset)
{
    switch (offset) {
        case REG_CONTROL:
            return reg_control;
        case REG_STATUS:
            return reg_status;
        case REG_DATA: {
            const uint32_t data = reg_data;
            reg_status &= ~STATUS_EOC; // clear EOC when DATA is read
            return data;
        }
        case REG_INTR_ENABLE:
            return reg_intr_enable;
        default:
            return 0;
    }
}

void ADC_Model::writeReg(uint32_t offset, uint32_t data)
{
    switch (offset) {
        case REG_CONTROL:
            reg_control = data & CTRL_MASK;
            if ((reg_control & CTRL_ADC_EN) != 0u && (reg_control & CTRL_START) != 0u) {
                reg_data = static_cast<uint32_t>(std::rand()) & 0x0FFFu;
                reg_status |= STATUS_EOC;
                reg_control &= ~CTRL_START; // START is self-clearing
            }
            break;
        case REG_STATUS:
            if ((data & STATUS_EOC) != 0u) {
                reg_status &= ~STATUS_EOC; // W1C
            }
            break;
        case REG_INTR_ENABLE:
            reg_intr_enable = data & INTR_EOC;
            break;
        default:
            break;
    }
}

bool ADC_Model::hasInterrupt() const
{
    return ((reg_status & STATUS_EOC) != 0u) && ((reg_intr_enable & INTR_EOC) != 0u);
}

uint32_t ADC_Model::debugReadReg(uint32_t offset) const
{
    switch (offset) {
        case REG_CONTROL:
            return reg_control;
        case REG_STATUS:
            return reg_status;
        case REG_DATA:
            return reg_data;
        case REG_INTR_ENABLE:
            return reg_intr_enable;
        default:
            return 0;
    }
}

void ADC_Model::debugWriteReg(uint32_t offset, uint32_t data)
{
    switch (offset) {
        case REG_CONTROL:
            reg_control = data & CTRL_MASK;
            break;
        case REG_STATUS:
            reg_status = data & STATUS_EOC;
            break;
        case REG_DATA:
            reg_data = data & 0x0FFFu;
            break;
        case REG_INTR_ENABLE:
            reg_intr_enable = data & INTR_EOC;
            break;
        default:
            break;
    }
}
