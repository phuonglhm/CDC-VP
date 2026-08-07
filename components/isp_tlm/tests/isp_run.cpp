#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <cstdlib>
#include <sstream>

#include <systemc>
#include <tlm>

#include "isp_regmap.h"
#include "isp_tlm.h"
#include "isp_config.h"
#include "tlm_probe.h"
#include "memory_tlm.h"

namespace {

constexpr std::uint32_t CTRL_ENABLE = 1u << 0;
constexpr std::uint32_t CTRL_START = 1u << 1;

void write_json_metadata(const std::string &path,
                         std::uint32_t width,
                         std::uint32_t height,
                         std::uint32_t bit_depth,
                         std::uint32_t bayer_pattern,
                         std::uint32_t csc_standard,
                         const std::string &source_raw,
                         const std::string &format = "yuv420p") {
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
   case 0:
      f << "RGGB";
      break;
   case 1:
      f << "GRBG";
      break;
   case 2:
      f << "BGGR";
      break;
   case 3:
      f << "GBRG";
      break;
   default:
      f << "UNKNOWN";
      break;
   }
   f << "\",\n";
   f << "  \"plane_order\": [\"Y\", \"U\", \"V\"],\n";
   f << "  \"chroma_subsampling\": \"4:2:0\",\n";
   f << "  \"conv_standard\": \"" << (csc_standard == 1 ? "BT.709" : "BT.601") << "\",\n";
   f << "  \"source_raw\": \"" << source_raw << "\"\n";
   f << "}\n";
   f.close();
   std::cout << "Metadata written to: " << path << std::endl;
}

bool ensure_parent_directory(const std::string &path) {
   std::filesystem::path parent = std::filesystem::path(path).parent_path();
   if (parent.empty()) {
      return true;
   }

   std::error_code ec;
   std::filesystem::create_directories(parent, ec);
   if (ec) {
      std::cerr << "Error: Could not create output directory " << parent.string() << ": " << ec.message()
                << std::endl;
      return false;
   }
   return true;
}

} // anonymous namespace

