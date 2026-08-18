// SPDX-License-Identifier: Apache-2.0
// End-to-end AXI4-Lite -> asynchronous Im2Col -> native SRAM gate.

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
#include "tpu_v3/architecture_config.h"
#include "tpu_v3/core/neo_local_sram_fabric.h"
#include "tpu_v3/sram/core_sram.h"
#include "tpu_v3/transform/image_transform.h"
#include "tpu_v3/transform/transform_provenance.h"

namespace am = cdc::components::tpu_v3::address_map;
namespace core = cdc::components::tpu_v3::core;
namespace sram = cdc::components::tpu_v3::sram;
namespace tr = cdc::components::tpu_v3::transform;
namespace tpu = cdc::components::tpu_v3;

namespace {
int failures = 0;
#define CHECK(x) do { if (!(x)) { \
    std::cerr << "FAIL line " << __LINE__ << ": " #x "\n"; ++failures; \
} } while (false)

constexpr std::uint64_t sram_base = 0x2000'0000;
constexpr std::uint64_t control_base = 0x2103'0000;

sram::core_sram_config make_sram_config()
{
    sram::core_sram_config config;
    config.base_address = sram_base;
    config.window_bytes = am::core_sram_window;
    config.capacity_bytes = 64 * 1024;
    return config;
}

tpu::local_sram_fabric_config make_fabric_config()
{
    tpu::local_sram_fabric_config config;
    config.data_width_bits = 128;
    config.bank_count = 4;
    config.pipeline_stages = 2;
    config.mapping = tpu::bank_mapping::low_order_interleaved;
    config.max_outstanding_per_requester = 1;
    config.arbitration = tpu::arbitration_policy::round_robin;
    return config;
}

