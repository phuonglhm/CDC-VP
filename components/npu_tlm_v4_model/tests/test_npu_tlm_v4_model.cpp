// SPDX-License-Identifier: Apache-2.0

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#include <systemc>
#include <tlm>

#include "bus_router.h"
#include "memory_tlm.h"
#include "npu_tlm_v4_model.h"
#include "npu_tlm_v4_regmap.h"
#include "tlm_probe.h"

namespace {

using namespace cdc::components;
using namespace cdc::components::npu_v4_reg;

constexpr std::uint64_t kNpuBase = 0x100F'0000ULL;
constexpr std::uint64_t kRamBase = 0x8000'0000ULL;
constexpr std::uint32_t kSrcOffset = 0x0001'0000;
constexpr std::uint32_t kWeightsOffset = 0x0002'0000;
constexpr std::uint32_t kDstOffset = 0x0003'0000;
// Exercise a non-power-of-two reduction dimension here; the full-SoC
// firmware regression independently covers K=64.
constexpr std::uint32_t kK = 17;
constexpr std::uint32_t kRows = 32;
constexpr std::uint32_t kCols = 32;

void settle()
{
    wait(sc_core::SC_ZERO_TIME);
    wait(sc_core::SC_ZERO_TIME);
}

} // namespace

int sc_main(int, char*[])
{
    cdc::components::bus_router bus("bus", 2, 2);
    cdc::components::memory_tlm ram("ram", 2u * 1024u * 1024u);
    cdc::components::npu_tlm_v4_model npu(
        "npu", sc_core::sc_time(2, sc_core::SC_NS));
    cdc::test::tlm_probe probe("probe");
    sc_core::sc_signal<bool> reset_n("reset_n");
    sc_core::sc_signal<bool> irq("irq");

    probe.socket.bind(bus.cpu_port(0));
    npu.master_socket.bind(bus.cpu_port(1));
    bus.add_target(kRamBase, ram.size()).bind(ram.socket);
    bus.add_target(kNpuBase, MMIO_SIZE).bind(npu.target_socket);
    npu.reset_n(reset_n);
    npu.irq_out(irq);

    sc_core::sc_spawn([&] {
        auto write_reg = [&](std::uint32_t offset, std::uint32_t value) {
            return probe.write(kNpuBase + offset, &value, sizeof(value));
        };
        auto read_reg = [&](std::uint32_t offset) {
            std::uint32_t value = 0;
            CDC_CHECK(probe.read(kNpuBase + offset, &value, sizeof(value)) ==
                      tlm::TLM_OK_RESPONSE);
            return value;
        };

        reset_n.write(false);
        wait(10, sc_core::SC_NS);
        reset_n.write(true);
        wait(20, sc_core::SC_NS);
        settle();

        CDC_CHECK(read_reg(CORE_ID) == CORE_ID_VALUE);
        CDC_CHECK(read_reg(STATUS) == STATUS_IDLE);
        CDC_CHECK(irq.read() == false);

        std::vector<std::uint8_t> activations(kRows * kK);
        std::vector<std::uint8_t> weights(kK * kCols);
        std::array<std::int32_t, kRows * kCols> expected{};

        for (std::uint32_t row = 0; row < kRows; ++row) {
            for (std::uint32_t k = 0; k < kK; ++k) {
                const auto value =
                    static_cast<std::int8_t>((row + 3u * k) % 16u);
                activations[row * kK + k] =
                    static_cast<std::uint8_t>(value);
            }
        }
        for (std::uint32_t k = 0; k < kK; ++k) {
            for (std::uint32_t col = 0; col < kCols; ++col) {
                const auto value =
                    static_cast<std::int8_t>(
                        static_cast<int>((2u * k + col) % 15u) - 7);
                weights[k * kCols + col] =
                    static_cast<std::uint8_t>(value);
            }
        }
        for (std::uint32_t row = 0; row < kRows; ++row) {
            for (std::uint32_t col = 0; col < kCols; ++col) {
                std::int32_t sum = 0;
                for (std::uint32_t k = 0; k < kK; ++k) {
                    const auto a = static_cast<std::int8_t>(
                        activations[row * kK + k]);
                    const auto b =
                        static_cast<std::int8_t>(weights[k * kCols + col]);
                    sum += static_cast<std::int32_t>(a) *
                           static_cast<std::int32_t>(b);
                }
                expected[row * kCols + col] = sum;
            }
        }

        CDC_CHECK(probe.debug(tlm::TLM_WRITE_COMMAND,
                              kRamBase + kSrcOffset, activations.data(),
                              activations.size()) == activations.size());
        CDC_CHECK(probe.debug(tlm::TLM_WRITE_COMMAND,
                              kRamBase + kWeightsOffset, weights.data(),
                              weights.size()) == weights.size());

        CDC_CHECK(write_reg(SRC_ADDR,
                            static_cast<std::uint32_t>(kRamBase + kSrcOffset)) ==
                  tlm::TLM_OK_RESPONSE);
        CDC_CHECK(write_reg(SRC_SIZE_BYTES, activations.size()) ==
                  tlm::TLM_OK_RESPONSE);
        CDC_CHECK(write_reg(SRC_STRIDE_BYTES, kK) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(write_reg(WEIGHTS_ADDR,
                            static_cast<std::uint32_t>(
                                kRamBase + kWeightsOffset)) ==
                  tlm::TLM_OK_RESPONSE);
        CDC_CHECK(write_reg(WEIGHTS_SIZE_BYTES, weights.size()) ==
                  tlm::TLM_OK_RESPONSE);
        CDC_CHECK(write_reg(DST_ADDR,
                            static_cast<std::uint32_t>(kRamBase + kDstOffset)) ==
                  tlm::TLM_OK_RESPONSE);
        CDC_CHECK(write_reg(DST_SIZE_BYTES,
                            kRows * kCols * sizeof(std::int32_t)) ==
                  tlm::TLM_OK_RESPONSE);
        CDC_CHECK(write_reg(WIDTH, kCols) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(write_reg(HEIGHT, kRows) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(write_reg(K_DIMENSION, kK) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(write_reg(FORMAT, FORMAT_INT8_INT8_INT32) ==
                  tlm::TLM_OK_RESPONSE);
        CDC_CHECK(write_reg(OP_MODE, OP_GEMM) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(write_reg(IRQ_ENABLE, IRQ_DONE | IRQ_ERROR) ==
                  tlm::TLM_OK_RESPONSE);

        std::uint32_t ctrl = CTRL_ENABLE | CTRL_IRQ_EN;
        CDC_CHECK(write_reg(CTRL, ctrl) == tlm::TLM_OK_RESPONSE);
        ctrl |= CTRL_START;
        CDC_CHECK(write_reg(CTRL, ctrl) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK((read_reg(STATUS) & STATUS_BUSY) != 0u);

        bool completed = false;
        for (unsigned poll = 0; poll < 2000; ++poll) {
            wait(100, sc_core::SC_NS);
            const std::uint32_t status = read_reg(STATUS);
            if ((status & (STATUS_DONE | STATUS_ERROR)) != 0u) {
                completed = true;
                break;
            }
        }
        CDC_CHECK(completed);

        const std::uint32_t status = read_reg(STATUS);
        if ((status & STATUS_ERROR) != 0u) {
            std::cerr << "NPU job failed: status=0x" << std::hex << status
                      << " last_error=0x" << read_reg(LAST_ERROR)
                      << " cycles=0x" << read_reg(CYCLE_COUNT)
                      << std::dec << '\n';
        }
        CDC_CHECK((status & STATUS_DONE) != 0u);
        CDC_CHECK((status & STATUS_ERROR) == 0u);
        CDC_CHECK(read_reg(LAST_ERROR) ==
                  static_cast<std::uint32_t>(error_code::none));
        CDC_CHECK(read_reg(BYTES_READ) ==
                  activations.size() + weights.size());
        CDC_CHECK(read_reg(BYTES_WRITTEN) ==
                  kRows * kCols * sizeof(std::int32_t));
        settle();
        CDC_CHECK(irq.read() == true);

        std::array<std::int32_t, kRows * kCols> actual{};
        CDC_CHECK(probe.debug(tlm::TLM_READ_COMMAND,
                              kRamBase + kDstOffset, actual.data(),
                              sizeof(actual)) == sizeof(actual));
        for (std::size_t i = 0; i < actual.size(); ++i) {
            if (actual[i] != expected[i]) {
                std::cerr << "NPU mismatch index " << i << ": got "
                          << actual[i] << ", expected " << expected[i] << '\n';
                CDC_CHECK(actual[i] == expected[i]);
                break;
            }
        }

        std::uint32_t clear = IRQ_DONE;
        CDC_CHECK(write_reg(IRQ_STATUS, clear) == tlm::TLM_OK_RESPONSE);
        settle();
        CDC_CHECK(irq.read() == false);
        clear = STATUS_DONE;
        CDC_CHECK(write_reg(STATUS, clear) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(read_reg(STATUS) == STATUS_IDLE);

        // Invalid MMIO width/address and invalid job bounds are visible.
        std::uint16_t small = 0;
        CDC_CHECK(probe.write(kNpuBase + CTRL, &small, sizeof(small)) ==
                  tlm::TLM_ADDRESS_ERROR_RESPONSE);
        std::uint32_t value = 0;
        CDC_CHECK(probe.read(kNpuBase + 0x2000, &value, sizeof(value)) ==
                  tlm::TLM_ADDRESS_ERROR_RESPONSE);

        CDC_CHECK(write_reg(DST_ADDR, 0x7000'0000u) ==
                  tlm::TLM_OK_RESPONSE);
        ctrl = CTRL_ENABLE | CTRL_IRQ_EN | CTRL_START;
        CDC_CHECK(write_reg(CTRL, ctrl) == tlm::TLM_OK_RESPONSE);
        wait(100, sc_core::SC_NS);
        CDC_CHECK((read_reg(STATUS) & STATUS_ERROR) != 0u);
        CDC_CHECK(read_reg(LAST_ERROR) ==
                  static_cast<std::uint32_t>(error_code::invalid_address));

        // External active-low reset clears the software-visible state.
        reset_n.write(false);
        wait(10, sc_core::SC_NS);
        reset_n.write(true);
        wait(20, sc_core::SC_NS);
        settle();
        CDC_CHECK(read_reg(STATUS) == STATUS_IDLE);
        CDC_CHECK(read_reg(CTRL) == 0u);
        CDC_CHECK(irq.read() == false);

        sc_core::sc_stop();
    });

    sc_core::sc_start();
    return cdc::test::failures() == 0 ? 0 : 1;
}
