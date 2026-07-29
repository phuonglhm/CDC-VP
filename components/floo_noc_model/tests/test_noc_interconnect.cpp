// SPDX-License-Identifier: Apache-2.0
//
// Contract test for the TLM wrapper.
//
// The network underneath is RTL-signed, so what this test has to establish is
// that the wrapper drives it correctly: that a TLM payload becomes the right
// AXI transaction, that data survives the round trip, that a burst stays one
// packet, that distance costs cycles, and that debug access does not touch the
// network at all.

#include "floo_noc_model/noc_interconnect.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>

namespace {

int failures = 0;

void check(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

/// A byte-addressed memory with an optional access latency, so the wrapper's
/// handling of a target's own delay is exercised.
class memory_target : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<memory_target> socket;

    SC_HAS_PROCESS(memory_target);

    memory_target(
        sc_core::sc_module_name name, std::size_t bytes,
        sc_core::sc_time latency = sc_core::SC_ZERO_TIME)
        : sc_core::sc_module(name)
        , socket("socket")
        , storage_(bytes, 0)
        , latency_(latency)
    {
        socket.register_b_transport(this, &memory_target::b_transport);
        socket.register_transport_dbg(this, &memory_target::transport_dbg);
    }

    std::vector<unsigned char>& storage() { return storage_; }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        access(trans);
        delay += latency_;
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        access(trans);
        return trans.get_data_length();
    }

    void access(tlm::tlm_generic_payload& trans)
    {
        const auto address = static_cast<std::size_t>(trans.get_address());
        const auto length = trans.get_data_length();
        if (address + length > storage_.size()) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        if (trans.is_write()) {
            std::memcpy(storage_.data() + address, trans.get_data_ptr(), length);
        } else {
            std::memcpy(trans.get_data_ptr(), storage_.data() + address, length);
        }
    }

    std::vector<unsigned char> storage_;
    sc_core::sc_time latency_;
};

constexpr std::uint64_t near_base = 0x8000'0000;
constexpr std::uint64_t far_base = 0x9000'0000;
constexpr std::uint64_t region_size = 0x1000;

/// A second AXI manager on the mesh, from its own node. The platform puts the
/// DMA on one of these, so the path has to be exercised: two managers issuing
/// concurrently share links and contend for the same target.
class second_manager : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<second_manager> socket;
    bool finished = false;
    unsigned completed = 0;

    SC_HAS_PROCESS(second_manager);

    explicit second_manager(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
        SC_THREAD(run);
    }

private:
    void run()
    {
        for (unsigned index = 0; index < 8; ++index) {
            std::uint64_t value = 0xA000 + index;
            unsigned char bytes[8] = {};
            std::memcpy(bytes, &value, sizeof(value));

            tlm::tlm_generic_payload trans;
            trans.set_command(tlm::TLM_WRITE_COMMAND);
            trans.set_address(far_base + 0x200 + index * 8);
            trans.set_data_ptr(bytes);
            trans.set_data_length(sizeof(value));
            trans.set_streaming_width(sizeof(value));
            trans.set_byte_enable_ptr(nullptr);
            trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            socket->b_transport(trans, delay);
            check(trans.is_response_ok(),
                  "the second manager's access must complete");
            ++completed;
        }
        finished = true;
    }
};

class driver : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<driver> socket;
    cdc::components::noc_interconnect* noc = nullptr;
    memory_target* near_memory = nullptr;
    memory_target* far_memory = nullptr;
    second_manager* other = nullptr;

    SC_HAS_PROCESS(driver);

    explicit driver(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
        SC_THREAD(run);
    }

