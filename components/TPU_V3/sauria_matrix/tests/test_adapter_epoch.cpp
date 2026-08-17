// SPDX-License-Identifier: Apache-2.0
//
// Reset/abort ownership gate for the buffered Sauria adapter.  The old job is
// parked inside a blocking native-SRAM access, a new job claims the adapter,
// and only then is the old access released.  This is the ordering that exposes
// a lost START event or an old worker publishing into the new job's registers.

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include <systemc>

#include "tpu_v3/sauria/sauria_matrix_adapter.h"
#include "tpu_v3/sram/native_port.h"

namespace sa = cdc::components::tpu_v3::sauria;
namespace sram = cdc::components::tpu_v3::sram;

namespace {

int failures = 0;

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            std::cerr << "FAIL line " << __LINE__ << ": " #condition "\n";   \
            ++failures;                                                       \
        }                                                                      \
    } while (false)

constexpr std::uint64_t base = 0x1000'0000;
constexpr std::uint64_t capacity = 64 * 1024;

using adapter_t = sa::sauria_matrix_adapter<
    sa::columns, sa::rows, sa::activation_t, sa::weight_t, sa::accumulator_t,
    /*SRAMA_CAP=*/1024, /*SRAMB_CAP=*/1024, /*SRAMC_CAP=*/2048>;

class blocking_memory : public sc_core::sc_module,
                        public virtual sram::neo_local_sram_if {
public:
    explicit blocking_memory(sc_core::sc_module_name name)
        : sc_core::sc_module(name), bytes_(capacity, 0)
    {
    }

    void arm()
    {
        block_next_ = true;
        entered_ = false;
    }

    bool entered() const noexcept { return entered_; }
    const sc_core::sc_event& entered_event() const noexcept { return entered_event_; }
    void release() { release_event_.notify(sc_core::SC_ZERO_TIME); }

    void b_access(const sram::neo_local_request& request,
                  sram::neo_local_response& response,
                  sc_core::sc_time& delay) override
    {
        response = {};
        if (block_next_) {
            block_next_ = false;
            entered_ = true;
            entered_event_.notify(sc_core::SC_ZERO_TIME);
            sc_core::wait(release_event_);
            response.status = sram::neo_status::aborted;
            return;
        }
        access(request, response);
        delay += sc_core::sc_time(1, sc_core::SC_NS);
        response.latency = sc_core::sc_time(1, sc_core::SC_NS);
    }

    std::uint32_t dbg_access(const sram::neo_local_request& request) override
    {
        sram::neo_local_response response;
        access(request, response);
        return response.status == sram::neo_status::ok ? response.bytes : 0;
    }

private:
    void access(const sram::neo_local_request& request,
                sram::neo_local_response& response)
    {
        response = {};
        if (request.data == nullptr || request.size == 0
            || request.size > sram::neo_max_transfer_bytes
            || request.address < base
            || request.address - base > capacity
            || request.size > capacity - (request.address - base)) {
            response.status = sram::neo_status::capacity_error;
            return;
        }
        auto* memory = bytes_.data() + (request.address - base);
        if (request.command == sram::neo_command::read) {
            std::memcpy(request.data, memory, request.size);
        } else {
            for (std::uint32_t i = 0; i < request.size; ++i) {
                if (request.strobes == nullptr || request.strobes[i] != 0) {
                    memory[i] = request.data[i];
                }
            }
        }
        response.status = sram::neo_status::ok;
        response.bytes = sram::enabled_byte_count(request.strobes, request.size);
        response.beats = 1;
    }

    std::vector<unsigned char> bytes_;
    bool block_next_ = false;
    bool entered_ = false;
    sc_core::sc_event entered_event_;
    sc_core::sc_event release_event_;
};

class scenario : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(scenario);

    scenario(sc_core::sc_module_name name, std::function<void()> body)
        : sc_core::sc_module(name), body_(std::move(body))
    {
        SC_THREAD(run);
    }

    bool finished() const noexcept { return finished_; }

private:
    void run()
    {
        body_();
        finished_ = true;
        sc_core::sc_stop();
    }

    std::function<void()> body_;
    bool finished_ = false;
};

} // namespace

