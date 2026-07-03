#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>

#include <systemc>
#include <tlm>

#include "isp_tlm.h"
#include "tlm_probe.h"

namespace {

void write_json_metadata(const std::string& path,
                        std::uint32_t width,
                        std::uint32_t height,
                        std::uint32_t bit_depth,
                        std::uint32_t bayer_pattern,
                        const std::string& source_raw,
                        const std::string& format = "yuv420p") {
    std::ofstream f(path);
    if (!f.is_open()) {
        std::cerr << "Warning: Could not write metadata to " << path << std::endl;
        return;
    }
    f << "{\n";
    f << "  \"width\": " << width << ",\n";
    f << "  \"height\": " << height << ",\n";
    f << "  \"format\": \"" << format << "\",\n";
    f << "  \"input_bit_depth\": " << bit_depth << ",\n";
    f << "  \"output_bit_depth\": 8,\n";
    f << "  \"bayer_pattern\": " << bayer_pattern << ",\n";
    f << "  \"bayer_pattern_name\": \"";
    switch (bayer_pattern) {
        case 0: f << "RGGB"; break;
        case 1: f << "GRBG"; break;
        case 2: f << "BGGR"; break;
        case 3: f << "GBRG"; break;
        default: f << "UNKNOWN"; break;
    }
    f << "\",\n";
    f << "  \"plane_order\": [\"Y\", \"U\", \"V\"],\n";
    f << "  \"chroma_subsampling\": \"4:2:0\",\n";
    f << "  \"conv_standard\": \"BT.709\",\n";
    f << "  \"source_raw\": \"" << source_raw << "\"\n";
    f << "}\n";
    f.close();
    std::cout << "Metadata written to: " << path << std::endl;
}

} // anonymous namespace

