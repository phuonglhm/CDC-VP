// Unit tests of fx1::ExclusiveMonitor and fx1::WriteGuard (plan C7), with
// hand-driven SystemC threads standing in for the harts and a DMA master, so
// every interleaving is fixed in simulated time.
#include <cstdint>
#include <cstring>
#include <iostream>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "fx1/exclusive_monitor.h"

using namespace sc_core;

namespace {

int failures = 0;
#define CHECK(cond)                                                                                 \
    do {                                                                                            \
        if (!(cond)) {                                                                              \
            std::cerr << "CHECK failed: " #cond " @ line " << __LINE__ << " t=" << sc_time_stamp()  \
                      << '\n';                                                                      \
            ++failures;                                                                             \
        }                                                                                           \
    } while (0)

constexpr unsigned kHarts = 2;
constexpr unsigned kDma = 16; // non-CPU master ID
constexpr std::uint64_t kGranule = 64;
constexpr std::uint64_t kWord = 0x1000;

// Memory that takes 30 ns per access, through wait(), like a contended DDR port.
struct SlowMemory : sc_module {
    tlm_utils::simple_target_socket<SlowMemory> socket{"socket"};
    std::uint8_t data[0x4000] = {};
    explicit SlowMemory(sc_module_name n) : sc_module(n)
    {
        socket.register_b_transport(this, &SlowMemory::b_transport);
    }
    void b_transport(tlm::tlm_generic_payload& tx, sc_time& delay)
    {
        wait(delay + sc_time(30, SC_NS));
        delay = SC_ZERO_TIME;
        std::uint8_t* p = data + tx.get_address();
        if (tx.is_write())
            std::memcpy(p, tx.get_data_ptr(), tx.get_data_length());
        else
            std::memcpy(tx.get_data_ptr(), p, tx.get_data_length());
        tx.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

struct Bench : sc_module {
    fx1::ExclusiveMonitor monitor{kHarts, kGranule};
    tlm_utils::simple_initiator_socket<Bench> dma{"dma"};
    fx1::WriteGuard guard;
    SlowMemory mem{"mem"};

    SC_HAS_PROCESS(Bench);
    explicit Bench(sc_module_name n) : sc_module(n), guard("guard", monitor, kDma)
    {
        dma.bind(guard.target);
        guard.out.bind(mem.socket);
        SC_THREAD(run);
    }

    sc_time dma_access(tlm::tlm_command cmd, std::uint64_t addr, std::uint32_t& value)
    {
        tlm::tlm_generic_payload tx;
        sc_time delay = SC_ZERO_TIME;
        tx.set_command(cmd);
        tx.set_address(addr);
        tx.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        tx.set_data_length(4);
        tx.set_streaming_width(4);
        tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        const sc_time start = sc_time_stamp();
        dma->b_transport(tx, delay);
        CHECK(tx.is_response_ok());
        return sc_time_stamp() - start;
    }

    // A CPU-side write bracket that takes `hold` simulated time.
    void cpu_write(unsigned hart, std::uint64_t addr, sc_time hold)
    {
        std::uint64_t t = 0;
        while (!monitor.try_begin_write(hart, addr, 4, t)) wait(monitor.changed());
        wait(hold);
        monitor.end_write(t);
    }

    void reservations()
    {
        std::uint64_t t = 0, base = 0;
        // SC without a reservation fails; a reservation is consumed by its SC.
        CHECK(!monitor.take_reservation(0, kWord, 4));
        monitor.load_reserved(0, kWord + 8, 4);
        CHECK(monitor.reserved(0, &base) && base == kWord);
        CHECK(monitor.take_reservation(0, kWord + 8, 4));
        CHECK(!monitor.take_reservation(0, kWord + 8, 4));
        // SC to another granule than the LR fails and consumes the reservation.
        monitor.load_reserved(0, kWord, 4);
        CHECK(!monitor.take_reservation(0, kWord + kGranule, 4));
        CHECK(!monitor.reserved(0));

        // Another hart's write: same word, same granule other word -> cancelled.
        for (std::uint64_t at : {kWord, kWord + 60}) {
            monitor.load_reserved(0, kWord, 4);
            CHECK(monitor.try_begin_write(1, at, 4, t));
            monitor.end_write(t);
            CHECK(!monitor.reserved(0));
        }
        // ... next granule or the hart's own write -> kept.
        monitor.load_reserved(0, kWord, 4);
        CHECK(monitor.try_begin_write(1, kWord + kGranule, 4, t));
        monitor.end_write(t);
        CHECK(monitor.try_begin_write(0, kWord, 4, t));
        monitor.end_write(t);
        CHECK(monitor.reserved(0));
        // A write straddling into the granule from below cancels it.
        CHECK(monitor.try_begin_write(kDma, kWord - 2, 4, t));
        monitor.end_write(t);
        CHECK(!monitor.reserved(0));
        CHECK(monitor.stats().cancelled_by_cpu == 2 && monitor.stats().cancelled_by_device == 1);

        // A faulted LR's reservation is dropped: the SC that follows fails.
        monitor.load_reserved(0, kWord, 4);
        monitor.drop_reservation(0);
        CHECK(!monitor.reserved(0));
        CHECK(!monitor.take_reservation(0, kWord, 4));
        // Byte granularity at the granule edges: a 1-byte write to the first
        // byte of the next granule keeps it, one to the last byte cancels it.
        monitor.load_reserved(0, kWord, 4);
        CHECK(monitor.try_begin_write(kDma, kWord + kGranule, 1, t));
        monitor.end_write(t);
        CHECK(monitor.reserved(0));
        CHECK(monitor.try_begin_write(kDma, kWord + kGranule - 1, 1, t));
        monitor.end_write(t);
        CHECK(!monitor.reserved(0));
        CHECK(monitor.try_begin_write(kDma, kWord - 1, 1, t)); // byte just below
        monitor.end_write(t);
        monitor.load_reserved(0, kWord, 4);
        CHECK(monitor.try_begin_write(kDma, kWord - 1, 1, t));
        monitor.end_write(t);
        CHECK(monitor.reserved(0));
        monitor.drop_reservation(0);

        // Reset drops the reservation and the bracket.
        monitor.load_reserved(1, kWord, 4);
        monitor.atomic_begin(1, kWord, 4);
        monitor.reset_hart(1);
        CHECK(!monitor.reserved(1) && !monitor.bracket_open(1));
        CHECK(monitor.writes_in_flight() == 0);
    }

    void brackets()
    {
        std::uint64_t t = 0;
        // An open bracket refuses another master's overlapping write, admits
        // its owner's and any write outside the granule.
        monitor.atomic_begin(0, kWord, 4);
        CHECK(!monitor.try_begin_write(1, kWord + 4, 4, t));
        CHECK(!monitor.try_begin_write(kDma, kWord, 4, t));
        CHECK(monitor.try_begin_write(kDma, kWord + kGranule, 4, t));
        monitor.end_write(t);
        CHECK(monitor.try_begin_write(0, kWord, 4, t));
        monitor.end_write(t);
        monitor.atomic_end(0);
        CHECK(monitor.try_begin_write(kDma, kWord, 4, t));
        monitor.end_write(t);
    }

    void run()
    {
        reservations();
        brackets();
        wait(10, SC_NS);

        // atomic_begin drains an overlapping write already in flight: hart 1
        // writes for 50 ns from t0, hart 0 opens its bracket at t0 + 10 ns and
        // must not get it before t0 + 50 ns.
        {
            const sc_time t0 = sc_time_stamp();
            sc_spawn([this] { cpu_write(1, kWord + 4, sc_time(50, SC_NS)); });
            wait(10, SC_NS);
            monitor.atomic_begin(0, kWord, 4);
            CHECK(sc_time_stamp() >= t0 + sc_time(50, SC_NS));
            CHECK(monitor.stats().brackets_waited == 1);
            monitor.atomic_end(0);
        }

        // WriteGuard: a DMA write into an open CPU bracket waits for it to
        // close; a DMA read does not wait; a DMA write cancels a reservation.
        {
            std::uint32_t v = 0xA5A5A5A5u;
            monitor.atomic_begin(0, kWord, 4);
            sc_event closed;
            sc_spawn([this, &closed] {
                wait(100, SC_NS);
                monitor.atomic_end(0);
                closed.notify();
            });
            std::uint32_t r = 0;
            const sc_time read_took = dma_access(tlm::TLM_READ_COMMAND, kWord, r);
            CHECK(read_took == sc_time(30, SC_NS));
            const sc_time t1 = sc_time_stamp();
            dma_access(tlm::TLM_WRITE_COMMAND, kWord, v);
            CHECK(sc_time_stamp() >= t1 + sc_time(70, SC_NS) + sc_time(30, SC_NS));
            CHECK(guard.writes_held() == 1 && guard.writes() == 1);
            std::uint32_t got = 0;
            std::memcpy(&got, mem.data + kWord, 4);
            CHECK(got == v);

            monitor.load_reserved(1, kWord + 32, 4);
            v = 1;
            dma_access(tlm::TLM_WRITE_COMMAND, kWord, v);
            CHECK(!monitor.reserved(1));
            CHECK(guard.writes_held() == 1);
            CHECK(monitor.writes_in_flight() == 0);
        }
        sc_stop();
    }
};

} // namespace

int sc_main(int, char*[])
{
    sc_report_handler::set_actions(SC_ID_INSTANCE_EXISTS_, SC_DO_NOTHING);
    Bench bench("bench");
    sc_start();
    if (failures) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "fx1 exclusive monitor: all checks passed\n";
    return 0;
}
