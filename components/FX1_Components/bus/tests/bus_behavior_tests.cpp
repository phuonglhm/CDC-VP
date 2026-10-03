#include "bus/bus_system.h"
#include "support/mock_targets.h"
#include <tlm_utils/simple_initiator_socket.h>
#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

class Testbench : public sc_core::sc_module {
public:
    using Socket = tlm_utils::simple_initiator_socket<Testbench>;
    Socket dbg{"DBG"}, cpu{"CPU"}, dma{"DMA"};
    bus::BusSystem dut;
    bus::test::MockTargets targets;
    unsigned checks = 0, failures = 0;
    bool finished = false;
    SC_HAS_PROCESS(Testbench);
    Testbench(sc_core::sc_module_name name, bool trace)
        : sc_module(name), dut("bus", trace), targets("targets", dut) {
        dbg.bind(dut.target);
        cpu.bind(dut.target);
        dma.bind(dut.target);
        targets.rom.load(0, {0x13, 0x00, 0x00, 0x00});
        SC_THREAD(run);
        SC_THREAD(dbg_worker);
        SC_THREAD(dma_worker);
        SC_THREAD(watchdog);
    }
private:
    void start_of_simulation() override {
        // No SC_THREAD is active here: any accidental wait in debug is an error.
        using namespace bus::config;
        std::vector<std::uint64_t> addresses{ROM+0x100, ISRAM+0x100, DSRAM+0x100, AES+0x100, QSPI+0x100};
        for (unsigned i = 0; i < PERIPHERALS; ++i) {
            addresses.push_back(PP1+i*PERIPHERAL_SIZE+0x100);
            addresses.push_back(PP0+i*PERIPHERAL_SIZE+0x100);
        }
        for (auto address : addresses) {
            unsigned char bytes[8] = {1,2,3,4,5,6,7,8};
            tlm::tlm_generic_payload tx;
            tx.set_address(address);
            tx.set_data_ptr(bytes);
            tx.set_data_length(8);
            // Debug is not subject to functional streaming/APB beat restrictions.
            tx.set_command(tlm::TLM_WRITE_COMMAND);
            expect(dbg->transport_dbg(tx) == 8, "debug write all routes including ROM/APB");
            expect(tx.get_address() == address, "debug address restored");
            std::fill(std::begin(bytes), std::end(bytes), 0);
            tx.set_command(tlm::TLM_READ_COMMAND);
            expect(dbg->transport_dbg(tx) == 8 && bytes[0] == 1 && bytes[7] == 8, "debug readback");
            expect(sc_core::sc_time_stamp() == sc_core::SC_ZERO_TIME, "debug is untimed");
        }
        unsigned char bytes[8]{};
        tlm::tlm_generic_payload tx;
        tx.set_data_ptr(bytes); tx.set_data_length(8); tx.set_command(tlm::TLM_WRITE_COMMAND);
        for (auto address : {ROM+0xfffc, std::uint64_t(0x30000000), PP0+0x8000}) {
            tx.set_address(address);
            expect(dbg->transport_dbg(tx) == 0 && tx.get_response_status() == tlm::TLM_ADDRESS_ERROR_RESPONSE,
                   "debug unmapped/crossing rejected");
        }
        expect(dut.sysbus1.forwarded(0) == 0, "debug does not change functional statistics");
    }
    sc_core::sc_event start_workers_, workers_done_;
    unsigned done_ = 0;
    bool same_target_ = true;
    std::array<sc_core::sc_time, 3> completion_;

