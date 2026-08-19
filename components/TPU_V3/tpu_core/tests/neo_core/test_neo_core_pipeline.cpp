// SPDX-License-Identifier: Apache-2.0
//
// The Phase 7 pipeline gate: `DMA -> Transform (Im2Col) -> MXU -> RVV`.
//
// One firmware ELF boots on the composed NEO-CORE and drives every engine
// through MMIO, exactly as the phase's gate asks. Nothing in this file
// programs an engine; the hart does all of it, which is the difference between
// proving the blocks are reachable and proving the core works.
//
// The golden result is computed here, in plain host C++, from the same input
// bytes the firmware is given. It is deliberately *not* computed by calling
// the model's own Im2Col or its GEMM: a comparison against the code under test
// agrees with it by construction. Im2Col is re-derived from the D18 layout
// (`A[oy*OW+ox][c*KH*KW + ky*KW + kx]`) and the matrix product is an ordinary
// triple loop over INT8 operands with INT32 accumulation, which is exact
// integer arithmetic with no tolerance to argue about.
//
// Usage: test_neo_core_pipeline <neo_core_pipeline.elf>
// Exit codes: 0 pass, 1 fail, 77 skip (image not built — no cross toolchain).

#include "tpu_v3/core/tpu_core.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "tpu_v3/address_map.h"

extern "C" {
#include "neo_core_map.h"
}

namespace tpu = cdc::components::tpu_v3;
namespace core = cdc::components::tpu_v3::core;
namespace sram = cdc::components::tpu_v3::sram;
namespace am = cdc::components::tpu_v3::address_map;

using core::local_fabric_timing;
using core::tpu_core;
using core::tpu_core_config;
using sram::neo_requester;

namespace {

int failures = 0;
constexpr int kSkip = 77;

#define CHECK_MSG(cond, msg)                                                  \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::cerr << "CHECK failed: " << (msg) << " @ " << __FILE__       \
                      << ':' << __LINE__ << '\n';                             \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

constexpr tpu::chip_id_t kChip = 0;
constexpr tpu::core_id_t kCore = 0;
constexpr std::uint64_t kCapacity = 64 * 1024;
constexpr double kWatchdogMilliseconds = 20.0;

/// The operands, generated rather than tabulated so the pattern is visible and
/// the values are not all the same sign — a weight set that was all positive
/// would make the ReLU stage untestable.
std::vector<std::int8_t> make_input()
{
    std::vector<std::int8_t> input(INPUT_BYTES);
    for (unsigned i = 0; i < INPUT_BYTES; ++i) {
        input[i] = static_cast<std::int8_t>(static_cast<int>(i * 7u % 13u) - 6);
    }
    return input;
}

std::vector<std::int8_t> make_weights()
{
    std::vector<std::int8_t> weights(WEIGHT_BYTES);
    for (unsigned i = 0; i < WEIGHT_BYTES; ++i) {
        weights[i] = static_cast<std::int8_t>(static_cast<int>(i * 5u % 11u) - 5);
    }
    return weights;
}

struct golden_result {
    std::vector<std::int8_t> a_matrix;
    std::vector<std::int32_t> c_matrix;
    std::uint32_t checksum = 0;
    std::int32_t maximum = 0;
    std::uint32_t negatives = 0;
};

golden_result compute_golden(const std::vector<std::int8_t>& input,
                             const std::vector<std::int8_t>& weights)
{
    golden_result golden;
    golden.a_matrix.assign(A_BYTES, 0);

    // D18's frozen order: rows are output positions, columns run c, ky, kx.
    for (unsigned oy = 0; oy < OUT_HEIGHT; ++oy) {
        for (unsigned ox = 0; ox < OUT_WIDTH; ++ox) {
            const unsigned row = oy * OUT_WIDTH + ox;
            for (unsigned c = 0; c < IN_CHANNELS; ++c) {
                for (unsigned ky = 0; ky < KERNEL_H; ++ky) {
                    for (unsigned kx = 0; kx < KERNEL_W; ++kx) {
                        const unsigned column
                            = c * KERNEL_H * KERNEL_W + ky * KERNEL_W + kx;
                        const unsigned iy = oy + ky;
                        const unsigned ix = ox + kx;
                        golden.a_matrix[row * MAT_K + column]
                            = input[(c * IN_HEIGHT + iy) * IN_WIDTH + ix];
                    }
                }
            }
        }
    }

    golden.c_matrix.assign(MAT_M * MAT_N, 0);
    for (unsigned m = 0; m < MAT_M; ++m) {
        for (unsigned n = 0; n < MAT_N; ++n) {
            std::int32_t sum = 0;
            for (unsigned k = 0; k < MAT_K; ++k) {
                sum += static_cast<std::int32_t>(golden.a_matrix[m * MAT_K + k])
                    * static_cast<std::int32_t>(weights[k * MAT_N + n]);
            }
            golden.c_matrix[m * MAT_N + n] = sum;
        }
    }

    for (std::int32_t value : golden.c_matrix) {
        if (value < 0) {
            ++golden.negatives;
        }
        const std::int32_t relu = value > 0 ? value : 0;
        golden.checksum += static_cast<std::uint32_t>(relu);
        if (relu > golden.maximum) {
            golden.maximum = relu;
        }
    }
    return golden;
}

/// Everything outside the core: boot ROM, global RAM, and the simulator-only
/// host-I/O block the firmware reports through.
///
/// The host-I/O window sits in the unmapped low-address hole, the same place
/// Phase 2 and Phase 4.5 put theirs. It is not a TPU_V3 architectural
/// peripheral and must not leak into the SoC firmware ABI.
class outside_world : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<outside_world> socket;

