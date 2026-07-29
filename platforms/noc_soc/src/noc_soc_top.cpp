// SPDX-License-Identifier: Apache-2.0

#include "noc_soc_top.h"

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <floo_noc_model/noc_interconnect.h>

#include <clint_tlm.h>
#include <plic_tlm.h>
#include <riscv_vp_wrapper.h>

#include <memory_tlm.h>
#include <uart.h>          // UartTLM                    (global)
#include <i2c.h>           // i2c                        (global)
#include <spi_tlm.h>       // cdc::components::spi_tlm
#include <timer.h>         // cdc::components::Timer
#include <wdt_tlm.h>       // cdc::components::wdt_tlm
#include <pwm.h>           // PWM                        (global)
#include <dma_tlm.h>       // cdc::components::dma_tlm
#include <trng_tlm.h>      // cdc::components::trng_tlm
#include <clkmgr.h>        // Clkmgr                     (global)
#include <dmic.h>          // cdc::components::DmicTLM
#include <otp.h>           // otp                        (global)
#include <qspi_tlm.h>      // cdc::components::qspi_tlm
#include <flash_nor_tlm.h>
#include <rtc_tlm.h>       // cdc::components::rtc_tlm
#include <adc_tlm.h>       // cdc::components::adc_tlm
#include <gpio_tlm.h>      // cdc::components::gpio_tlm

