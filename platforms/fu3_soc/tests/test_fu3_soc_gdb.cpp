// Plan T19 / decision Q04-d: the Debug Module (over JTAG) and the GDB stub
// share one hart control (cpu_debug_hart), one owner at a time.
//   - GDB owns Ibex: the DM sees it unavailable, its halt request does not
//     halt it and its abstract register access fails (cmderr);
//   - GDB detach: the DM's pending halt request applies, the DM owns the hart
//     and GDB attach is refused until the DM resumes it;
//   - a reset of the hart while GDB owns it halts it on its first instruction
//     (dcsr/trigger re-arm point of the stub);
//   - through the TCP server: an attach while the DM owns the hart is closed,
//     an attach afterwards halts the hart and answers the stop reply.

#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#include <systemc>

#include <fu3_dbg/registers.h>

#include "cpu_debug_hart.h"
#include "fu3_soc_top.h"
#include "gdb_server.h"

using namespace cdc::platforms::fu3_soc;

namespace {

// Ibex ROM stub at 0x80 (as in test_fu3_soc_debug): final IBEX_ISRAM PMP entry
// (RX, which also makes the debug partition executable), then wfi loop.
constexpr std::uint32_t kIbexRom[] = {0x9b9da2b7, 0xb8d28293, 0x3a129073, 0x10500073, 0xffdff06f};
constexpr std::uint32_t kPark = 0x0000006f;   // j .

// One TCP client in its own thread: connects, optionally sends a packet,
// receives until `want` appears, sends `then` and holds the connection until
// `release`, or stops when the peer closes. (While its hart is stopped the
// server holds the simulation, so the bench only runs again after `then`
// resumes it.)
struct client {
    std::atomic<bool> done{false}, got{false}, release{false};
    std::string received;
    bool closed = false;
    std::thread worker;

    void start(int port, std::string packet, std::string want, std::string then = {})
    {
        worker = std::thread([this, port, packet, want, then] {
            const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            addr.sin_port = htons(static_cast<std::uint16_t>(port));
            if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr) == 0) {
                if (!packet.empty())
                    ::send(fd, packet.data(), packet.size(), MSG_NOSIGNAL);
                char buffer[256];
                while (want.empty() || received.find(want) == std::string::npos) {
                    const auto n = ::recv(fd, buffer, sizeof buffer, 0);
                    if (n <= 0) {
                        closed = true;
                        break;
                    }
                    received.append(buffer, std::size_t(n));
                }
                if (!closed && !then.empty())
                    ::send(fd, then.data(), then.size(), MSG_NOSIGNAL);
                got = true;
                while (!closed && !release)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            ::close(fd);
            done = true;
        });
    }
};

struct bench : sc_core::sc_module {
    fu3_soc_top top;
    fu3_dbg::Jtag2Axi& jtag;
    gdb_server gdb;
    unsigned checks = 0, failures = 0;
    bool finished = false;

    SC_HAS_PROCESS(bench);
    explicit bench(sc_core::sc_module_name n)
        : sc_module(n), top("fu3_soc", fu3_soc_options{}), jtag(*top.jtag()), gdb("gdb", top, {-1, 0}, false)
    {
        auto poke = [&](const char* mem, std::uint64_t offset, std::uint32_t word) {
            for (unsigned i = 0; i < 4; ++i)
                top.memory(mem)->poke(offset + i, (word >> (8 * i)) & 0xff);
        };
        for (unsigned i = 0; i < 5; ++i)
            poke("ROM", 0x80 + 4 * i, kIbexRom[i]);
        poke("IBEX_ISRAM", 0x10000, kPark);
        poke("IBEX_ISRAM", 0x10008, kPark);
        SC_THREAD(run);
    }

