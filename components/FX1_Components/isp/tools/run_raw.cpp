// SPDX-License-Identifier: Apache-2.0
// fx1_isp_run_raw: runs RAW frames through the complete FX1 ISP model
// (CSR -> IDMA -> pipeline -> ODMA), the way a driver would, and writes the
// NV12 results.
//
//   fx1_isp_run_raw (--input F.isp16 | --input-list LIST | --synthetic) --width W --height H
//                   --profile p.csrw --out prefix [--frames N] [--queue Q] [--per-frame]
//                   [--stats-script expected_stats.txt] [--check-stats] [--expect-sha256 HEX]
//
// Inputs: --input is an ISP-contract container file (tools/raw_fixture.py
// convert); --input-list names one such file per line, all of the same
// geometry (a real-frame sequence); --synthetic generates the deterministic
// pattern of tools/gen_4k_vectors.py (same integer formula, see
// synthetic_sample()). Frame k uses input k mod (number of inputs); --frames
// defaults to the number of inputs.
//
// Frames go through the 4-buffer rotation with distinct input and output
// buffers. Up to --queue frames (1-4, default 1) are in flight back to back;
// each output is read out after its DONE and the buffer recycled
// (HAS §6.25.10.2). One profile is applied once, before the first frame, so
// temporal state (GTM curve, 2DNR variance, statistics IDs) carries from frame
// to frame.
//
// Outputs:
//   <prefix>.nv12          NV12 of the last frame, active bytes, Y then UV
//   <prefix>.fNNN.nv12     with --per-frame: NV12 of every frame
//   <prefix>.txt           geometry, status and the SHA-256 of every frame
//   <prefix>.stats.txt     with --stats-script: the script's reads after the
//                          last frame ("SET" lines are executed and echoed)
//   <prefix>.events.log    time-stamped interrupt and frame events
// --check-stats fails the run if a read differs from the script's value;
// --expect-sha256 fails it if the last frame's NV12 SHA-256 differs.
//
// DMA_ERR after the run: IDMA_EN stays set, so once the IDMA retires the last
// frame its next head buffer is not valid and it reports IDMA_UNDERRUN
// (DMA_ERR bit 1, level-held; HAS §6.25.8.3, CSR-06). That is the specified
// behaviour of a stream that has run out of input, not an error of the frame.
// Any other DMA_ERR bit is a real failure. Exit status: 0 if every frame
// completed in order, the checks pass and no bit other than IDMA_UNDERRUN is set.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include "fx1_isp/fx1_isp_csr.h"
#include "fx1_isp/fx1_isp_tlm.h"
#include "registers/csr_desc.h"
#include "sha256.h"
#include "test_memory.h"
#include "vector_io.h"

using namespace sc_core;
using cdc::components::fx1_isp::fx1_isp_params;
using cdc::components::fx1_isp::fx1_isp_tlm;
namespace csr = cdc::components::fx1_isp::csr;

namespace {

// Four input and four output buffers, each sized for 3840x2160.
constexpr std::uint64_t mem_base = 0x8000'0000ull;
constexpr std::size_t mem_size = 128u << 20;
constexpr std::uint64_t in_slot_bytes = 0x110'0000ull;  // 17 MB >= 7680 * 2160
constexpr std::uint64_t y_slot_bytes = 0x90'0000ull;    //  9 MB >= 3840 * 2160
constexpr std::uint64_t uv_slot_bytes = 0x48'0000ull;   // 4.5 MB
std::uint64_t in_slot(unsigned k) { return mem_base + in_slot_bytes * k; }
std::uint64_t y_slot(unsigned k) { return mem_base + 4 * in_slot_bytes + y_slot_bytes * k; }
std::uint64_t uv_slot(unsigned k) { return mem_base + 4 * in_slot_bytes + 4 * y_slot_bytes + uv_slot_bytes * k; }

struct options {
   std::vector<std::string> inputs;
   std::string profile, out, stats_script, expect_sha256;
   std::uint32_t width = 0, height = 0, frames = 0, queue = 1;
   bool synthetic = false, check_stats = false, per_frame = false;
};

// Deterministic test pattern, bit-identical to tools/gen_4k_vectors.py
// pattern(): a hashed noise term on a gradient whose sense flips on a 256-pixel
// checkerboard (edges for Demosaic, BPC, 2DNR, EE, AF), 12-bit result.
std::uint16_t synthetic_sample(std::uint32_t x, std::uint32_t y, std::uint32_t w, std::uint32_t h) {
   std::uint32_t k = (x * 0x9E3779B1u) ^ (y * 0x85EBCA77u);
   k ^= k >> 15;
   k *= 0x2C1B3C6Du;
   k ^= k >> 12;
   std::int64_t base = std::int64_t{x} * 3000 / w + std::int64_t{y} * 1000 / h;
   if (((x / 256u) + (y / 256u)) & 1u) {
      base = 4000 - base;
   }
   const std::int64_t v = base + std::int64_t{k & 0xFFu} - 128;
   return static_cast<std::uint16_t>(v < 0 ? 0 : v > 4095 ? 4095 : v);
}

std::uint32_t align16(std::uint32_t v) { return (v + 15u) & ~15u; }

std::string hex(std::uint32_t v) {
   std::ostringstream os;
   os << "0x" << std::hex << v;
   return os.str();
}

const csr::reg_desc *find_register(const std::string &name) {
   for (std::size_t i = 0; i < csr::num_registers; ++i) {
      if (name == csr::registers[i].name) {
         return &csr::registers[i];
      }
   }
   return nullptr;
}

struct runner : sc_module {
   tlm_utils::simple_initiator_socket<runner> socket;
   sc_out<bool> rst_n;
   sc_in<bool> irq;
   fx1_isp_tlm *dut = nullptr;
   fx1_test::test_memory *mem = nullptr;
   options opt;
   int status = 1;
   std::vector<std::string> events;
   unsigned stats_mismatches = 0;

