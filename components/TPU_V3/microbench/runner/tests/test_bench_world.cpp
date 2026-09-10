// SPDX-License-Identifier: Apache-2.0
//
// `bench_world` against the generic TLM payload contract (`INTERFACE_CONTRACT`
// §1, §2, §6, §8, §9).
//
// This exists because the target it tests is *only* a testbench, and that is
// exactly why it needed the test. The benchmark runner generates well-formed
// traffic, so a permissive host-I/O target never misbehaves in the runs anyone
// looks at — while a malformed payload would have been copied four bytes at a
// time into whatever the address decoded to. A target that is more permissive
// than the model it stands in for hides the defect it exists to catch.
//
// Every case below drives the socket directly rather than booting an image:
// the point is the payloads a real initiator must never send, and a correct
// initiator will not send them.

#include "bench_world.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

namespace bench = cdc::components::tpu_v3::bench;
namespace am = cdc::components::tpu_v3::address_map;

namespace {

int failures = 0;

#define CHECK_MSG(cond, msg)                                                  \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::cerr << "CHECK failed: " << (msg) << " @ " << __FILE__       \
                      << ':' << __LINE__ << '\n';                             \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

class prober : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<prober> socket;

    explicit prober(sc_core::sc_module_name name)
        : sc_core::sc_module(name), socket("socket") {}

    tlm::tlm_response_status send(tlm::tlm_generic_payload& trans)
    {
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        socket->b_transport(trans, delay);
        return trans.get_response_status();
    }

    unsigned int send_dbg(tlm::tlm_generic_payload& trans)
    {
        return socket->transport_dbg(trans);
    }
};

/// A payload with everything defaulted to a legal shape, so each test changes
/// exactly one thing.
void shape(tlm::tlm_generic_payload& trans, tlm::tlm_command command,
           std::uint64_t address, unsigned char* data, unsigned int length)
{
    trans.set_command(command);
    trans.set_address(address);
    trans.set_data_ptr(data);
    trans.set_data_length(length);
    trans.set_streaming_width(length);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_byte_enable_length(0);
    trans.set_dmi_allowed(true);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
}

} // namespace