    void check(bool ok, const std::string& what)
    {
        ++checks;
        if (!ok) {
            ++failures;
            std::cerr << "FAIL @" << sc_core::sc_time_stamp() << ": " << what << '\n';
        }
    }
    void bus_write(std::uint32_t address, std::uint32_t value)
    {
        jtag.scan_ir(FU3_DBG_IR_CMD_ADDR);
        jtag.scan_dr(address);
        jtag.scan_ir(FU3_DBG_IR_CMD_DATA);
        jtag.scan_dr(FU3_DBG_DATA_WRITE | value);
    }
    std::uint32_t bus_read(std::uint32_t address)
    {
        jtag.scan_ir(FU3_DBG_IR_CMD_ADDR);
        jtag.scan_dr(address);
        jtag.scan_ir(FU3_DBG_IR_CMD_DATA);
        jtag.scan_dr(FU3_DBG_DATA_READ);
        return static_cast<std::uint32_t>(jtag.scan_dr(0));
    }
    void dm_write(unsigned reg, std::uint32_t value) { bus_write(FU3_DBG_DM_BASE + FU3_DBG_DM_OFFSET(reg), value); }
    std::uint32_t dm_read(unsigned reg) { return bus_read(FU3_DBG_DM_BASE + FU3_DBG_DM_OFFSET(reg)); }
    std::uint32_t dmstatus() { return dm_read(FU3_DBG_DMSTATUS); }
    void dm_control(std::uint32_t bits) { dm_write(FU3_DBG_DMCONTROL, FU3_DBG_DMCONTROL_DMACTIVE | FU3_DBG_DMCONTROL_HARTSEL(1) | bits); }
    // Abstract read of x5 (t0); returns cmderr.
    std::uint32_t dm_read_gpr()
    {
        dm_write(FU3_DBG_COMMAND, (2u << 20) | (1u << 17) | 0x1005);
        const auto cmderr = (dm_read(FU3_DBG_ABSTRACTCS) & FU3_DBG_ABSTRACTCS_CMDERR_MASK) >> FU3_DBG_ABSTRACTCS_CMDERR_SHIFT;
        dm_write(FU3_DBG_ABSTRACTCS, FU3_DBG_ABSTRACTCS_CMDERR_MASK);
        return cmderr;
    }
    bool wait_for(const std::function<bool()>& cond, sc_core::sc_time limit)
    {
        const auto end = sc_core::sc_time_stamp() + limit;
        while (!cond() && sc_core::sc_time_stamp() < end)
            wait(200, sc_core::SC_NS);
        return cond();
    }
    // Simulated time runs while the client thread talks to the server.
    void wait_until(const std::atomic<bool>& flag)
    {
        for (unsigned i = 0; i < 200000 && !flag; ++i)
            wait(1, sc_core::SC_US);
    }
    void wait_client(client& c)
    {
        c.release = true;
        wait_until(c.done);
        c.worker.join();
    }

