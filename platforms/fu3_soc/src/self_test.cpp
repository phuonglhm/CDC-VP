#include "self_test.h"

#include <algorithm>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <vector>

#include <fu3_bus/access.h>

namespace cdc::platforms::fu3_soc {

namespace {

std::string hex(std::uint64_t value)
{
    std::ostringstream s;
    s << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
    return s.str();
}

// First address not covered by any enabled target, word aligned, below 4 GiB.
// Derived from the configuration so the check follows the map when T04 changes it.
bool unmapped_address(const fu3_bus::BusConfig& cfg, std::uint64_t& address)
{
    std::vector<std::pair<std::uint64_t, std::uint64_t>> regions;
    for (const auto& t : cfg.targets)
        if (t.enabled)
            regions.emplace_back(t.base, t.base + t.size);
    std::sort(regions.begin(), regions.end());
    std::uint64_t candidate = 0;
    for (const auto& [begin, end] : regions) {
        if (candidate + 4 <= begin)
            break;
        candidate = std::max(candidate, end);
    }
    address = candidate;
    return candidate + 4 <= (std::uint64_t{1} << 32);
}

} // namespace

self_test::self_test(sc_core::sc_module_name name, fu3_soc_top& top, std::ostream& log)
    : sc_core::sc_module(name), top_(top), log_(log)
{
    socket.bind(top_.fabric().initiator("IBEX"));
    SC_THREAD(run);
}

void self_test::check(bool ok, const std::string& what)
{
    ++checks_;
    if (!ok) {
        ++failures_;
        log_ << "FAIL: " << what << '\n';
    }
}

tlm::tlm_response_status self_test::access(bool write, std::uint64_t address)
{
    unsigned char data[4] = {0x5a, 0x5a, 0x5a, 0x5a};
    tlm::tlm_generic_payload tx;
    tx.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
    tx.set_address(address);
    tx.set_data_ptr(data);
    tx.set_data_length(4);
    tx.set_streaming_width(4);
    tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    fu3_bus::ScopedAccess meta(tx, 4);
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    socket->b_transport(tx, delay);
    wait(delay);
    const auto status = tx.get_response_status();
    // Nothing may be written into the initiator buffer by a failed access.
    if (!write && status != tlm::TLM_OK_RESPONSE)
        check(data[0] == 0x5a && data[1] == 0x5a && data[2] == 0x5a && data[3] == 0x5a,
              "failed read at " + hex(address) + " leaves the data buffer untouched");
    return status;
}

unsigned self_test::debug_access(bool write, std::uint64_t address, unsigned char* data)
{
    tlm::tlm_generic_payload tx;
    tx.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
    tx.set_address(address);
    tx.set_data_ptr(data);
    tx.set_data_length(4);
    tx.set_streaming_width(4);
    return socket->transport_dbg(tx);
}

std::uint64_t self_test::total_hits() const
{
    std::uint64_t sum = 0;
    for (const auto& i : top_.instances())
        if (const auto* r = top_.responder(i.name))
            sum += r->accesses() + r->debug_accesses();
    return sum;
}

void self_test::run()
{
    const auto& cfg = top_.fabric().configuration();

    // Functional accesses need the system domain out of reset (PRSTN, CRM startup).
    while (!top_.system_ready())
        wait(100, sc_core::SC_NS);

    // 1. Decode: each target reaches its own model or responder at the local offset.
    for (const auto& i : top_.instances()) {
        const std::uint64_t offset = i.size >= 8 ? i.size - 4 : 0;
        if (auto* mem = top_.memory(i.name)) {
            check(i.integrated && mem->size() == i.size, i.name + ": memory spans the whole window");
            unsigned char pattern[4] = {0xa5, 0x5a, 0x3c, static_cast<unsigned char>(i.name.size())};
            const auto before = total_hits();
            check(debug_access(true, i.base + offset, pattern) == 4, i.name + ": loader writes its last word");
            bool landed = true;
            for (unsigned b = 0; b < 4; ++b)
                landed = landed && mem->peek(offset + b) == pattern[b];
            check(landed, i.name + ": loader bytes land at local offset " + hex(offset));
            check(total_hits() == before, i.name + ": loader access reaches no other block");
            continue;
        }
        if (const auto* p = top_.probe(i.name)) {
            // An integrated model: a register with a known reset value at its local offset.
            unsigned char data[4] = {};
            const auto before = total_hits();
            check(debug_access(false, i.base + p->offset, data) == 4, i.name + ": debug read of a register");
            const std::uint32_t value = data[0] | data[1] << 8 | data[2] << 16 | std::uint32_t(data[3]) << 24;
            check(i.integrated && value == p->value,
                  i.name + ": register at local offset " + hex(p->offset) + " reads " + hex(value) + ", expected " +
                      hex(p->value));
            check(total_hits() == before, i.name + ": debug read reaches no other block");
            continue;
        }
        if (i.integrated) {
            // A model without a debug-readable register (QSPI_XIP, HYPERBUS:
            // data windows behind their controllers; AP_DMA: no debug
            // transport); its decode is checked functionally.
            check(top_.responder(i.name) == nullptr, i.name + ": integrated, no responder");
            continue;
        }
        const auto* r = top_.responder(i.name);
        check(r != nullptr, i.name + " has a responder");
        if (!r)
            continue;
        const auto before = total_hits();
        const auto own = r->debug_accesses();
        unsigned char data[4] = {};
        const auto n = debug_access(false, i.base + offset, data);
        check(n == 0, i.name + ": debug read transfers no bytes");
        check(r->debug_accesses() == own + 1, i.name + ": debug read at " + hex(i.base + offset) + " reaches it");
        check(total_hits() == before + 1, i.name + ": debug read reaches no other block");
        check(r->last_offset() == offset, i.name + ": responder sees local offset " + hex(offset));
    }

    // 2. Functional access on each bus path: the not-integrated answer arrives
    // through the real routers and APB bridges.
    for (const auto path : {fu3_bus::TargetPath::SysBus1Axi, fu3_bus::TargetPath::SysBus0Axi,
                            fu3_bus::TargetPath::Peribus0Apb, fu3_bus::TargetPath::Peribus1Apb}) {
        const fu3_bus::TargetConfig* pick = nullptr;
        for (const auto& t : cfg.targets)
            if (t.enabled && t.path == path && t.access == fu3_bus::AccessPolicy::Open && top_.responder(t.name)) {
                pick = &t;
                break;
            }
        // A path whose open targets are all integrated models has no
        // responder left; its routing is covered by the model checks above
        // and the platform's peripheral tests.
        if (!pick) {
            log_ << "self-test: " << path_name(path) << ": every open target integrated, no responder to probe\n";
            continue;
        }
        const auto* r = top_.responder(pick->name);
        for (const bool write : {false, true}) {
            const auto own = r->accesses();
            const auto status = access(write, pick->base);
            const std::string what = std::string(write ? "write " : "read ") + pick->name + " (" + path_name(path) + ")";
            check(status == tlm::TLM_GENERIC_ERROR_RESPONSE, what + " answers TLM_GENERIC_ERROR_RESPONSE");
            check(r->accesses() == own + 1, what + " reaches the responder once");
        }
    }

    // 3. Unmapped space is a decode error and reaches no responder.
    std::uint64_t hole = 0;
    check(unmapped_address(cfg, hole), "the map leaves an unmapped word below 4 GiB");
    const auto before = total_hits();
    check(access(false, hole) == tlm::TLM_ADDRESS_ERROR_RESPONSE,
          "read of unmapped " + hex(hole) + " answers TLM_ADDRESS_ERROR_RESPONSE");
    check(total_hits() == before, "unmapped access reaches no responder");

    done_ = true;
    log_ << "fu3_soc self-test: " << checks_ << " checks, " << failures_ << " failures -> "
         << (failures_ ? "FAIL" : "PASS") << '\n';
    sc_core::sc_stop();
}

} // namespace cdc::platforms::fu3_soc