    std::uint64_t rom_accesses = 0;
    std::uint64_t ram_accesses = 0;
    bool exited = false;
    std::uint32_t sim[64] = {};

    explicit outside_world(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
        , rom_(am::boot_rom_size, 0)
        , ram_(64 * 1024, 0)
    {
        socket.register_b_transport(this, &outside_world::b_transport);
        socket.register_transport_dbg(this, &outside_world::transport_dbg);
    }

    void stage(std::uint64_t address, const void* data, std::size_t length)
    {
        std::memcpy(&ram_[address - am::global_ram_base], data, length);
    }

private:
    bool access(tlm::tlm_generic_payload& trans, bool debug)
    {
        const std::uint64_t address = trans.get_address();
        const unsigned int length = trans.get_data_length();
        const bool write = trans.get_command() == tlm::TLM_WRITE_COMMAND;

        if (address >= SIM_BASE && address < SIM_BASE + sizeof(sim)) {
            const std::size_t index = (address - SIM_BASE) / 4;
            if (write) {
                std::uint32_t value = 0;
                std::memcpy(&value, trans.get_data_ptr(), 4);
                sim[index] = value;
                // `SIM_EXIT_KIND` is the trigger and is written last, so
                // status, cause and mepc are already in place by the time the
                // run stops.
                if (address == SIM_EXIT_KIND) {
                    exited = true;
                    sc_core::sc_stop();
                }
            } else {
                std::memcpy(trans.get_data_ptr(), &sim[index], 4);
            }
            return true;
        }

        if (am::contains(am::boot_rom_base, am::boot_rom_size, address,
                         length)) {
            ++rom_accesses;
            const std::uint64_t offset = address - am::boot_rom_base;
            if (write) {
                // The boot ROM refuses ordinary writes; only the host loader
                // reaches it, through debug transport (`ADDRESS_MAP.md` §6).
                if (!debug) {
                    return false;
                }
                std::memcpy(&rom_[offset], trans.get_data_ptr(), length);
            } else {
                std::memcpy(trans.get_data_ptr(), &rom_[offset], length);
            }
            return true;
        }

        if (am::contains(am::global_ram_base, ram_.size(), address, length)) {
            ++ram_accesses;
            const std::uint64_t offset = address - am::global_ram_base;
            if (write) {
                std::memcpy(&ram_[offset], trans.get_data_ptr(), length);
            } else {
                std::memcpy(trans.get_data_ptr(), &ram_[offset], length);
            }
            return true;
        }
        return false;
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        const bool served = access(trans, /*debug=*/false);
        trans.set_response_status(served ? tlm::TLM_OK_RESPONSE
                                         : tlm::TLM_ADDRESS_ERROR_RESPONSE);
        delay += sc_core::sc_time(1, sc_core::SC_NS);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        return access(trans, /*debug=*/true) ? trans.get_data_length() : 0;
    }

    std::vector<unsigned char> rom_;
    std::vector<unsigned char> ram_;
};

tpu_core_config core_config()
{
    tpu_core_config config;
    config.chip = kChip;
    config.core = kCore;
    config.sram_capacity_bytes = kCapacity;
    config.fabric.data_width_bits = 128;
    config.fabric.bank_count = 4;
    config.fabric.mapping = tpu::bank_mapping::low_order_interleaved;
    config.fabric.pipeline_stages = 1;
    config.fabric.max_outstanding_per_requester = 1;
    // D16: a full-system run uses `annotated`. An `arbitrated` fabric would
    // make the hart synchronise to global time on every local load and store.
    config.timing = local_fabric_timing::annotated;
    config.cycle = sc_core::sc_time(10, sc_core::SC_NS);
    return config;
}

/// The firmware carries its own copy of the address map, because the generator
/// plan §8 calls for is Phase 10 work. Until it exists, this is what stops the
/// two copies drifting: every constant the image uses is asserted against the
/// C++ map before the image is allowed to run.
void firmware_map_agrees_with_the_c_plus_plus_map()
{
    CHECK_MSG(GLOBAL_RAM_BASE == am::global_ram_base,
              "the firmware's global RAM base disagrees with address_map.h");
    CHECK_MSG(CORE_BASE == am::core_base(kChip, kCore),
              "the firmware's core base disagrees with address_map.h");
    CHECK_MSG(CORE_SRAM_BASE == am::core_sram_base(kChip, kCore),
              "the firmware's core SRAM base disagrees with address_map.h");
    CHECK_MSG(SA_CONTROL_BASE == am::sa_control(kChip, kCore),
              "the firmware's SA_CONTROL base disagrees with address_map.h");
    CHECK_MSG(DMA_CONTROL_BASE == am::dma_control(kChip, kCore),
              "the firmware's DMA_CONTROL base disagrees with address_map.h");
    CHECK_MSG(TRF_CONTROL_BASE == am::transform_control(kChip, kCore),
              "the firmware's TRANSFORM_CONTROL base disagrees with "
              "address_map.h");
}

const char* stage_name(std::uint32_t stage)
{
    switch (stage) {
    case STAGE_START: return "START (identity reads)";
    case STAGE_DMA_IN: return "DMA of the input tensor";
    case STAGE_DMA_WEI: return "DMA of the weights";
    case STAGE_IM2COL: return "Transform (Im2Col)";
    case STAGE_MXU: return "MXU";
    case STAGE_RVV: return "RVV post-processing";
    case STAGE_DONE: return "DONE";
    default: return "before the first stage";
    }
}

} // namespace