   SC_HAS_PROCESS(runner);
   explicit runner(sc_module_name n) : sc_module(n), socket("socket"), rst_n("rst_n"), irq("irq") {
      SC_THREAD(run);
      SC_METHOD(on_irq);
      sensitive << irq;
      dont_initialize();
   }

   void log(const std::string &what) {
      std::ostringstream os;
      os << std::fixed;
      os.precision(1);
      os << sc_time_stamp().to_seconds() * 1e9 << " ns  " << what;
      events.push_back(os.str());
   }
   void on_irq() {
      log(std::string("irq ") + (irq.read() ? "1" : "0") +
          "  COMMON_IRQ_STATUS=" + hex(dut->debug_peek(FX1_ISP_COMMON_IRQ_STATUS_OFFSET)) +
          " DMA_IRQ_STAT=" + hex(dut->debug_peek(FX1_ISP_DMA_IRQ_STAT_OFFSET)));
   }

   std::uint32_t access(tlm::tlm_command cmd, std::uint32_t addr, std::uint32_t v) {
      unsigned char b[4];
      std::memcpy(b, &v, 4);
      tlm::tlm_generic_payload t;
      t.set_command(cmd);
      t.set_address(addr);
      t.set_data_ptr(b);
      t.set_data_length(4);
      t.set_streaming_width(4);
      sc_time d = SC_ZERO_TIME;
      socket->b_transport(t, d);
      wait(d);
      std::memcpy(&v, b, 4);
      return v;
   }
   void write32(std::uint32_t a, std::uint32_t v) { access(tlm::TLM_WRITE_COMMAND, a, v); }
   std::uint32_t read32(std::uint32_t a) { return access(tlm::TLM_READ_COMMAND, a, 0); }

   bool apply_profile() {
      std::ifstream f(opt.profile);
      if (!f) {
         std::cerr << "cannot open profile " << opt.profile << "\n";
         return false;
      }
      for (const fx1_test::csr_line &l : fx1_test::read_profile(opt.profile)) {
         const csr::reg_desc *d = find_register(l.reg);
         if (!d) {
            std::cerr << "profile: unknown register " << l.reg << "\n";
            return false;
         }
         write32(d->offset, l.value);
      }
      return true;
   }

   bool run_stats_script() {
      std::ofstream out(opt.out + ".stats.txt");
      for (const fx1_test::csr_line &l : fx1_test::read_expected(opt.stats_script)) {
         const csr::reg_desc *d = find_register(l.reg);
         if (!d) {
            std::cerr << "stats script: unknown register " << l.reg << "\n";
            return false;
         }
         if (l.set) {
            write32(d->offset, l.value);
            out << "SET " << l.reg << " " << l.value << "\n";
         } else {
            const std::uint32_t got = read32(d->offset);
            out << l.reg << " " << got << "\n";
            if (opt.check_stats && got != l.value && stats_mismatches++ < 10) {
               std::cout << "  stats mismatch: " << l.reg << " got " << got << " expected " << l.value << "\n";
            }
         }
      }
      return !(opt.check_stats && stats_mismatches);
   }

