// SPDX-License-Identifier: Apache-2.0

#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include "tpu_v3/address_map.h"
#include "tpu_v3/sauria/sa_control.h"
#include "tpu_v3/sauria/sa_registers.h"

namespace sa = cdc::components::tpu_v3::sauria;
namespace am = cdc::components::tpu_v3::address_map;

namespace {
int failures = 0;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL line " << __LINE__ \
    << ": " #x "\n"; ++failures; } } while (false)

class fake_engine : public sa::sauria_matrix_if {
public:
    sa::submit_status next_submit = sa::submit_status::accepted;
    bool running = false;
    sa::error_cause cause = sa::error_cause::none;
    std::uint64_t committed = 0;
    std::uint64_t requests = 0;
    std::uint64_t bytes = 0;
    timing measured{};
    sa::job captured{};
    unsigned resets = 0;

    sa::submit_status submit(const sa::job& work) override
    {
        if (running) return sa::submit_status::busy;
        if (next_submit != sa::submit_status::accepted) {
            const auto result = next_submit;
            next_submit = sa::submit_status::accepted;
            return result;
        }
        captured = work;
        cause = sa::error_cause::none;
        committed = requests = bytes = 0;
        measured = {};
        running = true;
        return sa::submit_status::accepted;
    }
    void abort() override { running = false; cause = sa::error_cause::aborted; }
    void reset() override
    {
        running = false;
        cause = sa::error_cause::none;
        requests = bytes = 0;
        ++resets;
    }
    bool busy() const override { return running; }
    sa::error_cause last_error() const override { return cause; }
    std::uint64_t committed_bytes() const override { return committed; }
    sa::engine_identity identity() const override
    {
        return {64, 64, sa::capability_bit::int8_int32, "fake@test"};
    }
    timing last_timing() const override { return measured; }
    std::uint64_t local_requests() const override { return requests; }
    std::uint64_t local_bytes() const override { return bytes; }
};

struct master : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<master> socket{"socket"};
    explicit master(sc_core::sc_module_name name) : sc_module(name) {}

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

    unsigned debug_write(std::uint64_t address, std::uint32_t value)
    {
        tlm::tlm_generic_payload trans;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(address);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_data_length(4);
        return socket->transport_dbg(trans);
    }
};
} // namespace