int sc_main(int argc, char* argv[])
{
    const std::string elf_path = argc > 1 ? argv[1] : std::string();
    if (elf_path.empty() || !std::ifstream(elf_path).good()) {
        std::cout << "SKIP: " << (elf_path.empty() ? "<no image argument>"
                                                   : elf_path)
                  << " was not built (no RISC-V cross toolchain?)\n";
        return kSkip;
    }

    firmware_map_agrees_with_the_c_plus_plus_map();
    if (failures != 0) {
        std::cerr << "the firmware and the C++ map disagree; not running the "
                     "image, because every address below would be suspect\n";
        return 1;
    }

    const auto input = make_input();
    const auto weights = make_weights();
    const auto golden = compute_golden(input, weights);

    tpu_core neo("neo_core", core_config());
    outside_world outside("outside");
    neo.external().bind(outside.socket);

    // Nothing drives the core's inbound side in this run — the hart is the
    // only initiator — but SystemC still requires the port bound.
    struct idle_remote : sc_core::sc_module {
        tlm_utils::simple_initiator_socket<idle_remote> socket;
        explicit idle_remote(sc_core::sc_module_name name)
            : sc_core::sc_module(name), socket("socket") {}
    } unused_remote("unused_remote");
    unused_remote.socket.bind(neo.inbound());

    // The operands start in global RAM so the DMA has something to move. If
    // they were staged straight into core SRAM the pipeline would begin at its
    // second stage.
    outside.stage(HOST_INPUT_ADDR, input.data(), input.size());
    outside.stage(HOST_WEIGHT_ADDR, weights.data(), weights.size());

    neo.cpu().load_elf(elf_path);

    sc_core::sc_start(
        sc_core::sc_time(kWatchdogMilliseconds, sc_core::SC_MS));

    // ── what the firmware reported ───────────────────────────────────────────

    const std::uint32_t stage = outside.sim[(SIM_STAGE_MARK - SIM_BASE) / 4];
    CHECK_MSG(outside.exited,
              std::string("the image never reached its exit protocol; the last "
                          "stage it entered was ")
                  + stage_name(stage)
                  + ". A watchdog expiry here means an engine never left BUSY "
                    "or the hart never got to program it");

    const std::uint32_t kind = outside.sim[(SIM_EXIT_KIND - SIM_BASE) / 4];
    const std::uint32_t status = outside.sim[(SIM_EXIT_STATUS - SIM_BASE) / 4];
    const std::uint32_t mcause = outside.sim[(SIM_EXIT_MCAUSE - SIM_BASE) / 4];
    const std::uint32_t mepc = outside.sim[(SIM_EXIT_MEPC - SIM_BASE) / 4];

    CHECK_MSG(kind == SIM_EXIT_KIND_NORMAL,
              std::string("the image trapped instead of finishing: mcause ")
                  + std::to_string(mcause) + ", mepc 0x"
                  + std::to_string(mepc) + ", during " + stage_name(stage));
    CHECK_MSG(status == SIM_EXIT_PASS,
              std::string("the image reported failure status 0x")
                  + std::to_string(status) + " during " + stage_name(stage));
    CHECK_MSG(stage == STAGE_DONE,
              std::string("the image stopped at ") + stage_name(stage));

    // ── the data itself ──────────────────────────────────────────────────────
    //
    // Read out of core SRAM through the debug path, which bypasses arbitration
    // and latency but not decode or bounds. Comparing the intermediate Im2Col
    // matrix as well as the final product is what makes a failure diagnosable:
    // a wrong A with a correct GEMM points at the Transform block, and a
    // correct A with a wrong C points at the MXU.

    const auto read_sram = [&](std::uint64_t address, void* into,
                               std::uint32_t length) {
        auto* bytes = static_cast<unsigned char*>(into);
        std::uint32_t moved = 0;
        while (moved < length) {
            const std::uint32_t chunk
                = std::min<std::uint32_t>(length - moved, 64);
            sram::neo_local_request request;
            request.requester = sram::neo_requester::cpu;
            request.command = sram::neo_command::read;
            request.address = address + moved;
            request.size = chunk;
            request.data = bytes + moved;
            moved += neo.fabric().dbg_access(request);
        }
    };

    std::vector<std::int8_t> a_matrix(A_BYTES);
    read_sram(SRAM_A_ADDR, a_matrix.data(), A_BYTES);
    CHECK_MSG(a_matrix == golden.a_matrix,
              "the Im2Col matrix in core SRAM does not match the D18 layout "
              "recomputed on the host. The MXU result below is therefore "
              "measuring the wrong input, whatever it says");

    std::vector<std::int32_t> c_matrix(MAT_M * MAT_N);
    read_sram(SRAM_C_ADDR, c_matrix.data(), C_BYTES);
    CHECK_MSG(c_matrix == golden.c_matrix,
              "the MXU result in core SRAM does not match the host GEMM. "
              "INT8 x INT8 -> INT32 is exact integer arithmetic, so there is "
              "no tolerance to argue about");

    // ── what RVV computed ────────────────────────────────────────────────────

    const std::uint32_t checksum
        = outside.sim[(SIM_RVV_CHECKSUM - SIM_BASE) / 4];
    const std::uint32_t maximum = outside.sim[(SIM_RVV_MAXIMUM - SIM_BASE) / 4];
    const std::uint32_t negatives
        = outside.sim[(SIM_RVV_NEGATIVES - SIM_BASE) / 4];

    CHECK_MSG(checksum == golden.checksum,
              "the RVV ReLU checksum disagrees with the host reduction ("
                  + std::to_string(checksum) + " vs "
                  + std::to_string(golden.checksum) + ")");
    CHECK_MSG(maximum == static_cast<std::uint32_t>(golden.maximum),
              "the RVV maximum disagrees with the host reduction");
    CHECK_MSG(negatives == golden.negatives,
              "the count of clamped values disagrees, so the ReLU stage did "
              "not see the matrix the host did");
    CHECK_MSG(golden.negatives > 0,
              "the golden data has no negative results, so the ReLU stage "
              "would pass without doing anything -- pick different operands");

    // ── the paths the traffic took ───────────────────────────────────────────
    //
    // The pipeline could produce the right numbers over the wrong plane, and
    // that is exactly the integration defect this phase exists to catch.

    const auto& hart = neo.hart_port();
    CHECK_MSG(hart.requests(core::hart_destination::control) > 0,
              "no MMIO reached the control plane, so the engines were not "
              "programmed by the hart");
    CHECK_MSG(hart.requests(core::hart_destination::local_sram) > 0,
              "the hart never touched core SRAM over the native local plane; "
              "the RVV stage reads its results from there");
    CHECK_MSG(hart.requests(core::hart_destination::external) > 0,
              "no traffic crossed the external bridge, but the reset vector "
              "is in global boot ROM");

    for (auto requester : {neo_requester::dma, neo_requester::transform,
                           neo_requester::sa, neo_requester::cpu}) {
        CHECK_MSG(neo.fabric().counters(requester).request_count > 0,
                  std::string("requester '") + sram::to_string(requester)
                      + "' moved nothing on the local data plane, so that "
                        "stage of the pipeline did not run through the fabric");
    }

    std::cout << neo.report() << hart.report();

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "test_neo_core_pipeline: DMA -> Transform (Im2Col) -> MXU -> "
                 "RVV, all checks passed\n";
    return 0;
}