namespace cdc::platforms::noc_soc {
namespace {

using node = cdc::components::noc_interconnect::node;
using cpu_backend_t = cdc::cpu::riscv_vp_cpu;

// ── SoC address map (docs/peripheral_memory_map.md) ─────────────────────────
//
// The bases are the SoC-level assignment, unchanged, so firmware defines and
// existing tests read the same here as on VP_FX1_Full_SoC. What differs is only
// where each peripheral physically sits.
constexpr std::uint64_t kMmio = 0x1000;
constexpr std::uint64_t kUart0 = 0x1000'0000, kI2c0  = 0x1001'0000, kSpi0  = 0x1002'0000;
constexpr std::uint64_t kTimer0= 0x1003'0000, kWdt0  = 0x1004'0000, kPwm0  = 0x1005'0000;
constexpr std::uint64_t kDma0  = 0x1006'0000, kTrng0 = 0x1007'0000, kCmu0  = 0x1008'0000;
constexpr std::uint64_t kDmic0 = 0x100A'0000, kOtp0  = 0x100B'0000, kQspi0 = 0x100C'0000;
constexpr std::uint64_t kUart1 = 0x1010'0000, kI2c1  = 0x1011'0000, kSpi1  = 0x1012'0000;
constexpr std::uint64_t kTimer1= 0x1013'0000, kRtc0  = 0x1014'0000, kAdc0  = 0x1015'0000;
constexpr std::uint64_t kGpio0 = 0x1016'0000;
constexpr std::uint64_t kBootromBase = 0x0000'0000, kBootromSize = 0x1'0000;
constexpr std::uint64_t kClintBase = 0x0200'0000, kClintSize = 0x1'0000;
constexpr std::uint64_t kPlicBase  = 0x0C00'0000, kPlicSize  = 0x40'0000;
constexpr std::uint64_t kRamBase = 0x8000'0000, kRamSize = 0x100'0000;  // 16 MiB
constexpr std::size_t   kFlashSize = 16u * 1024u * 1024u;
constexpr unsigned      kNumPlic = 31;   // sources 1..31, id 0 reserved

// ── Mesh floorplan ─────────────────────────────────────────────────────────
//
// This is the part a flat bus has no equivalent for. On `bus_router` every
// peripheral is equidistant; here the placement decides the latency, so it is a
// design input rather than an implementation detail.
//
// The layout below puts the CPU port at the origin, RAM one hop away because it
// carries the most traffic, the high-rate peripherals near it, and the
// configure-once blocks in the far corner where their cost does not matter.
//
//        x=0            x=1            x=2            x=3
//  y=3   dma *          cmu0 dmic0     rtc0 adc0      probe *
//                                      gpio0
//  y=2   trng0          timer1         i2c1 spi1      uart1
//  y=1   ram            dma0-regs      wdt0 pwm0      timer0
//  y=0   cpu *          uart0 clint    i2c0 spi0      bootrom plic
//                                                     otp0 qspi0
//
//  * = an AXI manager port. No target may share a node with one: the router
//      defaults to `NoLoopback = 1`, so a flit addressed to the node that
//      injected it is undeliverable and would wedge that port for good. The
//      interconnect refuses such a placement at construction.
//
// CLINT sits next to the CPU because every trap entry touches it. The boot ROM
// and the secure blocks share the far corner of the CPU's row.
constexpr node kCpuNode  {0, 0};   // manager
constexpr node kDmaNode  {0, 3};   // manager
constexpr node kProbeNode{3, 3};   // manager
constexpr node kRamNode  {0, 1};
constexpr node kUart0Node{1, 0};   // uart0, clint
constexpr node kSerial0  {2, 0};   // i2c0, spi0
constexpr node kBootNode {3, 0};   // bootrom, plic, otp0, qspi0
constexpr node kDmaRegs  {1, 1};   // the DMA's own register window
constexpr node kCtrlNode {2, 1};   // wdt0, pwm0
constexpr node kTimer0N  {3, 1};
constexpr node kTrngNode {0, 2};
constexpr node kTimer1N  {1, 2};
constexpr node kSerial1  {2, 2};   // i2c1, spi1
constexpr node kUart1Node{3, 2};
constexpr node kClkNode  {1, 3};   // cmu0, dmic0
constexpr node kIoNode   {2, 3};   // rtc0, adc0, gpio0

constexpr unsigned kTargetCount = 23;  // 20 peripherals, CLINT, PLIC, BOOTROM

/// A spin loop, `jal x0, 0`, little endian. Without firmware the CPU would
/// fetch zeroed RAM, trap, and fetch more zeros — a trap storm that says
/// nothing about the interconnect. This gives it a valid instruction to fetch
/// so its traffic is representative.
constexpr std::uint8_t kSpinLoop[] = {0x6F, 0x00, 0x00, 0x00};

/// A benign off-chip SPI peripheral so the controller's initiator is bound.
class spi_dummy : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<spi_dummy> socket;
    explicit spi_dummy(sc_core::sc_module_name name)
        : sc_core::sc_module(name), socket("socket")
    {
        socket.register_b_transport(this, &spi_dummy::b_transport);
    }
private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time&)
    {
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

/// Idle PDM source, so DMIC's stimulus socket is bound.
class pdm_dummy : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<pdm_dummy> socket;
    explicit pdm_dummy(sc_core::sc_module_name name)
        : sc_core::sc_module(name), socket("socket") {}
};

/// One measured access, for the report at the end of the run.
struct probe_result {
    const char* name;
    node where;
    unsigned hops;
    sc_core::sc_time latency;
    /// The interconnect's own share, with the target's access latency removed.
    std::uint64_t network_cycles = 0;
    /// Whether the peripheral accepted the access. A refusal still travels the
    /// network both ways, so the latency is measured either way — it is the
    /// register choice that was wrong, not the interconnect.
    bool accepted = true;
};

/// Walks the whole peripheral map and reports what each access cost.
///
/// On a flat bus every one of these would be identical, which is exactly why
/// the numbers are worth printing here.
class traffic_stub : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<traffic_stub> bus_socket;
    cdc::components::noc_interconnect* noc = nullptr;
    cdc::cpu::cpu_base* cpu = nullptr;

    SC_HAS_PROCESS(traffic_stub);

