#include <h264/control/encoder_controller.h>

namespace h264 {
EncoderController::EncoderController(sc_core::sc_module_name name, ControlRegs& regs,
                                     ResetDomain& reset, FrameExecutorIf& executor)
    : sc_module(name), regs_(regs), reset_(reset), executor_(executor) {
    executor_.set_nal_progress([this](uint64_t generation,uint32_t words) { regs_.nal_words_accepted(generation,words); });
    SC_THREAD(run);
}
bool EncoderController::validate(const FrameConfig& c) const {
    if (!c.width || !c.height || c.width%16 || c.height%16 || c.width>1920 || c.height>1088 ||
        !c.cmb || !c.refm || !c.nal || c.cmb%4 || c.refm%4 || c.nal%4 ||
        !c.sequence.frame_count || c.sequence.frame_count>65535 || c.sequence.qp>51 ||
        !c.sequence.nal_capacity_bytes || c.sequence.nal_capacity_bytes%4) return false;
    if (!c.sequence.gop_m || !c.sequence.gop_n || c.sequence.gop_n>c.sequence.gop_m ||
        !c.sequence.cmb_frames || c.sequence.cmb_frames>127 || !c.activation_frames ||
        c.crop_bottom*2>=c.height || int(c.sequence.qp)+c.slice_qp_delta<0 ||
        int(c.sequence.qp)+c.slice_qp_delta>51 || !(c.spara0 & 0x100)) return false;
    const uint64_t f = c.frame_bytes();
    const uint64_t out = c.sequence.nal_capacity_bytes;
    const std::array<std::pair<uint64_t,uint64_t>,3> regions = {{
        {c.cmb,uint64_t(c.cmb)+f*(c.sequence.frame_address_mode?1:c.sequence.cmb_frames)},
        {c.refm,uint64_t(c.refm)+3*f}, {c.nal,uint64_t(c.nal)+out}}};
    for (const auto& r: regions) if (r.second > (uint64_t(1)<<32)) return false;
    for (unsigned i=0;i<3;++i) for (unsigned j=i+1;j<3;++j)
        if (regions[i].first<regions[j].second && regions[j].first<regions[i].second) return false;
    return true;
}
void EncoderController::run() {
    for (;;) {
        if (reset_.active || !regs_.take_start()) {
            wait(regs_.start_event | reset_.changed); continue;
        }
        const auto generation = reset_.generation;
        const auto config = regs_.config();
        uint32_t words = 0;
        bool success = validate(config);
        bool started=false, recovered=true;
        if (success) {
            try {
                if (!syntax_widths_valid(executor_.syntax_requirements(config)))
                    throw std::runtime_error("FN/POC syntax width insufficient or invalid");
                if (config.completed_frames==0) display_seen_.clear();
                started=true; // begin_activation can partially launch work before throwing.
                executor_.begin_activation(config, generation);
                for (unsigned frame=0; frame<config.activation_frames; ++frame) {
                    const auto task=executor_.picture_task(config,frame);
                    if (task.coding_index!=config.completed_frames+frame || task.display_index>=config.sequence.frame_count ||
                        display_seen_.count(task.display_index) || task.source_slot >= (config.sequence.frame_address_mode?1:config.sequence.cmb_frames))
                        throw std::runtime_error("invalid/duplicate picture schedule");
                    const auto added = executor_.execute_picture(config, generation, task);
                    if (added > config.sequence.nal_capacity_bytes/4 - words)
                        throw std::runtime_error("NAL capacity exceeded");
                    words += added;
                    if (!reset_.valid(generation)) throw std::runtime_error("reset");
                    display_seen_.insert(task.display_index);
                    regs_.frame_complete();
                }
                const auto total = executor_.end_activation(config, generation, words);
                if (total < words || total > config.sequence.nal_capacity_bytes/4 || total!=regs_.stream_words())
                    throw std::runtime_error("invalid final NAL length");
                words = total;
            } catch (const std::runtime_error&) { success = false; }
        }
        if (!success && started && reset_.valid(generation)) {
            try { recovered=executor_.abort_and_drain(generation); }
            catch (const std::exception&) { recovered=false; }
        }
        if (!recovered && reset_.valid(generation)) {
            // No false completion or buffer release. Host timeout must request reset.
            while(reset_.valid(generation)) wait(reset_.changed);
        }
        // A reset invalidates both successful and failed old completions.
        if (reset_.valid(generation)) regs_.finish(success, regs_.stream_words());
    }
}
}
