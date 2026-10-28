// Plan T04: the on-chip memories on the real FU3 bus of the platform.
//
// Window edges are an independent transcription of Common Spec v2.1 MemoryMap
// rows 1-9 (not read back from BusConfig). Checks byte enables without RMW,
// per-master access rules, the staging/handoff switch, the CPU debug
// partitions, ROM write protection and the disabled R2 windows. Error codes
// follow decision Q02-f: decode miss -> ADDRESS_ERROR, refused -> GENERIC_ERROR.

#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <fu3_bus/access.h>

#include "fu3_soc_top.h"

using namespace cdc::platforms::fu3_soc;
using tlm::TLM_ADDRESS_ERROR_RESPONSE;
using tlm::TLM_GENERIC_ERROR_RESPONSE;
using tlm::TLM_OK_RESPONSE;

namespace {

struct window {
    const char* name;
    std::uint64_t first, last; // inclusive, as written in the spec
};
constexpr window kWindows[] = {
    {"ROM", 0x00000000, 0x00000FFF},
    {"CVA6_ISRAM", 0x00010000, 0x00040FFF}, // 192 KiB image + 4 KiB debug code
    {"IBEX_ISRAM", 0x00050000, 0x00060FFF}, // 64 KiB image + 4 KiB debug code
    {"DSRAM", 0x01000000, 0x0103FFFF},
};
constexpr std::uint64_t kCva6Debug = 0x00040000; // HaltAddress
constexpr std::uint64_t kIbexDebug = 0x00060000; // DmHaltAddr

struct master : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<master> socket{"socket"};
    explicit master(sc_core::sc_module_name n) : sc_module(n) {}
};

struct bench : sc_core::sc_module {
    fu3_soc_top top{"fu3_soc", [] {
        fu3_soc_options o;
        o.cpus = false;          // bench masters take the CPU ports
        o.dma_master = false;    // and the DMA0 port
        return o;
    }()};
    master cva6{"cva6"}, ibex{"ibex"}, dma{"dma"};
    unsigned checks = 0, failures = 0;
    bool done = false;

    SC_HAS_PROCESS(bench);
    explicit bench(sc_core::sc_module_name n) : sc_module(n)
    {
        cva6.socket.bind(top.fabric().initiator("CVA6"));
        ibex.socket.bind(top.fabric().initiator("IBEX"));
        dma.socket.bind(top.fabric().initiator("DMA0"));
        SC_THREAD(run);
    }

    void check(bool ok, const std::string& what)
    {
        ++checks;
        if (!ok) {
            ++failures;
            std::cerr << "FAIL: " << what << '\n';
        }
    }

    // One functional request with FU3 Access metadata; returns the status.
    tlm::tlm_response_status xfer(master& m, bool write, std::uint64_t address, unsigned char* data,
                                  unsigned length, bool fetch = false, bool debug = false)
    {
        tlm::tlm_generic_payload tx;
        tx.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
        tx.set_address(address);
        tx.set_data_ptr(data);
        tx.set_data_length(length);
        tx.set_streaming_width(length);
        tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        fu3_bus::ScopedAccess meta(tx, length < 4 ? length : 4, fu3_bus::Burst::Increment, fetch, 0, debug);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        m.socket->b_transport(tx, delay);
        wait(delay);
        return tx.get_response_status();
    }
    std::uint32_t word(master& m, std::uint64_t address, tlm::tlm_response_status expect = TLM_OK_RESPONSE,
                       bool fetch = false, bool debug = false)
    {
        unsigned char b[4] = {};
        const auto status = xfer(m, false, address, b, 4, fetch, debug);
        check(status == expect, "read status at 0x" + hex(address));
        return b[0] | b[1] << 8 | b[2] << 16 | std::uint32_t(b[3]) << 24;
    }
    void store(master& m, std::uint64_t address, std::uint32_t value,
               tlm::tlm_response_status expect = TLM_OK_RESPONSE, bool debug = false)
    {
        unsigned char b[4] = {std::uint8_t(value), std::uint8_t(value >> 8), std::uint8_t(value >> 16),
                              std::uint8_t(value >> 24)};
        check(xfer(m, true, address, b, 4, false, debug) == expect, "write status at 0x" + hex(address));
    }
    unsigned load(std::uint64_t address, std::uint32_t value)
    {
        unsigned char b[4] = {std::uint8_t(value), std::uint8_t(value >> 8), std::uint8_t(value >> 16),
                              std::uint8_t(value >> 24)};
        tlm::tlm_generic_payload tx;
        tx.set_command(tlm::TLM_WRITE_COMMAND);
        tx.set_address(address);
        tx.set_data_ptr(b);
        tx.set_data_length(4);
        tx.set_streaming_width(4);
        return ibex.socket->transport_dbg(tx);
    }
    static std::string hex(std::uint64_t v)
    {
        char s[20];
        std::snprintf(s, sizeof s, "%08llx", static_cast<unsigned long long>(v));
        return s;
    }

