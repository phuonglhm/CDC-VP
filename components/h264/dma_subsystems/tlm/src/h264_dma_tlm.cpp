#include "h264_dma_tlm.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace cdc::components {
h264_dma_tlm::h264_dma_tlm(sc_core::sc_module_name name, H264DmaOptions options)
    : sc_core::sc_module(name), target_socket("target_socket"), master_socket("master_socket"),
      reset_n("reset_n"), irq("irq"), options_(std::move(options)) {
    if (options_.memory_base >= (1ull << 32) || !options_.memory_bytes ||
        options_.memory_bytes > (1ull << 32) - options_.memory_base ||
        options_.service_latency <= sc_core::SC_ZERO_TIME ||
        options_.register_latency < sc_core::SC_ZERO_TIME || !options_.max_service_steps)
        throw std::invalid_argument("h264_dma_tlm: invalid memory/timing options");
    // Validate width/burst configuration before SystemC elaboration completes.
    h264::AxiMasterBridge validate(options_.bridge);
    reset_registers();
    target_socket.register_b_transport(this, &h264_dma_tlm::b_transport);
    target_socket.register_transport_dbg(this, &h264_dma_tlm::transport_dbg);
    SC_THREAD(worker);
    SC_METHOD(reset_changed);
    sensitive << reset_n;
    SC_METHOD(drive_irq);
    sensitive << irq_event_ << reset_n;
}
h264_dma_tlm::~h264_dma_tlm() = default;
void h264_dma_tlm::reset_registers() {
    registers_.fill(0);
    registers_[SPARA0/4] = 0x12C; // COMPAT=1, GOP_N=2, GOP_M=12 (HAS B.5)
    stm_len_ = frames_done_ = 0;
    busy_ = normal_ = error_ = pending_irq_ = abort_ = false;
    launch_.reset();
    last_error_.clear();
}
void h264_dma_tlm::reset_changed() {
    if (!reset_n.read()) {
        ++epoch_; // invalidate in-flight completion without freeing a suspended worker's core
        reset_registers();
        workloads_.clear();
        irq_event_.notify(sc_core::SC_ZERO_TIME);
    }
}
void h264_dma_tlm::drive_irq() {
    irq.write(reset_n.read() && pending_irq_ && (registers_[SCON/4] & 2));
}
void h264_dma_tlm::check_epoch(uint64_t epoch) const {
    if (!reset_n.read() || epoch != epoch_) throw Cancelled{};
}
void h264_dma_tlm::check_running(uint64_t epoch) const {
    check_epoch(epoch);
    if (abort_) throw std::runtime_error("h264_dma_tlm: controlled stop before completion");
}
void h264_dma_tlm::set_workload_provider(WorkloadProvider provider) {
    if (busy_) throw std::logic_error("h264: provider change while busy");
    provider_ = std::move(provider);
}
void h264_dma_tlm::set_filter(h264::DfDma::Filter filter) {
    if (busy_) throw std::logic_error("h264: filter change while busy");
    filter_ = std::move(filter);
}
void h264_dma_tlm::enqueue_workload(H264ActivationWorkload plan) {
    if (busy_ || (registers_[SCON/4] & 1))
        throw std::logic_error("h264: workload must be installed while disabled/idle");
    workloads_.push_back(std::move(plan));
}
uint32_t h264_dma_tlm::read_register(uint32_t offset) const {
    if (offset == STAT) return (normal_ ? NORMAL : 0) | (error_ ? ERROR : 0) |
                               (busy_ ? BUSY : 0) | (frames_done_ & 0xFFFF);
    if (offset == STM_LEN) return stm_len_;
    return registers_[offset/4];
}
void h264_dma_tlm::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    delay += options_.register_latency;
    trans.set_dmi_allowed(false);
    const auto address = trans.get_address();
    auto* bytes = trans.get_data_ptr();
    if (address % 4 || trans.get_data_length() != 4 || !bytes ||
        (trans.get_streaming_width() && trans.get_streaming_width() < 4)) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE); return;
    }
    auto* enables = trans.get_byte_enable_ptr();
    const auto enable_length = trans.get_byte_enable_length();
    if (enables && !enable_length) {
        trans.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE); return;
    }
    uint32_t mask = 0;
    for (unsigned b=0; b<4; ++b) {
        if (enables && enables[b % enable_length] != 0 &&
            enables[b % enable_length] != 0xFF) {
            trans.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE); return;
        }
        if (!enables || enables[b % enable_length]) mask |= 0xFFu << (b*8);
    }
    if (!trans.is_read() && !trans.is_write()) {
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE); return;
    }
    if (address > STM_LEN) {
        if (options_.undefined_access == H264UndefinedAccess::ReadZero && trans.is_read()) {
            for (unsigned b=0; b<4; ++b) if (mask & (0xFFu << (b*8))) bytes[b]=0;
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
        } else trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return; // explicit VP policy; HAS does not specify undefined register behavior
    }
    const auto offset = static_cast<uint32_t>(address);
    if (trans.is_read()) {
        const uint32_t value = read_register(offset);
        for (unsigned b=0; b<4; ++b)
            if (mask & (0xFFu << (b*8))) bytes[b]=static_cast<uint8_t>(value >> (b*8));
        if (mask && (offset == STAT || offset == STM_LEN)) {
            pending_irq_ = false;
            irq_event_.notify(sc_core::SC_ZERO_TIME);
        }
    } else if (offset == STAT || offset == STM_LEN) {
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE); return;
    } else if (reset_n.read() && mask) {
        uint32_t incoming = 0;
        for (unsigned b=0; b<4; ++b) incoming |= static_cast<uint32_t>(bytes[b]) << (b*8);
        const uint32_t value = (registers_[offset/4] & ~mask) | (incoming & mask);
        const uint32_t reserved_mask =
            offset == SCON ? ~0x3u : offset == FMSIZE ? ~0x07FF07FFu :
            offset == DFCON ? ~0x7FFu : offset == SPARA0 ? ~0x07FFF3FFu :
            offset == SPARA2 ? ~0x1FFFFu : 0;
        if (value & reserved_mask) {
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE); return;
        }
        if (offset != SCON && (busy_ || (registers_[SCON/4] & 1))) {
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE); return;
        }
        if (offset == SCON) {
            const bool old_enable = registers_[SCON/4] & 1;
            const bool new_enable = value & 1;
            if (busy_ && !old_enable && new_enable) {
                trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE); return;
            }
            registers_[SCON/4] = value;
            if (old_enable && !new_enable && busy_) abort_ = true;
            if (!old_enable && new_enable) {
                pending_irq_ = normal_ = error_ = abort_ = false;
                frames_done_ = stm_len_ = 0;
                last_error_.clear();
                busy_ = true; // reserve ownership before asynchronous worker wakes
                launch_ = Launch{epoch_, registers_};
                start_event_.notify(sc_core::SC_ZERO_TIME);
            }
            irq_event_.notify(sc_core::SC_ZERO_TIME);
        } else registers_[offset/4] = value;
    }
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}
unsigned h264_dma_tlm::transport_dbg(tlm::tlm_generic_payload& trans) {
    // Debug reads inspect without acknowledging IRQ. Debug writes never launch work.
    if (!trans.is_read() || !trans.get_data_ptr() || trans.get_data_length()!=4 ||
        trans.get_address()%4 || trans.get_address()>STM_LEN) {
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE); return 0;
    }
    const auto value = read_register(static_cast<uint32_t>(trans.get_address()));
    for (unsigned b=0; b<4; ++b) trans.get_data_ptr()[b]=static_cast<uint8_t>(value>>(b*8));
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
    return 4;
}
H264ActivationConfig h264_dma_tlm::configuration(const Launch& launch) const {
    const auto& r=launch.registers;
    H264ActivationConfig config;
    config.dims = {(r[FMSIZE/4]>>16)&0x7FF, r[FMSIZE/4]&0x7FF};
    config.bases = {r[CMB/4],r[REFM/4],r[NAL/4],(r[SPARA1/4]>>25)&0x7F,options_.nal_capacity};
    config.dfcon=r[DFCON/4]; config.spara0=r[SPARA0/4];
    config.spara1=r[SPARA1/4]; config.spara2=r[SPARA2/4];
    h264::validate_dims(config.dims);
    if (((config.spara0>>21)&0x3F)>51 || !(config.spara1&0xFFFF))
        throw std::invalid_argument("h264: invalid QP or frame count");
    const auto crop=(config.spara2>>6)&0x7FF;
    if (crop*2 >= config.dims.Hc)
        throw std::invalid_argument("h264: bottom crop removes the entire coded picture");
    return config;
}
void h264_dma_tlm::TlmMemory::write(uint64_t addr, const uint8_t* data, size_t n) {
    owner_.transport_memory(tlm::TLM_WRITE_COMMAND,addr,const_cast<uint8_t*>(data),n,epoch_);
}
void h264_dma_tlm::TlmMemory::read(uint64_t addr, uint8_t* data, size_t n) const {
    owner_.transport_memory(tlm::TLM_READ_COMMAND,addr,data,n,epoch_);
}
uint8_t h264_dma_tlm::TlmMemory::read_byte(uint64_t addr) const {
    uint8_t value=0; read(addr,&value,1); return value;
}
void h264_dma_tlm::transport_memory(tlm::tlm_command command, uint64_t addr, uint8_t* data,
                                    size_t n, uint64_t epoch) {
    check_running(epoch); // after stop, finish only the already-accepted memory call
    if (addr<options_.memory_base || addr-options_.memory_base>options_.memory_bytes ||
        n>options_.memory_bytes-(addr-options_.memory_base) || n>std::numeric_limits<unsigned>::max())
        throw std::out_of_range("h264: DMA outside shared memory window");
    tlm::tlm_generic_payload trans;
    trans.set_command(command); trans.set_address(addr);
    trans.set_data_ptr(data); trans.set_data_length(static_cast<unsigned>(n));
    trans.set_streaming_width(static_cast<unsigned>(n));
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    const auto issued=sc_core::sc_time_stamp();
    const auto client=core_->arb().granted();
    sc_core::sc_time delay=sc_core::SC_ZERO_TIME;
    master_socket->b_transport(trans,delay);
    if (delay>sc_core::SC_ZERO_TIME) wait(delay);
    transfers_.push_back({addr,n,command==tlm::TLM_WRITE_COMMAND,client,issued,
                          sc_core::sc_time_stamp(),trans.get_response_status(),
                          epoch!=epoch_ || !reset_n.read()});
    check_epoch(epoch);
    if (trans.get_response_status()!=tlm::TLM_OK_RESPONSE)
        throw std::out_of_range("h264: failed shared-memory TLM response");
}
void h264_dma_tlm::drain(uint64_t epoch, uint32_t& steps) {
    while (core_->busy()) {
        check_running(epoch);
        if (++steps>options_.max_service_steps) throw std::runtime_error("h264: DMA service timeout");
        core_->service_one();
        check_epoch(epoch);
        stm_len_=core_->nal().stm_len();
        wait(options_.service_latency);
        check_running(epoch);
        if (core_->error()) throw std::runtime_error("h264: DMA transfer failed");
    }
}
void h264_dma_tlm::execute(const H264ActivationConfig& config,
                          const H264ActivationWorkload& plan, uint64_t epoch) {
    if (plan.frames.empty() || plan.frames.size()>(config.spara1&0xFFFF))
        throw std::invalid_argument("h264: workload exceeds configured frame count or is empty");
    const bool coding_order=config.spara0&(1u<<9);
    if (coding_order && plan.frames.size()!=1)
        throw std::invalid_argument("h264: host-repoint mode requires one picture per activation");
    core_->cmb().configure(config.dims,config.bases.REG_CMB,
        coding_order ? h264::CmbFrameMode::CodingOrderPicture : h264::CmbFrameMode::WorkingSet);
    core_->df().latch_df_enable(config.dfcon&1);
    core_->df().set_filter(filter_);
    uint32_t steps=0;
    // A frame trace must cover the full coded picture, exactly once. Validate
    // all traces before DMA so an incomplete trace cannot publish NORMAL.
    const uint32_t columns=config.dims.Wc/16, rows=config.dims.Hc/16;
    for (const auto& frame : plan.frames) {
        if (frame.macroblocks.size()!=static_cast<size_t>(columns)*rows)
            throw std::invalid_argument("h264: incomplete coded-picture workload");
        std::vector<bool> visited(static_cast<size_t>(columns)*rows,false);
        for (const auto& mb : frame.macroblocks) {
            if (mb.x>=columns || mb.y>=rows || visited[mb.y*columns+mb.x])
                throw std::invalid_argument("h264: duplicate/out-of-range macroblock");
            visited[mb.y*columns+mb.x]=true;
        }
    }
    for (const auto& frame : plan.frames) {
        check_running(epoch);
        if (frame.macroblocks.empty())
            throw std::invalid_argument("h264: workload frame has no macroblocks");
        core_->cmb().update_enable(false);
        if (!coding_order) core_->cmb().select_working_frame(frame.source_slot);
        core_->cmb().update_enable(true);
        core_->df().on_sofm();
        core_->df().set_ref_slot(frame.reference_slot);
        core_->sw().invalidate_all();
        for (const auto& mb : frame.macroblocks) {
            check_running(epoch);
            core_->cmb().fetch_macroblock(mb.x,mb.y);
            drain(epoch,steps);
            for (const auto& ref : mb.references) {
                core_->sw().set_ref_slot(ref.list,ref.slot,ref.picture_tag);
                core_->sw().fill_window(ref.list,mb.x,mb.y);
            }
            drain(epoch,steps);
            if (frame.is_b_picture && !core_->sw().fme_mc_may_start(true))
                throw std::runtime_error("h264: B-picture requires both reference lists resident");
            if (!mb.reconstructed && ((config.dfcon&1) || !mb.replay_source_for_bypass))
                throw std::runtime_error("h264: reconstructed producer samples or explicit bypass replay required");
            core_->df().schedule_macroblock(mb.x,mb.y,
                mb.reconstructed ? *mb.reconstructed : core_->cmb().pixels(),
                mb.reconstructed ? mb.already_filtered : false);
            drain(epoch,steps);
        }
        core_->df().dma_fmdone();
        core_->df().consume_done();
        if (!core_->df().frame_complete())
            throw std::runtime_error("h264: reference frame completion missing");
        for (auto word : frame.nal_words) core_->nal().accept_word(word);
        core_->nal().flush_chunk();
        drain(epoch,steps);
        ++frames_done_; // completed pictures in this activation's supplied trace
    }
    if (plan.eos) { core_->nal().accept_eos(); core_->nal().flush_chunk(); drain(epoch,steps); }
    if (!core_->nal().final_b_accepted())
        throw std::runtime_error("h264: producer supplied no complete NAL output");
    core_->cmb().update_enable(false);
}
void h264_dma_tlm::finish(bool error, uint64_t epoch) {
    if (epoch!=epoch_ || !reset_n.read()) return;
    busy_=false; error_=error; normal_=!error;
    pending_irq_=true;
    irq_event_.notify(sc_core::SC_ZERO_TIME);
}
void h264_dma_tlm::worker() {
    for (;;) {
        if (!launch_) wait(start_event_);
        if (!launch_) continue;
        const auto launch=*launch_;
        launch_.reset();
        // Consume the submitted trace even if this launch's config is invalid.
        // Otherwise a later activation could silently execute a stale trace.
        std::optional<H264ActivationWorkload> queued;
        if (!workloads_.empty()) {
            queued=std::move(workloads_.front()); workloads_.pop_front();
        }
        try {
            check_running(launch.epoch);
            const auto config=configuration(launch);
            memory_=std::make_unique<TlmMemory>(*this,launch.epoch);
            core_=std::make_unique<h264::DmaSubsystem>(config.dims,config.bases,*memory_,
                options_.memory_base,options_.memory_bytes,options_.bridge);
            core_->nal().configure(config.bases.REG_NAL,core_->bases().nal_capacity,
                                   options_.nal_word_order);
            H264ActivationWorkload plan;
            if (queued) plan=std::move(*queued);
            else if (provider_) plan=provider_(config);
            else throw std::runtime_error("h264: no codec/workload producer installed");
            check_running(launch.epoch);
            execute(config,plan,launch.epoch);
            finish(false,launch.epoch);
        } catch (const Cancelled&) {
            // Reset already cleared architectural state. Accepted memory accesses
            // before reset are not rolled back; no stale completion can assert IRQ.
        } catch (const std::exception& e) {
            if (launch.epoch==epoch_ && reset_n.read()) {
                last_error_=e.what();
                finish(true,launch.epoch);
            }
        }
    }
}
}