int sc_main(int, char*[])
{
    bench::bench_world world("world");
    prober probe("probe");
    probe.socket.bind(world.socket);

    // 64 bytes, the largest payload a memory-like target must accept (one RVV
    // register at VLEN=512) and the largest this file sends. An 8-word buffer
    // was here first, and the 64-byte case in the final loop read 32 bytes
    // past its end — the target was right to accept the access, so the fault
    // was entirely in the probe.
    std::uint32_t buffer[16] = {};
    auto* bytes = reinterpret_cast<unsigned char*>(buffer);
    static_assert(sizeof(buffer) >= 64,
                  "the probe buffer must cover the largest payload sent below");
    tlm::tlm_generic_payload trans;

    // ── the shape rules of §1 ────────────────────────────────────────────────

    shape(trans, tlm::TLM_IGNORE_COMMAND, SIM_STAGE_MARK, bytes, 4);
    CHECK_MSG(probe.send(trans) == tlm::TLM_COMMAND_ERROR_RESPONSE,
              "a command that is neither read nor write was accepted");

    shape(trans, tlm::TLM_WRITE_COMMAND, SIM_STAGE_MARK, bytes, 0);
    CHECK_MSG(probe.send(trans) == tlm::TLM_BURST_ERROR_RESPONSE,
              "a zero-length payload was accepted");

    shape(trans, tlm::TLM_WRITE_COMMAND, SIM_STAGE_MARK, nullptr, 4);
    CHECK_MSG(probe.send(trans) == tlm::TLM_BURST_ERROR_RESPONSE,
              "a null data pointer with a non-zero length was accepted");

    shape(trans, tlm::TLM_WRITE_COMMAND, am::global_ram_base, bytes, 8);
    trans.set_streaming_width(4);
    CHECK_MSG(probe.send(trans) == tlm::TLM_BURST_ERROR_RESPONSE,
              "a wrapped streaming transfer was accepted");

    unsigned char enables[4] = {TLM_BYTE_ENABLED, TLM_BYTE_ENABLED,
                                TLM_BYTE_ENABLED, TLM_BYTE_ENABLED};
    shape(trans, tlm::TLM_WRITE_COMMAND, am::global_ram_base, bytes, 4);
    trans.set_byte_enable_ptr(enables);
    trans.set_byte_enable_length(0);
    CHECK_MSG(probe.send(trans) == tlm::TLM_BURST_ERROR_RESPONSE,
              "a non-null byte-enable pointer describing no bytes was accepted; "
              "it cannot be repeated into anything");

    // ── the host-I/O window is a 32-bit register file (§6) ───────────────────

    shape(trans, tlm::TLM_WRITE_COMMAND, SIM_STAGE_MARK, bytes, 8);
    CHECK_MSG(probe.send(trans) == tlm::TLM_BURST_ERROR_RESPONSE,
              "an 8-byte access to the host-I/O register file was accepted");

    shape(trans, tlm::TLM_WRITE_COMMAND, SIM_STAGE_MARK, bytes, 1);
    CHECK_MSG(probe.send(trans) == tlm::TLM_BURST_ERROR_RESPONSE,
              "a 1-byte access to the host-I/O register file was accepted");

    shape(trans, tlm::TLM_WRITE_COMMAND, SIM_STAGE_MARK + 2, bytes, 4);
    CHECK_MSG(probe.send(trans) == tlm::TLM_BURST_ERROR_RESPONSE,
              "a misaligned 4-byte register access was accepted");

    // All four bytes enabled is what §2 *requires* of an MMIO write, so an
    // initiator that spells it explicitly must be served rather than refused.
    buffer[0] = 0x0BAD'0001u;
    shape(trans, tlm::TLM_WRITE_COMMAND, SIM_STAGE_MARK, bytes, 4);
    trans.set_byte_enable_ptr(enables);
    trans.set_byte_enable_length(4);
    CHECK_MSG(probe.send(trans) == tlm::TLM_OK_RESPONSE,
              "an explicit all-ones strobe on a 32-bit register was refused; "
              "§2 asks for all-ones, not for a null pointer");

    // A strobe that leaves any addressed byte out is the one that must be
    // refused, because applying it would be a read-modify-write on a register
    // that may have write-1-to-clear bits.
    unsigned char partial[4]
        = {TLM_BYTE_ENABLED, TLM_BYTE_DISABLED, TLM_BYTE_ENABLED,
           TLM_BYTE_ENABLED};
    shape(trans, tlm::TLM_WRITE_COMMAND, SIM_STAGE_MARK, bytes, 4);
    trans.set_byte_enable_ptr(partial);
    trans.set_byte_enable_length(4);
    CHECK_MSG(probe.send(trans) == tlm::TLM_BURST_ERROR_RESPONSE,
              "a partial-strobe register write was accepted; §2 refuses it "
              "rather than applying a read-modify-write");

    // A repeating pattern shorter than the access still has to be expanded
    // before it is judged: a two-byte {enabled, disabled} pattern covers a
    // 4-byte register as {E, D, E, D}, which is partial.
    unsigned char repeating[2] = {TLM_BYTE_ENABLED, TLM_BYTE_DISABLED};
    shape(trans, tlm::TLM_WRITE_COMMAND, SIM_STAGE_MARK, bytes, 4);
    trans.set_byte_enable_ptr(repeating);
    trans.set_byte_enable_length(2);
    CHECK_MSG(probe.send(trans) == tlm::TLM_BURST_ERROR_RESPONSE,
              "a repeating strobe that disables half the register was accepted");

    // A 4-byte aligned access is the one shape it must serve.
    buffer[0] = 0xA5A5'1234u;
    shape(trans, tlm::TLM_WRITE_COMMAND, SIM_STAGE_MARK, bytes, 4);
    CHECK_MSG(probe.send(trans) == tlm::TLM_OK_RESPONSE,
              "the register file refused a well-formed 4-byte write");
    buffer[0] = 0;
    shape(trans, tlm::TLM_READ_COMMAND, SIM_STAGE_MARK, bytes, 4);
    CHECK_MSG(probe.send(trans) == tlm::TLM_OK_RESPONSE
                  && buffer[0] == 0xA5A5'1234u,
              "the register file did not read back what was written");

    // ── decode (§1) ──────────────────────────────────────────────────────────

    shape(trans, tlm::TLM_READ_COMMAND, 0x4000'0000ull, bytes, 4);
    CHECK_MSG(probe.send(trans) == tlm::TLM_ADDRESS_ERROR_RESPONSE,
              "an unmapped address did not report a decode error");

    // One byte past the instantiated global RAM.
    shape(trans, tlm::TLM_READ_COMMAND,
          am::global_ram_base + bench::default_global_ram_bytes, bytes, 4);
    CHECK_MSG(probe.send(trans) == tlm::TLM_ADDRESS_ERROR_RESPONSE,
              "an access past the end of global RAM was served");

    // ── the boot ROM refuses ordinary writes but accepts the loader (§6, §8) ─

    shape(trans, tlm::TLM_WRITE_COMMAND, am::boot_rom_base, bytes, 4);
    CHECK_MSG(probe.send(trans) == tlm::TLM_GENERIC_ERROR_RESPONSE,
              "the boot ROM accepted an ordinary write");

    buffer[0] = 0xDEAD'BEEFu;
    shape(trans, tlm::TLM_WRITE_COMMAND, am::boot_rom_base, bytes, 4);
    CHECK_MSG(probe.send_dbg(trans) == 4,
              "the boot ROM refused a debug write from the host loader");
    buffer[0] = 0;
    shape(trans, tlm::TLM_READ_COMMAND, am::boot_rom_base, bytes, 4);
    CHECK_MSG(probe.send(trans) == tlm::TLM_OK_RESPONSE
                  && buffer[0] == 0xDEAD'BEEFu,
              "the debug write to the boot ROM did not land");

    // §1: `transport_dbg` sets the response status and clears `dmi_allowed`
    // on every return, exactly as `b_transport` does. A payload handed back
    // still carrying TLM_INCOMPLETE_RESPONSE is a state §1 forbids.
    shape(trans, tlm::TLM_READ_COMMAND, am::global_ram_base, bytes, 4);
    trans.set_dmi_allowed(true);
    CHECK_MSG(probe.send_dbg(trans) == 4,
              "a well-formed debug read of global RAM was refused");
    CHECK_MSG(trans.get_response_status() == tlm::TLM_OK_RESPONSE,
              "debug transport returned without setting a response status");
    CHECK_MSG(!trans.is_dmi_allowed(),
              "debug transport left dmi_allowed set");

    shape(trans, tlm::TLM_READ_COMMAND, 0x4000'0000ull, bytes, 4);
    probe.send_dbg(trans);
    CHECK_MSG(trans.get_response_status() != tlm::TLM_INCOMPLETE_RESPONSE,
              "a refused debug access left the payload incomplete");

    // §8: debug transport bypasses timing, not payload validity.
    shape(trans, tlm::TLM_IGNORE_COMMAND, am::global_ram_base, bytes, 4);
    CHECK_MSG(probe.send_dbg(trans) == 0,
              "debug transport forwarded a payload with an invalid command");
    shape(trans, tlm::TLM_READ_COMMAND, 0x4000'0000ull, bytes, 4);
    CHECK_MSG(probe.send_dbg(trans) == 0,
              "debug transport served an address that does not decode");

    // ── byte enables on a memory-like target (§2) ────────────────────────────

    std::uint32_t pair[2] = {0x1111'1111u, 0x2222'2222u};
    auto* pair_bytes = reinterpret_cast<unsigned char*>(pair);
    shape(trans, tlm::TLM_WRITE_COMMAND, am::global_ram_base, pair_bytes, 8);
    CHECK_MSG(probe.send(trans) == tlm::TLM_OK_RESPONSE, "staging write failed");

    unsigned char mask[4]
        = {TLM_BYTE_ENABLED, TLM_BYTE_DISABLED, TLM_BYTE_ENABLED,
           TLM_BYTE_DISABLED};
    std::uint32_t overwrite[2] = {0xFFFF'FFFFu, 0xFFFF'FFFFu};
    auto* overwrite_bytes = reinterpret_cast<unsigned char*>(overwrite);
    shape(trans, tlm::TLM_WRITE_COMMAND, am::global_ram_base, overwrite_bytes,
          8);
    trans.set_byte_enable_ptr(mask);
    trans.set_byte_enable_length(4);
    CHECK_MSG(probe.send(trans) == tlm::TLM_OK_RESPONSE,
              "a masked write to global RAM was refused");

    std::uint32_t readback[2] = {};
    auto* readback_bytes = reinterpret_cast<unsigned char*>(readback);
    shape(trans, tlm::TLM_READ_COMMAND, am::global_ram_base, readback_bytes, 8);
    CHECK_MSG(probe.send(trans) == tlm::TLM_OK_RESPONSE, "readback failed");
    // The pattern repeats over the payload, so bytes 0 and 2 of each word are
    // replaced and bytes 1 and 3 keep their old value. Read as a
    // little-endian word that is `0x11FF'11FF`, not `0xFF11'FF11` — byte 0 is
    // the least significant one.
    CHECK_MSG(readback[0] == 0x11FF'11FFu && readback[1] == 0x22FF'22FFu,
              "the repeating byte-enable pattern was not applied over the whole "
              "payload; got " + std::to_string(readback[0]) + " and "
                  + std::to_string(readback[1]));

    // ── DMI stays off (§9) ───────────────────────────────────────────────────

    shape(trans, tlm::TLM_READ_COMMAND, am::global_ram_base, bytes, 4);
    trans.set_dmi_allowed(true);
    probe.send(trans);
    CHECK_MSG(!trans.is_dmi_allowed(),
              "the target left dmi_allowed set; DMI would let traffic bypass "
              "the counters the runner exists to read");

    // ── and no path returns TLM_INCOMPLETE_RESPONSE (§1) ─────────────────────

    for (std::uint64_t address :
         {static_cast<std::uint64_t>(SIM_STAGE_MARK), am::boot_rom_base,
          am::global_ram_base, static_cast<std::uint64_t>(0x4000'0000ull)}) {
        for (unsigned int length : {1u, 3u, 4u, 8u, 64u}) {
            shape(trans, tlm::TLM_WRITE_COMMAND, address, bytes, length);
            CHECK_MSG(probe.send(trans) != tlm::TLM_INCOMPLETE_RESPONSE,
                      "a path returned without setting a response status");
        }
    }

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "test_bench_world: all checks passed\n";
    return 0;
}