   // Loads the input of frame k into input buffer `slot`.
   bool load(std::uint32_t k, unsigned slot, std::uint32_t in_stride) {
      std::vector<char> raw;
      if (opt.synthetic) {
         raw.resize(2ull * opt.width * opt.height);
         for (std::uint32_t y = 0; y < opt.height; ++y) {
            for (std::uint32_t x = 0; x < opt.width; ++x) {
               const std::uint16_t v = synthetic_sample(x, y, opt.width, opt.height);
               const std::size_t i = 2ull * (std::size_t{y} * opt.width + x);
               raw[i] = static_cast<char>(v & 0xFFu);
               raw[i + 1] = static_cast<char>(v >> 8);
            }
         }
      } else {
         const std::string &file = opt.inputs[k % opt.inputs.size()];
         std::ifstream in(file, std::ios::binary);
         raw.assign(std::istreambuf_iterator<char>(in), {});
         if (raw.size() != 2ull * opt.width * opt.height) {
            std::cerr << file << ": " << raw.size() << " bytes, expected " << 2ull * opt.width * opt.height << "\n";
            return false;
         }
      }
      for (std::uint32_t y = 0; y < opt.height; ++y) {
         std::memcpy(mem->at(in_slot(slot) + std::uint64_t{y} * in_stride), raw.data() + 2ull * y * opt.width,
                     2u * opt.width);
      }
      return true;
   }

   // Reads out the NV12 of output buffer `slot`; returns its SHA-256.
   std::string read_out(unsigned slot, std::uint32_t ow, std::uint32_t oh, std::uint32_t stride, const std::string &file) {
      fx1_test::sha256 sha;
      std::ofstream f;
      if (!file.empty()) {
         f.open(file, std::ios::binary);
      }
      for (std::uint32_t y = 0; y < oh; ++y) {
         const std::uint8_t *p = mem->at(y_slot(slot) + std::uint64_t{y} * stride);
         sha.update(p, ow);
         if (f) {
            f.write(reinterpret_cast<const char *>(p), ow);
         }
      }
      for (std::uint32_t y = 0; y < oh / 2; ++y) {
         const std::uint8_t *p = mem->at(uv_slot(slot) + std::uint64_t{y} * stride);
         sha.update(p, ow);
         if (f) {
            f.write(reinterpret_cast<const char *>(p), ow);
         }
      }
      return sha.hex();
   }

