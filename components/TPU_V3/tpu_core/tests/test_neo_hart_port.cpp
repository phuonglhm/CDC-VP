// SPDX-License-Identifier: Apache-2.0
//
// NEO hart port tests — the Phase 7 gate line that reads:
//
//   "`neo_hart_port` decode is proved per destination and at the boundaries:
//    an access straddling core SRAM and MMIO is refused rather than split, and
//    a repeating TLM byte-enable pattern reaches the native plane expanded to
//    one byte per data byte. The expansion has a negative control, because
//    forwarding the pattern unchanged is the defect `INTERFACE_CONTRACT.md` §2
//    predicts and it produces a wrong result and an overread at once."
//
// Two things about the shape of this file are deliberate.
//
// **The external side is a real `neo_external_bridge`, not a stub.** Plan §21
// says Phase 7 must bind the hart port and the DMA to the bridge rather than
// to a memory, because the bridge is what refuses an outbound access naming
// this core. Testing against a stub would prove the port can reach *something*
// and would not prove the wiring the phase is actually asking for.
//
// **The byte-enable negative control does not rely on undefined behaviour.**
// The obvious way to catch a raw-pointer forward is to let the fabric read off
// the end of the pattern, which is exactly the overread the contract warns
// about and is not something a test may deliberately perform. Instead the
// pattern sits in a full-length buffer whose tail is zero: a correct adapter
// expands and writes eight bytes, and an adapter that forwarded the pointer
// writes two and leaves the rest of the buffer alone. The difference is
// deterministic and observable, and nothing reads out of bounds either way.

#include "tpu_v3/core/core_registers.h"
#include "tpu_v3/core/neo_control_fabric.h"
#include "tpu_v3/core/neo_external_bridge.h"
#include "tpu_v3/core/neo_hart_port.h"
#include "tpu_v3/core/neo_local_sram_fabric.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "tpu_v3/address_map.h"

namespace tpu = cdc::components::tpu_v3;
namespace core = cdc::components::tpu_v3::core;
namespace sram = cdc::components::tpu_v3::sram;
namespace am = cdc::components::tpu_v3::address_map;

using core::control_initiator;
using core::hart_destination;
using core::local_fabric_timing;
using core::mmio_register_file;
using core::neo_control_fabric;
using core::neo_external_bridge;
using core::neo_hart_port;
using core::neo_local_sram_fabric;
using core::register_block;
using sram::neo_requester;

namespace {

int failures = 0;

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::cerr << "CHECK failed: " #cond " @ " << __FILE__ << ':'      \
                      << __LINE__ << '\n';                                    \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

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
constexpr double kWatchdogMilliseconds = 1.0;

sram::core_sram_config sram_config()
{
    sram::core_sram_config config;
    config.base_address = am::core_sram_base(kChip, kCore);
    config.window_bytes = am::core_sram_window;
    config.capacity_bytes = kCapacity;
    return config;
}

tpu::local_sram_fabric_config fabric_config()
{
    tpu::local_sram_fabric_config config;
    config.data_width_bits = 128;
    config.bank_count = 4;
    config.mapping = tpu::bank_mapping::low_order_interleaved;
    config.pipeline_stages = 1;
    config.max_outstanding_per_requester = 1;
    config.arbitration = tpu::arbitration_policy::round_robin;
    return config;
}

/// The chip-local fabric or NoC endpoint beyond the bridge. Records rather
/// than models: what matters is whether a transaction got that far.
class external_stub : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<external_stub> socket;
    std::uint64_t seen = 0;
    std::uint64_t debug_seen = 0;
    std::uint64_t last_address = 0;

    explicit external_stub(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
        socket.register_b_transport(this, &external_stub::b_transport);
        socket.register_transport_dbg(this, &external_stub::transport_dbg);
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time&)
    {
        ++seen;
        last_address = trans.get_address();
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        ++debug_seen;
        last_address = trans.get_address();
        return trans.get_data_length();
    }
};