private:
    /// One blocking access. Returns the simulated time it took, which for this
    /// wrapper is spent rather than annotated.
    sc_core::sc_time access(
        bool write, std::uint64_t address, unsigned char* bytes,
        unsigned length)
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
        socket->b_transport(trans, delay);
        const auto elapsed = sc_core::sc_time_stamp() - before;

        check(trans.is_response_ok(),
              "the access must complete with an OK response");
        return elapsed;
    }

    void run()
    {
        // ---- A single-beat write, read back through the network -----------
        std::uint64_t written = 0xDEAD'BEEF'CAFE'F00Dull;
        unsigned char buffer[64] = {};
        std::memcpy(buffer, &written, sizeof(written));
        const auto write_time =
            access(true, near_base + 0x40, buffer, sizeof(written));
        check(write_time > sc_core::SC_ZERO_TIME,
              "a cycle-accurate interconnect must consume simulated time");

        std::memset(buffer, 0, sizeof(buffer));
        access(false, near_base + 0x40, buffer, sizeof(written));
        std::uint64_t read_back = 0;
        std::memcpy(&read_back, buffer, sizeof(read_back));
        check(read_back == written,
              "the value read back must be the value written");

        // The write really reached the peripheral, not just the model.
        std::uint64_t in_memory = 0;
        std::memcpy(&in_memory, near_memory->storage().data() + 0x40,
                    sizeof(in_memory));
        check(in_memory == written,
              "the write must land in the target's own storage");

        // ---- A burst: four beats in one packet ----------------------------
        std::uint64_t burst[4] = {0x1111'1111, 0x2222'2222, 0x3333'3333,
                                  0x4444'4444};
        std::memcpy(buffer, burst, sizeof(burst));
        access(true, near_base + 0x80, buffer, sizeof(burst));

        std::memset(buffer, 0, sizeof(buffer));
        access(false, near_base + 0x80, buffer, sizeof(burst));
        std::uint64_t burst_back[4] = {};
        std::memcpy(burst_back, buffer, sizeof(burst_back));
        for (unsigned beat = 0; beat < 4; ++beat) {
            check(burst_back[beat] == burst[beat],
                  "every beat of the burst must survive the round trip");
        }

        // ---- Distance costs cycles ----------------------------------------
        const auto near_time =
            access(false, near_base, buffer, sizeof(std::uint64_t));
        const auto far_time =
            access(false, far_base, buffer, sizeof(std::uint64_t));
        check(far_time > near_time,
              "a farther target must cost more than a nearer one");
        std::cout << "near " << near_time << ", far " << far_time << '\n';

        // ---- Debug access bypasses the network ----------------------------
        const auto before = sc_core::sc_time_stamp();
        tlm::tlm_generic_payload dbg;
        std::uint64_t probe = 0;
        dbg.set_command(tlm::TLM_READ_COMMAND);
        dbg.set_address(near_base + 0x40);
        dbg.set_data_ptr(reinterpret_cast<unsigned char*>(&probe));
        dbg.set_data_length(sizeof(probe));
        const auto served = socket->transport_dbg(dbg);
        check(served == sizeof(probe), "debug access must serve every byte");
        check(probe == written, "debug access must see the stored value");
        check(sc_core::sc_time_stamp() == before,
              "debug access must not consume simulated time");

        // ---- An unmapped address is an address error ----------------------
        tlm::tlm_generic_payload bad;
        bad.set_command(tlm::TLM_READ_COMMAND);
        bad.set_address(0xDEAD'0000);
        bad.set_data_ptr(buffer);
        bad.set_data_length(sizeof(std::uint64_t));
        bad.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        socket->b_transport(bad, delay);
        check(bad.get_response_status() == tlm::TLM_ADDRESS_ERROR_RESPONSE,
              "an unmapped address must be reported, not routed");

        // ---- Two managers on the mesh --------------------------------------
        //
        // The second manager has been writing to the far memory concurrently.
        // Wait for it, then check every one of its writes landed: two managers
        // sharing links must not corrupt or lose each other's traffic.
        while (!other->finished) {
            wait(sc_core::sc_time(20, sc_core::SC_NS));
        }
        check(other->completed == 8, "every second-manager write must complete");
        for (unsigned index = 0; index < 8; ++index) {
            std::uint64_t stored = 0;
            std::memcpy(&stored,
                        far_memory->storage().data() + 0x200 + index * 8,
                        sizeof(stored));
            check(stored == 0xA000 + index,
                  "each second-manager write must land at its own address");
        }

        check(noc->completed_transactions() >= 6,
              "the wrapper must count the transactions it completed");
        check(noc->total_latency_cycles() > 0,
              "the wrapper must accumulate measured network cycles");

        sc_core::sc_stop();
    }
};

} // namespace

int sc_main(int, char**)
{
    // A 2x2 mesh: initiator at (0,0), a near memory at (1,0) and a far one at
    // (1,1), so distance is observable.
    // Two upstream ports: the driver at (0,0) and a second manager at (0,1),
    // matching how the platform places its CPU and DMA on separate nodes.
    cdc::components::noc_interconnect noc{"noc", 2, 2, 2, 2};
    memory_target near_memory{"near_memory", region_size};
    memory_target far_memory{"far_memory", region_size,
                             sc_core::sc_time(5, sc_core::SC_NS)};
    driver cpu{"cpu"};
    second_manager dma{"dma"};

    noc.place_initiator(0, {0, 0});
    noc.place_initiator(1, {0, 1});
    cpu.socket.bind(noc.target_socket);
    dma.socket.bind(noc.cpu_port(1));
    noc.add_target(near_base, region_size, {1, 0}).bind(near_memory.socket);
    noc.add_target(far_base, region_size, {1, 1}).bind(far_memory.socket);

    cpu.noc = &noc;
    cpu.near_memory = &near_memory;
    cpu.far_memory = &far_memory;
    cpu.other = &dma;

    sc_core::sc_start();

    if (failures == 0) {
        std::cout << "PASS: NoC TLM interconnect\n";
        return 0;
    }
    std::cerr << "FAIL: " << failures << " interconnect checks failed\n";
    return 1;
}