    void run()
    {
        using owner = cpu_debug_hart::owner;
        auto& ibex = *top.debug_hart(1);
        wait(300, sc_core::SC_US);   // CRM startup released Ibex: in its ROM wfi loop
        check(!ibex.in_reset() && !ibex.cpu().debug_halted(), "Ibex runs");
        dm_control(0);

        // GDB owns Ibex: the DM cannot halt it nor read its registers.
        check(ibex.gdb_attach() && ibex.current_owner() == owner::gdb, "GDB attaches to a hart nobody controls");
        check(dmstatus() & FU3_DBG_DMSTATUS_ALLUNAVAIL, "DM sees the GDB-owned hart unavailable");
        dm_control(FU3_DBG_DMCONTROL_HALTREQ);
        wait(20, sc_core::SC_US);
        check(!ibex.cpu().debug_halted(), "DM halt request does not reach a GDB-owned hart");
        ibex.gdb_halt_request(true);
        check(wait_for([&] { return ibex.gdb_halted(); }, sc_core::sc_time(50, sc_core::SC_US)), "GDB halts Ibex");
        ibex.gdb_halt_request(false);
        check(dm_read_gpr() != 0, "DM abstract register access fails while GDB owns the hart");
        dm_control(FU3_DBG_DMCONTROL_HALTREQ | FU3_DBG_DMCONTROL_RESUMEREQ);
        dm_control(FU3_DBG_DMCONTROL_HALTREQ);
        wait(20, sc_core::SC_US);
        check(ibex.gdb_halted(), "DM resume request does not resume a GDB-owned hart");
        std::uint32_t pc = 0;
        check(ibex.gdb_access_register(0x7b1, false, pc) && pc >= 0x80 && pc < 0x94, "GDB reads dpc in the ROM loop");

        // Detach: the DM's halt request (still set) takes the hart over.
        ibex.gdb_detach();
        check(wait_for([&] { return ibex.cpu().debug_halted(); }, sc_core::sc_time(50, sc_core::SC_US)) &&
                  (dmstatus() & FU3_DBG_DMSTATUS_ALLHALTED) && ibex.current_owner() == owner::dm,
              "after GDB detach the pending DM halt request halts the hart, DM owns it");
        check(dm_read_gpr() == 0, "DM abstract register access works again");
        check(!ibex.gdb_attach() && ibex.current_owner() == owner::dm, "GDB attach refused while the DM owns the hart");

        // Through the TCP server: a client is closed while the DM owns the hart.
        client refused;
        refused.start(gdb.port(1), "$?#3f", "");
        wait_client(refused);
        check(refused.closed && refused.received.empty(), "TCP client closed while the DM owns the hart");

        // The DM resumes the hart and lets go: GDB can attach.
        dm_control(FU3_DBG_DMCONTROL_RESUMEREQ);
        dm_control(0);
        check(wait_for([&] { return !ibex.cpu().debug_halted(); }, sc_core::sc_time(50, sc_core::SC_US)) &&
                  ibex.current_owner() == owner::none,
              "DM resume releases the hart");
        client attached;
        attached.start(gdb.port(1), "$?#3f", "#", "$c#63");
        wait_until(attached.got);
        wait(50, sc_core::SC_US);
        check(attached.received.find("$S05#b8") != std::string::npos, "TCP attach halts the hart and answers S05");
        check(ibex.current_owner() == owner::gdb && !ibex.cpu().debug_halted() &&
                  (dmstatus() & FU3_DBG_DMSTATUS_ALLUNAVAIL),
              "after c the TCP client owns the running hart, the DM sees it unavailable");
        wait_client(attached);
        // Closing the connection detaches: the hart runs, nobody owns it.
        check(wait_for([&] { return ibex.current_owner() == owner::none && !ibex.cpu().debug_halted(); },
                       sc_core::sc_time(100, sc_core::SC_US)),
              "connection closed: detached and running");

        // A reset while GDB owns the hart halts it on its first instruction.
        check(ibex.gdb_attach(), "GDB attaches again");
        const auto resets = ibex.resets();
        top.drive_watchdog_request(true);
        wait(1, sc_core::SC_US);
        top.drive_watchdog_request(false);
        check(wait_for([&] { return ibex.gdb_halted(); }, sc_core::sc_time(500, sc_core::SC_US)),
              "after a watchdog reset the GDB-owned hart halts");
        check(ibex.resets() > resets && ibex.reset_halt_pending(), "reset counted, re-arm halt pending");
        check(ibex.gdb_access_register(0x7b1, false, pc) && pc == 0x80, "halted on its first instruction (dpc 0x80)");
        ibex.clear_reset_halt();
        ibex.gdb_detach();
        check(wait_for([&] { return !ibex.cpu().debug_halted(); }, sc_core::sc_time(50, sc_core::SC_US)),
              "detach after the reset: the hart runs (DM request was cleared by the DBG reset)");
        finished = true;
        sc_core::sc_stop();
    }
};

} // namespace

int sc_main(int, char**)
{
    bench b("bench");
    sc_core::sc_start(sc_core::sc_time(100, sc_core::SC_MS));
    b.check(b.finished, "bench reached its end");
    std::cout << "fu3_soc gdb arbitration: " << b.checks << " checks, " << b.failures << " failures -> "
              << (b.failures ? "FAIL" : "PASS") << '\n';
    return b.failures ? 1 : 0;
}
