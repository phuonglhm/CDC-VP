#ifndef RTC_MODEL_H
#define RTC_MODEL_H

#include <stdbool.h>
#include <stdint.h>

// Pure-C++ register/behaviour model for a PL031-style real-time clock.
//
// The counter advances one LSB per call to tick() while counting is enabled.
// The SystemC wrapper is responsible for deciding how often tick() is called
// (i.e. the wall-clock meaning of one count). The model itself is untimed and
// has no SystemC dependency, which keeps it unit-testable in isolation.
class RTC_Model {
private:
    uint32_t reg_data;     // 0x00 DR   current counter value (free-running)
    uint32_t reg_match;    // 0x04 MR   alarm compare value
    uint32_t reg_load;     // 0x08 LR   last value written to load the counter
    uint32_t reg_control;  // 0x0C CR   bit0 = counter enable (write-once, PL031 RTCEN)
    uint32_t reg_imsc;     // 0x10 IMSC bit0 = alarm interrupt mask (1 = enabled)
    uint32_t reg_ris;      // 0x14 RIS  bit0 = raw alarm interrupt status

public:
    RTC_Model();
    void reset();

    // Advance the counter by one LSB if counting is enabled. Returns true if
    // this tick caused the alarm (DR == MR) to fire.
    bool tick();

    uint32_t readReg(uint32_t offset);
    void writeReg(uint32_t offset, uint32_t data);

    // Asserted while a masked alarm interrupt is pending (MIS bit0).
    bool hasInterrupt() const;

    // Backdoor accessors with no side effects (for transport_dbg / inspection).
    uint32_t debugReadReg(uint32_t offset) const;
    void debugWriteReg(uint32_t offset, uint32_t data);
};

#endif // RTC_MODEL_H