   void run() {
      rst_n.write(false);
      wait(10, SC_NS);
      rst_n.write(true);
      wait(10, SC_NS);
      const std::uint32_t in_stride = align16(2 * opt.width);
      if (std::uint64_t{in_stride} * opt.height > in_slot_bytes) {
         std::cerr << "geometry " << opt.width << "x" << opt.height << " exceeds the input buffers\n";
         sc_stop();
         return;
      }
      // HAS §9.1 order: geometry and profile, ISP_EN, then the DMA.
      write32(FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, opt.width);
      write32(FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, opt.height);
      if (!apply_profile()) {
         sc_stop();
         return;
      }
      write32(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_ISP_EN_MASK);
      write32(FX1_ISP_COMMON_IRQ_EN_OFFSET, 0xFFFFFFFFu);  // every source on the pin (reserved bits read 0)
      write32(FX1_ISP_DMA_IRQ_EN_OFFSET, 0xFFFFFFFFu);
      const std::uint32_t ow = read32(FX1_ISP_RESIZER_OUT_W_OFFSET), oh = read32(FX1_ISP_RESIZER_OUT_H_OFFSET);
      const std::uint32_t out_stride = align16(ow);
      write32(FX1_ISP_IDMA_STRIDE_OFFSET, in_stride);
      write32(FX1_ISP_ODMA_Y_STRIDE_OFFSET, out_stride);
      write32(FX1_ISP_ODMA_UV_STRIDE_OFFSET, out_stride);
      for (unsigned k = 0; k < 4; ++k) {
         write32(FX1_ISP_IDMA_BUF_ADDR_L0_OFFSET + 8u * k, static_cast<std::uint32_t>(in_slot(k)));
         write32(FX1_ISP_ODMA_Y_ADDR_L0_OFFSET + 16u * k, static_cast<std::uint32_t>(y_slot(k)));
         write32(FX1_ISP_ODMA_UV_ADDR_L0_OFFSET + 16u * k, static_cast<std::uint32_t>(uv_slot(k)));
      }
      sc_time t0 = sc_time_stamp();
      bool dma_on = false;
      std::vector<std::string> shas;
      std::uint32_t submitted = 0, completed = 0;
      bool ok = true;
      const std::uint32_t base = read32(FX1_ISP_ODMA_FRAME_COUNT_OFFSET);
      while (ok && completed < opt.frames) {
         // Keep up to `queue` frames in flight (buffers k mod 4 follow the rotation).
         while (submitted < opt.frames && submitted - completed < opt.queue) {
            const unsigned slot = submitted % 4u;
            if (!load(submitted, slot, in_stride)) {
               ok = false;
               break;
            }
            write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 1u << slot);
            write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 1u << slot);
            log("frame " + std::to_string(submitted) + " submitted, buffer " + std::to_string(slot));
            ++submitted;
         }
         if (!ok) {
            break;
         }
         if (!dma_on) {  // the first buffers are queued before the engines start: no start-up underrun
            t0 = sc_time_stamp();
            write32(FX1_ISP_DMA_CTRL_OFFSET, 0x3F03);
            dma_on = true;
         }
         const sc_time end = sc_time_stamp() + sc_time(1, SC_SEC);
         while (read32(FX1_ISP_ODMA_FRAME_COUNT_OFFSET) - base < completed + 1 && sc_time_stamp() < end) {
            wait(20, SC_US);
         }
         if (read32(FX1_ISP_ODMA_FRAME_COUNT_OFFSET) - base < completed + 1) {
            log("frame " + std::to_string(completed) + " NOT DONE");
            ok = false;
            break;
         }
         const unsigned slot = completed % 4u;
         // In order: the frame just completed must be in the next output buffer.
         if (!(read32(FX1_ISP_ODMA_BUF_DONE_OFFSET) & (1u << slot))) {
            log("frame " + std::to_string(completed) + " DONE missing in buffer " + std::to_string(slot));
            ok = false;
            break;
         }
         char suffix[16];
         std::snprintf(suffix, sizeof suffix, ".f%03u.nv12", completed);
         shas.push_back(read_out(slot, ow, oh, out_stride, opt.per_frame ? opt.out + suffix : std::string()));
         // A driver's interrupt handler: read and acknowledge both groups, recycle the buffer.
         const std::uint32_t common = read32(FX1_ISP_COMMON_IRQ_STATUS_OFFSET);
         const std::uint32_t dma = read32(FX1_ISP_DMA_IRQ_STAT_OFFSET);
         log("frame " + std::to_string(completed) + " done, buffer " + std::to_string(slot) +
             "  COMMON_IRQ_STATUS=" + hex(common) + " DMA_IRQ_STAT=" + hex(dma) +
             " DMA_ERR=" + hex(read32(FX1_ISP_DMA_ERR_OFFSET)) +
             " AEC/AWB/AF_FRAME_ID=" + std::to_string(read32(FX1_ISP_AEC_FRAME_ID_OFFSET)) + "/" +
             std::to_string(read32(FX1_ISP_AWB_FRAME_ID_OFFSET)) + "/" +
             std::to_string(read32(FX1_ISP_AF_FRAME_ID_OFFSET)) + " sha256=" + shas.back().substr(0, 16));
         write32(FX1_ISP_COMMON_IRQ_STATUS_OFFSET, common);
         write32(FX1_ISP_DMA_IRQ_STAT_OFFSET, dma);
         write32(FX1_ISP_ODMA_BUF_DONE_OFFSET, 1u << slot);
         ++completed;
      }
      const bool done = ok && completed == opt.frames;
      const sc_time elapsed = sc_time_stamp() - t0;
      const std::uint32_t err = read32(FX1_ISP_DMA_ERR_OFFSET);
      const std::string digest = completed ? shas.back() : std::string();
      if (completed) {
         read_out((completed - 1) % 4u, ow, oh, out_stride, opt.out + ".nv12");
      }
      const bool sha_ok = opt.expect_sha256.empty() || digest == opt.expect_sha256;
      bool stats_ok = true;
      if (!opt.stats_script.empty()) {
         stats_ok = run_stats_script();
      }
      std::ofstream ev(opt.out + ".events.log");
      for (const std::string &e : events) {
         ev << e << "\n";
      }
      std::ofstream txt(opt.out + ".txt");
      txt << "width " << ow << "\nheight " << oh << "\ncsc_std " << (read32(FX1_ISP_CSC_CTRL_OFFSET) & 1u)
          << "\nframes " << opt.frames << "\ncompleted " << completed << "\nframe_done " << done << "\ndma_err 0x"
          << std::hex << err << std::dec << "\nsimulated_ns " << elapsed.to_seconds() * 1e9 << "\nnv12_sha256 "
          << digest << "\n";
      for (std::size_t k = 0; k < shas.size(); ++k) {
         txt << "frame_sha256_" << k << " " << shas[k] << "\n";
      }
      std::cout << "NV12 " << ow << "x" << oh << ", " << completed << "/" << opt.frames << " frame(s) "
                << (done ? "done" : "NOT DONE") << " (queue " << opt.queue << "), DMA_ERR=0x" << std::hex << err
                << std::dec << ", " << elapsed.to_seconds() * 1e3 << " ms simulated\n";
      const std::uint32_t underrun = FX1_ISP_DMA_ERR_ERR_IDMA_UNDERRUN_BIT;
      if (err & underrun) {
         std::cout << "  DMA_ERR.IDMA_UNDERRUN: expected, IDMA_EN is still set after the last buffer was consumed\n";
      }
      if (err & ~underrun) {
         std::cout << "  DMA_ERR has error bits other than IDMA_UNDERRUN: the run FAILED\n";
      }
      std::cout << "  last NV12 SHA-256 " << digest
                << (opt.expect_sha256.empty() ? "" : sha_ok ? " (as expected)" : " MISMATCH, expected " + opt.expect_sha256)
                << "\n";
      if (opt.check_stats) {
         std::cout << "  statistics: " << stats_mismatches << " mismatching read(s)\n";
      }
      status = done && stats_ok && sha_ok && !(err & ~underrun) ? 0 : 1;
      sc_stop();
   }
};

