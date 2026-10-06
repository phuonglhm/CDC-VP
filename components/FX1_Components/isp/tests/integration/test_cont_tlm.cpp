// SPDX-License-Identifier: Apache-2.0
// Continuous operation (gate G-CONT, DEC-36): replays the fixed-seed script
// of tools/gen_sequence.py over TLM, as a driver would. Between bursts the
// configuration changes while the pipeline is idle; each burst submits 1-3
// frames through the 4-buffer rotation, with soft resets, AXI read/write
// errors, underrun and overflow stalls injected as the script says. Every
// completed frame is compared bit for bit with the Python reference, which
// processed the frames in the same order including the effects of the
// aborted ones; after every burst the statistics (through every readout
// mux), the frame counters and the error state are checked. A wait that
// times out is reported as a deadlock.
// Usage: fx1_isp_test_cont_tlm <sequence dir>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include "fx1_check.h"
#include "fx1_isp/fx1_isp_csr.h"
#include "fx1_isp/fx1_isp_tlm.h"
#include "registers/csr_desc.h"
#include "test_memory.h"
#include "vector_io.h"

using namespace sc_core;
using namespace cdc::components::fx1_isp;
namespace fs = std::filesystem;

namespace {

constexpr std::uint64_t mem_base = 0x8000'0000ull;
constexpr std::size_t mem_size = 8u << 20;
constexpr std::uint32_t in_stride = 2048, out_stride = 1024;  // fit every geometry of the script
std::uint64_t in_slot(unsigned k) { return mem_base + 0x100000ull * k; }
std::uint64_t y_slot(unsigned k) { return mem_base + 0x400000ull + 0x80000ull * k; }
std::uint64_t uv_slot(unsigned k) { return mem_base + 0x600000ull + 0x40000ull * k; }

constexpr std::uint32_t err_align = FX1_ISP_DMA_ERR_ERR_ALIGN_OR_GEOMETRY_BIT;
constexpr std::uint32_t err_underrun = FX1_ISP_DMA_ERR_ERR_IDMA_UNDERRUN_BIT;
constexpr std::uint32_t err_overflow = FX1_ISP_DMA_ERR_ERR_ODMA_OVERFLOW_BIT;
constexpr std::uint32_t err_idma_axi = FX1_ISP_DMA_ERR_ERR_IDMA_AXI_BIT;
constexpr std::uint32_t err_odma_axi = FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT;

std::vector<std::uint8_t> read_file(const fs::path &p) {
   std::ifstream f(p, std::ios::binary);
   return {std::istreambuf_iterator<char>(f), {}};
}

struct expect_entry {
   std::string file;
   std::uint32_t w, h;
};

struct bench : sc_module {
   tlm_utils::simple_initiator_socket<bench> socket;
   sc_out<bool> rst_n;
   fx1_isp_tlm *dut = nullptr;
   fx1_test::test_memory *mem = nullptr;
   fs::path dir;
   unsigned idma_head = 0, odma_head = 0;  // software's view of the rotation
   std::uint32_t geom_w = 0, geom_h = 0;
   bool started = false;
   std::size_t bursts = 0, frames_compared = 0, deadlocks = 0;
   std::string where;

   SC_HAS_PROCESS(bench);
   explicit bench(sc_module_name n) : sc_module(n), socket("socket"), rst_n("rst_n") { SC_THREAD(run); }

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
      FX1_CHECK(t.is_response_ok());
      wait(d);
      std::memcpy(&v, b, 4);
      return v;
   }
   void write32(std::uint32_t a, std::uint32_t v) { access(tlm::TLM_WRITE_COMMAND, a, v); }
   std::uint32_t read32(std::uint32_t a) { return access(tlm::TLM_READ_COMMAND, a, 0); }
   const csr::reg_desc *reg(const std::string &name) {
      for (std::size_t i = 0; i < csr::num_registers; ++i) {
         if (name == csr::registers[i].name) {
            return &csr::registers[i];
         }
      }
      FX1_CHECK_EQ_CTX(1, 0, "unknown register " + name);
      return nullptr;
   }