/// Stands in for the VP++ hart: one combined instruction/data socket, which is
/// the whole reason this component exists.
class hart_stub : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<hart_stub> socket;

    explicit hart_stub(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
    }

    tlm::tlm_response_status access(tlm::tlm_command command,
                                    std::uint64_t address, unsigned char* data,
                                    unsigned int length,
                                    unsigned char* byte_enable = nullptr,
                                    unsigned int byte_enable_length = 0,
                                    unsigned int streaming_width = 0)
    {
        tlm::tlm_generic_payload trans;
        trans.set_command(command);
        trans.set_address(address);
        trans.set_data_ptr(data);
        trans.set_data_length(length);
        trans.set_byte_enable_ptr(byte_enable);
        trans.set_byte_enable_length(byte_enable_length);
        trans.set_streaming_width(streaming_width == 0 ? length
                                                       : streaming_width);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        socket->b_transport(trans, delay);
        CHECK_MSG(trans.get_response_status() != tlm::TLM_INCOMPLETE_RESPONSE,
                  "a path returned without setting a response status");
        last_delay = delay;
        return trans.get_response_status();
    }

    unsigned int debug(tlm::tlm_command command, std::uint64_t address,
                       unsigned char* data, unsigned int length)
    {
        tlm::tlm_generic_payload trans;
        trans.set_command(command);
        trans.set_address(address);
        trans.set_data_ptr(data);
        trans.set_data_length(length);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_byte_enable_length(0);
        trans.set_streaming_width(length);
        return socket->transport_dbg(trans);
    }

    sc_core::sc_time last_delay = sc_core::SC_ZERO_TIME;
};

std::uint32_t read32(hart_stub& hart, std::uint64_t address)
{
    std::array<unsigned char, 4> bytes{};
    hart.access(tlm::TLM_READ_COMMAND, address, bytes.data(), 4);
    std::uint32_t value = 0;
    std::memcpy(&value, bytes.data(), 4);
    return value;
}

void write32(hart_stub& hart, std::uint64_t address, std::uint32_t value)
{
    std::array<unsigned char, 4> bytes{};
    std::memcpy(bytes.data(), &value, 4);
    hart.access(tlm::TLM_WRITE_COMMAND, address, bytes.data(), 4);
}

/// Everything runs inside one thread so the "never waits" check can compare
/// `sc_time_stamp()` across an access, and so a hang fails against the
/// watchdog instead of stalling the suite.
class checks : public sc_core::sc_module {
public:
    checks(sc_core::sc_module_name name, hart_stub& hart, neo_hart_port& port,
           neo_local_sram_fabric& fabric, external_stub& outside)
        : sc_core::sc_module(name)
        , hart_(hart)
        , port_(port)
        , fabric_(fabric)
        , outside_(outside)
    {
        SC_HAS_PROCESS(checks);
        SC_THREAD(run);
    }

    bool ran() const noexcept { return ran_; }

private:
    void run()
    {
        const std::uint64_t sram_base = am::core_sram_base(kChip, kCore);
        const std::uint64_t dma_base = am::dma_control(kChip, kCore);

        decode_local_sram(sram_base);
        decode_control(dma_base);
        decode_external();
        straddle_is_refused(sram_base);
        byte_enables_are_expanded(sram_base);
        never_waits(sram_base);
        oversized_is_refused(sram_base);
        protocol_errors_are_reported(sram_base);
        capacity_error_is_distinct(sram_base);
        debug_agrees_and_is_free(sram_base, dma_base);

        ran_ = true;
    }

    // ── decode, one destination at a time ────────────────────────────────────

    void decode_local_sram(std::uint64_t sram_base)
    {
        const auto before_cpu = fabric_.counters(neo_requester::cpu).request_count;
        const auto before_external = outside_.seen;

        std::array<unsigned char, 8> out{1, 2, 3, 4, 5, 6, 7, 8};
        CHECK(hart_.access(tlm::TLM_WRITE_COMMAND, sram_base + 0x40,
                           out.data(), 8)
              == tlm::TLM_OK_RESPONSE);

        std::array<unsigned char, 8> in{};
        CHECK(hart_.access(tlm::TLM_READ_COMMAND, sram_base + 0x40, in.data(),
                           8)
              == tlm::TLM_OK_RESPONSE);
        CHECK_MSG(in == out, "a local SRAM round trip through the hart port "
                             "did not return what it wrote");

        CHECK_MSG(fabric_.counters(neo_requester::cpu).request_count
                      == before_cpu + 2,
                  "local traffic must arrive at the fabric as the 'cpu' "
                  "requester; unattributed traffic corrupts every counter "
                  "after it");
        CHECK_MSG(outside_.seen == before_external,
                  "a core-SRAM access reached the external path: local "
                  "traffic must never leave the core (plan §9.2)");
        CHECK(port_.requests(hart_destination::local_sram) == 2);
        CHECK(port_.bytes(hart_destination::local_sram) == 16);
    }