    explicit traffic_stub(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , bus_socket("bus_socket")
    {
        SC_THREAD(run);
    }

private:
    static unsigned hops(node from, node to)
    {
        const auto dx = from.x > to.x ? from.x - to.x : to.x - from.x;
        const auto dy = from.y > to.y ? from.y - to.y : to.y - from.y;
        return dx + dy;
    }

    sc_core::sc_time access(
        bool write, std::uint64_t address, unsigned char* bytes,
        unsigned length, bool expect_ok = true)
    {
        tlm::tlm_generic_payload trans;
        trans.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
        trans.set_address(address);
        trans.set_data_ptr(bytes);
        trans.set_data_length(length);
        trans.set_streaming_width(length);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        const auto before = sc_core::sc_time_stamp();
        bus_socket->b_transport(trans, delay);
        if (expect_ok && !trans.is_response_ok()) {
            throw std::runtime_error("noc_soc: access failed at address "
                                     + std::to_string(address));
        }
        return sc_core::sc_time_stamp() - before;
    }

    /// A register read, which is what firmware does to a control block.
    ///
    /// The width is per peripheral because the IPs disagree: most want exactly
    /// four bytes, `spi_tlm` rejects anything wider than two. The NoC carries
    /// whichever `AxSIZE` the access needs, so a narrow read stays narrow all
    /// the way to the target.
    sc_core::sc_time probe(
        std::uint64_t base, unsigned width, bool& accepted)
    {
        std::uint64_t value = 0;
        tlm::tlm_generic_payload trans;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(base);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_data_length(width);
        trans.set_streaming_width(width);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        const auto before = sc_core::sc_time_stamp();
        bus_socket->b_transport(trans, delay);
        accepted = trans.is_response_ok();
        return sc_core::sc_time_stamp() - before;
    }

    void run()
    {
        std::vector<unsigned char> buffer(64, 0);

        // Warm-up, not measured: the mesh holds its reset for a few cycles
        // after time zero and the first access would be charged for it.
        access(true, kRamBase + 0x800, buffer.data(), sizeof(std::uint64_t));

        // ── RAM correctness, one beat and a burst ───────────────────────────
        std::uint64_t pattern = 0xCAFE'BABE'DEAD'BEEFull;
        std::memcpy(buffer.data(), &pattern, sizeof(pattern));
        const auto ram_write =
            access(true, kRamBase, buffer.data(), sizeof(pattern));
        std::memset(buffer.data(), 0, buffer.size());
        const auto ram_read =
            access(false, kRamBase, buffer.data(), sizeof(pattern));
        std::uint64_t read_back = 0;
        std::memcpy(&read_back, buffer.data(), sizeof(read_back));
        if (read_back != pattern) {
            throw std::runtime_error("noc_soc: RAM read-back mismatch");
        }

        std::uint64_t burst[4] = {1, 2, 3, 4};
        std::memcpy(buffer.data(), burst, sizeof(burst));
        const auto burst_write =
            access(true, kRamBase + 0x100, buffer.data(), sizeof(burst));
        std::memset(buffer.data(), 0, buffer.size());
        const auto burst_read =
            access(false, kRamBase + 0x100, buffer.data(), sizeof(burst));
        std::uint64_t burst_back[4] = {};
        std::memcpy(burst_back, buffer.data(), sizeof(burst_back));
        for (unsigned beat = 0; beat < 4; ++beat) {
            if (burst_back[beat] != burst[beat]) {
                throw std::runtime_error("noc_soc: burst beat mismatch");
            }
        }

        // ── Every peripheral, one register read each ────────────────────────
        // The register offset and access width are per IP, because the models
        // disagree: `spi_tlm` rejects anything wider than two bytes, and
        // `trng_tlm`'s first valid register is at `0x100`, not zero. The NoC
        // carries whichever `AxSIZE` an access needs, so a narrow read stays
        // narrow all the way to the target.
        const struct {
            const char* name; std::uint64_t base; std::uint64_t offset;
            node where; unsigned width;
        } map[] = {
            {"uart0",  kUart0,  0x000, kUart0Node, 4},
            {"i2c0",   kI2c0,   0x000, kSerial0,   4},
            {"spi0",   kSpi0,   0x000, kSerial0,   2},
            {"timer0", kTimer0, 0x000, kTimer0N,   4},
            {"wdt0",   kWdt0,   0x000, kCtrlNode,  4},
            {"pwm0",   kPwm0,   0x000, kCtrlNode,  4},
            {"dma0",   kDma0,   0x000, kDmaRegs,   4},
            {"trng0",  kTrng0,  0x100, kTrngNode,  4},
            {"cmu0",   kCmu0,   0x000, kClkNode,   4},
            {"dmic0",  kDmic0,  0x000, kClkNode,   4},
            {"otp0",   kOtp0,   0x000, kBootNode,  4},
            {"qspi0",  kQspi0,  0x000, kBootNode,  4},
            {"uart1",  kUart1,  0x000, kUart1Node, 4},
            {"i2c1",   kI2c1,   0x000, kSerial1,   4},
            {"spi1",   kSpi1,   0x000, kSerial1,   2},
            {"timer1", kTimer1, 0x000, kTimer1N,   4},
            {"rtc0",   kRtc0,   0x000, kIoNode,    4},
            {"adc0",   kAdc0,   0x000, kIoNode,    4},
            {"gpio0",  kGpio0,  0x000, kIoNode,    4},
        };

        std::vector<probe_result> results;
        results.reserve(sizeof(map) / sizeof(map[0]));
        for (const auto& entry : map) {
            bool accepted = true;
            const auto latency =
                probe(entry.base + entry.offset, entry.width, accepted);
            results.push_back(probe_result{
                entry.name, entry.where, hops(kProbeNode, entry.where),
                latency,
                noc != nullptr ? noc->last_latency_cycles() : 0,
                accepted});
        }

        // ── An unmapped address must be reported, not routed ────────────────
        std::uint32_t sink = 0;
        tlm::tlm_generic_payload bad;
        bad.set_command(tlm::TLM_READ_COMMAND);
        bad.set_address(0x100D'0000);  // ISP0, reserved and not bound here
        bad.set_data_ptr(reinterpret_cast<unsigned char*>(&sink));
        bad.set_data_length(sizeof(sink));
        bad.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        bus_socket->b_transport(bad, delay);
        if (bad.get_response_status() != tlm::TLM_ADDRESS_ERROR_RESPONSE) {
            throw std::runtime_error(
                "noc_soc: an unmapped address was not reported");
        }

        // Let the CPU run for a while so its fetch traffic is measurable and
        // it contends with this survey for links.
        wait(sc_core::sc_time(5, sc_core::SC_US));

        // ── Report ─────────────────────────────────────────────────────────
        std::cout << "\nnoc_soc: RAM at " << kRamNode.x << ',' << kRamNode.y
                  << " (" << hops(kProbeNode, kRamNode)
                  << " hops from the probe port)\n"
                  << "  write 1 beat   " << ram_write << '\n'
                  << "  read  1 beat   " << ram_read << '\n'
                  << "  write 4 beats  " << burst_write << '\n'
                  << "  read  4 beats  " << burst_read << '\n';

        std::cout << "\nnoc_soc: one 32-bit register read per peripheral\n"
                  << "  hops are from the probe port at (3,3), not the CPU;\n"
                     "  the network column is the interconnect alone, total\n"
                     "  also includes the peripheral's own access latency\n"
                  << "  " << std::left << std::setw(8) << "block"
                  << std::setw(8) << "node" << std::setw(6) << "hops"
                  << std::setw(10) << "network" << "total\n";
        for (const auto& result : results) {
            std::cout << "  " << std::left << std::setw(8) << result.name
                      << std::setw(8)
                      << ('(' + std::to_string(result.where.x) + ','
                          + std::to_string(result.where.y) + ')')
                      << std::setw(6) << result.hops
                      << std::setw(10)
                      << (std::to_string(result.network_cycles) + " cyc")
                      << result.latency
                      << (result.accepted ? "" : "   (IP refused the register,"
                                                 " latency still measured)")
                      << '\n';
        }

        if (noc != nullptr) {
            const auto count = noc->completed_transactions();
            std::cout << "\n  " << count << " transactions, "
                      << noc->total_latency_cycles() << " network cycles";
            if (count != 0) {
                std::cout << ", mean "
                          << static_cast<double>(noc->total_latency_cycles())
                              / static_cast<double>(count) << " cycles";
            }
            std::cout << "\n  mesh clocked " << noc->elapsed_cycles()
                      << " cycles (idle cycles are skipped, which is exact:"
                         " nothing changes state in them)\n";
        }

        // Distance has to show up in the numbers, or the geometry is not being
        // modelled. `uart0` is one hop, `otp0` is four.
        // ── What a real CPU costs on this interconnect ──────────────────────
        //
        // The CPU has been fetching from RAM throughout, one instruction per
        // fetch, every one of them crossing the mesh. This is the number that
        // decides whether a cycle-accurate interconnect is affordable for the
        // workload you have in mind.
        if (cpu != nullptr) {
            const auto retired = cpu->get_instret();
            const auto pc = cpu->get_pc();
            const auto elapsed = sc_core::sc_time_stamp();
            const bool in_ram = pc >= kRamBase && pc < kRamBase + kRamSize;
            const bool in_rom = pc < kBootromBase + kBootromSize;
            const char* region = in_ram ? "RAM" : (in_rom ? "BOOTROM0" : nullptr);

            std::cout << "\n  CPU pc 0x" << std::hex << pc << std::dec
                      << ", retired " << retired << " instructions in "
                      << elapsed << '\n';
            if (region != nullptr && retired != 0) {
                std::cout << "  " << elapsed.to_seconds() * 1e9
                              / static_cast<double>(retired)
                          << " ns per instruction, fetching from " << region
                          << " over the mesh\n";
                if (!in_ram) {
                    // Be precise about what is being measured. Without
                    // firmware the Bremen ISS traps to `mtvec = 0` at startup —
                    // it does the same on `bus_router`, so this is the ISS, not
                    // the interconnect — and then spins on the boot ROM image.
                    // The fetch traffic is real and the figure is a fair cost
                    // per fetch, but it is a trap loop, not a workload.
                    std::cout << "  (a trap loop, not firmware: the ISS traps "
                                 "to mtvec = 0 without --fw,\n"
                                 "   which it also does on bus_router. Pass "
                                 "--fw <elf> for a real workload.)\n";
                }
            } else {
                std::cout << "  the CPU is not executing mapped instructions; "
                             "pass --fw <elf>\n";
            }
        }

        // Compared on the network column, not the total: the total also carries
        // each peripheral's own access latency, which has nothing to do with
        // the floorplan.
        const auto& nearest = results[12];   // uart1 (3,2), 1 hop from (3,3)
        const auto& farthest = results[0];   // uart0 (1,0), 5 hops
        if (farthest.network_cycles <= nearest.network_cycles) {
            throw std::runtime_error(
                "noc_soc: a five-hop peripheral did not cost the network more "
                "than a one-hop one");
        }

        sc_core::sc_stop();
    }
};

} // namespace

struct noc_soc_top::impl : public sc_core::sc_module {
    SC_HAS_PROCESS(impl);

