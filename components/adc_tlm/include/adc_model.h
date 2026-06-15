#ifndef ADC_MODEL_H
#define ADC_MODEL_H

#include <stdint.h>
#include <stdbool.h>

class ADC_Model {
private:
    uint32_t reg_control;      // 0x00
    uint32_t reg_status;       // 0x04
    uint32_t reg_data;         // 0x08
    uint32_t reg_intr_enable;  // 0x0C

public:
    ADC_Model();
    void reset();
    uint32_t readReg(uint32_t offset);
    void writeReg(uint32_t offset, uint32_t data);
    bool hasInterrupt() const;

    uint32_t debugReadReg(uint32_t offset) const;
    void debugWriteReg(uint32_t offset, uint32_t data);
};

#endif
