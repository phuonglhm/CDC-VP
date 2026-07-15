#include "model/vpu_mmio.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

namespace {

void write64(model::VpuMmioDevice& vpu, std::uint32_t lo, std::uint32_t hi,
             std::uint64_t value) {
    vpu.write32(lo, static_cast<std::uint32_t>(value));
    vpu.write32(hi, static_cast<std::uint32_t>(value >> 32));
}

} // namespace

int main() {
    using namespace model::vpu_reg;
    constexpr std::uint32_t width = 32;
    constexpr std::uint32_t height = 32;
    constexpr std::uint32_t frames = 2;
    constexpr std::size_t frame_bytes = width * height * 3 / 2;
    constexpr std::uint64_t src = 0x1000;
    constexpr std::uint64_t dst = 0x3000;

    model::FlatMemory memory(0x10000);
    std::vector<std::uint8_t> raw(frame_bytes * frames);
    for (std::size_t i = 0; i < raw.size(); ++i) raw[i] = static_cast<std::uint8_t>(i);
    assert(memory.write(src, raw));

    std::vector<bool> irq_levels;
    model::VpuMmioDevice vpu(memory, [&](bool level) { irq_levels.push_back(level); });
    assert(vpu.read32(ID) == 0x56505533U);
    assert(vpu.read32(VERSION) == 0x00030008U);
    assert(vpu.read32(FIFO_CONFIG) == 0x04040404U);
    write64(vpu, SRC_ADDR_LO, SRC_ADDR_HI, src);
    write64(vpu, DST_ADDR_LO, DST_ADDR_HI, dst);
    vpu.write32(DST_CAPACITY, 0x8000);
    vpu.write32(WIDTH, width);
    vpu.write32(HEIGHT, height);
    vpu.write32(STRIDE_Y, width);
    vpu.write32(FRAME_COUNT, frames);
    vpu.write32(QP, 26);
    vpu.write32(INPUT_FORMAT, FORMAT_YUV420P8);
    vpu.write32(ENCODER_MODE, MODE_PCM);
    vpu.write32(CONTROL, CONTROL_IRQ_ENABLE | CONTROL_START);

    assert(vpu.busy());
    assert(vpu.read32(FRAMES_DONE) == 0);
    assert(vpu.tick());
    assert(vpu.read32(CYCLES_LO) == 1);
    assert(vpu.read32(FRAMES_DONE) == 0);
    assert(vpu.step());
    assert(vpu.busy());
    assert(vpu.read32(FRAMES_DONE) == 1);
    assert(vpu.step());
    assert(!vpu.busy());
    assert(vpu.read32(STATUS) & STATUS_DONE);
    assert(vpu.read32(STATUS) & STATUS_IRQ);
    assert(vpu.read32(IRQ_STATUS) == IRQ_DONE);
    assert(vpu.read32(FRAMES_DONE) == frames);
    assert(vpu.read32(BITSTREAM_BYTES) > raw.size());
    assert(vpu.read32(CYCLES_LO) > 0);
    assert(vpu.read32(DMA_READ_ACTIVE) > 0);
    assert(vpu.read32(DMA_WRITE_ACTIVE) > 0);
    assert(vpu.read32(FIFO_MAX_OCCUPANCY) != 0);
    assert(irq_levels.size() == 1 && irq_levels[0]);

    std::vector<std::uint8_t> header(6);
    assert(memory.read(dst, header));
    const std::vector<std::uint8_t> expected{0x00,0x00,0x00,0x01,0x40,0x01};
    assert(header == expected);

    vpu.write32(IRQ_STATUS, IRQ_DONE);
    assert(!vpu.irq_level());
    assert((vpu.read32(STATUS) & STATUS_IRQ) == 0);
    assert(irq_levels.size() == 2 && !irq_levels[1]);

    // The no-residual intra-DC mode uses the same MMIO/DMA path and compresses
    // this frame to substantially fewer bytes than raw PCM.
    vpu.write32(CONTROL, CONTROL_SOFT_RESET | CONTROL_IRQ_ENABLE);
    write64(vpu, SRC_ADDR_LO, SRC_ADDR_HI, src);
    write64(vpu, DST_ADDR_LO, DST_ADDR_HI, dst);
    vpu.write32(DST_CAPACITY, 0x8000);
    vpu.write32(WIDTH, width);
    vpu.write32(HEIGHT, height);
    vpu.write32(STRIDE_Y, width);
    vpu.write32(FRAME_COUNT, 1);
    vpu.write32(QP, 26);
    vpu.write32(INPUT_FORMAT, FORMAT_YUV420P8);
    vpu.write32(ENCODER_MODE, MODE_INTRA_DC);
    vpu.write32(CONTROL, CONTROL_START | CONTROL_IRQ_ENABLE);
    vpu.run_to_completion();
    assert(vpu.read32(STATUS) & STATUS_DONE);
    assert(vpu.read32(BITSTREAM_BYTES) < frame_bytes);

    // Invalid config must complete with ERROR IRQ rather than touching memory.
    vpu.write32(CONTROL, CONTROL_SOFT_RESET | CONTROL_IRQ_ENABLE);
    vpu.write32(CONTROL, CONTROL_START | CONTROL_IRQ_ENABLE);
    assert(vpu.read32(STATUS) & STATUS_ERROR);
    assert(vpu.read32(ERROR_CODE) == static_cast<std::uint32_t>(model::VpuError::InvalidConfig));

    // A valid job with an intentionally tiny output buffer reports overflow.
    vpu.write32(CONTROL, CONTROL_SOFT_RESET | CONTROL_IRQ_ENABLE);
    write64(vpu, SRC_ADDR_LO, SRC_ADDR_HI, src);
    write64(vpu, DST_ADDR_LO, DST_ADDR_HI, dst);
    vpu.write32(DST_CAPACITY, 8);
    vpu.write32(WIDTH, width);
    vpu.write32(HEIGHT, height);
    vpu.write32(STRIDE_Y, width);
    vpu.write32(FRAME_COUNT, 1);
    vpu.write32(INPUT_FORMAT, FORMAT_YUV420P8);
    vpu.write32(CONTROL, CONTROL_START | CONTROL_IRQ_ENABLE);
    assert(vpu.step());
    assert(vpu.read32(STATUS) & STATUS_ERROR);
    assert(vpu.read32(ERROR_CODE) ==
           static_cast<std::uint32_t>(model::VpuError::OutputOverflow));

    // An out-of-range input address is a DMA read error, not a crash.
    vpu.write32(CONTROL, CONTROL_SOFT_RESET | CONTROL_IRQ_ENABLE);
    write64(vpu, SRC_ADDR_LO, SRC_ADDR_HI, 0xfF00);
    write64(vpu, DST_ADDR_LO, DST_ADDR_HI, dst);
    vpu.write32(DST_CAPACITY, 0x8000);
    vpu.write32(WIDTH, width);
    vpu.write32(HEIGHT, height);
    vpu.write32(STRIDE_Y, width);
    vpu.write32(FRAME_COUNT, 1);
    vpu.write32(INPUT_FORMAT, FORMAT_YUV420P8);
    vpu.write32(CONTROL, CONTROL_START | CONTROL_IRQ_ENABLE);
    assert(vpu.step());
    assert(vpu.read32(STATUS) & STATUS_ERROR);
    assert(vpu.read32(ERROR_CODE) ==
           static_cast<std::uint32_t>(model::VpuError::DmaRead));

    std::cout << "MMIO/DMA/IRQ tests passed\n";
    return 0;
}