    void run()
    {
        // Functional accesses need the system domain out of reset (T07).
        while (!top.system_ready())
            wait(100, sc_core::SC_NS);
        // Window edges: last word answers, the byte after the window is a decode miss.
        for (const auto& w : kWindows) {
            auto* mem = top.memory(w.name);
            check(mem && mem->size() == w.last - w.first + 1, std::string(w.name) + " size per Common v2.1");
            check(load(w.last - 3, 0xc0ffee00u | (w.last & 0xff)) == 4, std::string(w.name) + " loader at last word");
            check(mem && mem->peek(mem->size() - 1) == 0xc0, std::string(w.name) + " last byte is local");
            unsigned char b[4] = {};
            check(xfer(dma, false, w.last + 1, b, 4) == TLM_ADDRESS_ERROR_RESPONSE,
                  std::string(w.name) + " byte after the window is unmapped");
        }

        // Byte enables reach the memory as lanes: no read-modify-write of the
        // neighbouring bytes (DSRAM is R/W for both CPUs and DMA).
        const std::uint64_t d = 0x01000100;
        store(ibex, d, 0x44332211u);
        unsigned char b1 = 0xaa, h[2] = {0xbb, 0xcc};
        check(xfer(cva6, true, d + 1, &b1, 1) == TLM_OK_RESPONSE, "byte store to DSRAM");
        check(xfer(dma, true, d + 2, h, 2) == TLM_OK_RESPONSE, "halfword store to DSRAM");
        check(word(ibex, d) == 0xccbbaa11u, "byte and halfword lanes merged, byte 0 untouched");
        unsigned char rb = 0;
        check(xfer(cva6, false, d + 3, &rb, 1) == TLM_OK_RESPONSE && rb == 0xcc, "byte load from DSRAM");
        word(cva6, d, TLM_GENERIC_ERROR_RESPONSE, true);          // DSRAM is non-executable
        check(word(dma, d) == 0xccbbaa11u, "DMA reads DSRAM");

        // ROM: Ibex reads and fetches, nobody writes, CVA6 and DMA are refused.
        check(load(0x80, 0x00000013u) == 4, "ROM image through the loader");
        check(word(ibex, 0x80, TLM_OK_RESPONSE, true) == 0x13u, "Ibex fetches its reset entry from ROM");
        store(ibex, 0x80, 0xffffffffu, TLM_GENERIC_ERROR_RESPONSE);
        check(word(ibex, 0x80) == 0x13u, "refused ROM write leaves the image");
        word(cva6, 0x80, TLM_GENERIC_ERROR_RESPONSE);
        word(dma, 0x80, TLM_GENERIC_ERROR_RESPONSE);

        // Staging: Ibex writes and verifies the CVA6 image, including its debug code.
        store(ibex, 0x10000, 0x0000006fu);
        check(word(ibex, 0x10000) == 0x6fu, "Ibex verifies the staged CVA6 image");
        store(ibex, kCva6Debug, 0x7b200073u);
        store(ibex, 0x50000, 0x00000297u);
        store(cva6, 0x50000, 0, TLM_GENERIC_ERROR_RESPONSE);      // IBEX_ISRAM is Ibex's
        word(cva6, 0x50000, TLM_GENERIC_ERROR_RESPONSE);

        // Debug partitions: code is fetched only in debug mode and never written by its CPU.
        word(cva6, kCva6Debug, TLM_GENERIC_ERROR_RESPONSE, true);
        check(word(cva6, kCva6Debug, TLM_OK_RESPONSE, true, true) == 0x7b200073u, "CVA6 debug-mode fetch");
        store(cva6, kCva6Debug, 0, TLM_GENERIC_ERROR_RESPONSE, true);
        store(ibex, kIbexDebug, 0x00100073u);
        word(ibex, kIbexDebug, TLM_GENERIC_ERROR_RESPONSE, true);
        word(ibex, kIbexDebug, TLM_OK_RESPONSE, true, true);

        // Handoff (Q02-b mechanism; the CRM trigger is T07): staging closes.
        top.fabric().finish_staging();
        store(ibex, 0x10000, 0, TLM_GENERIC_ERROR_RESPONSE);
        word(ibex, 0x10000, TLM_GENERIC_ERROR_RESPONSE);
        check(word(cva6, 0x10000, TLM_OK_RESPONSE, true) == 0x6fu, "CVA6 fetches the staged image");
        store(cva6, 0x10004, 0x12345678u);
        store(ibex, 0x50004, 0, TLM_GENERIC_ERROR_RESPONSE);      // Ibex image is locked
        check(word(ibex, 0x50000, TLM_OK_RESPONSE, true) == 0x297u, "Ibex fetches its own image");

        // R2 windows are in the map but disabled: decode misses.
        for (const auto* t : top.disabled_targets()) {
            unsigned char x[4] = {};
            check(xfer(ibex, false, t->base, x, 4) == TLM_ADDRESS_ERROR_RESPONSE,
                  t->name + " (disabled) is a decode miss");
        }
        check(top.disabled_targets().size() == 4, "EDHOC_OSCORE, PWM0, PWM1, CAN disabled for R1");

        done = true;
        sc_core::sc_stop();
    }
};

} // namespace

int sc_main(int, char**)
{
    sc_core::sc_report_handler::set_actions("fu3_soc/not_integrated", sc_core::SC_DO_NOTHING);
    bench b("bench");
    sc_core::sc_start();
    std::cout << "fu3_soc memory/map: " << b.checks << " checks, " << b.failures << " failures -> "
              << (b.done && !b.failures ? "PASS" : "FAIL") << '\n';
    return b.done && !b.failures ? 0 : 1;
}