   bool wait_until(const std::function<bool()> &cond, const std::string &what, sc_time timeout = sc_time(50, SC_MS)) {
      const sc_time end = sc_time_stamp() + timeout;
      while (!cond()) {
         if (sc_time_stamp() >= end) {
            FX1_CHECK_EQ_CTX(0, 1, where + ": deadlock waiting for " + what);
            ++deadlocks;
            return false;
         }
         wait(100, SC_NS);
      }
      return true;
   }
   std::uint32_t odma_count() { return read32(FX1_ISP_ODMA_FRAME_COUNT_OFFSET); }

   void load(unsigned slot, const std::string &file) {
      const std::vector<std::uint8_t> raw = read_file(dir / file);
      FX1_CHECK_EQ_CTX(raw.size(), 2ull * geom_w * geom_h, where + " input size " + file);
      for (std::uint32_t y = 0; y < geom_h; ++y) {
         std::memcpy(mem->at(in_slot(slot) + std::uint64_t{y} * in_stride), raw.data() + 2ull * y * geom_w, 2u * geom_w);
      }
   }
   void submit(unsigned k) {  // input buffer idma_head + k, output buffer odma_head + k
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 1u << ((odma_head + k) % 4u));
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 1u << ((idma_head + k) % 4u));
   }
   // Compares the output buffer at the software head with the next expected frame.
   void compare_done(const expect_entry &e) {
      const std::vector<std::uint8_t> want = read_file(dir / e.file);
      std::size_t bad = 0;
      for (std::uint32_t y = 0; y < e.h; ++y) {
         const std::uint8_t *got = mem->at(y_slot(odma_head) + std::uint64_t{y} * out_stride);
         for (std::uint32_t x = 0; x < e.w; ++x) {
            bad += got[x] != want[std::size_t{y} * e.w + x];
         }
      }
      for (std::uint32_t y = 0; y < e.h / 2; ++y) {
         const std::uint8_t *got = mem->at(uv_slot(odma_head) + std::uint64_t{y} * out_stride);
         for (std::uint32_t x = 0; x < e.w; ++x) {
            bad += got[x] != want[std::size_t{e.w} * e.h + std::size_t{y} * e.w + x];
         }
      }
      FX1_CHECK_EQ_CTX(bad, 0, where + " output " + e.file);
      write32(FX1_ISP_ODMA_BUF_DONE_OFFSET, 1u << odma_head);  // recycle (HAS §6.25.10.2)
      odma_head = (odma_head + 1) % 4u;
      ++frames_compared;
   }
   // Waits until `n` more frames than already compared are DONE and compares
   // them in order (the counter may wrap: uint32 differences).
   void collect(std::vector<expect_entry> &exp, std::size_t &next, std::size_t n) {
      for (std::size_t i = 0; i < n; ++i) {
         if (!wait_until([&] { return static_cast<std::uint32_t>(odma_count() - odma_seen) >= 1u; }, "frame done")) {
            return;
         }
         compare_done(exp[next++]);
         ++odma_seen;
      }
   }
   std::uint32_t odma_seen = 0;

   void start_engines() {
      write32(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_ISP_EN_MASK);
      write32(FX1_ISP_COMMON_IRQ_EN_OFFSET, 0xFFFFFFFFu);
      write32(FX1_ISP_DMA_IRQ_EN_OFFSET, 0xFFFFFFFFu);
      write32(FX1_ISP_IDMA_STRIDE_OFFSET, in_stride);
      write32(FX1_ISP_ODMA_Y_STRIDE_OFFSET, out_stride);
      write32(FX1_ISP_ODMA_UV_STRIDE_OFFSET, out_stride);
      for (unsigned k = 0; k < 4; ++k) {
         write32(FX1_ISP_IDMA_BUF_ADDR_L0_OFFSET + 8u * k, static_cast<std::uint32_t>(in_slot(k)));
         write32(FX1_ISP_ODMA_Y_ADDR_L0_OFFSET + 16u * k, static_cast<std::uint32_t>(y_slot(k)));
         write32(FX1_ISP_ODMA_UV_ADDR_L0_OFFSET + 16u * k, static_cast<std::uint32_t>(uv_slot(k)));
      }
      write32(FX1_ISP_DMA_CTRL_OFFSET, 0x3F03);
      started = true;
   }

   void run_burst(std::size_t n, const std::string &event, const std::vector<std::string> &inputs,
                  std::vector<expect_entry> &exp, const std::string &stats, std::uint32_t idma_want,
                  std::uint32_t odma_want) {
      std::size_t next = 0;
      odma_seen = odma_count();
      for (std::size_t k = 0; k < n; ++k) {
         load((idma_head + k) % 4u, inputs[k]);
      }
      write32(FX1_ISP_DMA_IRQ_STAT_OFFSET, 0xFFFFFFFFu);
      write32(FX1_ISP_COMMON_IRQ_STATUS_OFFSET, 0xFFFFFFFFu);
      if (event == "none") {
         for (std::size_t k = 0; k < n; ++k) {
            submit(static_cast<unsigned>(k));
         }
         collect(exp, next, n);
      } else if (event == "underrun") {
         // Idle with IDMA_EN set: the underrun condition holds, so W1C cannot clear it (CSR-06).
         FX1_CHECK_EQ_CTX(read32(FX1_ISP_DMA_ERR_OFFSET) & err_underrun, err_underrun, where + " underrun level");
         write32(FX1_ISP_DMA_ERR_OFFSET, err_underrun);
         FX1_CHECK_EQ_CTX(read32(FX1_ISP_DMA_ERR_OFFSET) & err_underrun, err_underrun, where + " underrun held");
         for (std::size_t k = 0; k < n; ++k) {
            submit(static_cast<unsigned>(k));
         }
         collect(exp, next, n);
         // The condition dropped while frames were available and rose again: one new edge.
         FX1_CHECK(read32(FX1_ISP_DMA_IRQ_STAT_OFFSET) & FX1_ISP_DMA_IRQ_STAT_IRQ_IDMA_UNDERRUN_BIT);
      } else if (event == "overflow") {
         // Only the first output buffer is free: the pipeline stalls losslessly (DEC-18).
         const std::uint32_t base = odma_count();
         write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 1u << odma_head);
         for (std::size_t k = 0; k < n; ++k) {
            write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 1u << ((idma_head + k) % 4u));
         }
         wait_until([&] { return (read32(FX1_ISP_DMA_ERR_OFFSET) & err_overflow) != 0; }, "overflow");
         wait(20, SC_US);
         FX1_CHECK_EQ_CTX(odma_count(), base + 1, where + " stalled after one frame");
         // Stalled, not idle: the pixel blocks still hold the waiting frame (DEC-38).
         FX1_CHECK_EQ_CTX(read32(FX1_ISP_DEMOSAIC_STATUS_OFFSET) & 1u, 1, where + " DEMOSAIC busy while stalled");
         FX1_CHECK_EQ_CTX(read32(FX1_ISP_RESIZER_STATUS_OFFSET) & 1u, 1, where + " RESIZER busy while stalled");
         collect(exp, next, 1);
         for (std::size_t k = 1; k < n; ++k) {
            write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 1u << ((odma_head + k - 1) % 4u));
         }
         collect(exp, next, n - 1);
         FX1_CHECK(read32(FX1_ISP_DMA_IRQ_STAT_OFFSET) & FX1_ISP_DMA_IRQ_STAT_IRQ_ODMA_OVERFLOW_BIT);
      } else if (event == "soft_reset_common" || event == "soft_reset_dma") {
         const std::uint32_t base = odma_count();
         for (std::size_t k = 0; k < n; ++k) {
            submit(static_cast<unsigned>(k));
         }
         wait_until([&] { return (read32(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK) != 0; },
                    "first frame in the pipeline");
         const std::uint32_t quarter = geom_h / 4u;
         wait(dut->params().core_period * static_cast<double>(geom_w) * static_cast<double>(quarter));  // a quarter in
         FX1_CHECK_EQ_CTX(odma_count(), base, where + " reset inside the first frame");
         if (event == "soft_reset_common") {
            write32(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_ISP_EN_MASK | FX1_ISP_COMMON_CTRL_SOFT_RST_MASK);
         } else {
            write32(FX1_ISP_DMA_CTRL_OFFSET, 0x3F03 | FX1_ISP_DMA_CTRL_SOFT_RESET_MASK);
         }
         wait(dut->params().core_period * 40.0);  // past the 32-cycle window
         FX1_CHECK_EQ_CTX(read32(FX1_ISP_IDMA_BUF_VALID_OFFSET), 0, where + " ownership cleared");
         FX1_CHECK_EQ_CTX(odma_count(), base, where + " counters kept, no late DONE");
         idma_head = odma_head = 0;  // M2-A1: rotation back to buffer 0
         for (std::size_t k = 0; k < n; ++k) {
            load(static_cast<unsigned>(k), inputs[k]);
         }
         for (std::size_t k = 0; k < n; ++k) {
            submit(static_cast<unsigned>(k));
         }
         collect(exp, next, n);
      } else if (event == "idma_axi_error") {
         const std::uint64_t row = in_slot(idma_head) + std::uint64_t{geom_h / 2u} * in_stride;
         mem->faults.push_back({tlm::TLM_READ_COMMAND, row, row + 2u * geom_w, 1});
         for (std::size_t k = 0; k < n; ++k) {
            submit(static_cast<unsigned>(k));
         }
         wait_until([&] { return (read32(FX1_ISP_DMA_ERR_OFFSET) & err_idma_axi) != 0; }, "IDMA AXI error");
         FX1_CHECK_EQ_CTX(read32(FX1_ISP_IDMA_BUF_VALID_OFFSET) & (1u << idma_head), 0, where + " input released");
         write32(FX1_ISP_DMA_ERR_OFFSET, err_idma_axi);
         write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 1u << idma_head);  // DEC-19: re-arm the same index
         collect(exp, next, n);
      } else if (event == "odma_axi_error") {
         const std::uint64_t row = y_slot(odma_head) + std::uint64_t{exp[0].h / 2u} * out_stride;
         mem->faults.push_back({tlm::TLM_WRITE_COMMAND, row, row + exp[0].w, 1});
         for (std::size_t k = 0; k < n; ++k) {
            submit(static_cast<unsigned>(k));
         }
         wait_until([&] { return (read32(FX1_ISP_DMA_ERR_OFFSET) & err_odma_axi) != 0; }, "ODMA AXI error");
         FX1_CHECK_EQ_CTX(read32(FX1_ISP_ODMA_BUF_FREE_OFFSET) & (1u << odma_head), 0, where + " output not FREE");
         FX1_CHECK_EQ_CTX(read32(FX1_ISP_ODMA_BUF_DONE_OFFSET) & (1u << odma_head), 0, where + " output not DONE");
         write32(FX1_ISP_DMA_ERR_OFFSET, err_odma_axi);
         write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 1u << odma_head);  // DEC-19: same index again
         collect(exp, next, n - 1);
         // Resubmit the failed frame: next input buffer, next output buffer.
         const unsigned in_k = (idma_head + static_cast<unsigned>(n)) % 4u;
         idma_head = in_k;
         load(in_k, inputs[0]);
         write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 1u << odma_head);
         write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 1u << in_k);
         idma_head = (in_k + 1) % 4u;
         collect(exp, next, 1);
      } else {
         FX1_CHECK_EQ_CTX(0, 1, where + " unknown event " + event);
      }
      if (event != "odma_axi_error") {
         idma_head = static_cast<unsigned>((idma_head + n) % 4u);
      }
      mem->faults.clear();
      FX1_CHECK_EQ_CTX(next, exp.size(), where + " all expected frames seen");
      // Counters and error state.
      FX1_CHECK_EQ_CTX(read32(FX1_ISP_IDMA_FRAME_COUNT_OFFSET), idma_want, where + " IDMA_FRAME_COUNT");
      FX1_CHECK_EQ_CTX(read32(FX1_ISP_ODMA_FRAME_COUNT_OFFSET), odma_want, where + " ODMA_FRAME_COUNT");
      FX1_CHECK_EQ_CTX(read32(FX1_ISP_DMA_ERR_OFFSET) & (err_align | err_idma_axi | err_odma_axi), 0,
                       where + " no unexpected DMA error");
      write32(FX1_ISP_DMA_ERR_OFFSET, err_overflow);
      FX1_CHECK_EQ_CTX(dut->debug_idma_head(), idma_head, where + " IDMA rotation");
      for (std::uint32_t off : {FX1_ISP_DEMOSAIC_STATUS_OFFSET, FX1_ISP_CCM_STATUS_OFFSET, FX1_ISP_GAMMA_STATUS_OFFSET, FX1_ISP_GTM_STATUS_OFFSET,
           FX1_ISP_NR_2D_STATUS_OFFSET, FX1_ISP_EE_STATUS_OFFSET, FX1_ISP_CNF_STATUS_OFFSET, FX1_ISP_RESIZER_STATUS_OFFSET}) {
         FX1_CHECK_EQ_CTX(read32(off) & 1u, 0, where + " block busy when idle");
      }
      FX1_CHECK_EQ_CTX(dut->debug_odma_head(), odma_head, where + " ODMA rotation");
      // Statistics as software reads them.
      std::string sel;
      for (const fx1_test::csr_line &l : fx1_test::read_expected(dir / stats)) {
         const csr::reg_desc *d = reg(l.reg);
         if (!d) {
            continue;
         }
         if (l.set) {
            write32(d->offset, l.value);
            sel = " @" + l.reg + "=" + std::to_string(l.value);
         } else {
            FX1_CHECK_EQ_CTX(read32(d->offset), l.value, where + " " + l.reg + sel);
         }
      }
      ++bursts;
   }

   void run() {
      rst_n.write(false);
      wait(10, SC_NS);
      rst_n.write(true);
      wait(10, SC_NS);
      std::ifstream f(dir / "sequence.txt");
      std::string line;
      std::size_t lineno = 0;
      while (std::getline(f, line)) {
         ++lineno;
         std::istringstream is(line);
         std::string cmd;
         if (!(is >> cmd) || cmd[0] == '#') {
            continue;
         }
         where = "sequence.txt:" + std::to_string(lineno);
         if (cmd == "write") {
            std::string name, value;
            is >> name >> value;
            if (const csr::reg_desc *d = reg(name)) {
               write32(d->offset, static_cast<std::uint32_t>(std::stoul(value, nullptr, 0)));
            }
         } else if (cmd == "counter") {
            std::string value;
            is >> value;
            const std::uint32_t v = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0));
            dut->debug_set_frame_counter(v);
            dut->debug_hw_write(FX1_ISP_IDMA_FRAME_COUNT_OFFSET, 0xFFFFFFFFu, v);
            dut->debug_hw_write(FX1_ISP_ODMA_FRAME_COUNT_OFFSET, 0xFFFFFFFFu, v);
         } else if (cmd == "burst") {
            if (!started) {
               start_engines();  // HAS §9.1: profile first, then ISP_EN, then the DMA
            }
            std::size_t n = 0;
            std::string event;
            is >> n >> event >> geom_w >> geom_h;
            std::vector<std::string> inputs;
            std::vector<expect_entry> exp;
            std::string stats;
            std::uint32_t idma_want = 0, odma_want = 0;
            while (std::getline(f, line)) {
               ++lineno;
               std::istringstream bs(line);
               std::string c;
               bs >> c;
               if (c == "input") {
                  std::string file;
                  bs >> file;
                  inputs.push_back(file);
               } else if (c == "expect") {
                  expect_entry e;
                  bs >> e.file >> e.w >> e.h;
                  exp.push_back(e);
               } else if (c == "end") {
                  bs >> stats >> idma_want >> odma_want;
                  break;
               }
            }
            where = "burst " + std::to_string(bursts) + " (" + event + ", sequence.txt:" + std::to_string(lineno) + ")";
            run_burst(n, event, inputs, exp, stats, idma_want, odma_want);
            if (deadlocks) {
               break;
            }
         }
      }
      sc_stop();
   }
};

}  // namespace

int sc_main(int argc, char *argv[]) {
   if (argc < 2) {
      std::cerr << "usage: " << argv[0] << " <sequence dir>\n";
      return 2;
   }
   sc_signal<bool> rst_n("rst_n");
   sc_signal<bool> irq("irq");
   fx1_isp_tlm dut("isp", fx1_isp_params{});
   fx1_test::test_memory mem("mem", mem_base, mem_size);
   bench tb("tb");
   tb.dir = argv[1];
   tb.socket.bind(dut.csr_socket);
   dut.idma_socket.bind(mem.idma);
   dut.odma_socket.bind(mem.odma);
   dut.rst_n(rst_n);
   dut.irq(irq);
   tb.rst_n(rst_n);
   tb.dut = &dut;
   tb.mem = &mem;
   sc_start();
   FX1_CHECK_EQ(tb.deadlocks, 0);
   FX1_CHECK(tb.bursts >= 50);
   FX1_CHECK(tb.frames_compared >= 100);
   std::cout << "G-CONT: " << tb.bursts << " bursts, " << tb.frames_compared << " frames compared\n";
   return fx1_test::summary("fx1_isp_test_cont_tlm");
}