int sc_main(int argc, char *argv[]) {
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
   std::string config_path = "tuning.bin";

   // Canonical output directory (relative to workspace root).
   // If CWD is already inside components/isp_tlm, use output/ there.
   // Otherwise, default to components/isp_tlm/output/.
   auto detect_output_dir = []() -> std::string {
      const char *cwd = std::getenv("PWD");
      if (cwd == nullptr)
         cwd = ".";
      std::string s(cwd);
      if (s.find("components/isp_tlm") != std::string::npos) {
         return "output";
      }
      return "components/isp_tlm/output";
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
         std::cout << "  -o <path>    Output YUV file path (default: output/output.yuv)\n";
         std::cout << "  -w <width>   Image width in pixels (required)\n";
         std::cout << "  --height <h> Image height in pixels (required)\n";
         std::cout << "  -b <bits>    Bit depth (default: 12)\n";
         std::cout << "  -p <0-3>     Bayer pattern: 0=RGGB, 1=GRBG, 2=BGGR, 3=GBRG (default: 0)\n";
         std::cout << "  -c <path>    IQ Tuning config binary (default: tuning.bin)\n";
         std::cout << "\nExample:\n";
         std::cout << "  " << argv[0] << " -i input.raw -c tuning.bin\n";
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
      } else if (arg == "-c" && i + 1 < argc) {
         config_path = argv[++i];
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

   cdc::components::memory_tlm dram("dram", 128 * 1024 * 1024);
   isp.dma_socket.bind(dram.socket);

   reset_n.write(true);
   sc_start(SC_ZERO_TIME);

   // Configure DMA Addresses
   std::uint32_t src_addr = 0x81000000;
   std::uint32_t dst_addr = 0x82000000;
   probe.write(REG_SRC_ADDR, &src_addr, 4);
   probe.write(REG_DST_ADDR, &dst_addr, 4);

   // Load and apply IQ Configuration
   cdc::components::isp_iq_config iq_cfg;
   std::ifstream bin_file(config_path, std::ios::binary);
   if (bin_file.is_open()) {
      bin_file.read(reinterpret_cast<char *>(&iq_cfg), sizeof(iq_cfg));
      bin_file.close();
      if (iq_cfg.magic_word == 0x49535021) {
         std::cout << "Loaded IQ config from " << config_path << std::endl;

         // Override global dimensions if CLI did not specify them
         if (width == 0)
            width = iq_cfg.width;
         if (height == 0)
            height = iq_cfg.height;

         probe.write(REG_WIDTH, &iq_cfg.width, 4);
         probe.write(REG_HEIGHT, &iq_cfg.height, 4);
         probe.write(REG_BIT_DEPTH, &iq_cfg.bit_depth, 4);
         probe.write(REG_BAYER_PATTERN, &iq_cfg.bayer_pattern, 4);

         std::uint32_t enable_32;

         // BLC
         enable_32 = iq_cfg.blc_enable;
         probe.write(REG_BLC_ENABLE, &enable_32, 4);
         enable_32 = iq_cfg.blc_linear;
         probe.write(REG_BLC_LINEAR, &enable_32, 4);
         std::uint32_t r_off = iq_cfg.blc_r_offset;
         probe.write(REG_BLC_R_OFFSET, &r_off, 4);
         std::uint32_t gr_off = iq_cfg.blc_gr_offset;
         probe.write(REG_BLC_GR_OFFSET, &gr_off, 4);
         std::uint32_t gb_off = iq_cfg.blc_gb_offset;
         probe.write(REG_BLC_GB_OFFSET, &gb_off, 4);
         std::uint32_t b_off = iq_cfg.blc_b_offset;
         probe.write(REG_BLC_B_OFFSET, &b_off, 4);
         std::uint32_t r_sat = iq_cfg.blc_r_sat;
         probe.write(REG_BLC_R_SAT, &r_sat, 4);
         std::uint32_t gr_sat = iq_cfg.blc_gr_sat;
         probe.write(REG_BLC_GR_SAT, &gr_sat, 4);
         std::uint32_t gb_sat = iq_cfg.blc_gb_sat;
         probe.write(REG_BLC_GB_SAT, &gb_sat, 4);
         std::uint32_t b_sat = iq_cfg.blc_b_sat;
         probe.write(REG_BLC_B_SAT, &b_sat, 4);

         // DPC
         enable_32 = iq_cfg.dpc_enable;
         probe.write(REG_DPC_ENABLE, &enable_32, 4);
         std::uint32_t dpc_thresh = iq_cfg.dpc_thresh;
         probe.write(REG_DPC_THRESH, &dpc_thresh, 4);

         // BNR
         enable_32 = iq_cfg.bnr_enable;
         probe.write(REG_BNR_ENABLE, &enable_32, 4);

         // Demosaic
         enable_32 = iq_cfg.demosaic_enable;
         probe.write(REG_DEMOSAIC_ENABLE, &enable_32, 4);

         // AWB
         enable_32 = iq_cfg.awb_enable;
         probe.write(REG_AWB_ENABLE, &enable_32, 4);

         // WB
         enable_32 = iq_cfg.wb_enable;
         probe.write(REG_WB_ENABLE, &enable_32, 4);

         // CCM
         enable_32 = iq_cfg.ccm_enable;
         probe.write(REG_CCM_ENABLE, &enable_32, 4);

         // AEC
         enable_32 = iq_cfg.aec_enable;
         probe.write(REG_AEC_ENABLE, &enable_32, 4);

         // LSC
         enable_32 = iq_cfg.lsc_enable;
         probe.write(REG_LSC_ENABLE, &enable_32, 4);
         probe.write(REG_LSC_GRID_W, &iq_cfg.lsc_grid_w, 4);
         probe.write(REG_LSC_GRID_H, &iq_cfg.lsc_grid_h, 4);

         // DG
         enable_32 = iq_cfg.dg_enable;
         probe.write(REG_DG_ENABLE, &enable_32, 4);
         probe.write(REG_DG_AUTO, &iq_cfg.dg_auto, 4);
         std::uint32_t dg_gain = iq_cfg.dg_gain;
         probe.write(REG_DG_GAIN, &dg_gain, 4);

         // CSC
         enable_32 = iq_cfg.csc_enable;
         probe.write(REG_CSC_ENABLE, &enable_32, 4);

         // Sharpen
         enable_32 = iq_cfg.sharpen_enable;
         probe.write(REG_SHARPEN_ENABLE, &enable_32, 4);

         // 2DNR
         enable_32 = iq_cfg.twodnr_enable;
         probe.write(REG_2DNR_ENABLE, &enable_32, 4);

         // CCM Matrix
         float ccm_matrix[9] = {iq_cfg.ccm_matrix00, iq_cfg.ccm_matrix01, iq_cfg.ccm_matrix02,
                                iq_cfg.ccm_matrix10, iq_cfg.ccm_matrix11, iq_cfg.ccm_matrix12,
                                iq_cfg.ccm_matrix20, iq_cfg.ccm_matrix21, iq_cfg.ccm_matrix22};
         for (int i = 0; i < 9; ++i) {
            probe.write(REG_CCM_MATRIX00 + i * 4, &ccm_matrix[i], 4);
         }

         // GC
         enable_32 = iq_cfg.gc_enable;
         probe.write(REG_GC_ENABLE, &enable_32, 4);

         // CSE
         enable_32 = iq_cfg.cse_enable;
         probe.write(REG_CSE_ENABLE, &enable_32, 4);
         probe.write(REG_CSE_SAT_GAIN, &iq_cfg.cse_sat_gain, 4);

         // CSC Standard
         std::uint32_t csc_std = iq_cfg.csc_standard;
         probe.write(REG_CSC_STANDARD, &csc_std, 4);

         // YUV420
         enable_32 = iq_cfg.yuv420_enable;
         probe.write(REG_YUV420_ENABLE, &enable_32, 4);

      } else {
         std::cerr << "Warning: Invalid IQ config magic word. Using default parameters." << std::endl;
      }
   } else {
      std::cerr << "Warning: Could not open " << config_path << ", using default parameters." << std::endl;
      // Fallback manual settings
      probe.write(REG_WIDTH, &width, 4);
      probe.write(REG_HEIGHT, &height, 4);
      probe.write(REG_BIT_DEPTH, &bit_depth, 4);
      probe.write(REG_BAYER_PATTERN, &bayer_pattern, 4);
      std::uint32_t enable = 1;
      // probe.write(REG_BLC_ENABLE, &enable, 4);
      // probe.write(REG_DPC_ENABLE, &enable, 4);
      // probe.write(REG_LSC_ENABLE, &enable, 4);
      // probe.write(REG_DG_ENABLE, &enable, 4);
      // probe.write(REG_BNR_ENABLE, &enable, 4);
      probe.write(REG_DEMOSAIC_ENABLE, &enable, 4);
      // probe.write(REG_AWB_ENABLE, &enable, 4);
      // probe.write(REG_WB_ENABLE, &enable, 4);
      // probe.write(REG_CCM_ENABLE, &enable, 4);
      // probe.write(REG_GC_ENABLE, &enable, 4);
      // probe.write(REG_AEC_ENABLE, &enable, 4);
      probe.write(REG_CSC_ENABLE, &enable, 4);
      // probe.write(REG_CSE_ENABLE, &enable, 4);
      // probe.write(REG_SHARPEN_ENABLE, &enable, 4);
      // probe.write(REG_2DNR_ENABLE, &enable, 4);
      probe.write(REG_YUV420_ENABLE, &enable, 4);
   }

   std::uint32_t ctrl = CTRL_ENABLE;
   probe.write(REG_CTRL, &ctrl, 4);

   // Allocate buffers before reading file
   isp.allocate_buffers();

   // Load input image
   std::FILE *fp_in = std::fopen(input_path.c_str(), "rb");
   if (!fp_in) {
      std::cerr << "Error: Could not open input file: " << input_path << std::endl;
      return 1;
   }

   std::size_t expected_bytes = static_cast<std::size_t>(width) * height * 2;
   std::vector<std::uint8_t> tmp_raw(expected_bytes);
   std::size_t read_bytes = std::fread(tmp_raw.data(), 1, expected_bytes, fp_in);
   std::fclose(fp_in);

   // Load raw data into DRAM at translated local address (0x81000000 -> 0x01000000)
   dram.load(tmp_raw.data(), expected_bytes, 0x01000000);

   if (read_bytes < expected_bytes) {
      std::cerr << "Warning: Read only " << read_bytes << " of " << expected_bytes << " expected bytes"
                << std::endl;
   }
   std::cout << "Read " << read_bytes << " bytes from " << input_path << std::endl;

   // Trigger processing
   std::cout << "Triggering ISP pipeline..." << std::endl;
   ctrl = CTRL_ENABLE | CTRL_START;

   probe.write(REG_CTRL, &ctrl, 4);
   sc_start(1, SC_MS); // Run simulation for 1ms to complete processing

   // Write output
   if (!ensure_parent_directory(output_path)) {
      return 1;
   }

   std::FILE *fp_out = std::fopen(output_path.c_str(), "wb");
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
   write_json_metadata(metadata_path, width, height, bit_depth, bayer_pattern, iq_cfg.csc_standard,
                       input_path, "yuv420p");

   return 0;
}
