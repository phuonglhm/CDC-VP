#pragma once
#include <h264/types.h>
#include <functional>
namespace h264 {
struct SyntaxRequirements {
    unsigned frame_num_bits, poc_lsb_bits;
    // Adapter translates register encoding explicitly; controller never guesses minus-4.
    unsigned configured_frame_num_bits, configured_poc_lsb_bits;
};
inline bool syntax_widths_valid(const SyntaxRequirements& s) {
    return s.frame_num_bits<=32 && s.poc_lsb_bits<=32 &&
        s.configured_frame_num_bits<=32 && s.configured_poc_lsb_bits<=32 &&
        s.configured_frame_num_bits>=s.frame_num_bits && s.configured_poc_lsb_bits>=s.poc_lsb_bits;
}
struct FrameExecutorIf {
    virtual ~FrameExecutorIf() = default;
    // Pure/nonblocking preflight; must not launch producers or DMA.
    virtual SyntaxRequirements syntax_requirements(const FrameConfig&) const = 0;
    // Stop producers first, then drain ALL submitted transactions. true means quiescent.
    // On reset, return promptly without modifying the new generation. false/exception
    // means recovery failed: controller retains BUSY until reset.
    virtual bool abort_and_drain(uint64_t generation) = 0;
    void set_nal_progress(std::function<void(uint64_t,uint32_t)> sink) { progress_ = std::move(sink); }
    // No guessed B-frame coding order: a release adapter must supply that schedule.
    virtual PictureTask picture_task(const FrameConfig& c,unsigned local_index) {
        if (c.sequence.gop_n != 1) throw std::runtime_error("B-picture coding order requires release adapter");
        const unsigned index=c.completed_frames+local_index;
        return {index,index,c.sequence.frame_address_mode?0u:local_index,
                index%c.sequence.gop_m==0?PictureType::I:PictureType::P};
    }
    virtual uint32_t execute_picture(const FrameConfig& c,uint64_t generation,const PictureTask& task) {
        return execute(c,generation,task.source_slot);
    }
    virtual void reset() = 0; // Nonblocking; invalidate local state.
    virtual void begin_activation(const FrameConfig&, uint64_t generation) = 0;
    // Newly committed words. No fixed word count per macroblock.
    virtual uint32_t execute(const FrameConfig&, uint64_t generation, unsigned frame) = 0;
    // Drain formatter AND output/reference DMA; return TOTAL words including EOS/padding.
    virtual uint32_t end_activation(const FrameConfig&, uint64_t generation, uint32_t committed_words) = 0;
protected:
    // Call once when NAL DMA accepts packed words, BEFORE waiting for external response.
    void nal_words_accepted(uint64_t generation,uint32_t count) {
        if (!progress_) throw std::runtime_error("NAL progress sink not connected");
        progress_(generation,count);
    }
private:
    std::function<void(uint64_t,uint32_t)> progress_;
};
}