    cpu_backend_t cpu;
    traffic_stub probe;
    cdc::components::noc_interconnect noc;
    cdc::components::memory_tlm ram;
    cdc::components::memory_tlm bootrom;
    cdc::components::clint_tlm clint;
    cdc::components::plic_tlm plic;

    UartTLM uart0, uart1;
    i2c i2c0, i2c1;
    cdc::components::spi_tlm spi0, spi1;
    spi_dummy spi0_peri, spi1_peri;
    cdc::components::Timer timer0, timer1;
    cdc::components::wdt_tlm wdt0;
    PWM pwm0;
    cdc::components::dma_tlm dma0;
    cdc::components::trng_tlm trng0;
    Clkmgr cmu0;
    cdc::components::DmicTLM dmic0;
    pdm_dummy pdm_src;
    otp otp0;
    cdc::components::qspi_tlm qspi0;
    cdc::components::flash_nor_tlm flash0;
    cdc::components::rtc_tlm rtc0;
    cdc::components::adc_tlm adc0;
    cdc::components::gpio_tlm gpio0;

    // ── Signals ────────────────────────────────────────────────────────────
    sc_core::sc_buffer<unsigned char> uart0_tx, uart1_tx;
    sc_core::sc_signal<bool> uart0_irq, uart1_irq;
    sc_core::sc_signal<bool> i2c0_irq, i2c1_irq;
    sc_core::sc_signal<bool> spi0_irq, spi1_irq, spi0_rst, spi1_rst;
    sc_core::sc_signal<bool> timer0_irq, timer1_irq;
    sc_core::sc_signal<bool> timer0_rst, timer1_rst, timer0_ext, timer1_ext;
    sc_core::sc_signal<bool> wdt0_irq, wdt0_rst, wdt0_rsto;
    sc_core::sc_signal<bool> pwm0_out;
    sc_core::sc_signal<std::uint32_t> dma0_irq;
    sc_core::sc_signal<bool> dma0_irq_abort, dma0_rst;
    sc_core::sc_signal<bool> trng0_irq, trng0_rst;
    /// `trng_tlm` requires a clock input but never reads it. A real `sc_clock`
    /// would cost an edge pair per period for nothing, so a static level is
    /// bound instead — the same choice VP_FX1_Full_SoC makes.
    sc_core::sc_signal<bool> trng0_clk;
    sc_core::sc_signal<bool> dmic0_irq, dmic0_rst;
    sc_core::sc_signal<bool> otp0_irq;
    sc_core::sc_signal<bool> qspi0_irq, qspi0_rst;
    sc_core::sc_signal<bool> rtc0_irq, rtc0_rst;
    sc_core::sc_signal<bool> adc0_irq, adc0_rst;
    sc_core::sc_vector<sc_core::sc_signal<bool>> cmu_idle;
    sc_core::sc_signal<bool> cmu_byp_req, cmu_byp_ack;
    /// DMA reports a vector; the PLIC wants a level.
    sc_core::sc_signal<bool> dma0_irq_nonzero;
    /// Reserved PLIC sources.
    sc_core::sc_signal<bool> tie_low;
    std::string firmware_path;

