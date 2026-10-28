#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <systemc>

namespace cdc::platforms::fu3_soc {

class fu3_soc_top;
class cpu_debug_hart;

// GDB remote serial protocol stub of the FU3 VP (plan T19, decision Q04-d):
// one TCP port per CPU on 127.0.0.1, one client each. It drives the hart
// through cpu_debug_hart, the control the Debug Module also uses, and owns the
// hart from attach to detach (the DM sees it unavailable meanwhile; attach is
// refused while the DM owns it).
//
//   - Attach halts the hart at its next instruction (after its reset release
//     when it is held in reset). Stop is Debug Mode: halt request, single step
//     (dcsr.step, interrupts masked), EBREAK with dcsr.ebreakm/ebreaku (Z0
//     software breakpoints patched into memory) and execute triggers (Z1
//     hardware breakpoints). No watchpoints (Q04-e: execute triggers only).
//   - Registers through abstract access: x0-x31, pc (dpc), CVA6 f0-f31 and
//     fflags/frm/fcsr, and a set of machine CSRs (target description).
//   - Memory through the hart's own bus port, as a debug transaction: the bus
//     map without access policy, no side effects, no simulated time; a target
//     without debug access answers an error.
//   - After a reset of the hart the stub halts it on its first instruction,
//     re-arms dcsr and the triggers and lets it run on (or keeps it stopped).
//   - Simulated time stands still while every attached hart is stopped (the
//     whole SoC waits for the debugger, timers and watchdogs included); while
//     one runs, the stub polls its clients every `poll` of simulated time.
//   - Detach (D) or a closed connection removes the breakpoints and lets the
//     hart run; kill (k) also stops the simulation.
//   - A packet with a wrong checksum is never executed: '-' in acknowledgement
//     mode (the client resends it), dropped in no-ack mode; a '-' from the
//     client resends the last packet.
class gdb_server : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(gdb_server);
    // ports[hart]: TCP port (0 = any free port), -1 = no server for that hart.
    // wait_for_clients: hold the simulation at time 0 until every server has a client.
    gdb_server(sc_core::sc_module_name name, fu3_soc_top& top, std::array<int, 2> ports, bool wait_for_clients,
               sc_core::sc_time poll = sc_core::sc_time(10, sc_core::SC_US));
    ~gdb_server() override;

    // Port the server of `hart` listens on (after the constructor), -1 if none.
    int port(unsigned hart) const { return conns_[hart].port; }
    // A client asked to kill the target (the simulation was stopped).
    bool killed() const { return killed_; }

private:
    enum class state { detached, halting, stopped, running, stepping };

    struct connection {
        unsigned hart = 0;
        const char* name = "";
        cpu_debug_hart* control = nullptr;
        int listen_fd = -1, fd = -1, port = -1;
        std::string in;            // received bytes not yet parsed
        bool ack = true;           // '+'/'-' acknowledgements (until QStartNoAckMode)
        bool head_acked = false;   // first packet of `in` acknowledged (it may wait for a stop)
        std::string last_sent;     // last packet sent, for a NACK ('-')
        state st = state::detached;
        int stop_signal = 5;       // reported when the halt completes
        bool interrupt = false;    // halt requested by ^C
        bool armed = false;        // dcsr.ebreakm/ebreaku set by the stub
        std::uint32_t dcsr_saved = 0;
        std::map<std::uint32_t, std::vector<std::uint8_t>> sw_breaks;   // address -> original bytes
        std::vector<std::uint32_t> hw_breaks;                           // trigger index -> address (0 free)
        std::vector<bool> hw_used;
    };

    fu3_soc_top& top_;
    std::array<connection, 2> conns_;
    bool wait_for_clients_;
    sc_core::sc_time poll_;
    bool killed_ = false;

    void run();
    void accept(connection& c);
    void close_client(connection& c, const char* why);
    bool receive(connection& c);
    void process(connection& c);
    void after_halt(connection& c);
    void detach(connection& c);
    void send_packet(connection& c, const std::string& body);
    std::string handle(connection& c, const std::string& packet, bool& defer);

    bool read_reg(connection& c, unsigned regnum, std::uint32_t& value);
    bool write_reg(connection& c, unsigned regnum, std::uint32_t value);
    bool csr(connection& c, std::uint16_t number, bool write, std::uint32_t& value);
    bool memory(connection& c, bool write, std::uint32_t address, std::uint8_t* data, unsigned length);
    void arm(connection& c);
    void disarm(connection& c);
    std::string set_sw_break(connection& c, std::uint32_t address, unsigned kind);
    std::string clear_sw_break(connection& c, std::uint32_t address);
    std::string set_hw_break(connection& c, std::uint32_t address, bool insert);
    void resume(connection& c, bool step);
    std::string target_xml(const connection& c) const;
};

} // namespace cdc::platforms::fu3_soc
