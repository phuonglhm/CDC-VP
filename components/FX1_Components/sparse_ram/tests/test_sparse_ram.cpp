#include "fx1/sparse_ram.h"
#include <tlm_utils/simple_initiator_socket.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
unsigned checks = 0, failures = 0;
void expect(bool ok, const std::string& what) {
    ++checks;
    if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; }
}

void storage_tests() {
    fx1::SparseStorage s(512ull << 20, 0xA5);
    std::vector<std::uint8_t> buf(8, 0);
    s.read((512ull << 20) - 8, buf.data(), buf.size());
    expect(buf == std::vector<std::uint8_t>(8, 0xA5), "unwritten bytes read as fill");
    expect(s.resident_pages() == 0, "reads never allocate");

    std::vector<std::uint8_t> data(100);
    for (unsigned i = 0; i < data.size(); ++i) data[i] = static_cast<std::uint8_t>(i);
    s.write(4096 - 50, data.data(), data.size());  // spans two pages
    expect(s.resident_pages() == 2, "page-crossing write touches two pages");
    std::vector<std::uint8_t> back(100, 0);
    s.read(4096 - 50, back.data(), back.size());
    expect(back == data, "page-crossing readback");

    const std::uint8_t enables[2] = {0xFF, 0x00};
    std::vector<std::uint8_t> ones(4, 0x11);
    s.write_masked(0, ones.data(), 4, enables, 2);
    s.read(0, back.data(), 4);
    expect(back[0] == 0x11 && back[1] == 0xA5 && back[2] == 0x11 && back[3] == 0xA5,
           "masked write keeps disabled bytes");
    std::vector<std::uint8_t> dst(4, 0x77);
    s.read_masked(0, dst.data(), 4, enables, 2);
    expect(dst[0] == 0x11 && dst[1] == 0x77, "masked read leaves disabled bytes untouched");

    bool threw = false;
    try { s.read(512ull << 20, back.data(), 1); } catch (const std::out_of_range&) { threw = true; }
    expect(threw, "read past end throws");
    expect(!s.contains(~0ull, 2), "contains() guards overflow");
    s.clear();
    expect(s.resident_pages() == 0, "clear drops pages");
}

struct Tb : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<Tb> ram_port{"ram_port"}, rom_port{"rom_port"};
    fx1::SparseRam ram{"ram", 512ull << 20, sc_core::sc_time(10, sc_core::SC_NS)};
    fx1::SparseRam rom{"rom", 0x1000, sc_core::sc_time(5, sc_core::SC_NS), true};
    SC_HAS_PROCESS(Tb);
    explicit Tb(sc_core::sc_module_name n) : sc_module(n) {
        ram_port.bind(ram.socket);
        rom_port.bind(rom.socket);
        SC_THREAD(run);
    }
    tlm::tlm_response_status access(tlm_utils::simple_initiator_socket<Tb>& port, tlm::tlm_command cmd,
                                    std::uint64_t address, std::vector<std::uint8_t>& data,
                                    sc_core::sc_time& delay, std::vector<std::uint8_t> enables = {}) {
        tlm::tlm_generic_payload tx;
        tx.set_command(cmd);
        tx.set_address(address);
        tx.set_data_ptr(data.data());
        tx.set_data_length(static_cast<unsigned>(data.size()));
        tx.set_streaming_width(static_cast<unsigned>(data.size()));
        tx.set_byte_enable_ptr(enables.empty() ? nullptr : enables.data());
        tx.set_byte_enable_length(static_cast<unsigned>(enables.size()));
        tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        port->b_transport(tx, delay);
        expect(!tx.is_dmi_allowed(), "DMI never granted");
        return tx.get_response_status();
    }
    void run() {
        sc_core::sc_time delay;
        std::vector<std::uint8_t> w{1, 2, 3, 4}, r(4, 0);
        const std::uint64_t top = (512ull << 20) - 4;
        expect(access(ram_port, tlm::TLM_WRITE_COMMAND, top, w, delay) == tlm::TLM_OK_RESPONSE, "write last word");
        expect(delay == sc_core::sc_time(10, sc_core::SC_NS), "latency annotated, not waited");
        expect(access(ram_port, tlm::TLM_READ_COMMAND, top, r, delay) == tlm::TLM_OK_RESPONSE && r == w,
               "read last word");
        expect(ram.storage().resident_pages() == 1, "512 MiB RAM holds one page");
        std::vector<std::uint8_t> eight(8, 0);
        expect(access(ram_port, tlm::TLM_WRITE_COMMAND, top, eight, delay) == tlm::TLM_ADDRESS_ERROR_RESPONSE,
               "access crossing the end rejected");
        r.assign(4, 0);
        expect(access(ram_port, tlm::TLM_READ_COMMAND, top, r, delay) == tlm::TLM_OK_RESPONSE && r == w,
               "rejected access wrote nothing");
        std::vector<std::uint8_t> nine{9, 9, 9, 9};
        expect(access(ram_port, tlm::TLM_WRITE_COMMAND, top, nine, delay, {0x00, 0xFF}) == tlm::TLM_OK_RESPONSE,
               "byte-enabled write");
        access(ram_port, tlm::TLM_READ_COMMAND, top, r, delay);
        expect(r == std::vector<std::uint8_t>({1, 9, 3, 9}), "byte enables honoured");
        expect(access(ram_port, tlm::TLM_WRITE_COMMAND, 0, nine, delay, {0x01}) ==
                   tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE, "invalid byte-enable value rejected");

        expect(access(rom_port, tlm::TLM_WRITE_COMMAND, 0, w, delay) == tlm::TLM_GENERIC_ERROR_RESPONSE,
               "ROM write is a slave error");
        tlm::tlm_generic_payload dbg;
        std::vector<std::uint8_t> image{0x13, 0, 0, 0};
        dbg.set_command(tlm::TLM_WRITE_COMMAND);
        dbg.set_address(0);
        dbg.set_data_ptr(image.data());
        dbg.set_data_length(4);
        expect(rom_port->transport_dbg(dbg) == 4, "debug write preloads ROM");
        r.assign(4, 0);
        expect(access(rom_port, tlm::TLM_READ_COMMAND, 0, r, delay) == tlm::TLM_OK_RESPONSE && r == image,
               "ROM returns preloaded image");
        dbg.set_address(0xFFE);
        expect(rom_port->transport_dbg(dbg) == 2, "debug access clamped at the end");
        expect(sc_core::sc_time_stamp() == sc_core::SC_ZERO_TIME, "no access waited");
        sc_core::sc_stop();
    }
};
} // namespace

int sc_main(int, char**) {
    storage_tests();
    Tb tb("tb");
    sc_core::sc_start();
    std::cout << "fx1_sparse_ram: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