    impl(sc_core::sc_module_name name, const std::string& config_path,
         std::string firmware)
        : sc_core::sc_module(name)
        , cpu("cpu")
        , probe("probe")
        // Three AXI managers: the CPU, the DMA, and the latency survey.
        , noc("noc", 4, 4, kTargetCount, /*num_initiators=*/3)
        , ram("ram", kRamSize)
        , bootrom("bootrom", kBootromSize, /*read_only=*/true)
        , clint("clint", cpu)
        , plic("plic", cpu, kNumPlic)
        , uart0("uart0"), uart1("uart1")
        , i2c0("i2c0"), i2c1("i2c1")
        , spi0("spi0"), spi1("spi1")
        , spi0_peri("spi0_peri"), spi1_peri("spi1_peri")
        , timer0("timer0", 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4,
                 sc_core::sc_time(10, sc_core::SC_NS))
        , timer1("timer1", 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4,
                 sc_core::sc_time(10, sc_core::SC_NS))
        , wdt0("wdt0", sc_core::sc_time(10, sc_core::SC_NS))
        , pwm0("pwm0")
        , dma0("dma0")
        , trng0("trng0")
        , cmu0("cmu0")
        , dmic0("dmic0")
        , pdm_src("pdm_src")
        , otp0("otp0")
        , qspi0("qspi0")
        , flash0("flash0", kFlashSize)
        , rtc0("rtc0")
        , adc0("adc0")
        , gpio0("gpio0")
        , uart0_tx("uart0_tx"), uart1_tx("uart1_tx")
        , trng0_clk("trng0_clk")
        , cmu_idle("cmu_idle", 4)
        , firmware_path(std::move(firmware))
    {
        // ── Upstream ports and their placement ──────────────────────────────
        //
        // Two AXI managers sit on this mesh: the CPU port at the origin and the
        // DMA, which is a real manager and gets its own node rather than being
        // tunnelled through the CPU's. On a flat bus that choice is invisible;
        // here it decides how far the DMA's traffic has to travel and whose
        // links it shares.
        noc.place_initiator(0, kCpuNode);
        noc.place_initiator(1, kDmaNode);
        noc.place_initiator(2, kProbeNode);
        cpu.data_bus().bind(noc.target_socket);   // unified instruction+data
        dma0.master_socket.bind(noc.cpu_port(1));
        probe.bus_socket.bind(noc.cpu_port(2));
        probe.noc = &noc;
        probe.cpu = &cpu;

        // ── Downstream map. The node is the new argument versus `bus_router`.
        noc.add_target(kRamBase, kRamSize, kRamNode).bind(ram.socket);
        noc.add_target(kBootromBase, kBootromSize, kBootNode).bind(bootrom.socket);
        noc.add_target(kClintBase, kClintSize, kUart0Node).bind(clint.socket);
        noc.add_target(kPlicBase, kPlicSize, kBootNode).bind(plic.socket);
        noc.add_target(kUart0, kMmio, kUart0Node).bind(uart0.bus);
        noc.add_target(kI2c0,  kMmio, kSerial0).bind(i2c0.socket);
        noc.add_target(kSpi0,  kMmio, kSerial0).bind(spi0.from_apb_socket);
        noc.add_target(kTimer0,kMmio, kTimer0N).bind(timer0.socket);
        noc.add_target(kWdt0,  kMmio, kCtrlNode).bind(wdt0.target_socket);
        noc.add_target(kPwm0,  kMmio, kCtrlNode).bind(pwm0.socket);
        noc.add_target(kDma0,  kMmio, kDmaRegs).bind(dma0.target_socket);
        noc.add_target(kTrng0, kMmio, kTrngNode).bind(trng0.socket);
        noc.add_target(kCmu0,  kMmio, kClkNode).bind(cmu0.socket);
        noc.add_target(kDmic0, kMmio, kClkNode).bind(dmic0.bus_target_socket);
        noc.add_target(kOtp0,  kMmio, kBootNode).bind(otp0.socket);
        noc.add_target(kQspi0, kMmio, kBootNode).bind(qspi0.from_apb_socket);
        noc.add_target(kUart1, kMmio, kUart1Node).bind(uart1.bus);
        noc.add_target(kI2c1,  kMmio, kSerial1).bind(i2c1.socket);
        noc.add_target(kSpi1,  kMmio, kSerial1).bind(spi1.from_apb_socket);
        noc.add_target(kTimer1,kMmio, kTimer1N).bind(timer1.socket);
        noc.add_target(kRtc0,  kMmio, kIoNode).bind(rtc0.socket);
        noc.add_target(kAdc0,  kMmio, kIoNode).bind(adc0.socket);
        noc.add_target(kGpio0, kMmio, kIoNode).bind(gpio0.socket);

        // ── Per-IP secondary sockets and signal outputs ─────────────────────
        uart0.tx(uart0_tx); uart0.irq(uart0_irq);
        uart1.tx(uart1_tx); uart1.irq(uart1_irq);
        SC_METHOD(monitor_uart0); sensitive << uart0_tx; dont_initialize();

        i2c0.irq(i2c0_irq); i2c1.irq(i2c1_irq);

        spi0.to_peri_socket.bind(spi0_peri.socket);
        spi0.irq(spi0_irq); spi0.reset_n(spi0_rst);
        spi1.to_peri_socket.bind(spi1_peri.socket);
        spi1.irq(spi1_irq); spi1.reset_n(spi1_rst);

        timer0.irq_out(timer0_irq); timer0.reset_n(timer0_rst);
        timer0.extin(timer0_ext);
        timer1.irq_out(timer1_irq); timer1.reset_n(timer1_rst);
        timer1.extin(timer1_ext);

        wdt0.irq(wdt0_irq); wdt0.reset_n(wdt0_rst); wdt0.reset_o(wdt0_rsto);
        pwm0.pwm_out(pwm0_out);

        dma0.reset_n(dma0_rst); dma0.irq(dma0_irq);
        dma0.irq_abort(dma0_irq_abort);

        trng0.reset_n(trng0_rst); trng0.irq_out(trng0_irq);
        trng0.clk(trng0_clk);

        for (int index = 0; index < 4; ++index) cmu0.idle_i[index](cmu_idle[index]);
        cmu0.io_clk_byp_req_o(cmu_byp_req);
        cmu0.io_clk_byp_ack_i(cmu_byp_ack);

        dmic0.reset_n(dmic0_rst); dmic0.irq_out(dmic0_irq);
        pdm_src.socket.bind(dmic0.pdm_target_socket);

        otp0.irq_out(otp0_irq);

        qspi0.to_flash_socket.bind(flash0.from_qspi_socket);
        qspi0.irq(qspi0_irq); qspi0.reset_n(qspi0_rst);

        rtc0.irq_out(rtc0_irq); rtc0.reset_n(rtc0_rst);
        adc0.irq_out(adc0_irq); adc0.reset_n(adc0_rst);

        // ── PLIC sources, id = index + 1, same assignment as VP_FX1 ─────────
        SC_METHOD(dma_irq_bridge); sensitive << dma0_irq; dont_initialize();
        plic.irq_in[0](uart0_irq);          // 1  UART0
        plic.irq_in[1](i2c0_irq);           // 2  I2C0
        plic.irq_in[2](spi0_irq);           // 3  SPI0
        plic.irq_in[3](timer0_irq);         // 4  TIMER0
        plic.irq_in[4](wdt0_irq);           // 5  WDT0
        plic.irq_in[5](tie_low);            // 6  PWM0 (no IRQ output)
        plic.irq_in[6](dma0_irq_nonzero);   // 7  DMA0 done
        plic.irq_in[7](dma0_irq_abort);     // 8  DMA0 abort
        plic.irq_in[8](trng0_irq);          // 9  TRNG0
        plic.irq_in[9](tie_low);            // 10 CMU0 (reserved)
        plic.irq_in[10](tie_low);           // 11 PMU0 (not instantiated here)
        plic.irq_in[11](dmic0_irq);         // 12 DMIC0
        plic.irq_in[12](otp0_irq);          // 13 OTP0
        plic.irq_in[13](qspi0_irq);         // 14 QSPI0
        plic.irq_in[14](tie_low);           // 15 ISP0 (reserved)
        plic.irq_in[15](tie_low);           // 16 VPU0 (reserved)
        plic.irq_in[16](tie_low);           // 17 NPU0 (not instantiated here)
        plic.irq_in[17](uart1_irq);         // 18 UART1
        plic.irq_in[18](i2c1_irq);          // 19 I2C1
        plic.irq_in[19](spi1_irq);          // 20 SPI1
        plic.irq_in[20](timer1_irq);        // 21 TIMER1
        plic.irq_in[21](rtc0_irq);          // 22 RTC0
        plic.irq_in[22](adc0_irq);          // 23 ADC0
        for (unsigned source = 23; source < kNumPlic; ++source) {
            plic.irq_in[source](tie_low);   // 24..31 reserved
        }
        tie_low.write(false);

        // ── Static reset levels ────────────────────────────────────────────
        spi0_rst.write(true);   spi1_rst.write(true);
        timer0_rst.write(true); timer1_rst.write(true);
        timer0_ext.write(false); timer1_ext.write(false);
        wdt0_rst.write(true);   dma0_rst.write(true);
        trng0_rst.write(true);  trng0_clk.write(false);
        dmic0_rst.write(true);  qspi0_rst.write(true);
        rtc0_rst.write(true);   adc0_rst.write(true);
        cmu_byp_ack.write(false);
        for (int index = 0; index < 4; ++index) cmu_idle[index].write(false);

        // The spin-loop image goes in during construction, not
        // `start_of_simulation`: the ISS may fetch on the first delta, and a
        // CPU that reaches zeroed memory first takes a trap and never recovers.
        // This is a backdoor load and deliberately does not cross the NoC —
        // the image is not traffic the design would carry, and charging it to
        // the interconnect would corrupt every number reported below.
        if (firmware_path.empty()) {
            for (std::uint64_t offset = 0; offset < 0x2000; offset += 4) {
                ram.load(kSpinLoop, sizeof(kSpinLoop), offset);
                bootrom.load(kSpinLoop, sizeof(kSpinLoop), offset);
            }
        }

        if (!config_path.empty()) {
            std::cout << "noc_soc config: " << config_path << '\n';
            std::cout << "interconnect: cycle-accurate FlooNoC, 4x4 mesh, "
                         "1 ns network clock\n";
            std::cout << "IPs: UARTx2 I2Cx2 SPIx2 TIMERx2 WDT PWM DMA TRNG CMU "
                         "DMIC OTP QSPI(+flash) RTC ADC GPIO | RAM 16 MiB @ "
                         "0x8000_0000 | CLINT + PLIC | RISC-V CPU\n";
        }
    }

    void monitor_uart0()
    {
        std::cout << uart0_tx.read();
        std::cout.flush();
    }

    void dma_irq_bridge() { dma0_irq_nonzero.write(dma0_irq.read() != 0u); }

    /// Firmware, or a spin loop so the CPU fetches something valid.
    ///
    /// This is a backdoor load straight into the memory model. It must not go
    /// through the NoC: the image is not traffic the design would ever carry,
    /// and charging it to the interconnect would corrupt every number the run
    /// reports.
    void start_of_simulation() override
    {
        if (!firmware_path.empty()) {
            // The ELF loader writes through the CPU's bus, so it can only run
            // once bindings resolve.
            cpu.load_elf(firmware_path);
            std::cout << "noc_soc firmware: " << firmware_path << '\n';
        }
    }
};

noc_soc_top::noc_soc_top(
    sc_core::sc_module_name name, std::string config_path, std::string firmware)
    : sc_core::sc_module(name)
    , impl_(new impl("impl", config_path, std::move(firmware)))
{
}

noc_soc_top::~noc_soc_top() = default;

} // namespace cdc::platforms::noc_soc
