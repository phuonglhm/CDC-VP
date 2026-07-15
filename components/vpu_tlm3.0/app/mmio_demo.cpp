#include "model/vpu_mmio.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Options {
    std::string input;
    std::string output;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t frames = 0;
    std::uint32_t qp = 26;
    std::uint32_t mode = model::vpu_reg::MODE_PCM;
    std::uint32_t fifo_depth = 4;
};

Options parse(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&]() -> const char* {
            if (++i >= argc) throw std::invalid_argument("missing value after " + arg);
            return argv[i];
        };
        if (arg == "-i" || arg == "--input") o.input = next();
        else if (arg == "-o" || arg == "--output") o.output = next();
        else if (arg == "-w" || arg == "--width") o.width = std::stoul(next());
        else if (arg == "-h" || arg == "--height") o.height = std::stoul(next());
        else if (arg == "-n" || arg == "--frames") o.frames = std::stoul(next());
        else if (arg == "--qp") o.qp = std::stoul(next());
        else if (arg == "--fifo-depth") {
            o.fifo_depth = std::stoul(next());
            if (o.fifo_depth == 0 || o.fifo_depth > 255) {
                throw std::invalid_argument("--fifo-depth must be 1..255");
            }
        }
        else if (arg == "--mode") {
            const std::string mode = next();
            if (mode == "pcm") o.mode = model::vpu_reg::MODE_PCM;
            else if (mode == "intra-dc") o.mode = model::vpu_reg::MODE_INTRA_DC;
            else if (mode == "hybrid-dc") o.mode = model::vpu_reg::MODE_HYBRID_DC;
            else if (mode == "intra-dc-tq") o.mode = model::vpu_reg::MODE_INTRA_DC_TQ;
            else if (mode == "intra-full-tq") o.mode = model::vpu_reg::MODE_INTRA_FULL_TQ;
            else if (mode == "intra-full-tq16") o.mode = model::vpu_reg::MODE_INTRA_FULL_TQ16;
            else if (mode == "intra-adaptive-tq") o.mode = model::vpu_reg::MODE_INTRA_ADAPTIVE_TQ;
            else if (mode == "intra-directional-tq") o.mode = model::vpu_reg::MODE_INTRA_DIRECTIONAL_TQ;
            else throw std::invalid_argument(
                "--mode must be pcm, intra-dc, hybrid-dc, intra-dc-tq, "
                "intra-full-tq, intra-full-tq16, intra-adaptive-tq or "
                "intra-directional-tq");
        }
        else throw std::invalid_argument("unknown option: " + arg);
    }
    if (o.input.empty() || o.output.empty() || !o.width || !o.height || !o.frames) {
        throw std::invalid_argument("-i, -o, -w, -h and -n are required");
    }
    return o;
}

void write64(model::VpuMmioDevice& vpu, std::uint32_t lo_register,
             std::uint32_t hi_register, std::uint64_t value) {
    vpu.write32(lo_register, static_cast<std::uint32_t>(value));
    vpu.write32(hi_register, static_cast<std::uint32_t>(value >> 32));
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto options = parse(argc, argv);
        std::ifstream input(options.input, std::ios::binary);
        if (!input) throw std::runtime_error("cannot open raw input");
        const std::vector<std::uint8_t> raw{
            std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        const std::size_t expected = static_cast<std::size_t>(options.width) *
                                     options.height * 3 / 2 * options.frames;
        if (raw.size() < expected) throw std::runtime_error("raw input is truncated");

        constexpr std::uint64_t src = 0x1000;
        const std::uint64_t dst = (src + expected + 0xfffU) & ~0xfffULL;
        const std::uint32_t capacity = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(expected * 2 + 1024 * 1024,
                                    std::numeric_limits<std::uint32_t>::max()));
        model::FlatMemory memory(static_cast<std::size_t>(dst) + capacity + 4096);
        if (!memory.write(src, std::span(raw).first(expected))) {
            throw std::runtime_error("failed to stage input DMA buffer");
        }

        unsigned irq_edges = 0;
        model::VpuMmioDevice vpu(memory, [&](bool level) {
            ++irq_edges;
            std::cout << "IRQ=" << level << '\n';
        });
        using namespace model::vpu_reg;
        write64(vpu, SRC_ADDR_LO, SRC_ADDR_HI, src);
        write64(vpu, DST_ADDR_LO, DST_ADDR_HI, dst);
        vpu.write32(DST_CAPACITY, capacity);
        vpu.write32(WIDTH, options.width);
        vpu.write32(HEIGHT, options.height);
        vpu.write32(STRIDE_Y, options.width);
        vpu.write32(FRAME_COUNT, options.frames);
        vpu.write32(QP, options.qp);
        vpu.write32(INPUT_FORMAT, FORMAT_YUV420P8);
        vpu.write32(ENCODER_MODE, options.mode);
        const auto fifo_config = options.fifo_depth |
            (options.fifo_depth << 8) | (options.fifo_depth << 16) |
            (options.fifo_depth << 24);
        vpu.write32(FIFO_CONFIG, fifo_config);
        vpu.write32(CONTROL, CONTROL_IRQ_ENABLE | CONTROL_START);
        vpu.run_to_completion();

        const auto status = vpu.read32(STATUS);
        if ((status & STATUS_DONE) == 0 || (status & STATUS_ERROR)) {
            throw std::runtime_error("VPU failed, error=" +
                std::to_string(vpu.read32(ERROR_CODE)));
        }
        const auto output_size = vpu.read32(BITSTREAM_BYTES);
        std::vector<std::uint8_t> bitstream(output_size);
        if (!memory.read(dst, bitstream)) throw std::runtime_error("failed to read output DMA");
        std::ofstream output(options.output, std::ios::binary);
        output.write(reinterpret_cast<const char*>(bitstream.data()), bitstream.size());
        if (!output) throw std::runtime_error("failed to write bitstream file");

        const std::uint64_t cycles =
            (static_cast<std::uint64_t>(vpu.read32(CYCLES_HI)) << 32) |
            vpu.read32(CYCLES_LO);
        const auto stalls = static_cast<std::uint64_t>(vpu.read32(STALL_INPUT_FULL)) +
            vpu.read32(STALL_PREDICTION_FULL) +
            vpu.read32(STALL_TRANSFORM_FULL) +
            vpu.read32(STALL_CABAC_FULL);
        std::cout << "MMIO encode complete: frames=" << vpu.read32(FRAMES_DONE)
                  << ", bytes=" << output_size << ", cycles=" << cycles
                  << ", fifo_stalls=" << stalls
                  << ", fifo_peak=0x" << std::hex
                  << vpu.read32(FIFO_MAX_OCCUPANCY) << std::dec
                  << ", irq_edges=" << irq_edges << '\n'
                  << "FIFO active: dma_read=" << vpu.read32(DMA_READ_ACTIVE)
                  << ", prediction=" << vpu.read32(PREDICTION_ACTIVE)
                  << ", transform=" << vpu.read32(TRANSFORM_ACTIVE)
                  << ", cabac=" << vpu.read32(CABAC_ACTIVE)
                  << ", dma_write=" << vpu.read32(DMA_WRITE_ACTIVE) << '\n'
                  << "FIFO stalls: input=" << vpu.read32(STALL_INPUT_FULL)
                  << ", prediction=" << vpu.read32(STALL_PREDICTION_FULL)
                  << ", transform=" << vpu.read32(STALL_TRANSFORM_FULL)
                  << ", cabac=" << vpu.read32(STALL_CABAC_FULL) << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
