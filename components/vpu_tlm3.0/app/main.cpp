#include "model/tlm_pipeline.hpp"

#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

struct Options {
    std::string input;
    std::string output;
    std::string reconstruction;
    unsigned width = 0;
    unsigned height = 0;
    std::uint64_t frames = 0; // 0 means until EOF
    int qp = 26;
    hevc::CodingMode mode = hevc::CodingMode::Pcm;
};

unsigned parse_unsigned(const char* value, const char* name) {
    const auto parsed = std::stoull(value);
    if (parsed > 0xffffffffULL) {
        throw std::invalid_argument(std::string(name) + " is too large");
    }
    return static_cast<unsigned>(parsed);
}

Options parse_options(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&]() -> const char* {
            if (++i >= argc) {
                throw std::invalid_argument("missing value after " + arg);
            }
            return argv[i];
        };
        if (arg == "--input" || arg == "-i") options.input = next();
        else if (arg == "--output" || arg == "-o") options.output = next();
        else if (arg == "--width" || arg == "-w") options.width = parse_unsigned(next(), "width");
        else if (arg == "--height" || arg == "-h") options.height = parse_unsigned(next(), "height");
        else if (arg == "--frames" || arg == "-n") options.frames = std::stoull(next());
        else if (arg == "--qp") options.qp = std::stoi(next());
        else if (arg == "--mode") {
            const std::string mode = next();
            if (mode == "pcm") options.mode = hevc::CodingMode::Pcm;
            else if (mode == "intra-dc") options.mode = hevc::CodingMode::IntraDc;
            else if (mode == "hybrid-dc") options.mode = hevc::CodingMode::HybridDc;
            else if (mode == "intra-dc-tq") options.mode = hevc::CodingMode::IntraDcTq;
            else if (mode == "intra-full-tq") options.mode = hevc::CodingMode::IntraFullTq;
            else if (mode == "intra-full-tq16") options.mode = hevc::CodingMode::IntraFullTq16;
            else if (mode == "intra-adaptive-tq") options.mode = hevc::CodingMode::IntraAdaptiveTq;
            else if (mode == "intra-directional-tq") options.mode = hevc::CodingMode::IntraDirectionalTq;
            else throw std::invalid_argument(
                "--mode must be pcm, intra-dc, hybrid-dc, intra-dc-tq, "
                "intra-full-tq, intra-full-tq16, intra-adaptive-tq or "
                "intra-directional-tq");
        }
        else if (arg == "--recon") options.reconstruction = next();
        else if (arg == "--help") {
            std::cout << "Usage: vpu_tlm3.0 -i input.yuv -w WIDTH -h HEIGHT "
                         "[-n FRAMES] [-o output.h265] [--qp 26] "
                         "[--mode pcm|intra-dc|hybrid-dc|intra-dc-tq|"
                         "intra-full-tq|intra-full-tq16|intra-adaptive-tq|"
                         "intra-directional-tq] "
                         "[--recon recon.yuv]\n";
            std::exit(0);
        } else {
            throw std::invalid_argument("unknown option: " + arg);
        }
    }
    if (options.input.empty() || options.width == 0 || options.height == 0) {
        throw std::invalid_argument("--input, --width and --height are required");
    }
    if (options.output.empty()) options.output = "output.h265";
    return options;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto options = parse_options(argc, argv);
        const hevc::EncoderConfig config{
            options.width, options.height, options.qp, options.mode};
        model::RawYuvDma source(options.input, options.width, options.height);
        model::HevcVpuTarget vpu(config);
        model::AnnexBSink sink(options.output);
        std::ofstream reconstruction;
        if (!options.reconstruction.empty()) {
            reconstruction.open(options.reconstruction, std::ios::binary);
            if (!reconstruction) {
                throw std::runtime_error("cannot open reconstruction output");
            }
        }
        model::TimingStats timing;
        sink.b_transport(vpu.headers(), timing);

        std::uint64_t encoded = 0;
        while (options.frames == 0 || encoded < options.frames) {
            model::FrameTransaction transaction;
            if (!source.transport(encoded, transaction, timing)) break;
            sink.b_transport(vpu.b_transport(transaction, timing), timing);
            if (reconstruction.is_open() &&
                !hevc::write_yuv420_frame(reconstruction, vpu.reconstruct(transaction))) {
                throw std::runtime_error("failed while writing reconstruction");
            }
            ++encoded;
        }
        if (encoded == 0) {
            throw std::runtime_error("input contains no complete YUV420p frame");
        }
        std::cout << "Encoded " << encoded << " frame(s) to " << options.output << "\n"
                  << "Model cycles: DMA=" << timing.dma_cycles
                  << ", VPU=" << timing.encoder_cycles
                  << ", sink=" << timing.sink_cycles << "\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