int sc_main(int, char*[])
{
    const std::uint64_t base = am::sa_control(0, 0);
    fake_engine engine;
    sa::sa_control_config config;
    config.control_base = base;
    sa::sa_control dut("sa_control", config, engine);
    master cpu("cpu");
    sc_core::sc_clock clock("clock", 10, sc_core::SC_NS);
    sc_core::sc_signal<bool> irq{"irq"};
    dut.i_clk(clock);
    dut.irq(irq);
    cpu.socket.bind(dut.control);
    sc_core::sc_start(sc_core::SC_ZERO_TIME);

    const auto wr = [&](std::uint64_t offset, std::uint32_t value) {
        return cpu.write(base + offset, value);
    };
    const auto rd = [&](std::uint64_t offset) { return cpu.read(base + offset); };

    CHECK(rd(sa::reg::id) == sa::identity_value);
    CHECK(rd(sa::reg::geometry) == (64u << 16 | 64u));
    CHECK(rd(sa::reg::capability) == sa::capability_bit::int8_int32);
    CHECK(rd(0x1000) == 0); // reserved, never aliases ID

    CHECK(wr(sa::reg::dim_m, 7) == tlm::TLM_OK_RESPONSE);
    CHECK(wr(sa::reg::dim_n, 13) == tlm::TLM_OK_RESPONSE);
    CHECK(wr(sa::reg::dim_k, 5) == tlm::TLM_OK_RESPONSE);
    CHECK(wr(sa::reg::a_addr_lo, 0x1000) == tlm::TLM_OK_RESPONSE);
    CHECK(wr(sa::reg::b_addr_lo, 0x2000) == tlm::TLM_OK_RESPONSE);
    CHECK(wr(sa::reg::c_addr_lo, 0x3000) == tlm::TLM_OK_RESPONSE);
    CHECK(wr(sa::reg::datatype, sa::datatype_value::int8_int32)
          == tlm::TLM_OK_RESPONSE);
    CHECK(wr(sa::reg::irq_enable, sa::irq_enable_bit::completion)
          == tlm::TLM_OK_RESPONSE);

    CHECK(wr(sa::reg::control, sa::control_bit::start) == tlm::TLM_OK_RESPONSE);
    CHECK((rd(sa::reg::status) & sa::status_bit::busy) != 0);
    CHECK(engine.running && engine.captured.m == 7 && engine.captured.n == 13
          && engine.captured.k == 5);
    CHECK(!irq.read());
    CHECK(wr(sa::reg::dim_m, 8) == tlm::TLM_GENERIC_ERROR_RESPONSE);
    CHECK(wr(sa::reg::control, sa::control_bit::start)
          == tlm::TLM_GENERIC_ERROR_RESPONSE);
    CHECK(rd(sa::reg::overrun_count) == 1);

    engine.running = false;
    engine.committed = 7 * 13 * 4;
    engine.requests = 11;
    engine.bytes = 777;
    engine.measured = {17, std::uint64_t{1} << 40, 23};
    sc_core::sc_start(20, sc_core::SC_NS);
    CHECK((rd(sa::reg::status) & sa::status_bit::done) != 0);
    CHECK(irq.read());
    CHECK(rd(sa::reg::c_bytes_done_lo) == 7 * 13 * 4);
    CHECK(rd(sa::reg::local_requests) == 11);
    CHECK(rd(sa::reg::local_bytes_lo) == 777);
    CHECK(rd(sa::reg::compute_ns) == std::numeric_limits<std::uint32_t>::max());
    CHECK(wr(sa::reg::status, sa::status_bit::done) == tlm::TLM_OK_RESPONSE);
    sc_core::sc_start(sc_core::SC_ZERO_TIME);
    CHECK(!irq.read());

    // A rejected descriptor is a command-level completion error, not a TLM
    // protocol error, and is visible through the same level IRQ.
    engine.next_submit = sa::submit_status::staging_capacity_exceeded;
    CHECK(wr(sa::reg::control, sa::control_bit::start) == tlm::TLM_OK_RESPONSE);
    CHECK((rd(sa::reg::status) & sa::status_bit::error) != 0);
    CHECK(rd(sa::reg::error_cause)
          == static_cast<std::uint32_t>(sa::error_cause::dimension_too_large));
    sc_core::sc_start(sc_core::SC_ZERO_TIME);
    CHECK(irq.read());
    CHECK(wr(sa::reg::status, sa::status_bit::error) == tlm::TLM_OK_RESPONSE);
    sc_core::sc_start(sc_core::SC_ZERO_TIME);

    // Abort is immediate, sticky and deliberately does not raise completion.
    CHECK(wr(sa::reg::control, sa::control_bit::start) == tlm::TLM_OK_RESPONSE);
    CHECK(wr(sa::reg::control, sa::control_bit::abort) == tlm::TLM_OK_RESPONSE);
    CHECK((rd(sa::reg::status) & sa::status_bit::aborted) != 0);
    CHECK(!engine.running && !irq.read());
    CHECK(rd(sa::reg::abort_count) == 1);

    // Debug writes are side-effect free: a loader cannot start a job.
    CHECK(cpu.debug_write(base + sa::reg::control, sa::control_bit::start) == 4);
    CHECK(!engine.running);

    // Reset abandons the engine, clears status/IRQ and preserves event history.
    CHECK(wr(sa::reg::status, sa::status_bit::aborted) == tlm::TLM_OK_RESPONSE);
    CHECK(wr(sa::reg::control, sa::control_bit::start) == tlm::TLM_OK_RESPONSE);
    dut.reset();
    sc_core::sc_start(sc_core::SC_ZERO_TIME);
    CHECK(rd(sa::reg::status) == 0 && !irq.read() && engine.resets == 1);
    CHECK(rd(sa::reg::job_count) == 3);

    if (failures != 0) {
        std::cerr << failures << " SA control check(s) failed\n";
        return 1;
    }
    std::cout << "SA AXI4-Lite control/status/IRQ PASS\n";
    return 0;
}