int sc_main(int argc, char* argv[]) {
    using namespace sc_core;
    using namespace cdc::components;
    using namespace cdc::test;

    std::string input_path;
    std::string output_path = "output.yuv";
    std::string metadata_path = "output.yuv.json";
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t bit_depth = 12;
    std::uint32_t bayer_pattern = 0; // 0 = RGGB

    // Canonical output directory (relative to workspace root).
    // If CWD is already inside components/isp_tlm, use that.
    // Otherwise, default to components/isp_tlm/output.yuv.
    auto detect_output_dir = []() -> std::string {
        const char* cwd = std::getenv("PWD");
        if (cwd == nullptr) cwd = ".";
        std::string s(cwd);
        if (s.find("components/isp_tlm") != std::string::npos) {
            return ".";
        }
        return "components/isp_tlm";
    };
    output_path = detect_output_dir() + "/output.yuv";
    metadata_path = output_path + ".json";

    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-?") {
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  -i <path>    Input RAW file path (required)\n";
            std::cout << "  -o <path>    Output YUV file path (default: output.yuv)\n";
            std::cout << "  -w <width>   Image width in pixels (required)\n";
            std::cout << "  --height <h> Image height in pixels (required)\n";
            std::cout << "  -b <bits>    Bit depth (default: 12)\n";
            std::cout << "  -p <0-3>     Bayer pattern: 0=RGGB, 1=GRBG, 2=BGGR, 3=GBRG (default: 0)\n";
            std::cout << "\nExample:\n";
            std::cout << "  " << argv[0] << " -i input.raw -o output.yuv -w 2592 --height 1536 -b 12 -p 0\n";
            return 0;
        } else if (arg == "-i" && i + 1 < argc) {
            input_path = argv[++i];
        } else if (arg == "-o" && i + 1 < argc) {
            output_path = argv[++i];
            metadata_path = output_path + ".json";
        } else if (arg == "-w" && i + 1 < argc) {
            width = static_cast<std::uint32_t>(std::atoi(argv[++i]));
        } else if ((arg == "--height" || arg == "-H") && i + 1 < argc) {
            height = static_cast<std::uint32_t>(std::atoi(argv[++i]));
        } else if (arg == "-b" && i + 1 < argc) {
            bit_depth = static_cast<std::uint32_t>(std::atoi(argv[++i]));
        } else if (arg == "-p" && i + 1 < argc) {
            bayer_pattern = static_cast<std::uint32_t>(std::atoi(argv[++i]));
        }
    }

    // Validate required parameters
    if (input_path.empty()) {
        std::cerr << "Error: Input file (-i) is required" << std::endl;
        return 1;
    }
    if (width == 0 || height == 0) {
        std::cerr << "Error: Width (-w) and height (-h) are required" << std::endl;
        return 1;
    }

    std::cout << "========================================" << std::endl;
    std::cout << "   ISP TLM Pipeline Runner" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Input:  " << input_path << std::endl;
    std::cout << "Output: " << output_path << std::endl;
    std::cout << "Width:  " << width << std::endl;
    std::cout << "Height: " << height << std::endl;
    std::cout << "Bit depth: " << bit_depth << std::endl;
    std::cout << "Bayer pattern: " << bayer_pattern << std::endl;
    std::cout << "========================================" << std::endl;

    // Create ISP module
    isp_tlm isp("isp");
    sc_signal<bool> reset_n("reset_n");
    sc_signal<bool> irq("irq");
    isp.reset_n(reset_n);
    isp.irq_out(irq);

    cdc::test::tlm_probe probe("probe");
    probe.socket.bind(isp.socket);

    reset_n.write(true);
    sc_start(SC_ZERO_TIME);

    // Configure ISP
    probe.write(REG_WIDTH, &width, 4);
    probe.write(REG_HEIGHT, &height, 4);
    probe.write(REG_BIT_DEPTH, &bit_depth, 4);
    probe.write(REG_BAYER_PATTERN, &bayer_pattern, 4);

    // Enable ISP
    std::uint32_t enable = 1;
    probe.write(REG_ISP_ENABLE, &enable, 4);

    // Enable all processing blocks
    probe.write(REG_DEMOSAIC_ENABLE, &enable, 4);
    probe.write(REG_AWB_ENABLE, &enable, 4);
    probe.write(REG_WB_ENABLE, &enable, 4);
    probe.write(REG_CCM_ENABLE, &enable, 4);

    // Set identity CCM
    float identity_ccm[9] = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    for (int i = 0; i < 9; ++i) {
        probe.write(REG_CCM_MATRIX00 + i * 4, &identity_ccm[i], 4);
    }

    // Enable GC with identity LUT
    probe.write(REG_GC_ENABLE, &enable, 4);

    // Enable CSC (BT.709)
    std::uint32_t csc_standard = 1; // BT.709
    probe.write(REG_CSC_STANDARD, &csc_standard, 4);

    // Enable CSE
    probe.write(REG_CSE_ENABLE, &enable, 4);

    // Enable YUV420 output
    probe.write(REG_YUV420_ENABLE, &enable, 4);

    // Allocate buffers before reading file
    isp.allocate_buffers();

    // Load input image
    std::FILE* fp_in = std::fopen(input_path.c_str(), "rb");
    if (!fp_in) {
        std::cerr << "Error: Could not open input file: " << input_path << std::endl;
        return 1;
    }

    std::size_t expected_bytes = static_cast<std::size_t>(width) * height * 2;
    std::size_t read_bytes = std::fread(isp.get_raw_buffer(), 1, expected_bytes, fp_in);
    std::fclose(fp_in);

    if (read_bytes < expected_bytes) {
        std::cerr << "Warning: Read only " << read_bytes << " of " << expected_bytes << " expected bytes" << std::endl;
    }
    std::cout << "Read " << read_bytes << " bytes from " << input_path << std::endl;

    // Trigger processing
    std::cout << "Triggering ISP pipeline..." << std::endl;
    probe.write(REG_TRIGGER, &enable, 4);
    sc_start(1, SC_MS); // Run simulation for 1ms to complete processing

    // Write output
    std::FILE* fp_out = std::fopen(output_path.c_str(), "wb");
    if (!fp_out) {
        std::cerr << "Error: Could not open output file: " << output_path << std::endl;
        return 1;
    }

    // YUV420 size
    std::size_t yuv_size = static_cast<std::size_t>(width) * height * 3 / 2;
    std::fwrite(isp.get_yuv_buffer(), 1, yuv_size, fp_out);
    std::fclose(fp_out);

    std::cout << "Successfully saved YUV output to " << output_path << std::endl;
    std::cout << "Output size: " << yuv_size << " bytes" << std::endl;

    // Write metadata JSON
    write_json_metadata(metadata_path, width, height, bit_depth, bayer_pattern, input_path, "yuv420p");

    // Print ffplay command
    std::cout << "\n========================================" << std::endl;
    std::cout << "To view the output, run:" << std::endl;
    std::cout << "  ffplay -f rawvideo -pixel_format yuv420p -video_size " << width << "x" << height << " " << output_path << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
