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
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include "tpu_v3/address_map.h"
#include "tpu_v3/sauria/sa_control.h"
#include "tpu_v3/sauria/sa_registers.h"
#include "tpu_v3/sauria/sauria_matrix_adapter.h"
#include "tpu_v3/sram/native_port.h"

namespace sa = cdc::components::tpu_v3::sauria;
namespace sram = cdc::components::tpu_v3::sram;
namespace am = cdc::components::tpu_v3::address_map;

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
        block_writes_only_ = false;
        commit_before_release_ = false;
        entered_ = false;
    }

    /// Park the next write after its bytes have entered memory but before the
    /// response reaches the adapter. This is the ABORT/C_BYTES_DONE race: the
    /// control snapshot sees zero, then the late successful response attributes
    /// bytes that are already physically present in C.
    void arm_committed_write()
    {
        block_next_ = true;
        block_writes_only_ = true;
        commit_before_release_ = true;
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
        const bool matches_block = block_next_
            && (!block_writes_only_
                || request.command == sram::neo_command::write);
        if (matches_block) {
            block_next_ = false;
            if (commit_before_release_) {
                access(request, response);
            }
            entered_ = true;
            entered_event_.notify(sc_core::SC_ZERO_TIME);
            sc_core::wait(release_event_);
            if (!commit_before_release_) {
                response.status = sram::neo_status::aborted;
            }
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
    bool block_writes_only_ = false;
    bool commit_before_release_ = false;
    bool entered_ = false;
    sc_core::sc_event entered_event_;
    sc_core::sc_event release_event_;
};

class control_master : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<control_master> socket{"socket"};
    explicit control_master(sc_core::sc_module_name name) : sc_module(name) {}

    tlm::tlm_response_status write(std::uint64_t address, std::uint32_t value)
    {
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(address);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_data_length(4);
        trans.set_streaming_width(4);
        socket->b_transport(trans, delay);
        return trans.get_response_status();
    }

    std::uint32_t read(std::uint64_t address)
    {
        std::uint32_t value = 0xdeadbeef;
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(address);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_data_length(4);
        trans.set_streaming_width(4);
        socket->b_transport(trans, delay);
        CHECK(trans.get_response_status() == tlm::TLM_OK_RESPONSE);
        return value;
    }
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
    sc_core::sc_signal<bool> irq("irq");
    adapter.i_clk(clock);
    adapter.i_rstn(rstn);
    adapter.local_port.bind(memory);

    const std::uint64_t control_base = am::sa_control(0, 0);
    sa::sa_control_config control_config;
    control_config.control_base = control_base;
    sa::sa_control control("sa_control", control_config, adapter);
    control_master cpu("cpu");
    control.i_clk(clock);
    control.irq(irq);
    cpu.socket.bind(control.control);

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

        // Firmware-visible ABORT accounting. The C write commits inside the
        // target and is then held before b_access returns. ABORT therefore takes
        // its first snapshot at zero bytes. Once the response is released the
        // adapter must attribute all four bytes to the abandoned owner, and the
        // control register must reconcile even though BUSY and ABORTED have both
        // already been cleared/acknowledged.
        const sa::job late_write = make_job(0x400);
        store8(late_write.a_address, 7);
        store8(late_write.b_address, -3);
        const std::uint64_t requests_before = adapter.local_requests();
        const std::uint64_t bytes_before = adapter.local_bytes();

        const auto wr = [&](std::uint64_t offset, std::uint32_t value) {
            return cpu.write(control_base + offset, value);
        };
        const auto rd = [&](std::uint64_t offset) {
            return cpu.read(control_base + offset);
        };
        CHECK(wr(sa::reg::dim_m, late_write.m) == tlm::TLM_OK_RESPONSE);
        CHECK(wr(sa::reg::dim_n, late_write.n) == tlm::TLM_OK_RESPONSE);
        CHECK(wr(sa::reg::dim_k, late_write.k) == tlm::TLM_OK_RESPONSE);
        CHECK(wr(sa::reg::a_addr_lo,
                 static_cast<std::uint32_t>(late_write.a_address))
              == tlm::TLM_OK_RESPONSE);
        CHECK(wr(sa::reg::b_addr_lo,
                 static_cast<std::uint32_t>(late_write.b_address))
              == tlm::TLM_OK_RESPONSE);
        CHECK(wr(sa::reg::c_addr_lo,
                 static_cast<std::uint32_t>(late_write.c_address))
              == tlm::TLM_OK_RESPONSE);
        CHECK(wr(sa::reg::datatype, late_write.datatype)
              == tlm::TLM_OK_RESPONSE);

        memory.arm_committed_write();
        CHECK(wr(sa::reg::control, sa::control_bit::start)
              == tlm::TLM_OK_RESPONSE);
        if (!memory.entered()) {
            sc_core::wait(memory.entered_event());
        }
        CHECK(load32(late_write.c_address) == -21);
        CHECK(adapter.committed_bytes() == 0);
        CHECK(wr(sa::reg::control, sa::control_bit::abort)
              == tlm::TLM_OK_RESPONSE);
        CHECK(rd(sa::reg::c_bytes_done_lo) == 0);
        CHECK(wr(sa::reg::status, sa::status_bit::aborted)
              == tlm::TLM_OK_RESPONSE);
        memory.release();
        for (unsigned i = 0;
             i < 20 && adapter.committed_bytes() != sizeof(std::int32_t); ++i) {
            sc_core::wait(sc_core::sc_time(10, sc_core::SC_NS));
        }
        CHECK(adapter.committed_bytes() == sizeof(std::int32_t));
        CHECK(rd(sa::reg::c_bytes_done_lo) == sizeof(std::int32_t));
        CHECK(rd(sa::reg::local_requests) == requests_before + 3);
        CHECK(rd(sa::reg::local_bytes_lo) == bytes_before + 6);
        CHECK(rd(sa::reg::status) == 0);
        CHECK(!irq.read());
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
