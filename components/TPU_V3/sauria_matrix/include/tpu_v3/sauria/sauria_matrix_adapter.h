// SPDX-License-Identifier: Apache-2.0
//
// `sauria_matrix_adapter` — the prefetch/compute/writeback sequencer that turns
// the extracted composition into a `sauria_matrix_if` (decision record D17).
//
// ## The three phases, and why they are three
//
// D17 chose buffered tile staging over pass-through, and this is what that
// means concretely:
//
//   PREFETCH   read A and B out of core SRAM over `neo_local_sram_if` and place
//              them in the staging store
//   COMPUTE    load the configuration, pulse start, wait for done
//   WRITEBACK  read the results out of the staging store and write C back over
//              the same port
//
// The reason it cannot be one phase is that the kept Sauria modules have no way
// to be told to wait. They talk to memory over signal-level buses with a fixed
// read latency; a late answer from an arbitrated bank would be latched as data.
// So the operands are resident before the array starts, and the array's own
// timing is never influenced by fabric contention. The record has the full
// argument.
//
// `last_timing()` reports the three separately and never a total, because only
// the middle one is cycle-correlated with the pinned source. The other two are
// this adapter's traffic through a fabric the source never had. A single number
// would be quoted as though all of it meant something.
//
// ## The transpose
//
// The frozen `job` interface says `A[M x K]` row-major. The array's activation
// address generator walks channel-major — `ACT.CHSTEP` is the tile's spatial
// extent, so activation element `(k, m)` is at flat index `k * M + m` — so A has
// to be transposed on the way in. The pinned case's captured DRAM image confirms
// the layout independently: the bytes at its A offset are `A.T` flattened.
//
// Doing it here rather than asking firmware to store A transposed is the right
// side of the boundary: the interface is firmware-visible and frozen, and the
// engine's operand order is an implementation detail of the engine. The reads
// from core SRAM stay linear and burst-sized; only the placement into the store
// is permuted, and buffered staging already requires the whole tile resident, so
// this costs no extra buffering beyond what D17 already committed to.
//
// Weights are not transposed: B is `[K][N]` row-major in memory.  The physical
// SRAMB word is nevertheless X lanes wide, so each B row is staged into one
// vector and zero-padded from N through X-1.  That distinction is invisible at
// N=X and mandatory for a partial-width tile.

#pragma once

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <systemc>

#include "tpu_v3/sauria/config_loader.h"
#include "tpu_v3/sauria/matrix_composition.h"
#include "tpu_v3/sram/native_port.h"

namespace cdc::components::tpu_v3::sauria {

/// What the adapter needs to know about the machine around it.
struct adapter_config {
    /// The core-SRAM window operands and results must lie inside.
    std::uint64_t sram_base = 0;
    std::uint64_t sram_window = 0;

    /// Bitmask over `datatype_value`. Phase 5 ships INT8 x INT8 -> INT32 only.
    std::uint32_t supported_datatypes = 1u << datatype_value::int8_int32;

    /// Clock period, used to pace the host-port write protocol and to bound the
    /// wait for completion.
    sc_core::sc_time cycle{10, sc_core::SC_NS};