bool parse(int argc, char **argv, options &o) {
   std::string list;
   for (int i = 1; i < argc; i += 2) {
      const std::string k = argv[i];
      if (k == "--synthetic" || k == "--check-stats" || k == "--per-frame") {  // flags without a value
         (k == "--synthetic" ? o.synthetic : k == "--check-stats" ? o.check_stats : o.per_frame) = true;
         --i;
         continue;
      }
      if (i + 1 >= argc) {
         return false;
      }
      const std::string v = argv[i + 1];
      if (k == "--input") o.inputs.push_back(v);
      else if (k == "--input-list") list = v;
      else if (k == "--profile") o.profile = v;
      else if (k == "--out") o.out = v;
      else if (k == "--stats-script") o.stats_script = v;
      else if (k == "--expect-sha256") o.expect_sha256 = v;
      else if (k == "--width") o.width = static_cast<std::uint32_t>(std::stoul(v));
      else if (k == "--height") o.height = static_cast<std::uint32_t>(std::stoul(v));
      else if (k == "--frames") o.frames = static_cast<std::uint32_t>(std::stoul(v));
      else if (k == "--queue") o.queue = static_cast<std::uint32_t>(std::stoul(v));
      else return false;
   }
   if (!list.empty()) {
      std::ifstream f(list);
      std::string line;
      while (std::getline(f, line)) {
         if (!line.empty() && line[0] != '#') {
            o.inputs.push_back(line);
         }
      }
   }
   if (o.synthetic == !o.inputs.empty() || o.profile.empty() || o.out.empty() || !o.width || !o.height ||
       o.queue < 1 || o.queue > 4) {
      return false;
   }
   if (!o.frames) {
      o.frames = o.synthetic ? 1u : static_cast<std::uint32_t>(o.inputs.size());
   }
   return true;
}

}  // namespace

int sc_main(int argc, char *argv[]) {
   options opt;
   if (!parse(argc, argv, opt)) {
      std::cerr << "usage: fx1_isp_run_raw (--input F.isp16 | --input-list LIST | --synthetic) --width W --height H\n"
                   "                      --profile P.csrw --out PREFIX [--frames N] [--queue 1-4] [--per-frame]\n"
                   "                      [--stats-script EXPECTED_STATS] [--check-stats] [--expect-sha256 HEX]\n";
      return 2;
   }
   sc_signal<bool> rst_n("rst_n");
   sc_signal<bool> irq("irq");
   fx1_isp_tlm dut("isp", fx1_isp_params{});
   fx1_test::test_memory mem("mem", mem_base, mem_size);
   runner r("runner");
   r.opt = opt;
   r.socket.bind(dut.csr_socket);
   dut.idma_socket.bind(mem.idma);
   dut.odma_socket.bind(mem.odma);
   dut.rst_n(rst_n);
   dut.irq(irq);
   r.rst_n(rst_n);
   r.irq(irq);
   r.dut = &dut;
   r.mem = &mem;
   sc_start();
   return r.status;
}
