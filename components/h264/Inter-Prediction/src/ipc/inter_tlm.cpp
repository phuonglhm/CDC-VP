#include <h264/inter/inter_tlm.h>
#include <algorithm>

namespace h264::inter {
InterTlm::InterTlm(sc_core::sc_module_name name, Options options) : sc_module(name), options_(options) {
    target_socket.register_b_transport(this, &InterTlm::transport);
    SC_METHOD(signals_changed);
    sensitive << reset_n << frame_enable;
}
void InterTlm::invalidate() {
    ++generation_; cache_.invalidate(); decision_.reset(); layout_.clear(); held_.reset();
    committed_ = done_ = false; cursor_ = 0; progress_ = {};
    changed_.notify(sc_core::SC_ZERO_TIME);
}
void InterTlm::signals_changed() {
    const bool enabled = frame_enable.read();
    if (!reset_n.read() || (!enabled && previous_enable_)) {
        invalidate(); active_ = false;
    } else if (enabled && !previous_enable_) {
        invalidate(); active_ = true; last_error_.clear();
    }
    // A reset while enable is high still requires an enable-low rearm.
    previous_enable_ = enabled;
}
void InterTlm::retag(List list, Tag tag, unsigned w, unsigned h) {
    if (!active_ || !reset_n.read() || !frame_enable.read()) throw std::logic_error("frame inactive");
    cache_.retag(list, tag, w, h);
    if (decision_ && decision_->mode.winner.candidate.reference.list == list) {
        decision_.reset(); held_.reset(); layout_.clear(); committed_ = done_ = false;
    }
    changed_.notify(sc_core::SC_ZERO_TIME);
}
void InterTlm::refill(Reference ref, Plane plane, unsigned x, unsigned y, unsigned w,
                      unsigned h, const std::vector<uint8_t>& pixels) {
    if (!active_ || !reset_n.read() || !frame_enable.read()) throw std::logic_error("frame inactive");
    cache_.fill(ref, plane, x, y, w, h, pixels);
    changed_.notify(sc_core::SC_ZERO_TIME);
}
void InterTlm::check(uint64_t epoch, tlm::tlm_generic_payload& tx) const {
    if (!active_ || !reset_n.read() || !frame_enable.read() || generation_ != epoch || !h264::epoch_valid(tx))
        throw Cancelled{};
}
void InterTlm::pause(sc_core::sc_time latency, tlm::tlm_generic_payload& tx, const Guard& guard) {
    guard();
    const auto end = sc_core::sc_time_stamp() + latency;
    const auto* shared = tx.get_extension<h264::EpochExtension>();
    while (sc_core::sc_time_stamp() < end) {
        if (shared && shared->domain) sc_core::wait(end - sc_core::sc_time_stamp(), changed_ | shared->domain->changed);
        else sc_core::wait(end - sc_core::sc_time_stamp(), changed_);
        guard(); // Refill/release notifications preserve the absolute deadline.
    }
}
void InterTlm::wait_support(const std::function<void()>& probe, tlm::tlm_generic_payload& tx, const Guard& guard) {
    const auto* shared = tx.get_extension<h264::EpochExtension>();
    for (;;) {
        guard();
        try { probe(); return; } catch (const NotResident&) {}
        if (shared && shared->domain) sc_core::wait(changed_ | shared->domain->changed);
        else sc_core::wait(changed_);
    }
}
void InterTlm::check_token(Token token) const {
    if (!decision_ || !(token == token_)) throw std::logic_error("stale or missing mode token");
    validate_mode(cache_, decision_->mode);
}
void InterTlm::transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
    tx.set_dmi_allowed(false); tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    auto* ext = tx.get_extension<Extension>();
    if (!ext) { tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE); return; }
    ext->decision.reset(); ext->sample.reset(); ext->syntax.reset(); ext->done = false;
    if (tx.get_address() > uint64_t(Operation::Accept)) { tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE); return; }
    const auto operation = static_cast<Operation>(tx.get_address());
    const bool read = operation == Operation::Peek;
    if ((read && !tx.is_read()) || (!read && !tx.is_write())) { tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE); return; }
    if (tx.get_byte_enable_ptr()) { tx.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE); return; }
    const unsigned length = operation == Operation::Evaluate ? 256 : operation == Operation::Commit ? 0 : 1;
    if (tx.get_data_length() != length || (length && (!tx.get_data_ptr() || tx.get_streaming_width() < length))) {
        tx.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE); return;
    }
    if (in_transport_) { tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE); return; }
    in_transport_ = true;
    const auto epoch = generation_;
    bool owns = false;
    const auto entry_progress = progress_;
    Guard guard = [&] { check(epoch, tx); };
    try {
        guard();
        Request request;
        std::array<uint8_t, 256> current{};
        std::vector<uint64_t> incarnations;
        if (operation == Operation::Evaluate) {
            if (decision_) throw std::logic_error("finish committed mode before next search");
            request = ext->request; validate_request(cache_, request);
            std::copy_n(tx.get_data_ptr(), 256, current.begin()); // Own current-MB snapshot before waits.
            for (const auto& c : request.integer_candidates) incarnations.push_back(cache_.incarnation(c.reference));
            guard = [&] {
                check(epoch, tx);
                for (unsigned i = 0; i < incarnations.size(); ++i)
                    if (!cache_.matches(request.integer_candidates[i].reference) ||
                        cache_.incarnation(request.integer_candidates[i].reference) != incarnations[i])
                        throw std::logic_error("search reference retagged");
            };
        } else {
            check_token(ext->token);
            if (operation == Operation::Commit && committed_) throw std::logic_error("mode already committed");
            if ((operation == Operation::Peek || operation == Operation::Accept) && !committed_)
                throw std::logic_error("commit mode before MC");
            if (operation == Operation::Accept && (!held_ || ext->accept_sequence != cursor_ || tx.get_data_ptr()[0] != held_->value))
                throw std::logic_error("accept must match the held sample");
            const Token expected = ext->token;
            guard = [&, expected] { check(epoch, tx); check_token(expected); };
        }
        owns = true;
        const auto incoming = delay; delay = sc_core::SC_ZERO_TIME;
        progress_.stage = Progress::Stage::Incoming;
        pause(incoming, tx, guard);
        if (operation == Operation::Evaluate) {
            done_ = false;
            // B startup gates BOTH lists before any FME/MC. Candidate support
            // preflight also prevents partially evaluating an incomplete list.
            for (unsigned i = 0; i < request.integer_candidates.size(); ++i) {
                const auto& c = request.integer_candidates[i];
                progress_ = {Progress::Stage::Integer, i, 0, c};
                wait_support([&] { (void)predict_luma(cache_, request.mb_x, request.mb_y, {}, c); }, tx, guard);
            }
            const auto result = evaluate(cache_, request, current, [&](const Candidate& c, bool integer, unsigned index) {
                progress_ = {integer ? Progress::Stage::Integer : Progress::Stage::Fractional, index, 0, c};
                wait_support([&] { (void)predict_luma(cache_, request.mb_x, request.mb_y, integer ? Partition{} : request.partition, c); }, tx, guard);
                pause(options_.candidate_latency, tx, guard);
                pause(options_.compare_latency, tx, guard);
            });
            guard();
            decision_ = result; token_ = {generation_, ++sequence_};
            ext->decision = result; ext->token = token_; ext->syntax = motion_syntax(result.mode);
            predictor_ready.notify(sc_core::SC_ZERO_TIME);
        } else if (operation == Operation::Commit) {
            progress_.stage = Progress::Stage::Commit;
            layout_ = sample_layout(decision_->mode); cursor_ = 0; held_.reset(); committed_ = true;
            ext->syntax = motion_syntax(decision_->mode);
        } else if (operation == Operation::Peek) {
            progress_ = {Progress::Stage::Sample, 0, cursor_, decision_->mode.winner.candidate};
            if (!held_) {
                Sample next = layout_.at(cursor_);
                wait_support([&] { next.value = compensate_sample(cache_, decision_->mode, next); }, tx, guard);
                pause(options_.sample_latency, tx, guard);
                held_ = next;
            }
            tx.get_data_ptr()[0] = held_->value; ext->sample = held_;
        } else {
            progress_.stage = Progress::Stage::Accept;
            const Sample accepted = *held_;
            pause(options_.accept_latency, tx, guard);
            ext->sample = accepted; held_.reset(); ++cursor_;
            if (accepted.last) {
                decision_.reset(); layout_.clear(); committed_ = false; done_ = true; ext->done = true;
                block_completed.notify(sc_core::SC_ZERO_TIME);
            }
        }
        tx.set_response_status(tlm::TLM_OK_RESPONSE); last_error_.clear();
        if (operation != Operation::Peek) progress_ = {};
    } catch (const Cancelled&) {
        if (owns && generation_ == epoch) { invalidate(); active_ = false; }
        last_error_ = "inter transaction cancelled by reset/frame epoch";
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
    } catch (const std::exception& e) {
        last_error_ = e.what();
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        if (owns && (operation == Operation::Evaluate || !decision_)) progress_ = {};
        else if (generation_ == epoch) progress_ = entry_progress;
    }
    in_transport_ = false;
}
}