    /// How many clocks the engine may take before the job is abandoned.
    ///
    /// A bound, not a tuning parameter. The kept Control FSM can deadlock — it
    /// has a `o_feed_deadlock` output for exactly that — and a sequencer that
    /// waited forever would turn a diagnosable failure into a hung simulation.
    std::uint64_t compute_watchdog_cycles = 2'000'000;
};

/// One matrix engine as everything above it sees it.
template <int X_DIM, int Y_DIM, typename T_ACT, typename T_WEI, typename T_PSUM,
          int SRAMA_CAP, int SRAMB_CAP, int SRAMC_CAP>
class sauria_matrix_adapter : public sc_core::sc_module,
                              public virtual sauria_matrix_if {
public:
    using engine_type = matrix_composition<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM,
                                           SRAMA_CAP, SRAMB_CAP, SRAMC_CAP>;
    using act_vector = typename engine_type::store_t::act_vector;
    using wei_vector = typename engine_type::store_t::wei_vector;

    sc_core::sc_in<bool> i_clk{"i_clk"};
    sc_core::sc_in<bool> i_rstn{"i_rstn"};

    /// The one path to core SRAM. There is no second port and no backing
    /// pointer, which is what makes "no accelerator bypasses arbitration"
    /// structural rather than a promise.
    sc_core::sc_port<sram::neo_local_sram_if> local_port{"local_port"};

    SC_HAS_PROCESS(sauria_matrix_adapter);

    sauria_matrix_adapter(sc_core::sc_module_name name, adapter_config config)
        : sc_core::sc_module(name)
        , config_(config)
        , engine_("engine")
    {
        static_assert(SRAMA_CAP > 0 && SRAMB_CAP > 0 && SRAMC_CAP > 0,
                      "Sauria staging capacities must be explicit and non-zero");
        if (config_.supported_datatypes != capability_bit::int8_int32) {
            throw std::invalid_argument(
                "Phase 5 Sauria must expose exactly INT8/INT32; the pinned "
                "profile implements no BF16 or FP16 path");
        }
        if (config_.compute_watchdog_cycles == 0) {
            throw std::invalid_argument(
                "Sauria compute_watchdog_cycles must be non-zero");
        }
        // The naming rule `sauria_geometry.h` describes, enforced rather than
        // observed. The source's trace writers are compiled out as well, so this
        // is a second line of defence — but it is the line that survives someone
        // rebuilding without the patch.
        if (std::string(this->name()).find(reserved_trace_instance_name)
            != std::string::npos) {
            SC_REPORT_FATAL("sauria_matrix_adapter",
                            "the instance name contains the substring the "
                            "Sauria source's trace writers gate on");
        }

        engine_.i_clk(i_clk);
        engine_.i_rstn(i_rstn);
        engine_.i_soft_reset(soft_reset_);
        engine_.i_start(start_);
        engine_.o_done(done_);
        engine_.o_deadlock(deadlock_);
        engine_.i_host_addr(host_addr_);
        engine_.i_host_wren(host_wren_);
        engine_.i_host_rden(host_rden_);
        engine_.i_host_wdata(host_wdata_);
        engine_.i_host_wmask(host_wmask_);
        engine_.i_threshold(threshold_);
        engine_.i_mvm_k(mvm_k_);
        engine_.i_total_contexts(total_contexts_);

        SC_THREAD(sequencer);
        sensitive << i_clk.pos();
    }

    // ── sauria_matrix_if ─────────────────────────────────────────────────────

    submit_status submit(const job& work) override
    {
        if (busy_) {
            return submit_status::busy;
        }
        const submit_status status =
            validate_gemm(work, static_cast<std::uint32_t>(Y_DIM),
                          static_cast<std::uint32_t>(X_DIM), config_.sram_base,
                          config_.sram_window, config_.supported_datatypes);
        if (status != submit_status::accepted) {
            // A refusal happens before any state changes, so C is untouched and
            // the previous job's account stays readable.
            return status;
        }

        const std::uint64_t activation_elements =
            static_cast<std::uint64_t>(work.m) * work.k;
        const std::uint64_t activation_vectors =
            (activation_elements + Y_DIM - 1) / Y_DIM;
        // The weight feeder consumes one physical X-lane vector for every K
        // step.  A partial-N job therefore still occupies K vectors; packing
        // K*N contiguously would make the tail of row k become the beginning
        // of row k+1 whenever N != X_DIM.
        const std::uint64_t weight_vectors = work.k;
        if (activation_vectors > static_cast<std::uint64_t>(SRAMA_CAP)
            || weight_vectors > static_cast<std::uint64_t>(SRAMB_CAP)
            || work.n > static_cast<std::uint32_t>(SRAMC_CAP)) {
            return submit_status::staging_capacity_exceeded;
        }

        pending_ = work;
        pending_generation_ = ++generation_;
        active_generation_ = pending_generation_;
        bytes_owner_generation_ = pending_generation_;
        start_pending_ = true;
        busy_ = true;
        last_error_ = error_cause::none;
        committed_ = 0;
        timing_ = timing{};
        submitted_.notify(sc_core::SC_ZERO_TIME);
        return submit_status::accepted;
    }

    void abort() override
    {
        if (!busy_) {
            return;
        }
        ++generation_;
        active_generation_ = generation_;
        start_pending_ = false;
        busy_ = false;
        last_error_ = error_cause::aborted;
        completed_.notify(sc_core::SC_ZERO_TIME);
    }

    void reset() override
    {
        ++generation_;
        active_generation_ = generation_;
        start_pending_ = false;
        busy_ = false;
        last_error_ = error_cause::none;
        ++traffic_epoch_;
        local_requests_ = 0;
        local_bytes_ = 0;
    }

    bool busy() const override { return busy_; }
    error_cause last_error() const override { return last_error_; }
    std::uint64_t committed_bytes() const override { return committed_; }
    timing last_timing() const override { return timing_; }
    std::uint64_t local_requests() const override { return local_requests_; }
    std::uint64_t local_bytes() const override { return local_bytes_; }

    engine_identity identity() const override
    {
        engine_identity id;
        id.rows = static_cast<std::uint32_t>(Y_DIM);
        id.columns = static_cast<std::uint32_t>(X_DIM);
        // `accumulate` is deliberately absent: D17 refuses accumulation and C
        // preload for the whole of Phase 5.
        id.capability = capability_bit::int8_int32;
        // Profile *and* patched-tree hash. A geometry alone does not identify
        // what produced a number, and D14 requires every report to name the
        // source revision — an empty one is a defect, not a default.
        id.source_revision =
            std::string(profile_name) + '@' + source_patched_hash;
        return id;
    }

    /// Notified when a job finishes, however it finished. Exposed so a testbench
    /// or an MMIO front end can wait rather than poll.
    const sc_core::sc_event& completion_event() const { return completed_; }

    engine_type& engine() { return engine_; }

private:
    // ── the sequencer ────────────────────────────────────────────────────────

    void sequencer()
    {
        for (;;) {
            while (!start_pending_) {
                wait(submitted_);
            }
            const job work = pending_;
            const std::uint64_t job_generation = pending_generation_;
            start_pending_ = false;
            run_job(work, job_generation);
            if (owns_active_job(job_generation)) {
                busy_ = false;
                completed_.notify(sc_core::SC_ZERO_TIME);
            }
        }
    }

    bool owns_active_job(std::uint64_t job_generation) const
    {
        return busy_ && active_generation_ == job_generation;
    }

    void run_job(const job& work, std::uint64_t job_generation)
    {
        const std::uint64_t traffic_epoch = traffic_epoch_;

        const std::uint32_t operand_bytes =
            operand_element_bytes(work.datatype);
        const std::uint32_t result_bytes = result_element_bytes(work.datatype);

        // Results from the previous job are cleared here, at the *start* of the
        // next one, rather than at the end of the last. D17 makes the committed
        // byte count the only account of a partially written C, and wiping the
        // store when a job ends would destroy the evidence before anything could
        // read it. Clearing on entry keeps that evidence and still means a
        // result read back after this job is this job's.
        engine_.store.clear_results();

        // ── PREFETCH ─────────────────────────────────────────────────────────
        const sc_core::sc_time prefetch_start = sc_core::sc_time_stamp();

        std::vector<unsigned char> a_bytes;
        std::vector<unsigned char> b_bytes;
        const std::uint64_t a_row_bytes =
            static_cast<std::uint64_t>(work.k) * operand_bytes;
        const std::uint64_t b_row_bytes =
            static_cast<std::uint64_t>(work.n) * operand_bytes;

        if (!read_region(work.a_address,
                         effective_stride(work.a_stride_bytes, work.k,
                                          operand_bytes),
                         work.m, a_row_bytes, a_bytes, job_generation,
                         traffic_epoch)
            || !read_region(work.b_address,
                            effective_stride(work.b_stride_bytes, work.n,
                                             operand_bytes),
                            work.k, b_row_bytes, b_bytes, job_generation,
                            traffic_epoch)) {
            // `read_region` has already latched which refusal it was.
            if (owns_active_job(job_generation)) {
                timing_.prefetch_ns = elapsed_ns(prefetch_start);
            }
            return;
        }

        if (!owns_active_job(job_generation)) {
            return;
        }

        stage_activations(work, a_bytes);
        stage_weights(work, b_bytes);

        const sc_core::sc_time compute_start = sc_core::sc_time_stamp();
        timing_.prefetch_ns = elapsed_ns(prefetch_start, compute_start);

        // ── COMPUTE ──────────────────────────────────────────────────────────
        //
        // The register writes are counted here rather than in a fourth phase.
        // They are part of what it costs to run a job on this engine, and
        // splitting them out would invite the reading that the array's cycle
        // count is the whole story.
        if (!soft_reset_engine(job_generation)) {
            return;
        }
        for (const auto& write :
             build_config_writes(work, static_cast<std::uint32_t>(Y_DIM),
                                 static_cast<std::uint32_t>(X_DIM))) {
            if (!host_write(write, job_generation)) {
                return;
            }
        }
        // `mvm_k` is the activation read count per context, which for the 1x1
        // kernel a GEMM maps to is K. `total_contexts` is one because a job is
        // refused unless it fits a single pass of the array. The threshold is the
        // source's INT8 sparsity constant: no non-zero INT8 product has magnitude
        // below 1, so 0.5 means "skip exact zeros only".
        mvm_k_.write(work.k);
        total_contexts_.write(1);
        threshold_.write(0.5f);

        if (!start_engine(job_generation)) {
            return;
        }

        const bool completed = wait_for_done(job_generation);
        if (owns_active_job(job_generation)) {
            timing_.compute_ns = elapsed_ns(compute_start);
        }

        if (!completed) {
            return;
        }

        // ── WRITEBACK ────────────────────────────────────────────────────────
        const sc_core::sc_time writeback_start = sc_core::sc_time_stamp();
        write_results(work, result_bytes, job_generation, traffic_epoch);
        if (owns_active_job(job_generation)) {
            timing_.writeback_ns = elapsed_ns(writeback_start);
        }
    }

    // ── the local-SRAM traffic ───────────────────────────────────────────────

    /// Read `rows` rows of `row_bytes` at `stride` into a packed buffer.
    ///
    /// The buffer is packed, so the stride disappears here and the staging code
    /// below indexes rows contiguously. Returns false and latches the cause on
    /// any refusal — an operand read that failed must not be staged, because
    /// stale store contents would compute a plausible wrong answer.
    bool read_region(std::uint64_t address, std::uint64_t stride,
                     std::uint32_t rows, std::uint64_t row_bytes,
                     std::vector<unsigned char>& out,
                     std::uint64_t job_generation,
                     std::uint64_t traffic_epoch)
    {
        out.assign(static_cast<std::size_t>(rows) * row_bytes, 0);
        for (std::uint32_t row = 0; row < rows; ++row) {
            std::uint64_t offset = 0;
            while (offset < row_bytes) {
                if (!owns_active_job(job_generation)) {
                    return false;
                }
                const std::uint32_t chunk = static_cast<std::uint32_t>(
                    std::min<std::uint64_t>(row_bytes - offset,
                                            sram::neo_max_transfer_bytes));
                sram::neo_local_request request;
                request.requester = sram::neo_requester::sa;
                request.command = sram::neo_command::read;
                request.address = address + row * stride + offset;
                request.size = chunk;
                request.data =
                    out.data() + static_cast<std::size_t>(row) * row_bytes
                    + offset;

                sram::neo_local_response response;
                sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
                if (traffic_epoch == traffic_epoch_) {
                    ++local_requests_;
                }
                local_port->b_access(request, response, delay);
                if (response.status == sram::neo_status::ok
                    && response.bytes != chunk) {
                    if (owns_active_job(job_generation)) {
                        last_error_ = error_cause::operand_read_failed;
                    }
                    return false;
                }
                if (traffic_epoch == traffic_epoch_) {
                    local_bytes_ += response.bytes;
                }
                if (delay > sc_core::SC_ZERO_TIME) {
                    // Annotated mode never blocks and charges the caller
                    // instead; charging it is what makes the fabric's
                    // arbitration visible in `prefetch_ns` rather than free.
                    wait(delay);
                }
                if (!owns_active_job(job_generation)) {
                    return false;
                }
                if (response.status != sram::neo_status::ok) {
                    last_error_ = cause_of(response.status, /*writing=*/false);
                    return false;
                }
                offset += chunk;
            }
            if (!owns_active_job(job_generation)) {
                return false;
            }
        }
        return true;
    }

    void write_results(const job& work, std::uint32_t result_bytes,
                       std::uint64_t job_generation,
                       std::uint64_t traffic_epoch)
    {
        const std::uint64_t row_bytes =
            static_cast<std::uint64_t>(work.n) * result_bytes;
        const std::uint64_t stride =
            effective_stride(work.c_stride_bytes, work.n, result_bytes);

        std::vector<unsigned char> row(static_cast<std::size_t>(row_bytes));
        for (std::uint32_t m = 0; m < work.m; ++m) {
            // The output vector index is the array column, carrying the output
            // channel; the component within the vector is the array row,
            // carrying the output position. So `C[m][n]` is component `m` of
            // vector `n`.
            for (std::uint32_t n = 0; n < work.n; ++n) {
                const auto value = static_cast<std::int32_t>(
                    engine_.store.load_result(0, n)[static_cast<int>(m)]);
                const auto raw = static_cast<std::uint32_t>(value);
                for (std::uint32_t byte = 0; byte < result_bytes; ++byte) {
                    row[n * result_bytes + byte] = static_cast<unsigned char>(
                        (raw >> (8 * byte)) & 0xFFu);
                }
            }

            std::uint64_t offset = 0;
            while (offset < row_bytes) {
                if (!owns_active_job(job_generation)) {
                    return;
                }
                const std::uint32_t chunk = static_cast<std::uint32_t>(
                    std::min<std::uint64_t>(row_bytes - offset,
                                            sram::neo_max_transfer_bytes));
                sram::neo_local_request request;
                request.requester = sram::neo_requester::sa;
                request.command = sram::neo_command::write;
                request.address = work.c_address + m * stride + offset;
                request.size = chunk;
                request.data = row.data() + offset;

                sram::neo_local_response response;
                sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
                if (traffic_epoch == traffic_epoch_) {
                    ++local_requests_;
                }
                local_port->b_access(request, response, delay);
                if (response.status == sram::neo_status::ok
                    && response.bytes != chunk) {
                    if (owns_active_job(job_generation)) {
                        last_error_ = error_cause::result_write_failed;
                    }
                    return;
                }
                if (traffic_epoch == traffic_epoch_) {
                    local_bytes_ += response.bytes;
                }
                if (bytes_owner_generation_ == job_generation) {
                    committed_ += response.bytes;
                }
                if (delay > sc_core::SC_ZERO_TIME) {
                    wait(delay);
                }
                if (!owns_active_job(job_generation)) {
                    return;
                }
                if (response.status != sram::neo_status::ok) {
                    last_error_ = cause_of(response.status, /*writing=*/true);
                    return;
                }
                // Counted as it is committed, not at the end. Writeback is not
                // atomic, so a failure partway must leave a count that says how
                // far it got (D17).
                offset += chunk;
            }
            if (!owns_active_job(job_generation)) {
                return;
            }
        }
    }

    // ── staging ──────────────────────────────────────────────────────────────

    void stage_activations(const job& work,
                           const std::vector<unsigned char>& packed)
    {
        const std::size_t elements =
            static_cast<std::size_t>(work.k) * work.m;
        for (std::size_t base = 0; base < elements; base += Y_DIM) {
            act_vector vec;
            for (int component = 0; component < Y_DIM; ++component) {
                const std::size_t e = base + component;
                if (e >= elements) {
                    break;
                }
                // Channel-major: element `e` is `(k, m)` with `k = e / M`.
                const std::size_t k = e / work.m;
                const std::size_t m = e % work.m;
                vec[component] = static_cast<T_ACT>(
                    static_cast<std::int8_t>(packed[m * work.k + k]));
            }
            engine_.store.store_activation(0, base / Y_DIM, vec);
        }
    }

    void stage_weights(const job& work,
                       const std::vector<unsigned char>& packed)
    {
        // SRAMB word k is B[k][0..X_DIM-1].  Firmware supplies only N live
        // columns, so zero-pad every row independently rather than flattening
        // K*N across word boundaries.  The latter happens to work at N=X_DIM
        // and silently corrupts every partial-width GEMM.
        for (std::uint32_t k = 0; k < work.k; ++k) {
            wei_vector vec;
            for (std::uint32_t n = 0; n < work.n; ++n) {
                vec[static_cast<int>(n)] =
                    static_cast<T_WEI>(static_cast<std::int8_t>(
                        packed[static_cast<std::size_t>(k) * work.n + n]));
            }
            engine_.store.store_weight(0, k, vec);
        }
    }

    // ── the host-port protocol ───────────────────────────────────────────────

    bool host_write(const config_write& write, std::uint64_t job_generation)
    {
        ::sauria::host_data_t data;
        ::sauria::host_mask_t mask;
        if (write.how == config_write::encoding::byte_spread) {
            for (int lane = 0; lane < 4; ++lane) {
                data[lane] =
                    static_cast<double>((write.value >> (lane * 8)) & 0xFFu);
                mask[lane] = true;
            }
        } else {
            data[0] = static_cast<double>(write.value);
            mask[0] = true;
        }
        return drive_host(write.address, data, mask, job_generation);
    }

    bool soft_reset_engine(std::uint64_t job_generation)
    {
        // Control register bit 16..23, which `ConfigRegs` reads from lane 2 and
        // auto-clears after one clock. Issued before every job so a latched
        // `done` from the previous one cannot be mistaken for this one's.
        ::sauria::host_data_t data;
        ::sauria::host_mask_t mask;
        data[2] = 1.0;
        mask[2] = true;
        return drive_host(0x00, data, mask, job_generation);
    }

    bool start_engine(std::uint64_t job_generation)
    {
        ::sauria::host_data_t data;
        ::sauria::host_mask_t mask;
        data[0] = 1.0;
        mask[0] = true;
        return drive_host(0x00, data, mask, job_generation);
    }

    bool drive_host(std::uint32_t address, const ::sauria::host_data_t& data,
                    const ::sauria::host_mask_t& mask,
                    std::uint64_t job_generation)
    {
        if (!owns_active_job(job_generation)) {
            return false;
        }
        wait();
        if (!owns_active_job(job_generation)) {
            return false;
        }
        host_addr_.write(address);
        host_wdata_.write(data);
        host_wmask_.write(mask);
        host_wren_.write(true);
        wait();
        host_wren_.write(false);
        host_wmask_.write(::sauria::host_mask_t());
        wait();
        return owns_active_job(job_generation);
    }

    /// Wait for `o_done`, bounded. Returns false if it never came.
    bool wait_for_done(std::uint64_t job_generation)
    {
        for (std::uint64_t cycle = 0; cycle < config_.compute_watchdog_cycles;
            ++cycle) {
            wait();
            if (!owns_active_job(job_generation)) {
                return false;
            }
            if (done_.read()) {
                return true;
            }
            if (deadlock_.read()) {
                last_error_ = error_cause::engine_deadlock;
                return false;
            }
            if (!i_rstn.read()) {
                last_error_ = error_cause::aborted;
                return false;
            }
        }
        last_error_ = error_cause::timeout;
        return false;
    }

    static std::uint64_t elapsed_ns(const sc_core::sc_time& from)
    {
        return elapsed_ns(from, sc_core::sc_time_stamp());
    }

    static std::uint64_t elapsed_ns(const sc_core::sc_time& from,
                                    const sc_core::sc_time& to)
    {
        return static_cast<std::uint64_t>((to - from).to_seconds() * 1e9 + 0.5);
    }

    /// Which fabric refusal this was, told apart by *phase* rather than by
    /// status alone.
    ///
    /// `operand_read_failed` and `result_write_failed` are separate causes in the
    /// frozen map because they send a reader somewhere different: the first means
    /// the job named an operand wrongly and C is untouched, the second means C is
    /// partially written and `committed_bytes()` is the account of how much.
    /// Collapsing them into one address fault would lose exactly that.
    static error_cause cause_of(sram::neo_status status, bool writing)
    {
        if (status == sram::neo_status::aborted) {
            return error_cause::aborted;
        }
        if (status == sram::neo_status::ok) {
            return error_cause::none;
        }
        return writing ? error_cause::result_write_failed
                       : error_cause::operand_read_failed;
    }

    adapter_config config_;

    sc_core::sc_signal<bool> soft_reset_{"soft_reset"};
    sc_core::sc_signal<bool> start_{"start"};
    sc_core::sc_signal<bool> done_{"done"};
    sc_core::sc_signal<bool> deadlock_{"deadlock"};
    sc_core::sc_signal<bool> host_wren_{"host_wren"};
    sc_core::sc_signal<bool> host_rden_{"host_rden"};
    sc_core::sc_signal<std::uint32_t> host_addr_{"host_addr"};
    sc_core::sc_signal<std::uint32_t> mvm_k_{"mvm_k"};
    sc_core::sc_signal<std::uint32_t> total_contexts_{"total_contexts"};
    sc_core::sc_signal<::sauria::host_data_t> host_wdata_{"host_wdata"};
    sc_core::sc_signal<::sauria::host_mask_t> host_wmask_{"host_wmask"};
    sc_core::sc_signal<float> threshold_{"threshold"};

    engine_type engine_;

    job pending_;
    bool busy_ = false;
    bool start_pending_ = false;
    error_cause last_error_ = error_cause::none;
    std::uint64_t committed_ = 0;
    std::uint64_t local_requests_ = 0;
    std::uint64_t local_bytes_ = 0;
    std::uint64_t generation_ = 0;
    std::uint64_t active_generation_ = 0;
    std::uint64_t pending_generation_ = 0;
    std::uint64_t bytes_owner_generation_ = 0;
    std::uint64_t traffic_epoch_ = 0;
    timing timing_;

    sc_core::sc_event submitted_;
    sc_core::sc_event completed_;
};

} // namespace cdc::components::tpu_v3::sauria
