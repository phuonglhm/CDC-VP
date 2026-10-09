#pragma once
#include <h264/control/register_map.h>
#include <h264/sync/reset_domain.h>
#include <tlm_utils/simple_target_socket.h>

namespace h264 {
class ControlRegs : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<ControlRegs> socket{"socket"};
    sc_core::sc_out<bool> irq{"irq"};
    sc_core::sc_event start_event;
    SC_HAS_PROCESS(ControlRegs);
    ControlRegs(sc_core::sc_module_name name, ResetDomain& reset, SequenceParameters params);
    void reset();
    bool take_start();
    FrameConfig config() const;
    void begin();
    void frame_complete();
    void finish(bool success, uint32_t words);
    void nal_words_accepted(uint64_t generation, uint32_t words);
    uint32_t stream_words() const { return words_; }
    unsigned completed_frames() const { return completed_frames_; }
    bool enabled() const { return (values_[0] & reg::ENABLE) != 0; }
    bool busy() const { return (status_ & reg::BUSY) != 0; }
    bool irq_pending() const { return irq_level_; }
private:
    ResetDomain& reset_;
    SequenceParameters params_;
    std::array<uint32_t, 11> values_{};
    uint32_t status_ = 0, words_ = 0;
    unsigned completed_frames_ = 0;
    bool pending_ = false, irq_level_ = false;
    sc_core::sc_event irq_changed_;
    void drive_irq();
    void set_irq(bool level);
    void transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay);
};
}
