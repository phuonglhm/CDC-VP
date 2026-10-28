#include "gdb_server.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sstream>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

#include "cpu_debug_hart.h"
#include "fu3_soc_top.h"

namespace cdc::platforms::fu3_soc {

namespace {

constexpr std::uint16_t kDcsr = 0x7b0, kDpc = 0x7b1, kTselect = 0x7a0, kTdata1 = 0x7a1, kTdata2 = 0x7a2;
constexpr std::uint32_t kDcsrEbreakm = 1u << 15, kDcsrEbreaku = 1u << 12, kDcsrStep = 1u << 2;
constexpr std::uint32_t kMcontrolM = 1u << 6, kMcontrolU = 1u << 3, kMcontrolExecute = 1u << 2;
constexpr unsigned kPcRegnum = 32, kFirstFpr = 33, kFirstCsr = 65;
constexpr std::uint8_t kEbreak[4] = {0x73, 0x00, 0x10, 0x00}, kCEbreak[2] = {0x02, 0x90};
constexpr unsigned kMaxMemory = 4096;

// Machine CSRs offered to GDB beyond pc (GDB register number 65 + CSR number).
constexpr struct {
    const char* name;
    std::uint16_t number;
} kCsrs[] = {{"mstatus", 0x300}, {"misa", 0x301},  {"mie", 0x304},   {"mtvec", 0x305},   {"mscratch", 0x340},
             {"mepc", 0x341},    {"mcause", 0x342}, {"mtval", 0x343}, {"mip", 0x344},     {"mcycle", 0xb00},
             {"minstret", 0xb02}, {"mhartid", 0xf14}};

const char* const kGprNames[32] = {"zero", "ra", "sp", "gp", "tp",  "t0",  "t1", "t2", "fp", "s1", "a0",
                                   "a1",   "a2", "a3", "a4", "a5",  "a6",  "a7", "s2", "s3", "s4", "s5",
                                   "s6",   "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"};
const char* const kFprNames[32] = {"ft0", "ft1", "ft2",  "ft3",  "ft4", "ft5", "ft6",  "ft7",
                                   "fs0", "fs1", "fa0",  "fa1",  "fa2", "fa3", "fa4",  "fa5",
                                   "fa6", "fa7", "fs2",  "fs3",  "fs4", "fs5", "fs6",  "fs7",
                                   "fs8", "fs9", "fs10", "fs11", "ft8", "ft9", "ft10", "ft11"};

std::string hex_le(std::uint32_t v)
{
    char out[9];
    std::snprintf(out, sizeof out, "%02x%02x%02x%02x", v & 0xff, (v >> 8) & 0xff, (v >> 16) & 0xff, v >> 24);
    return out;
}

std::string hex_bytes(const std::uint8_t* data, std::size_t n)
{
    static const char* digits = "0123456789abcdef";
    std::string out;
    for (std::size_t i = 0; i < n; ++i) {
        out += digits[data[i] >> 4];
        out += digits[data[i] & 0xf];
    }
    return out;
}

int nibble(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

bool parse_bytes(const std::string& text, std::vector<std::uint8_t>& out)
{
    if (text.size() % 2)
        return false;
    out.clear();
    for (std::size_t i = 0; i < text.size(); i += 2) {
        const int hi = nibble(text[i]), lo = nibble(text[i + 1]);
        if (hi < 0 || lo < 0)
            return false;
        out.push_back(static_cast<std::uint8_t>(hi << 4 | lo));
    }
    return true;
}

std::uint32_t le32(const std::uint8_t* b)
{
    return b[0] | b[1] << 8 | b[2] << 16 | std::uint32_t(b[3]) << 24;
}

bool parse_hex(const std::string& text, std::uint64_t& value)
{
    if (text.empty() || text.size() > 16)
        return false;
    value = 0;
    for (const char c : text) {
        const int n = nibble(c);
        if (n < 0)
            return false;
        value = value << 4 | unsigned(n);
    }
    return true;
}

// "a,b" (and an optional ":rest") of hex numbers.
bool parse_pair(const std::string& text, std::uint64_t& a, std::uint64_t& b, std::string* rest = nullptr)
{
    const auto comma = text.find(',');
    if (comma == std::string::npos)
        return false;
    auto end = rest ? text.find(':', comma) : std::string::npos;
    if (rest) {
        if (end == std::string::npos)
            return false;
        *rest = text.substr(end + 1);
    }
    return parse_hex(text.substr(0, comma), a) &&
           parse_hex(text.substr(comma + 1, end == std::string::npos ? std::string::npos : end - comma - 1), b);
}

} // namespace

gdb_server::gdb_server(sc_core::sc_module_name name, fu3_soc_top& top, std::array<int, 2> ports, bool wait_for_clients,
                       sc_core::sc_time poll)
    : sc_module(name), top_(top), wait_for_clients_(wait_for_clients), poll_(poll)
{
    static const char* const names[2] = {"CVA6", "Ibex"};
    for (unsigned h = 0; h < 2; ++h) {
        auto& c = conns_[h];
        c.hart = h;
        c.name = names[h];
        if (ports[h] < 0)
            continue;
        c.control = top_.debug_hart(h);
        if (!c.control)
            throw std::runtime_error("GDB server: the platform has no CPU models");
        c.listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        const int one = 1;
        ::setsockopt(c.listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   // local debugger only
        addr.sin_port = htons(static_cast<std::uint16_t>(ports[h]));
        if (c.listen_fd < 0 || ::bind(c.listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0 ||
            ::listen(c.listen_fd, 1) != 0)
            throw std::runtime_error(std::string("GDB server: cannot listen on 127.0.0.1:") + std::to_string(ports[h]) +
                                     " for " + c.name + ": " + std::strerror(errno));
        socklen_t len = sizeof addr;
        ::getsockname(c.listen_fd, reinterpret_cast<sockaddr*>(&addr), &len);
        c.port = ntohs(addr.sin_port);
        std::cout << "vp_fu3_soc: GDB server for " << c.name << " (hart " << h << ") on 127.0.0.1:" << c.port
                  << std::endl;
    }
    SC_THREAD(run);
}

gdb_server::~gdb_server()
{
    for (auto& c : conns_) {
        if (c.fd >= 0)
            ::close(c.fd);
        if (c.listen_fd >= 0)
            ::close(c.listen_fd);
    }
}

void gdb_server::run()
{
    if (wait_for_clients_) {
        std::cout << "vp_fu3_soc: waiting for GDB (simulation held at 0 s)" << std::endl;
        while (true) {
            std::vector<pollfd> fds;
            for (auto& c : conns_)
                if (c.listen_fd >= 0 && c.fd < 0)
                    fds.push_back({c.listen_fd, POLLIN, 0});
            if (fds.empty())
                break;
            ::poll(fds.data(), fds.size(), -1);
            for (auto& c : conns_)
                for (const auto& f : fds)
                    if (f.fd == c.listen_fd && (f.revents & POLLIN))
                        accept(c);
        }
    }
    while (true) {
        // A completed halt first, then the packets that were waiting for it.
        for (auto& c : conns_) {
            if (c.fd < 0)
                continue;
            after_halt(c);
            process(c);
            if (sc_core::sc_get_status() == sc_core::SC_STOPPED)
                return;
        }
        // Simulated time stands still while every attached hart is stopped.
        auto frozen = [&] {
            bool attached = false;
            for (const auto& c : conns_) {
                if (c.fd < 0 || c.st == state::detached)
                    continue;
                attached = true;
                if (c.st != state::stopped || !c.control->gdb_halted())
                    return false;
            }
            return attached;
        };
        std::vector<pollfd> fds;
        for (auto& c : conns_) {
            if (c.listen_fd >= 0)
                fds.push_back({c.listen_fd, POLLIN, 0});
            if (c.fd >= 0)
                fds.push_back({c.fd, POLLIN, 0});
        }
        if (!fds.empty())
            ::poll(fds.data(), fds.size(), frozen() ? -1 : 0);
        for (auto& c : conns_) {
            for (const auto& f : fds) {
                if (!(f.revents & (POLLIN | POLLHUP | POLLERR)))
                    continue;
                if (f.fd == c.listen_fd)
                    accept(c);
                else if (f.fd == c.fd && !receive(c))
                    close_client(c, "connection closed");
            }
            if (c.fd >= 0)
                process(c);
            if (sc_core::sc_get_status() == sc_core::SC_STOPPED || sc_core::sc_end_of_simulation_invoked())
                return;
        }
        if (frozen())
            continue;
        bool fast = false;
        for (const auto& c : conns_)
            fast |= c.fd >= 0 && (c.st == state::halting || c.st == state::stepping);
        wait(fast ? sc_core::sc_time(100, sc_core::SC_NS) : poll_);
    }
}

void gdb_server::accept(connection& c)
{
    const int fd = ::accept(c.listen_fd, nullptr, nullptr);
    if (fd < 0)
        return;
    if (c.fd >= 0) {   // one client per hart
        ::close(fd);
        return;
    }
    if (!c.control->gdb_attach()) {
        std::cerr << "vp_fu3_soc: GDB attach to " << c.name
                  << " refused: the Debug Module controls this hart (resume it from the DM first)" << std::endl;
        ::close(fd);
        return;
    }
    const int one = 1;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    c.fd = fd;
    c.in.clear();
    c.ack = true;
    c.head_acked = false;
    c.last_sent.clear();
    c.armed = false;
    c.interrupt = false;
    c.sw_breaks.clear();
    c.control->gdb_halt_request(true);
    c.st = state::halting;
    std::cout << "vp_fu3_soc: GDB attached to " << c.name << " at " << sc_core::sc_time_stamp()
              << (c.control->in_reset() ? " (hart in reset: halts after its release)" : "") << std::endl;
}

void gdb_server::close_client(connection& c, const char* why)
{
    if (c.fd < 0)
        return;
    detach(c);
    ::close(c.fd);
    c.fd = -1;
    std::cout << "vp_fu3_soc: GDB detached from " << c.name << " (" << why << ") at " << sc_core::sc_time_stamp()
              << std::endl;
}

bool gdb_server::receive(connection& c)
{
    char buffer[4096];
    const auto n = ::recv(c.fd, buffer, sizeof buffer, MSG_DONTWAIT);
    if (n > 0) {
        c.in.append(buffer, std::size_t(n));
        return true;
    }
    return n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR);
}

void gdb_server::send_packet(connection& c, const std::string& body)
{
    std::string out = "$";
    unsigned sum = 0;
    for (const char ch : body) {
        if (ch == '#' || ch == '$' || ch == '}' || ch == '*') {
            out += '}';
            sum += '}';
            out += char(ch ^ 0x20);
            sum += std::uint8_t(ch ^ 0x20);
        } else {
            out += ch;
            sum += std::uint8_t(ch);
        }
    }
    char tail[4];
    std::snprintf(tail, sizeof tail, "#%02x", sum & 0xff);
    out += tail;
    c.last_sent = out;
    for (std::size_t done = 0; done < out.size();) {
        const auto n = ::send(c.fd, out.data() + done, out.size() - done, MSG_NOSIGNAL);
        if (n <= 0) {
            if (errno == EINTR)
                continue;
            return;   // the next receive reports the closed connection
        }
        done += std::size_t(n);
    }
}

void gdb_server::process(connection& c)
{
    while (c.fd >= 0 && !c.in.empty()) {
        const char first = c.in[0];
        if (first == '+') {
            c.in.erase(0, 1);
            continue;
        }
        if (first == '-') {   // NACK: the client got our last packet damaged, send it again
            c.in.erase(0, 1);
            if (c.ack && !c.last_sent.empty())
                ::send(c.fd, c.last_sent.data(), c.last_sent.size(), MSG_NOSIGNAL);
            continue;
        }
        if (first == '\x03') {   // ^C: halt a running hart
            c.in.erase(0, 1);
            if (c.st == state::running || c.st == state::stepping) {
                c.control->gdb_halt_request(true);
                c.interrupt = true;
                c.st = state::halting;
            }
            continue;
        }
        if (first != '$') {
            c.in.erase(0, 1);
            continue;
        }
        const auto hash = c.in.find('#');
        if (hash == std::string::npos || hash + 2 >= c.in.size())
            return;   // incomplete
        const std::string body = c.in.substr(1, hash - 1);
        // Checksum: modulo-256 sum of the bytes between '$' and '#' (review R03).
        // A damaged packet is not executed: NACK in acknowledgement mode, so the
        // client sends it again; dropped (and reported) in no-ack mode.
        unsigned sum = 0;
        for (const char ch : body)
            sum += std::uint8_t(ch);
        const int hi = nibble(c.in[hash + 1]), lo = nibble(c.in[hash + 2]);
        if (hi < 0 || lo < 0 || unsigned(hi << 4 | lo) != (sum & 0xff)) {
            c.in.erase(0, hash + 3);
            c.head_acked = false;
            if (c.ack)
                ::send(c.fd, "-", 1, MSG_NOSIGNAL);
            std::cerr << "vp_fu3_soc: GDB " << c.name << ": packet with a bad checksum dropped" << std::endl;
            continue;
        }
        // Acknowledged on receipt, also when it waits for the hart to stop
        // (GDB would resend an unacknowledged packet).
        if (c.ack && !c.head_acked)
            ::send(c.fd, "+", 1, MSG_NOSIGNAL);
        c.head_acked = true;
        bool defer = false;
        const auto reply = handle(c, body, defer);
        if (defer)
            return;   // needs the hart stopped: kept until it is
        c.in.erase(0, hash + 3);
        c.head_acked = false;
        if (body == "QStartNoAckMode") {
            send_packet(c, "OK");
            c.ack = false;
            continue;
        }
        if (reply == "\x01close") {
            send_packet(c, "OK");
            close_client(c, "detach");
            return;
        }
        if (reply == "\x01kill") {
            killed_ = true;
            close_client(c, "kill");
            sc_core::sc_stop();
            return;
        }
        if (reply != "\x01none")
            send_packet(c, reply);
    }
}

std::string gdb_server::handle(connection& c, const std::string& p, bool& defer)
{
    const char k = p.empty() ? 0 : p[0];
    const bool needs_stop = std::string("?gGpPmMcsZz").find(k) != std::string::npos && k != 0;
    if (needs_stop && c.st != state::stopped) {
        defer = true;
        return {};
    }
    if (p.rfind("qSupported", 0) == 0)
        return "PacketSize=4000;qXfer:features:read+;QStartNoAckMode+;swbreak+;hwbreak+";
    if (p == "QStartNoAckMode")
        return "OK";
    if (p.rfind("qXfer:features:read:target.xml:", 0) == 0) {
        std::uint64_t offset = 0, length = 0;
        if (!parse_pair(p.substr(31), offset, length))
            return "E01";
        const auto xml = target_xml(c);
        if (offset >= xml.size())
            return "l";
        const auto chunk = xml.substr(offset, length);
        return (offset + chunk.size() >= xml.size() ? "l" : "m") + chunk;
    }
    if (p == "qAttached")
        return "1";
    if (p == "qC")
        return "QC1";
    if (p == "qfThreadInfo")
        return "m1";
    if (p == "qsThreadInfo")
        return "l";
    if (p == "qSymbol::")
        return "OK";
    if (k == 'H' || k == 'T')
        return "OK";
    if (p == "?")
        return "S05";
    if (p == "D" || p.rfind("D;", 0) == 0)
        return "\x01close";
    if (p == "k" || p.rfind("vKill", 0) == 0)
        return "\x01kill";
    if (k == 'g') {
        std::string out;
        for (unsigned r = 0; r <= kPcRegnum; ++r) {
            std::uint32_t v = 0;
            out += read_reg(c, r, v) ? hex_le(v) : "xxxxxxxx";
        }
        return out;
    }
    if (k == 'G') {
        std::vector<std::uint8_t> bytes;
        if (!parse_bytes(p.substr(1), bytes))
            return "E01";
        for (unsigned r = 1; r <= kPcRegnum && (r + 1) * 4 <= bytes.size(); ++r)
            if (!write_reg(c, r, le32(&bytes[r * 4])))
                return "E01";
        return "OK";
    }
    if (k == 'p') {
        std::uint64_t r = 0;
        std::uint32_t v = 0;
        if (!parse_hex(p.substr(1), r))
            return "E01";
        if (!read_reg(c, unsigned(r), v))
            return "E01";
        return hex_le(v);
    }
    if (k == 'P') {
        const auto eq = p.find('=');
        std::uint64_t r = 0;
        std::vector<std::uint8_t> bytes;
        if (eq == std::string::npos || !parse_hex(p.substr(1, eq - 1), r) || !parse_bytes(p.substr(eq + 1), bytes) ||
            bytes.size() != 4)
            return "E01";
        return write_reg(c, unsigned(r), le32(bytes.data())) ? "OK" : "E01";
    }
    if (k == 'm') {
        std::uint64_t address = 0, length = 0;
        if (!parse_pair(p.substr(1), address, length) || length > kMaxMemory || address + length > 0x1'0000'0000ull)
            return "E01";
        std::vector<std::uint8_t> data(length);
        if (!memory(c, false, std::uint32_t(address), data.data(), unsigned(length)))
            return "E01";
        return hex_bytes(data.data(), data.size());
    }
    if (k == 'M') {
        std::uint64_t address = 0, length = 0;
        std::string rest;
        std::vector<std::uint8_t> data;
        if (!parse_pair(p.substr(1), address, length, &rest) || !parse_bytes(rest, data) || data.size() != length ||
            address + length > 0x1'0000'0000ull)
            return "E01";
        return memory(c, true, std::uint32_t(address), data.data(), unsigned(length)) ? "OK" : "E01";
    }
    if (k == 'c' || k == 's') {
        std::uint64_t address = 0;
        if (p.size() > 1) {
            if (!parse_hex(p.substr(1), address) || !write_reg(c, kPcRegnum, std::uint32_t(address)))
                return "E01";
        }
        resume(c, k == 's');
        return "\x01none";
    }
    if (k == 'Z' || k == 'z') {
        std::uint64_t address = 0, kind = 0;
        if (p.size() < 3 || !parse_pair(p.substr(3), address, kind))
            return "E01";
        if (p[1] == '0')
            return k == 'Z' ? set_sw_break(c, std::uint32_t(address), unsigned(kind))
                            : clear_sw_break(c, std::uint32_t(address));
        if (p[1] == '1')
            return set_hw_break(c, std::uint32_t(address), k == 'Z');
        return "";   // watchpoints: no load/store triggers (Q04-e)
    }
    return "";
}

void gdb_server::after_halt(connection& c)
{
    if (c.st == state::detached || !c.control->gdb_halted())
        return;
    if (c.control->reset_halt_pending()) {
        // First instruction after a reset of the hart: dcsr and triggers are
        // at their reset values. Re-arm, then carry on as before the reset.
        c.control->clear_reset_halt();
        c.control->gdb_halt_request(false);
        arm(c);
        if (c.st == state::running) {
            resume(c, false);
            return;
        }
        if (c.st == state::stopped)
            return;
    }
    if (c.st == state::stopped)
        return;
    c.control->gdb_halt_request(false);
    std::uint32_t dcsr = 0;   // dcsr.step stays as it is: every resume sets it anew
    csr(c, kDcsr, false, dcsr);
    const state was = c.st;
    c.st = state::stopped;
    if (!c.armed)
        arm(c);
    if (was == state::halting && !c.interrupt)
        return;   // attach: GDB asks with '?'
    const unsigned cause = (dcsr >> 6) & 7;
    std::string reply = c.interrupt ? "T02" : "T05";
    if (!c.interrupt && cause == 1)
        reply += "swbreak:;";
    else if (!c.interrupt && cause == 2)
        reply += "hwbreak:;";
    c.interrupt = false;
    send_packet(c, reply);
}

void gdb_server::detach(connection& c)
{
    if (c.st == state::detached)
        return;
    // Halt the hart to restore dcsr and the triggers (not possible in reset).
    if (!c.control->gdb_halted() && !c.control->in_reset()) {
        c.control->gdb_halt_request(true);
        for (unsigned i = 0; i < 100000 && !c.control->gdb_halted() && !c.control->in_reset(); ++i)
            wait(10, sc_core::SC_NS);
    }
    disarm(c);
    c.control->gdb_detach();
    c.st = state::detached;
}

bool gdb_server::csr(connection& c, std::uint16_t number, bool write, std::uint32_t& value)
{
    return c.control->gdb_access_register(number, write, value);
}

bool gdb_server::read_reg(connection& c, unsigned regnum, std::uint32_t& value)
{
    if (regnum < 32)
        return c.control->gdb_access_register(std::uint16_t(0x1000 + regnum), false, value);
    if (regnum == kPcRegnum)
        return csr(c, kDpc, false, value);
    if (regnum < kFirstCsr)
        return c.hart == 0 && c.control->gdb_access_register(std::uint16_t(0x1020 + regnum - kFirstFpr), false, value);
    if (regnum - kFirstCsr < 0x1000)
        return csr(c, std::uint16_t(regnum - kFirstCsr), false, value);
    return false;
}

bool gdb_server::write_reg(connection& c, unsigned regnum, std::uint32_t value)
{
    if (regnum < 32)
        return c.control->gdb_access_register(std::uint16_t(0x1000 + regnum), true, value);
    if (regnum == kPcRegnum)
        return csr(c, kDpc, true, value);
    if (regnum < kFirstCsr)
        return c.hart == 0 && c.control->gdb_access_register(std::uint16_t(0x1020 + regnum - kFirstFpr), true, value);
    const unsigned number = regnum - kFirstCsr;
    // dcsr, dpc and the trigger CSRs belong to the stub.
    if (number >= 0x1000 || number == kDcsr || number == kDpc || (number >= kTselect && number <= 0x7a5))
        return false;
    return csr(c, std::uint16_t(number), true, value);
}

bool gdb_server::memory(connection& c, bool write, std::uint32_t address, std::uint8_t* data, unsigned length)
{
    // The hart's own bus port, as a debug transaction (no policy, no time).
    auto& port = c.hart == 0 ? top_.cva6()->data_bus() : top_.ibex()->data_bus();
    while (length) {
        // Never across a 4 KiB boundary: one target per transaction.
        const unsigned n = std::min<unsigned>(length, 0x1000 - (address & 0xfff));
        tlm::tlm_generic_payload tx;
        tx.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
        tx.set_address(address);
        tx.set_data_ptr(data);
        tx.set_data_length(n);
        tx.set_streaming_width(n);
        if (port->transport_dbg(tx) != n)
            return false;
        address += n;
        data += n;
        length -= n;
    }
    return true;
}

void gdb_server::arm(connection& c)
{
    std::uint32_t dcsr = 0;
    if (!csr(c, kDcsr, false, dcsr))
        return;
    if (!c.armed) {
        c.dcsr_saved = dcsr & (kDcsrEbreakm | kDcsrEbreaku);
        // Trigger slots: tselect is WARL, an unimplemented index reads back another one.
        if (c.hw_used.empty()) {
            for (std::uint32_t i = 0; i < 16; ++i) {
                std::uint32_t v = i;
                if (!csr(c, kTselect, true, v) || !csr(c, kTselect, false, v) || v != i)
                    break;
                c.hw_used.push_back(false);
                c.hw_breaks.push_back(0);
            }
        }
        c.armed = true;
    }
    dcsr |= kDcsrEbreakm | kDcsrEbreaku;
    csr(c, kDcsr, true, dcsr);
    for (std::uint32_t i = 0; i < c.hw_used.size(); ++i) {
        if (!c.hw_used[i])
            continue;
        std::uint32_t v = i, address = c.hw_breaks[i], control = kMcontrolExecute | kMcontrolM | kMcontrolU;
        csr(c, kTselect, true, v);
        csr(c, kTdata2, true, address);
        csr(c, kTdata1, true, control);
    }
}

void gdb_server::disarm(connection& c)
{
    while (!c.sw_breaks.empty())
        clear_sw_break(c, c.sw_breaks.begin()->first);
    for (std::uint32_t i = 0; i < c.hw_used.size(); ++i)
        if (c.hw_used[i])
            set_hw_break(c, c.hw_breaks[i], false);
    std::uint32_t dcsr = 0;
    if (c.armed && csr(c, kDcsr, false, dcsr)) {
        dcsr = (dcsr & ~(kDcsrEbreakm | kDcsrEbreaku | kDcsrStep)) | c.dcsr_saved;
        csr(c, kDcsr, true, dcsr);
    }
    c.armed = false;
}

std::string gdb_server::set_sw_break(connection& c, std::uint32_t address, unsigned kind)
{
    if (kind != 2 && kind != 4)
        return "E01";
    if (c.sw_breaks.count(address))
        return "OK";
    std::vector<std::uint8_t> original(kind), check(kind);
    if (!memory(c, false, address, original.data(), kind))
        return "E01";
    std::vector<std::uint8_t> patch(kind == 4 ? kEbreak : kCEbreak, (kind == 4 ? kEbreak : kCEbreak) + kind);
    // The patch must read back: a target that ignores debug writes (QSPI XIP)
    // cannot hold a software breakpoint (use hbreak).
    if (!memory(c, true, address, patch.data(), kind) || !memory(c, false, address, check.data(), kind) ||
        check != patch) {
        memory(c, true, address, original.data(), kind);
        return "E01";
    }
    c.sw_breaks[address] = original;
    return "OK";
}

std::string gdb_server::clear_sw_break(connection& c, std::uint32_t address)
{
    const auto it = c.sw_breaks.find(address);
    if (it == c.sw_breaks.end())
        return "OK";
    auto original = it->second;
    c.sw_breaks.erase(it);
    // Restore only over our own EBREAK: code copied there meanwhile stays.
    std::vector<std::uint8_t> now(original.size());
    const auto* patch = original.size() == 4 ? kEbreak : kCEbreak;
    if (memory(c, false, address, now.data(), unsigned(now.size())) &&
        std::equal(now.begin(), now.end(), patch))
        memory(c, true, address, original.data(), unsigned(original.size()));
    return "OK";
}

std::string gdb_server::set_hw_break(connection& c, std::uint32_t address, bool insert)
{
    for (std::uint32_t i = 0; i < c.hw_used.size(); ++i) {
        if (insert ? c.hw_used[i] : !(c.hw_used[i] && c.hw_breaks[i] == address))
            continue;
        std::uint32_t v = i, a = address, control = insert ? kMcontrolExecute | kMcontrolM | kMcontrolU : 0;
        if (!csr(c, kTselect, true, v) || !csr(c, kTdata2, true, a) || !csr(c, kTdata1, true, control))
            return "E01";
        c.hw_used[i] = insert;
        c.hw_breaks[i] = insert ? address : 0;
        return "OK";
    }
    return insert ? "E01" : "OK";   // no free trigger / not set
}

void gdb_server::resume(connection& c, bool step)
{
    std::uint32_t dcsr = 0;
    if (csr(c, kDcsr, false, dcsr)) {
        dcsr = step ? dcsr | kDcsrStep : dcsr & ~kDcsrStep;
        csr(c, kDcsr, true, dcsr);
    }
    c.st = step ? state::stepping : state::running;
    c.control->gdb_resume();
}

std::string gdb_server::target_xml(const connection& c) const
{
    std::ostringstream x;
    x << "<?xml version=\"1.0\"?>\n<!DOCTYPE target SYSTEM \"gdb-target.dtd\">\n<target version=\"1.0\">\n"
      << "<architecture>riscv:rv32</architecture>\n<feature name=\"org.gnu.gdb.riscv.cpu\">\n";
    for (unsigned r = 0; r < 32; ++r)
        x << "<reg name=\"" << kGprNames[r] << "\" bitsize=\"32\" type=\"" << (r == 2 ? "data_ptr" : "int")
          << "\" regnum=\"" << r << "\"/>\n";
    x << "<reg name=\"pc\" bitsize=\"32\" type=\"code_ptr\" regnum=\"" << kPcRegnum << "\"/>\n</feature>\n";
    if (c.hart == 0) {   // CVA6: F (single precision)
        x << "<feature name=\"org.gnu.gdb.riscv.fpu\">\n";
        for (unsigned r = 0; r < 32; ++r)
            x << "<reg name=\"" << kFprNames[r] << "\" bitsize=\"32\" type=\"ieee_single\" regnum=\"" << kFirstFpr + r
              << "\"/>\n";
        x << "<reg name=\"fflags\" bitsize=\"32\" type=\"int\" regnum=\"" << kFirstCsr + 1 << "\"/>\n"
          << "<reg name=\"frm\" bitsize=\"32\" type=\"int\" regnum=\"" << kFirstCsr + 2 << "\"/>\n"
          << "<reg name=\"fcsr\" bitsize=\"32\" type=\"int\" regnum=\"" << kFirstCsr + 3 << "\"/>\n</feature>\n";
    }
    x << "<feature name=\"org.gnu.gdb.riscv.csr\">\n";
    for (const auto& r : kCsrs)
        x << "<reg name=\"" << r.name << "\" bitsize=\"32\" type=\"int\" regnum=\"" << kFirstCsr + r.number
          << "\"/>\n";
    x << "</feature>\n</target>\n";
    return x.str();
}

} // namespace cdc::platforms::fu3_soc