    void expect(bool condition, const std::string& message) {
        ++checks;
        if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
    }
    tlm::tlm_response_status transfer(Socket& socket, tlm::tlm_command command,
                                     std::uint64_t address, std::vector<unsigned char>& data,
                                     std::vector<unsigned char> enables = {},
                                     unsigned streaming = 0, unsigned initial_delay_ns = 0) {
        tlm::tlm_generic_payload tx;
        tx.set_command(command);
        tx.set_address(address);
        tx.set_data_ptr(data.data());
        tx.set_data_length(static_cast<unsigned>(data.size()));
        tx.set_streaming_width(streaming ? streaming : static_cast<unsigned>(data.size()));
        tx.set_byte_enable_ptr(enables.empty() ? nullptr : enables.data());
        tx.set_byte_enable_length(static_cast<unsigned>(enables.size()));
        tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time delay(initial_delay_ns, sc_core::SC_NS);
        socket->b_transport(tx, delay);
        expect(tx.get_address() == address, "global address restored after forwarding");
        expect(delay == sc_core::SC_ZERO_TIME, "annotated delay consumed exactly once");
        expect(!tx.is_dmi_allowed(), "DMI disabled so routing/timing cannot be bypassed");
        return tx.get_response_status();
    }
    void roundtrip(std::uint64_t address, unsigned char seed, unsigned count = 4) {
        std::vector<unsigned char> written(count), read(count, 0);
        for (unsigned i = 0; i < count; ++i) written[i] = static_cast<unsigned char>(seed+i);
        expect(transfer(cpu, tlm::TLM_WRITE_COMMAND, address, written) == tlm::TLM_OK_RESPONSE,
               "write completes");
        expect(transfer(cpu, tlm::TLM_READ_COMMAND, address, read) == tlm::TLM_OK_RESPONSE,
               "read completes");
        expect(read == written, "readback matches written bytes");
    }
    void worker(Socket& socket, unsigned slot) {
        while (true) {
            wait(start_workers_);
            const std::uint64_t address = same_target_ ? bus::config::ISRAM + slot*16 :
                (slot == 0 ? bus::config::ISRAM : bus::config::AES);
            std::vector<unsigned char> data(4, static_cast<unsigned char>(0xA0+slot));
            expect(transfer(socket, tlm::TLM_WRITE_COMMAND, address, data) == tlm::TLM_OK_RESPONSE,
                   "concurrent worker completes");
            completion_[slot] = sc_core::sc_time_stamp();
            ++done_;
            workers_done_.notify(sc_core::SC_ZERO_TIME);
        }
    }
    void dbg_worker() { worker(dbg, 0); }
    void dma_worker() { worker(dma, 2); }
    void concurrent(bool same) {
        same_target_ = same;
        done_ = 0;
        const auto start = sc_core::sc_time_stamp();
        start_workers_.notify(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);
        std::vector<unsigned char> data(4, 0xA1);
        expect(transfer(cpu, tlm::TLM_WRITE_COMMAND,
                        same ? bus::config::ISRAM+16 : bus::config::DSRAM, data) == tlm::TLM_OK_RESPONSE,
               "concurrent CPU completes");
        completion_[1] = sc_core::sc_time_stamp();
        while (done_ != 2) wait(workers_done_);
        expect(sc_core::sc_time_stamp()-start == sc_core::sc_time(same ? 36 : 12, sc_core::SC_NS),
               same ? "same output serializes 3 transactions" : "independent outputs overlap");
        if (same) {
            auto ordered = completion_;
            std::sort(ordered.begin(), ordered.end());
            for (unsigned i = 0; i < 3; ++i) {
                expect(ordered[i]-start == sc_core::sc_time(12*(i+1), sc_core::SC_NS),
                       "one completion per output service interval");
                std::vector<unsigned char> read(4, 0);
                expect(transfer(cpu, tlm::TLM_READ_COMMAND, bus::config::ISRAM+i*16, read) == tlm::TLM_OK_RESPONSE,
                       "concurrent data can be read");
                expect(read == std::vector<unsigned char>(4, static_cast<unsigned char>(0xA0+i)),
                       "initiators do not corrupt each other's data");
            }
        }
    }
    void watchdog() {
        wait(1, sc_core::SC_MS);
        expect(false, "simulation timed out / possible deadlock");
        sc_core::sc_stop();
    }
    void run() {
        using namespace bus::config;
        for (auto address : {ISRAM, DSRAM, AES, QSPI}) roundtrip(address, 0x31);
        for (unsigned i = 0; i < PERIPHERALS; ++i) {
            roundtrip(PP1+i*PERIPHERAL_SIZE, static_cast<unsigned char>(0x40+i));
            roundtrip(PP0+i*PERIPHERAL_SIZE, static_cast<unsigned char>(0x50+i));
        }
        roundtrip(DSRAM+0x100, 0x60, 64); // contiguous burst-like payload
        roundtrip(ISRAM+3, 0x20, 7); // byte-addressable SRAM
        roundtrip(PP1+8, 0x30, 1);
        roundtrip(PP0+8, 0x30, 2);
        roundtrip(ISRAM+0x1fffc, 0xE0); // final valid word

        std::vector<unsigned char> data(4, 0);
        expect(transfer(cpu, tlm::TLM_READ_COMMAND, ROM, data) == tlm::TLM_OK_RESPONSE && data[0] == 0x13,
               "ROM image loaded");
        data.assign(4, 0xFF);
        expect(transfer(cpu, tlm::TLM_WRITE_COMMAND, ROM, data) == tlm::TLM_COMMAND_ERROR_RESPONSE,
               "ROM writes rejected");
        expect(transfer(cpu, tlm::TLM_READ_COMMAND, ROM, data) == tlm::TLM_OK_RESPONSE &&
               data == std::vector<unsigned char>({0x13, 0, 0, 0}), "ROM unchanged after rejected write");

        for (auto address : {DSRAM+0x80, PP1+0x80, PP0+0x80}) {
            data = {1, 2, 3, 4};
            expect(transfer(cpu, tlm::TLM_WRITE_COMMAND, address, data) == tlm::TLM_OK_RESPONSE, "mask setup");
            data = {9, 9, 9, 9};
            expect(transfer(cpu, tlm::TLM_WRITE_COMMAND, address, data, {0xFF, 0}) == tlm::TLM_OK_RESPONSE,
                   "repeating write byte enables");
            data.assign(4, 0);
            expect(transfer(cpu, tlm::TLM_READ_COMMAND, address, data) == tlm::TLM_OK_RESPONSE &&
                   data == std::vector<unsigned char>({9, 2, 9, 4}), "disabled write bytes preserved");
            data.assign(4, 0xAA);
            expect(transfer(cpu, tlm::TLM_READ_COMMAND, address, data, {0, 0xFF}) == tlm::TLM_OK_RESPONSE &&
                   data == std::vector<unsigned char>({0xAA, 2, 0xAA, 4}), "disabled read bytes untouched");
        }
        for (auto address : std::vector<std::uint64_t>{0x30000000, SB0+0x20000, PP1+0x8000, PP0+0x8000,
                                                       ISRAM+0x20000, std::numeric_limits<std::uint64_t>::max()})
            expect(transfer(cpu, tlm::TLM_READ_COMMAND, address, data) == tlm::TLM_ADDRESS_ERROR_RESPONSE,
                   "unmapped address rejected at appropriate bus level");
        data.assign(8, 0x11);
        expect(transfer(cpu, tlm::TLM_WRITE_COMMAND, ISRAM+0x1fffc, data) == tlm::TLM_ADDRESS_ERROR_RESPONSE,
               "cross-boundary write rejected before partial write");
        data.assign(4, 0);
        expect(transfer(cpu, tlm::TLM_READ_COMMAND, ISRAM+0x1fffc, data) == tlm::TLM_OK_RESPONSE &&
               data == std::vector<unsigned char>({0xE0, 0xE1, 0xE2, 0xE3}), "boundary contents unchanged");
        expect(transfer(cpu, tlm::TLM_READ_COMMAND, DSRAM, data, {}, 2) == tlm::TLM_BURST_ERROR_RESPONSE,
               "streaming/wrapping payload explicitly unsupported");
        expect(transfer(cpu, tlm::TLM_WRITE_COMMAND, DSRAM, data, {1}) == tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,
               "invalid byte enable rejected");
        expect(transfer(cpu, tlm::TLM_IGNORE_COMMAND, DSRAM, data) == tlm::TLM_COMMAND_ERROR_RESPONSE,
               "unsupported command rejected");
        data.clear();
        expect(transfer(cpu, tlm::TLM_READ_COMMAND, DSRAM, data) == tlm::TLM_GENERIC_ERROR_RESPONSE,
               "empty payload rejected");
        data.assign(8, 0);
        expect(transfer(cpu, tlm::TLM_WRITE_COMMAND, PP0, data) == tlm::TLM_BURST_ERROR_RESPONSE,
               "APB burst rejected");
        data.assign(4, 0);
        expect(transfer(cpu, tlm::TLM_READ_COMMAND, PP1+1, data) == tlm::TLM_ADDRESS_ERROR_RESPONSE,
               "unaligned APB word rejected");

        // Updated workbook A25:D29: explicit write-response coverage at every
        // routing layer. TLM status models completion, not pin-level B signals.
        for (auto address : std::vector<std::uint64_t>{0x30000000, SB0+0x20000, PP1+0x8000, PP0+0x8000}) {
            data = {0x12, 0x34, 0x56, 0x78};
            const auto original = data;
            expect(transfer(cpu, tlm::TLM_WRITE_COMMAND, address, data) == tlm::TLM_ADDRESS_ERROR_RESPONSE,
                   "write decode-error response returns through each bus layer");
            expect(data == original, "write error response preserves initiator buffer");
        }
        for (auto base : {PP1, PP0}) {
            data.assign(4, 0x5A);
            expect(transfer(cpu, tlm::TLM_WRITE_COMMAND, base+1, data) == tlm::TLM_ADDRESS_ERROR_RESPONSE,
                   "both APB bridges return unaligned write error");
            data.assign(8, 0x5A);
            expect(transfer(cpu, tlm::TLM_WRITE_COMMAND, base, data) == tlm::TLM_BURST_ERROR_RESPONSE,
                   "both APB bridges return unsupported write-size error");
        }
        data.assign(4, 0);
        for (const auto& item : std::vector<std::pair<std::uint64_t, unsigned>>{
                 {ISRAM, 12}, {AES, 9}, {QSPI, 14}, {PP1, 29}, {PP0, 31}}) {
            const auto start = sc_core::sc_time_stamp();
            expect(transfer(cpu, tlm::TLM_READ_COMMAND, item.first, data, {}, 0, 7) == tlm::TLM_OK_RESPONSE,
                   "timed read completes");
            expect(sc_core::sc_time_stamp()-start == sc_core::sc_time(item.second+7, sc_core::SC_NS),
                   "route latency and input delay accounted for");
        }
        concurrent(true);
        concurrent(false);
        for (unsigned i = 0; i < 5; ++i) expect(dut.sysbus1.forwarded(i) > 0, "all SYSBUS_1 outputs exercised");
        for (unsigned i = 0; i < 3; ++i) expect(dut.sysbus0.forwarded(i) > 0, "all SYSBUS_0 outputs exercised");
        for (unsigned i = 0; i < PERIPHERALS; ++i) {
            expect(dut.peribus1.forwarded(i) > 0, "all PERIBUS_1 outputs exercised");
            expect(dut.peribus0.forwarded(i) > 0, "all PERIBUS_0 outputs exercised");
        }
        finished = true;
        std::cout << (failures ? "FAIL" : "PASS") << ": " << checks << " checks, " << failures
                  << " failures; simulation time " << sc_core::sc_time_stamp() << '\n';
        sc_core::sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    const bool trace = argc > 1 && std::string(argv[1]) == "--trace";
    Testbench tb("tb", trace);
    sc_core::sc_start();
    return tb.finished && tb.failures == 0 ? 0 : 1;
}