class mmio_master : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<mmio_master> socket{"socket"};
    explicit mmio_master(sc_core::sc_module_name name) : sc_module(name) {}

    tlm::tlm_response_status write(std::uint64_t address, std::uint32_t value)
    {
        return transact(tlm::TLM_WRITE_COMMAND, address,
                        reinterpret_cast<unsigned char*>(&value), 4, 4,
                        nullptr, 0);
    }

    std::uint32_t read(std::uint64_t address)
    {
        std::uint32_t value = 0xdeadbeef;
        CHECK(transact(tlm::TLM_READ_COMMAND, address,
                       reinterpret_cast<unsigned char*>(&value), 4, 4,
                       nullptr, 0) == tlm::TLM_OK_RESPONSE);
        return value;
    }

    tlm::tlm_response_status transact(
        tlm::tlm_command command, std::uint64_t address,
        unsigned char* data, unsigned length, unsigned streaming,
        unsigned char* enables, unsigned enable_length)
    {
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        trans.set_command(command);
        trans.set_address(address);
        trans.set_data_ptr(data);
        trans.set_data_length(length);
        trans.set_streaming_width(streaming);
        trans.set_byte_enable_ptr(enables);
        trans.set_byte_enable_length(enable_length);
        socket->b_transport(trans, delay);
        return trans.get_response_status();
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

class scenario : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(scenario);
    scenario(sc_core::sc_module_name name, std::function<void()> body)
        : sc_module(name), body_(std::move(body))
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
    sram::core_sram store("store", make_sram_config());
    core::neo_local_sram_fabric fabric(
        "fabric", make_fabric_config(), store,
        {sram::neo_requester::cpu, sram::neo_requester::transform},
        core::local_fabric_timing::annotated,
        sc_core::sc_time(1, sc_core::SC_NS));

    tr::image_transform_config config;
    config.control_base = control_base;
    config.sram_base = sram_base;
    tr::image_transform dut("image_transform", config);
    mmio_master cpu("cpu");
    sc_core::sc_signal<bool> irq{"irq"};
    dut.local.bind(fabric.native_port);
    dut.irq(irq);
    cpu.socket.bind(dut.control);

    const auto wr = [&](std::uint64_t reg, std::uint32_t value) {
        return cpu.write(control_base + reg, value);
    };
    const auto rd = [&](std::uint64_t reg) {
        return cpu.read(control_base + reg);
    };
    const auto debug_copy = [&](sram::neo_command command,
                                std::uint64_t address,
                                std::vector<unsigned char>& bytes) {
        std::uint64_t offset = 0;
        while (offset < bytes.size()) {
            const auto chunk = static_cast<std::uint32_t>(
                std::min<std::uint64_t>(bytes.size() - offset,
                                        sram::neo_max_transfer_bytes));
            sram::neo_local_request request;
            request.requester = sram::neo_requester::cpu;
            request.command = command;
            request.address = address + offset;
            request.size = chunk;
            request.data = bytes.data() + offset;
            CHECK(fabric.dbg_access(request) == chunk);
            offset += chunk;
        }
    };
    const auto program = [&](const tr::descriptor& work) {
        CHECK(wr(tr::reg::operation, work.operation) == tlm::TLM_OK_RESPONSE);
        CHECK(wr(tr::reg::src_addr_lo, static_cast<std::uint32_t>(work.source))
              == tlm::TLM_OK_RESPONSE);
        CHECK(wr(tr::reg::src_addr_hi,
                 static_cast<std::uint32_t>(work.source >> 32))
              == tlm::TLM_OK_RESPONSE);
        CHECK(wr(tr::reg::dst_addr_lo,
                 static_cast<std::uint32_t>(work.destination))
              == tlm::TLM_OK_RESPONSE);
        CHECK(wr(tr::reg::dst_addr_hi,
                 static_cast<std::uint32_t>(work.destination >> 32))
              == tlm::TLM_OK_RESPONSE);
        CHECK(wr(tr::reg::input_channels, work.channels)
              == tlm::TLM_OK_RESPONSE);
        CHECK(wr(tr::reg::input_height, work.input_height)
              == tlm::TLM_OK_RESPONSE);
        CHECK(wr(tr::reg::input_width, work.input_width)
              == tlm::TLM_OK_RESPONSE);
        CHECK(wr(tr::reg::kernel_height, work.kernel_height)
              == tlm::TLM_OK_RESPONSE);
        CHECK(wr(tr::reg::kernel_width, work.kernel_width)
              == tlm::TLM_OK_RESPONSE);
        CHECK(wr(tr::reg::stride_height, work.stride_height)
              == tlm::TLM_OK_RESPONSE);
        CHECK(wr(tr::reg::stride_width, work.stride_width)
              == tlm::TLM_OK_RESPONSE);
        CHECK(wr(tr::reg::dilation_height, work.dilation_height)
              == tlm::TLM_OK_RESPONSE);
        CHECK(wr(tr::reg::dilation_width, work.dilation_width)
              == tlm::TLM_OK_RESPONSE);
        CHECK(wr(tr::reg::pad_top, work.pad_top) == tlm::TLM_OK_RESPONSE);
        CHECK(wr(tr::reg::pad_left, work.pad_left) == tlm::TLM_OK_RESPONSE);
        CHECK(wr(tr::reg::pad_bottom, work.pad_bottom)
              == tlm::TLM_OK_RESPONSE);
        CHECK(wr(tr::reg::pad_right, work.pad_right)
              == tlm::TLM_OK_RESPONSE);
        CHECK(wr(tr::reg::datatype, work.datatype) == tlm::TLM_OK_RESPONSE);
    };
    const auto wait_idle = [&] {
        for (unsigned i = 0; i < 100'000 && dut.busy(); ++i) {
            sc_core::wait(sc_core::sc_time(1, sc_core::SC_NS));
        }
        CHECK(!dut.busy());
        // update_irq() immediately wakes the sole writer method; its
        // sc_signal update is observable at the next delta boundary.
        sc_core::wait(sc_core::SC_ZERO_TIME);
    };
    const auto settle_irq = [&] {
        sc_core::wait(sc_core::SC_ZERO_TIME);
    };

    scenario run("scenario", [&] {
        sc_core::wait(sc_core::SC_ZERO_TIME);
        CHECK(rd(tr::reg::id) == tr::identity);
        CHECK(rd(tr::reg::version) == tr::model_version);
        CHECK(rd(tr::reg::capability) == tr::capability_bit::value);
        CHECK((rd(tr::reg::capability) & tr::capability_bit::col2im) == 0);
        CHECK(rd(tr::reg::source_tag) == tr::provenance::source_tag);
        CHECK(rd(0x1000) == 0); // reserved, no alias to ID

        // Protocol refusals are TLM errors, not transform job errors.
        std::uint64_t wide = 0;
        CHECK(cpu.transact(tlm::TLM_READ_COMMAND, control_base,
                           reinterpret_cast<unsigned char*>(&wide), 8, 8,
                           nullptr, 0) == tlm::TLM_BURST_ERROR_RESPONSE);
        std::uint32_t word = 0;
        CHECK(cpu.transact(tlm::TLM_READ_COMMAND, control_base + 2,
                           reinterpret_cast<unsigned char*>(&word), 4, 4,
                           nullptr, 0) == tlm::TLM_BURST_ERROR_RESPONSE);
        unsigned char enables[2]{TLM_BYTE_ENABLED, TLM_BYTE_DISABLED};
        CHECK(cpu.transact(tlm::TLM_WRITE_COMMAND, control_base,
                           reinterpret_cast<unsigned char*>(&word), 4, 4,
                           enables, 2) == tlm::TLM_BURST_ERROR_RESPONSE);
        CHECK(cpu.transact(tlm::TLM_READ_COMMAND, control_base, nullptr, 4, 4,
                           nullptr, 0) == tlm::TLM_GENERIC_ERROR_RESPONSE);
        CHECK(cpu.transact(tlm::TLM_IGNORE_COMMAND, control_base,
                           reinterpret_cast<unsigned char*>(&word), 4, 4,
                           nullptr, 0) == tlm::TLM_COMMAND_ERROR_RESPONSE);
        CHECK(cpu.transact(tlm::TLM_READ_COMMAND,
                           control_base + am::transform_control_size,
                           reinterpret_cast<unsigned char*>(&word), 4, 4,
                           nullptr, 0) == tlm::TLM_ADDRESS_ERROR_RESPONSE);

        tr::descriptor work;
        work.source = sram_base + 0x1000;
        work.destination = sram_base + 0x2000;
        work.channels = 2;
        work.input_height = 5;
        work.input_width = 6;
        work.kernel_height = 2;
        work.kernel_width = 3;
        work.stride_height = work.stride_width = 1;
        work.dilation_height = work.dilation_width = 1;
        tr::matrix_shape shape;
        CHECK(tr::derive_shape(work, shape) == tr::error_cause::none);
        CHECK(shape.output_height == 4 && shape.output_width == 4);
        CHECK(shape.rows == 16 && shape.columns == 12);

        std::vector<std::int8_t> signed_input(shape.source_bytes);
        for (std::size_t i = 0; i < signed_input.size(); ++i) {
            signed_input[i] = static_cast<std::int8_t>(i - 30);
        }
        const auto signed_expected = tr::im2col_reference(work, signed_input);
        std::vector<unsigned char> input(signed_input.size());
        std::memcpy(input.data(), signed_input.data(), input.size());
        debug_copy(sram::neo_command::write, work.source, input);

        program(work);
        CHECK(rd(tr::reg::output_height) == shape.output_height);
        CHECK(rd(tr::reg::output_width) == shape.output_width);
        CHECK(rd(tr::reg::matrix_rows) == shape.rows);
        CHECK(rd(tr::reg::matrix_columns) == shape.columns);
        CHECK(wr(tr::reg::irq_enable, tr::irq_enable_bit::completion)
              == tlm::TLM_OK_RESPONSE);
        CHECK(cpu.debug_write(control_base + tr::reg::control,
                              tr::control_bit::start) == 4);
        CHECK(!dut.busy()); // debug writes are side-effect free

        CHECK(wr(tr::reg::control, tr::control_bit::start)
              == tlm::TLM_OK_RESPONSE);
        CHECK(dut.busy());
        CHECK(wr(tr::reg::input_width, 99)
              == tlm::TLM_GENERIC_ERROR_RESPONSE);
        CHECK(wr(tr::reg::control, tr::control_bit::start)
              == tlm::TLM_GENERIC_ERROR_RESPONSE);
        CHECK(rd(tr::reg::overrun_count) == 1);
        wait_idle();
        CHECK((rd(tr::reg::status) & tr::status_bit::done) != 0);
        CHECK(irq.read());
        CHECK(rd(tr::reg::bytes_done_lo) == shape.destination_bytes);
        CHECK(rd(tr::reg::local_requests) == 1 + shape.rows);
        CHECK(rd(tr::reg::local_bytes_lo)
              == shape.source_bytes + shape.destination_bytes);
        const auto& counters = fabric.counters(sram::neo_requester::transform);
        CHECK(counters.request_count == dut.local_requests());
        CHECK(counters.transferred_bytes == dut.local_bytes());

        std::vector<unsigned char> output(shape.destination_bytes, 0);
        debug_copy(sram::neo_command::read, work.destination, output);
        CHECK(output.size() == signed_expected.size());
        CHECK(std::memcmp(output.data(), signed_expected.data(), output.size())
              == 0);
        CHECK(wr(tr::reg::status, tr::status_bit::done)
              == tlm::TLM_OK_RESPONSE);
        settle_irq();
        CHECK(!irq.read());

        // Col2Im is visible as unavailable and START fails asynchronously with
        // no tensor traffic; it is never a fake no-op success.
        const std::uint64_t requests_before_col2im = dut.local_requests();
        work.operation = tr::operation_value::col2im;
        program(work);
        CHECK(wr(tr::reg::control, tr::control_bit::start)
              == tlm::TLM_OK_RESPONSE);
        wait_idle();
        CHECK((rd(tr::reg::status) & tr::status_bit::error) != 0);
        CHECK(rd(tr::reg::error_cause)
              == static_cast<std::uint32_t>(
                  tr::error_cause::unavailable_operation));
        CHECK(dut.local_requests() == requests_before_col2im);
        CHECK(irq.read());
        CHECK(wr(tr::reg::status, tr::status_bit::error)
              == tlm::TLM_OK_RESPONSE);
        settle_irq();
        CHECK(!irq.read());

        // Padding is also an explicit refusal because v4.2 supplies no
        // padding contract. It is not silently interpreted as zero-fill.
        work.operation = tr::operation_value::im2col;
        work.pad_top = 1;
        program(work);
        CHECK(wr(tr::reg::control, tr::control_bit::start)
              == tlm::TLM_OK_RESPONSE);
        wait_idle();
        CHECK(rd(tr::reg::error_cause)
              == static_cast<std::uint32_t>(
                  tr::error_cause::unsupported_padding));
        CHECK(dut.local_requests() == requests_before_col2im);
        CHECK(wr(tr::reg::status, tr::status_bit::error)
              == tlm::TLM_OK_RESPONSE);
        settle_irq();
        CHECK(!irq.read());
        work.pad_top = 0;

        // Descriptor errors are reported asynchronously and do not touch the
        // local plane. In-place lowering would overwrite input that later
        // matrix rows still need, so overlap is a hard refusal.
        work.destination = work.source;
        program(work);
        CHECK(wr(tr::reg::control, tr::control_bit::start)
              == tlm::TLM_OK_RESPONSE);
        wait_idle();
        CHECK(rd(tr::reg::error_cause)
              == static_cast<std::uint32_t>(tr::error_cause::region_overlap));
        CHECK(dut.local_requests() == requests_before_col2im);
        CHECK(wr(tr::reg::status, tr::status_bit::error)
              == tlm::TLM_OK_RESPONSE);
        settle_irq();

        work.destination = sram_base + 0x2000;
        work.source = sram_base + am::core_sram_window;
        program(work);
        CHECK(wr(tr::reg::control, tr::control_bit::start)
              == tlm::TLM_OK_RESPONSE);
        wait_idle();
        CHECK(rd(tr::reg::error_cause) == static_cast<std::uint32_t>(
                  tr::error_cause::source_out_of_range));
        CHECK(dut.local_requests() == requests_before_col2im);
        CHECK(wr(tr::reg::status, tr::status_bit::error)
              == tlm::TLM_OK_RESPONSE);
        settle_irq();
        work.source = sram_base + 0x1000;

        // Abort before the worker consumes START, then immediately replace it.
        // The persistent start_pending flag must not lose the replacement.
        program(work);
        CHECK(wr(tr::reg::control, tr::control_bit::start)
              == tlm::TLM_OK_RESPONSE);
        CHECK(wr(tr::reg::control, tr::control_bit::abort)
              == tlm::TLM_OK_RESPONSE);
        CHECK((rd(tr::reg::status) & tr::status_bit::aborted) != 0);
        settle_irq();
        CHECK(!irq.read());
        CHECK(wr(tr::reg::status, tr::status_bit::aborted)
              == tlm::TLM_OK_RESPONSE);
        program(work);
        CHECK(wr(tr::reg::control, tr::control_bit::start)
              == tlm::TLM_OK_RESPONSE);
        wait_idle();
        CHECK((rd(tr::reg::status) & tr::status_bit::done) != 0);
        CHECK(wr(tr::reg::status, tr::status_bit::done)
              == tlm::TLM_OK_RESPONSE);

        // The first output row fits exactly at the capacity limit; the next
        // reaches the target and reports local_write. The committed count must
        // describe the row already in memory rather than collapse to zero.
        work.destination = sram_base + 64 * 1024 - shape.columns;
        program(work);
        CHECK(wr(tr::reg::control, tr::control_bit::start)
              == tlm::TLM_OK_RESPONSE);
        wait_idle();
        CHECK(rd(tr::reg::error_cause)
              == static_cast<std::uint32_t>(tr::error_cause::local_write));
        CHECK(rd(tr::reg::bytes_done_lo) == shape.columns);
        CHECK(wr(tr::reg::status, tr::status_bit::error)
              == tlm::TLM_OK_RESPONSE);

        // A source inside the architectural window but above the instantiated
        // capacity reaches SRAM and fails as local_read rather than aliasing.
        work.source = sram_base + 0x20000;
        work.destination = sram_base + 0x2000;
        const std::uint64_t requests_before_read_error = dut.local_requests();
        program(work);
        CHECK(wr(tr::reg::control, tr::control_bit::start)
              == tlm::TLM_OK_RESPONSE);
        wait_idle();
        CHECK(rd(tr::reg::error_cause)
              == static_cast<std::uint32_t>(tr::error_cause::local_read));
        CHECK(rd(tr::reg::bytes_done_lo) == 0);
        CHECK(dut.local_requests() == requests_before_read_error + 1);
        CHECK(wr(tr::reg::status, tr::status_bit::error)
              == tlm::TLM_OK_RESPONSE);

        // Hierarchical active reset: close the transform and fabric traffic
        // epochs together, then prove a replacement job is not lost.
        work.source = sram_base + 0x1000;
        work.destination = sram_base + 0x3000;
        work.channels = 8;
        work.input_height = work.input_width = 16;
        work.kernel_height = work.kernel_width = 3;
        tr::matrix_shape large_shape;
        CHECK(tr::derive_shape(work, large_shape) == tr::error_cause::none);
        std::vector<unsigned char> large_input(large_shape.source_bytes, 3);
        debug_copy(sram::neo_command::write, work.source, large_input);
        program(work);
        CHECK(wr(tr::reg::control, tr::control_bit::start)
              == tlm::TLM_OK_RESPONSE);
        sc_core::wait(sc_core::sc_time(2, sc_core::SC_NS));
        CHECK(dut.busy());
        dut.reset();
        fabric.reset();
        CHECK(!dut.busy() && rd(tr::reg::status) == 0);
        CHECK(dut.local_requests() == 0 && dut.local_bytes() == 0);

        // reset clears the descriptor; reload the small known-good job.
        work = {};
        work.source = sram_base + 0x1000;
        work.destination = sram_base + 0x2000;
        work.channels = 2;
        work.input_height = 5;
        work.input_width = 6;
        work.kernel_height = 2;
        work.kernel_width = 3;
        work.stride_height = work.stride_width = 1;
        work.dilation_height = work.dilation_width = 1;
        debug_copy(sram::neo_command::write, work.source, input);
        program(work);
        CHECK(wr(tr::reg::control, tr::control_bit::start)
              == tlm::TLM_OK_RESPONSE);
        wait_idle();
        CHECK((rd(tr::reg::status) & tr::status_bit::done) != 0);
        CHECK(dut.abort_count() >= 2); // explicit abort + active reset
        CHECK(dut.report().find("Col2Im             : unavailable")
              != std::string::npos);
    });

    sc_core::sc_start(sc_core::sc_time(10, sc_core::SC_MS));
    CHECK(run.finished());
    if (failures != 0) {
        std::cerr << failures << " ImageTransform check(s) failed\n";
        return 1;
    }
    std::cout << "TPU_V3 ImageTransform MMIO/native-SRAM/epoch: PASS\n";
    return 0;
}