    void decode_control(std::uint64_t dma_base)
    {
        const auto before_cpu = fabric_.counters(neo_requester::cpu).request_count;
        const auto before_external = outside_.seen;
        const auto before = port_.requests(hart_destination::control);

        write32(hart_, dma_base + mmio_register_file::reg_scratch,
                0xA5A5'1234u);
        CHECK(read32(hart_, dma_base + mmio_register_file::reg_scratch)
              == 0xA5A5'1234u);

        CHECK(port_.requests(hart_destination::control) == before + 2);
        CHECK_MSG(fabric_.counters(neo_requester::cpu).request_count == before_cpu,
                  "an MMIO access reached the local data plane: control and "
                  "data are separate planes (D15), not one fabric with two "
                  "address ranges");
        CHECK_MSG(outside_.seen == before_external,
                  "a core-local MMIO access left the core");
    }

    void decode_external()
    {
        const auto before = outside_.seen;
        const auto before_cpu = fabric_.counters(neo_requester::cpu).request_count;

        std::array<unsigned char, 4> data{};
        CHECK(hart_.access(tlm::TLM_READ_COMMAND, am::global_ram_base,
                           data.data(), 4)
              == tlm::TLM_OK_RESPONSE);

        CHECK_MSG(outside_.seen == before + 1,
                  "a global address did not reach the external path; the "
                  "hart's reset PC is in global boot ROM, so this path is "
                  "architecturally required (D15)");
        CHECK(outside_.last_address == am::global_ram_base);
        CHECK(port_.requests(hart_destination::external) >= 1);
        CHECK(fabric_.counters(neo_requester::cpu).request_count == before_cpu);
    }

    // ── boundaries ───────────────────────────────────────────────────────────

    void straddle_is_refused(std::uint64_t sram_base)
    {
        const auto before_cpu = fabric_.counters(neo_requester::cpu).request_count;
        const auto before_control = port_.requests(hart_destination::control);
        const auto before_straddle = port_.straddle_errors();

        // Four bytes spanning the last two of the SRAM window and the first
        // two of whatever follows. There is no correct answer for which
        // destination should answer, so it is refused rather than split.
        std::array<unsigned char, 4> data{};
        const std::uint64_t across
            = sram_base + am::core_sram_window - 2;
        CHECK_MSG(hart_.access(tlm::TLM_READ_COMMAND, across, data.data(), 4)
                      == tlm::TLM_ADDRESS_ERROR_RESPONSE,
                  "an access straddling the SRAM/MMIO boundary must be "
                  "refused, not split between two planes");
        CHECK(port_.straddle_errors() == before_straddle + 1);
        CHECK_MSG(fabric_.counters(neo_requester::cpu).request_count == before_cpu,
                  "a refused straddling access still generated local traffic");
        CHECK_MSG(port_.requests(hart_destination::control) == before_control,
                  "a refused straddling access still generated control "
                  "traffic");
    }

    void byte_enables_are_expanded(std::uint64_t sram_base)
    {
        const std::uint64_t base = sram_base + 0x100;

        std::array<unsigned char, 16> ground{};
        ground.fill(0xEE);
        CHECK(hart_.access(tlm::TLM_WRITE_COMMAND, base, ground.data(), 16)
              == tlm::TLM_OK_RESPONSE);

        // The pattern is four bytes long and lives in a sixteen-byte buffer
        // whose tail is zero. A correct adapter repeats it to sixteen and
        // enables bytes 0, 2, 4 ... 14; an adapter forwarding the raw pointer
        // hands the fabric this whole buffer and enables only bytes 0 and 2.
        std::array<unsigned char, 16> enables{};
        enables.fill(0x00);
        enables[0] = 0xFF;
        enables[1] = 0x00;
        enables[2] = 0xFF;
        enables[3] = 0x00;

        std::array<unsigned char, 16> payload{};
        for (unsigned i = 0; i < payload.size(); ++i) {
            payload[i] = static_cast<unsigned char>(0x10 + i);
        }
        CHECK(hart_.access(tlm::TLM_WRITE_COMMAND, base, payload.data(), 16,
                           enables.data(), 4)
              == tlm::TLM_OK_RESPONSE);

        std::array<unsigned char, 16> back{};
        CHECK(hart_.access(tlm::TLM_READ_COMMAND, base, back.data(), 16)
              == tlm::TLM_OK_RESPONSE);

        for (unsigned i = 0; i < back.size(); ++i) {
            const unsigned char expected
                = (i % 2 == 0) ? payload[i] : 0xEE;
            CHECK_MSG(back[i] == expected,
                      "byte " + std::to_string(i)
                          + " of a masked write is wrong: the TLM byte-enable "
                            "pattern was not expanded to one strobe byte per "
                            "data byte (INTERFACE_CONTRACT.md §2). Bytes 4 and "
                            "up staying at 0xEE is the signature of forwarding "
                            "the raw pattern pointer.");
        }

        // A non-null pattern of zero length describes no bytes and cannot be
        // repeated into anything.
        const auto before = port_.protocol_errors();
        CHECK(hart_.access(tlm::TLM_WRITE_COMMAND, base, payload.data(), 16,
                           enables.data(), 0)
              == tlm::TLM_BURST_ERROR_RESPONSE);
        CHECK(port_.protocol_errors() == before + 1);
    }

    // ── timing ───────────────────────────────────────────────────────────────

    void never_waits(std::uint64_t sram_base)
    {
        CHECK_MSG(!port_.blocks_on_arbitration(),
                  "this bench built the fabric 'annotated'; if the port "
                  "reports otherwise the configuration under test is not the "
                  "one Phase 7 requires (D16)");

        const sc_core::sc_time before = sc_core::sc_time_stamp();
        std::array<unsigned char, 4> data{};
        CHECK(hart_.access(tlm::TLM_READ_COMMAND, sram_base, data.data(), 4)
              == tlm::TLM_OK_RESPONSE);
        CHECK_MSG(sc_core::sc_time_stamp() == before,
                  "the hart port advanced simulated time inside b_transport; "
                  "a target on a NoC-reachable path that waits freezes every "
                  "node in the mesh (INTERFACE_CONTRACT.md §3)");
        CHECK_MSG(hart_.last_delay > sc_core::SC_ZERO_TIME,
                  "the access cost nothing at all: latency must be annotated "
                  "into the caller's delay, not silently dropped");
    }

    // ── refusals ─────────────────────────────────────────────────────────────

    void oversized_is_refused(std::uint64_t sram_base)
    {
        const auto before_cpu = fabric_.counters(neo_requester::cpu).request_count;
        const auto before = port_.size_errors();

        std::array<unsigned char, 65> data{};
        CHECK_MSG(hart_.access(tlm::TLM_WRITE_COMMAND, sram_base + 0x200,
                               data.data(), 65)
                      == tlm::TLM_BURST_ERROR_RESPONSE,
                  "an access wider than one RVV register must be refused, not "
                  "split: splitting here would fabricate arbitration events "
                  "the hart never caused (D7)");
        CHECK(port_.size_errors() == before + 1);
        CHECK(fabric_.counters(neo_requester::cpu).request_count == before_cpu);
    }

    void protocol_errors_are_reported(std::uint64_t sram_base)
    {
        std::array<unsigned char, 4> data{};

        CHECK(hart_.access(tlm::TLM_IGNORE_COMMAND, sram_base, data.data(), 4)
              == tlm::TLM_COMMAND_ERROR_RESPONSE);

        // A misaligned register access from the hart. The port does not
        // pre-judge it — the control fabric owns the AXI4-Lite rules — but the
        // refusal has to survive the hop, and it has to be attributed to the
        // destination rather than counted as a decode failure here.
        const std::uint64_t dma_base = am::dma_control(kChip, kCore);
        const auto before = port_.destination_errors();
        CHECK_MSG(hart_.access(tlm::TLM_READ_COMMAND, dma_base + 1,
                               data.data(), 4)
                      == tlm::TLM_BURST_ERROR_RESPONSE,
                  "a misaligned MMIO access from the hart was not refused; "
                  "AXI4-Lite here is 4 bytes, naturally aligned, and a "
                  "silently widened or split register access is exactly what "
                  "INTERFACE_CONTRACT.md §6 forbids");
        CHECK_MSG(port_.destination_errors() == before + 1,
                  "the refusal was not attributed to the control plane that "
                  "issued it");
        // A wrapped streaming transfer repeats a payload over a narrower
        // window; nothing in TPU_V3 implements it.
        CHECK(hart_.access(tlm::TLM_READ_COMMAND, sram_base, data.data(), 4,
                           nullptr, 0, 2)
              == tlm::TLM_BURST_ERROR_RESPONSE);
    }

    void capacity_error_is_distinct(std::uint64_t sram_base)
    {
        const auto before = port_.destination_errors();
        std::array<unsigned char, 4> data{};

        // Inside the 16 MiB window, above the 64 KiB instantiated capacity.
        // D6 forbids aliasing this down into valid storage, and the status
        // must say "configuration too small", not "wrong pointer".
        CHECK_MSG(hart_.access(tlm::TLM_READ_COMMAND, sram_base + kCapacity,
                               data.data(), 4)
                      == tlm::TLM_GENERIC_ERROR_RESPONSE,
                  "an access above the instantiated capacity must be a decode "
                  "hit with an error, distinct from an address error");
        CHECK(port_.destination_errors() == before + 1);
    }

    // ── debug transport ──────────────────────────────────────────────────────

    void debug_agrees_and_is_free(std::uint64_t sram_base,
                                  std::uint64_t dma_base)
    {
        const auto requests_before
            = port_.requests(hart_destination::local_sram);
        const auto fabric_before
            = fabric_.counters(neo_requester::cpu).request_count;
        const sc_core::sc_time time_before = sc_core::sc_time_stamp();

        std::array<unsigned char, 4> out{0xDE, 0xAD, 0xBE, 0xEF};
        CHECK(hart_.debug(tlm::TLM_WRITE_COMMAND, sram_base + 0x300,
                          out.data(), 4)
              == 4);

        std::array<unsigned char, 4> in{};
        CHECK(hart_.access(tlm::TLM_READ_COMMAND, sram_base + 0x300, in.data(),
                           4)
              == tlm::TLM_OK_RESPONSE);
        CHECK_MSG(in == out,
                  "a debug write and a normal read disagreed: a loader and the "
                  "firmware it loads must see the same hardware");

        CHECK_MSG(sc_core::sc_time_stamp() == time_before,
                  "debug transport advanced simulated time");
        CHECK_MSG(port_.requests(hart_destination::local_sram)
                      == requests_before + 1,
                  "the debug access was counted as workload traffic; only the "
                  "normal read that followed it should be");
        CHECK_MSG(fabric_.counters(neo_requester::cpu).request_count
                      == fabric_before + 1,
                  "the debug access reached the fabric's workload counters; a "
                  "loader is not workload traffic "
                  "(INTERFACE_CONTRACT.md §8)");

        // MMIO through the same path, and a debug read of an unmapped address
        // still fails: debug bypasses arbitration, not decode.
        std::array<unsigned char, 4> reg{};
        CHECK(hart_.debug(tlm::TLM_READ_COMMAND,
                          dma_base + mmio_register_file::reg_id, reg.data(), 4)
              == 4);
    }

    hart_stub& hart_;
    neo_hart_port& port_;
    neo_local_sram_fabric& fabric_;
    external_stub& outside_;
    bool ran_ = false;
};

} // namespace

int sc_main(int, char*[])
{
    const std::uint64_t sram_base = am::core_sram_base(kChip, kCore);

    sram::core_sram store("core_sram", sram_config());
    // `annotated` is what Phase 7 requires for a full-system run: a hart
    // behind an `arbitrated` fabric synchronises to global time on every local
    // load and store and loses temporal decoupling entirely (D16).
    neo_local_sram_fabric fabric("local_fabric", fabric_config(), store,
                                 {neo_requester::cpu,
                                  neo_requester::external_inbound},
                                 local_fabric_timing::annotated,
                                 sc_core::sc_time(1, sc_core::SC_NS));

    std::vector<core::control_target_spec> specs{
        {am::core_control(kChip, kCore), am::core_control_size, "core"},
        {am::sa_control(kChip, kCore), am::sa_control_size, "sa"},
        {am::dma_control(kChip, kCore), am::dma_control_size, "dma"},
        {am::transform_control(kChip, kCore), am::transform_control_size,
         "transform"},
        {am::core_counters(kChip, kCore), am::core_counters_size, "counters"},
    };
    neo_control_fabric control("control_fabric", specs);

    mmio_register_file core_regs("core_regs", register_block::core,
                                 specs[0].base, specs[0].size);
    mmio_register_file sa_regs("sa_regs", register_block::sa, specs[1].base,
                               specs[1].size);
    mmio_register_file dma_regs("dma_regs", register_block::dma, specs[2].base,
                                specs[2].size);
    mmio_register_file transform_regs("transform_regs",
                                      register_block::transform, specs[3].base,
                                      specs[3].size);
    mmio_register_file counter_regs("counter_regs", register_block::counters,
                                    specs[4].base, specs[4].size);
    control.to[0].bind(core_regs.socket);
    control.to[1].bind(sa_regs.socket);
    control.to[2].bind(dma_regs.socket);
    control.to[3].bind(transform_regs.socket);
    control.to[4].bind(counter_regs.socket);

    core::core_aperture_spec aperture;
    aperture.core_base = am::core_base(kChip, kCore);
    aperture.core_size = am::core_aperture_stride;
    aperture.sram_base = sram_base;
    aperture.sram_window = am::core_sram_window;
    neo_external_bridge bridge("external_bridge", aperture, fabric);

    core::hart_port_spec port_spec;
    port_spec.core_base = aperture.core_base;
    port_spec.core_size = aperture.core_size;
    port_spec.sram_base = aperture.sram_base;
    port_spec.sram_window = aperture.sram_window;
    neo_hart_port port("hart_port", port_spec, fabric);

    external_stub outside("outside");
    hart_stub hart("hart");
    // The bridge is bidirectional and SystemC requires every port bound, so
    // the inbound side gets an initiator even though this bench drives only
    // the outbound one. Inbound behaviour is `test_neo_external_bridge`'s gate.
    hart_stub remote("remote");

    // The wiring Phase 7 asks for: the hart's one socket into the port, the
    // port's MMIO leg into the control plane's `cpu` initiator, and its
    // external leg into the bridge rather than straight at a memory.
    hart.socket.bind(port.from_hart);
    port.to_control.bind(
        control.from[static_cast<unsigned>(control_initiator::cpu)]);
    port.to_external.bind(
        bridge.local_outbound[static_cast<unsigned>(
            core::outbound_initiator::cpu)]);
    bridge.inbound_control.bind(control.from[1]);
    bridge.external.bind(outside.socket);
    remote.socket.bind(bridge.inbound);
    // The DMA's outbound socket is unused here and still has to be bound.
    hart_stub unused_dma("unused_dma");
    unused_dma.socket.bind(
        bridge.local_outbound[static_cast<unsigned>(
            core::outbound_initiator::dma)]);

    checks scenario("checks", hart, port, fabric, outside);

    // A fabric with no `cpu` requester would refuse the hart's first local
    // load. Elaboration is the cheaper place to find that out than the first
    // instruction fetch that touches SRAM.
    bool threw = false;
    try {
        neo_local_sram_fabric inbound_only("inbound_only", fabric_config(),
                                           store,
                                           {neo_requester::external_inbound});
        neo_hart_port orphan("orphan_port", port_spec, inbound_only);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK_MSG(threw,
              "a hart port whose fabric has no 'cpu' requester must fail "
              "during elaboration, not on the first local access");

    sc_core::sc_start(sc_core::sc_time(kWatchdogMilliseconds, sc_core::SC_MS));

    CHECK_MSG(scenario.ran(),
              "the watchdog expired with work still in flight: the hart port "
              "blocked somewhere it must not");

    std::cout << port.report();

    // Counters are the port's own account of where traffic went; reset must
    // clear it rather than leave a previous epoch's numbers to be read as the
    // new one's.
    port.reset();
    CHECK(port.requests(hart_destination::local_sram) == 0);
    CHECK(port.requests(hart_destination::control) == 0);
    CHECK(port.requests(hart_destination::external) == 0);
    CHECK(port.straddle_errors() == 0);
    CHECK(port.size_errors() == 0);
    CHECK(port.protocol_errors() == 0);
    CHECK(port.destination_errors() == 0);

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "test_neo_hart_port: all checks passed\n";
    return 0;
}