int sc_main(int, char*[])
{
    blocking_memory memory("memory");
    sa::adapter_config config;
    config.sram_base = base;
    config.sram_window = capacity;
    config.compute_watchdog_cycles = 100'000;
    adapter_t adapter("matrix_engine", config);

    sc_core::sc_clock clock("clock", 10, sc_core::SC_NS);
    sc_core::sc_signal<bool> rstn("rstn");
    adapter.i_clk(clock);
    adapter.i_rstn(rstn);
    adapter.local_port.bind(memory);

    const auto store8 = [&](std::uint64_t address, std::int8_t value) {
        unsigned char byte = static_cast<unsigned char>(value);
        sram::neo_local_request request;
        request.requester = sram::neo_requester::cpu;
        request.command = sram::neo_command::write;
        request.address = address;
        request.size = 1;
        request.data = &byte;
        CHECK(memory.dbg_access(request) == 1);
    };
    const auto load32 = [&](std::uint64_t address) {
        std::uint32_t raw = 0;
        sram::neo_local_request request;
        request.requester = sram::neo_requester::cpu;
        request.command = sram::neo_command::read;
        request.address = address;
        request.size = sizeof(raw);
        request.data = reinterpret_cast<unsigned char*>(&raw);
        CHECK(memory.dbg_access(request) == sizeof(raw));
        return static_cast<std::int32_t>(raw);
    };
    const auto make_job = [](std::uint64_t offset) {
        sa::job work;
        work.m = 1;
        work.n = 1;
        work.k = 1;
        work.a_address = base + offset;
        work.b_address = base + offset + 0x10;
        work.c_address = base + offset + 0x20;
        work.datatype = sa::datatype_value::int8_int32;
        return work;
    };
    const auto wait_idle = [&] {
        for (unsigned i = 0; i < 20'000 && adapter.busy(); ++i) {
            sc_core::wait(sc_core::sc_time(10, sc_core::SC_NS));
        }
        CHECK(!adapter.busy());
    };

    scenario run("scenario", [&] {
        rstn.write(false);
        sc_core::wait(sc_core::sc_time(100, sc_core::SC_NS));
        rstn.write(true);
        sc_core::wait(sc_core::sc_time(50, sc_core::SC_NS));

        // ABORT: START for the replacement job is issued while the only
        // sequencer thread is still parked in the abandoned SRAM request.
        const sa::job abandoned_abort = make_job(0x000);
        const sa::job after_abort = make_job(0x100);
        store8(after_abort.a_address, 3);
        store8(after_abort.b_address, 4);
        memory.arm();
        CHECK(adapter.submit(abandoned_abort) == sa::submit_status::accepted);
        if (!memory.entered()) {
            sc_core::wait(memory.entered_event());
        }
        adapter.abort();
        CHECK(!adapter.busy());
        CHECK(adapter.last_error() == sa::error_cause::aborted);
        CHECK(adapter.submit(after_abort) == sa::submit_status::accepted);
        memory.release();
        wait_idle();
        CHECK(adapter.last_error() == sa::error_cause::none);
        CHECK(load32(after_abort.c_address) == 12);
        CHECK(adapter.committed_bytes() == sizeof(std::int32_t));

        // RESET: it additionally closes the traffic-counter epoch.  The old
        // response arrives afterwards and must not repopulate the counters;
        // only A+B reads and the C write of the replacement job remain.
        const sa::job abandoned_reset = make_job(0x200);
        const sa::job after_reset = make_job(0x300);
        store8(after_reset.a_address, -5);
        store8(after_reset.b_address, 6);
        memory.arm();
        CHECK(adapter.submit(abandoned_reset) == sa::submit_status::accepted);
        if (!memory.entered()) {
            sc_core::wait(memory.entered_event());
        }
        adapter.reset();
        CHECK(!adapter.busy());
        CHECK(adapter.last_error() == sa::error_cause::none);
        CHECK(adapter.local_requests() == 0 && adapter.local_bytes() == 0);
        CHECK(adapter.submit(after_reset) == sa::submit_status::accepted);
        memory.release();
        wait_idle();
        CHECK(adapter.last_error() == sa::error_cause::none);
        CHECK(load32(after_reset.c_address) == -30);
        CHECK(adapter.committed_bytes() == sizeof(std::int32_t));
        CHECK(adapter.local_requests() == 3);
        CHECK(adapter.local_bytes() == 6);
    });

    sc_core::sc_start(sc_core::sc_time(10, sc_core::SC_MS));
    CHECK(run.finished());
    if (failures != 0) {
        std::cerr << failures << " adapter epoch check(s) failed\n";
        return 1;
    }
    std::cout << "Sauria adapter abort/reset epoch: PASS\n";
    return 0;
}
