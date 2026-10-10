#include <h264/intra/intra_tlm.h>
#include <algorithm>
#include <exception>

namespace h264::intra {
IntraTlm::IntraTlm(sc_core::sc_module_name name, Options options)
    : sc_module(name), options_(options) {
    // Validate configuration before elaboration; signals start the actual frame.
    core_.start_frame(options_.coded_width,options_.coded_height);
    core_.reset();
    target_socket.register_b_transport(this,&IntraTlm::transport);
    SC_METHOD(signals_changed);
    sensitive << reset_n << frame_enable;
}
void IntraTlm::signals_changed() {
    const bool enabled=frame_enable.read();
    if (!reset_n.read()) {
        core_.reset(); ready_=done_=replay_accepted_=false;
        generation_changed_.notify(sc_core::SC_ZERO_TIME);
    } else if (enabled && !previous_enable_) {
        // Logical ipc_rst_n low pulse: invalidate all neighbor/mode state.
        core_.start_frame(options_.coded_width,options_.coded_height);
        ready_=done_=replay_accepted_=false; last_error_.clear();
        generation_changed_.notify(sc_core::SC_ZERO_TIME);
    }
    previous_enable_=enabled;
}
void IntraTlm::check(uint64_t epoch, tlm::tlm_generic_payload& tx) const {
    if (!reset_n.read() || !core_.frame_active() || core_.generation()!=epoch || !epoch_valid(tx))
        throw Cancelled{};
}
void IntraTlm::pause(sc_core::sc_time latency, uint64_t epoch, tlm::tlm_generic_payload& tx) {
    check(epoch,tx);
    const auto deadline=sc_core::sc_time_stamp()+latency;
    const auto* shared=tx.get_extension<h264::EpochExtension>();
    while (sc_core::sc_time_stamp()<deadline) {
        const auto remaining=deadline-sc_core::sc_time_stamp();
        if (shared && shared->domain)
            sc_core::wait(remaining,generation_changed_ | shared->domain->changed);
        else sc_core::wait(remaining,generation_changed_);
        check(epoch,tx);
        // A domain notification need not invalidate this epoch (e.g. release).
        // Preserve the original deadline after any such notification.
    }
}
void IntraTlm::transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
    tx.set_dmi_allowed(false);
    tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    auto* ext=tx.get_extension<Extension>();
    if (ext) { ext->decision.reset(); ext->block_done=false; }
    if (!ext) { tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE); return; }
    if (tx.get_address()>uint64_t(Operation::ImportReconstructed)) {
        tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE); return;
    }
    const auto operation=static_cast<Operation>(tx.get_address());
    const bool read=operation==Operation::Replay;
    if ((read && !tx.is_read()) || (!read && !tx.is_write())) {
        tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE); return;
    }
    if (tx.get_byte_enable_ptr()) {
        tx.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE); return;
    }
    if (!tx.get_data_ptr() || !tx.get_data_length() || tx.get_streaming_width()<tx.get_data_length()) {
        tx.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE); return;
    }
    if (in_transport_) {
        last_error_="intra transport busy";
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE); return;
    }
    in_transport_=true;
    const uint64_t epoch=core_.generation();
    bool owns_frame=false;
    try {
        const auto incoming=delay; delay=sc_core::SC_ZERO_TIME;
        // Reject an already stale epoch or invalid request without touching the
        // current owner. A valid live transaction owns the frame before waits,
        // including incoming/reference/import waits that create no winner yet.
        check(epoch,tx);
        Block block=ext->block;
        if (operation==Operation::Replay || operation==Operation::Reconstruct) {
            block=core_.replay(ext->token).block;
        } else {
            if (core_.busy()) throw std::logic_error("reconstruction pending");
            (void)core_.resolve(block); // Validate plane/alignment/boundary.
            if (operation==Operation::Evaluate && ext->metric!=Metric::Sad && ext->metric!=Metric::Satd)
                throw std::invalid_argument("invalid cost metric");
        }
        const unsigned n=block.size();
        if (tx.get_data_length()!=n*n) {
            tx.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
            in_transport_=false; return;
        }
        owns_frame=true;
        pause(incoming,epoch,tx);
        if (operation==Operation::Evaluate) {
            if (core_.busy()) throw std::logic_error("reconstruction pending");
            // Own the source for the complete blocking call.
            const std::vector<uint8_t> source(tx.get_data_ptr(),tx.get_data_ptr()+n*n);
            pause(options_.reference_latency,epoch,tx);
            const Decision decision=core_.evaluate(block,source,ext->metric,ext->mode_penalty);
            ready_=done_=replay_accepted_=false;
            for (const auto& candidate : decision.candidates) {
                (void)candidate;
                pause(options_.candidate_latency,epoch,tx);
                pause(options_.compare_latency,epoch,tx);
            }
            ext->token=decision.token; ext->decision=decision;
            ready_=true; predictor_ready.notify(sc_core::SC_ZERO_TIME);
        } else if (operation==Operation::Replay) {
            if (!ready_) throw std::logic_error("predictor not ready");
            const Decision decision=core_.replay(ext->token);
            pause(options_.replay_latency,epoch,tx);
            std::copy(decision.predictor.begin(),decision.predictor.end(),tx.get_data_ptr());
            ext->decision=decision;
            replay_accepted_=true;
        } else if (operation==Operation::Reconstruct) {
            if (!ready_ || !replay_accepted_) throw std::logic_error("replay must be accepted before reconstruction");
            const std::vector<uint8_t> decoded(tx.get_data_ptr(),tx.get_data_ptr()+n*n);
            pause(options_.feedback_latency,epoch,tx);
            core_.reconstruct(ext->token,decoded);
            ready_=false; done_=true; replay_accepted_=false;
            ext->block_done=true; block_completed.notify(sc_core::SC_ZERO_TIME);
        } else {
            const std::vector<uint8_t> decoded(tx.get_data_ptr(),tx.get_data_ptr()+n*n);
            pause(options_.feedback_latency,epoch,tx);
            core_.import_reconstructed(block,decoded);
        }
        tx.set_response_status(tlm::TLM_OK_RESPONSE);
        last_error_.clear();
    } catch (const Cancelled&) {
        // A shared ResetDomain can cancel without toggling local reset_n.
        // Invalidate the cancelled frame even before winner creation; previous
        // reconstructed context is obsolete too. Never erase a newer frame or
        // the current owner in response to a stale/invalid request on entry.
        if (owns_frame && core_.generation()==epoch) {
            core_.reset(); ready_=done_=replay_accepted_=false;
            generation_changed_.notify(sc_core::SC_ZERO_TIME);
        }
        last_error_="intra work cancelled by reset/frame generation";
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
    } catch (const std::exception& e) {
        last_error_=e.what();
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
    }
    in_transport_=false;
}
}
