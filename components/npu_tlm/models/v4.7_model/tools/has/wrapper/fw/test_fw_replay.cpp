// test_fw_replay.cpp -- runs the firmware replay engine (npu_has_fw.c, plain C) on the host, through a system TLM wrapper
// (npu_tlm with npu_tlm_rich_window.patch, model root core_rtl/). The hardware abstraction of the engine is implemented
// with TLM transactions, so the C code that firmware will run is exercised unchanged against the real wrapper.
//
// Build: together with npu_has_fw.c (compiled as C or C++), the wrapper sources (npu_tlm.cpp, bus_router.cpp,
//        memory_tlm.cpp) and the wrapper's test support header tlm_probe.h; include path <core_rtl>, <core_rtl>/driver,
//        <core_rtl>/instrumentation and this directory.
// Run:   test_fw_replay <program.nhp> <dram image> [host-only]
//        <program.nhp> from make_fw_program.py; <dram image> = dram_init.bin of the same program.
//        host-only: give dram_golden.bin as the image; the instructions are skipped, host steps and every output
//        CRC are still executed (checks the stream, the CRC and the upsample code on a whole program in seconds).

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>

#include "bus_router.h"
#include "memory_tlm.h"
#include "npu_tlm.h"
#include "npu_tlm_regmap.h"
#include "tlm_probe.h"

#include "npu_has_fw.h"

namespace
{
    constexpr std::uint64_t kNpuBase = 0x1020'0000ULL;
    constexpr std::uint64_t kRamBase = 0x8000'0000ULL;
    cdc::test::tlm_probe *g_probe = nullptr;
    int g_printed = 0;

    bool read_file(const std::string &p, std::vector<std::uint8_t> &v)
    {
        std::ifstream f(p, std::ios::binary);
        if (!f) return false;
        v.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
        return true;
    }
} // namespace

// ---- hardware abstraction of npu_has_fw.h on TLM ----
extern "C" void npu_hal_write32(std::uint32_t off, std::uint32_t v) { g_probe->write(kNpuBase + off, &v, 4); }
extern "C" std::uint32_t npu_hal_read32(std::uint32_t off)
{
    std::uint32_t v = 0;
    g_probe->read(kNpuBase + off, &v, 4);
    return v;
}
extern "C" void npu_hal_ram_read(std::uint32_t phys, std::uint8_t *dst, std::uint32_t n)
{
    g_probe->debug(tlm::TLM_READ_COMMAND, phys, dst, n);
}
extern "C" void npu_hal_ram_write(std::uint32_t phys, const std::uint8_t *src, std::uint32_t n)
{
    g_probe->debug(tlm::TLM_WRITE_COMMAND, phys, const_cast<std::uint8_t *>(src), n);
}
extern "C" void npu_hal_idle(void) { sc_core::wait(10, sc_core::SC_US); }
extern "C" void npu_hal_report(char kind, std::uint32_t index, int ok, std::uint32_t status, std::uint64_t cycles)
{
    if (!ok || g_printed < 400)
        std::printf("[fw] %s %u %s: status 0x%x, PERF_EXEC_CYCLES %llu\n", kind == 'H' ? "host step" : "instruction", index,
                    ok ? "PASS" : "FAIL", status, (unsigned long long)cycles);
    g_printed++;
    std::fflush(stdout);
}

int sc_main(int argc, char *argv[])
{
    if (argc < 3)
    {
        std::fprintf(stderr, "usage: %s <program.nhp> <dram image> [host-only]\n", argv[0]);
        return 2;
    }
    const bool host_only = argc > 3 && std::strcmp(argv[3], "host-only") == 0;
    std::vector<std::uint8_t> stream_bytes, image;
    if (!read_file(argv[1], stream_bytes) || !read_file(argv[2], image) || stream_bytes.size() < 24)
    {
        std::fprintf(stderr, "cannot read %s or %s\n", argv[1], argv[2]);
        return 2;
    }
    std::vector<std::uint32_t> stream(stream_bytes.size() / 4);
    std::memcpy(stream.data(), stream_bytes.data(), stream.size() * 4);

    using namespace cdc::components;
    const std::uint32_t window = std::uint32_t((image.size() + 4095) & ~std::size_t(4095));
    const std::uint32_t ram_size = std::uint32_t((std::uint64_t(window) + (32u << 20) - 1) & ~std::uint64_t((16u << 20) - 1));
    bus_router bus("bus", 2, 2);
    memory_tlm ram("ram", ram_size);
    npu_tlm npu("npu", sc_core::sc_time(1.25, sc_core::SC_NS));
    cdc::test::tlm_probe probe("probe");
    sc_core::sc_signal<bool> reset_n("reset_n");
    sc_core::sc_signal<bool> irq("irq");
    probe.socket.bind(bus.cpu_port(0));
    npu.master_socket.bind(bus.cpu_port(1));
    bus.add_target(kRamBase, ram.size()).bind(ram.socket);
    bus.add_target(kNpuBase, npu_tlm_reg::MMIO_SIZE).bind(npu.target_socket);
    npu.reset_n(reset_n);
    npu.irq_out(irq);
    ram.load(image.data(), image.size(), 0);
    g_probe = &probe;

    int rc = -100;
    npu_has_result res{};
    sc_core::sc_spawn([&] {
        reset_n.write(false);
        sc_core::wait(10, sc_core::SC_NS);
        reset_n.write(true);
        sc_core::wait(20, sc_core::SC_NS);
        rc = npu_has_replay(stream.data(), std::uint32_t(kRamBase), host_only ? NPU_HAS_REPLAY_HOST_ONLY : 0u, &res);
        sc_core::sc_stop();
    });
    sc_core::sc_start();

    std::printf("[fw] RESULT: %s (rc %d, %u instructions, %u host steps, %u failed, first failed %u, sum of PERF_EXEC_CYCLES %llu%s)\n",
                rc == 0 ? "PASS" : "FAIL", rc, res.instructions, res.host_steps, res.failed, res.first_failed,
                (unsigned long long)res.exec_cycles, host_only ? ", host-only" : "");
    return rc == 0 ? 0 : 1;
}
