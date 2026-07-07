#include "uart_host_bridge.h"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace cdc::components {

namespace {

void set_nonblocking(int fd)
{
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags >= 0) ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

} // namespace

SC_HAS_PROCESS(uart_host_bridge);

uart_host_bridge::uart_host_bridge(sc_core::sc_module_name name,
                                   sc_core::sc_time byte_interval,
                                   sc_core::sc_time poll_interval)
    : sc_core::sc_module(name)
    , rx_out("rx_out")
    , tx_in("tx_in")
    , byte_interval_(byte_interval)
    , poll_interval_(poll_interval)
{
    SC_THREAD(rx_thread);
    SC_METHOD(tx_method);
    sensitive << tx_in;
    dont_initialize();
}

uart_host_bridge::~uart_host_bridge()
{
    drop_client();
    if (listen_fd_ >= 0) ::close(listen_fd_);
}

void uart_host_bridge::listen_on(std::uint16_t port, bool wait_for_client)
{
    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        SC_REPORT_ERROR(name(), "cannot create TCP socket");
        return;
    }
    int one = 1;
    ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);
    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 ||
        ::listen(listen_fd_, 1) < 0) {
        SC_REPORT_ERROR(name(), (std::string("cannot listen on TCP port ") +
                                 std::to_string(port) + ": " + std::strerror(errno)).c_str());
        ::close(listen_fd_);
        listen_fd_ = -1;
        return;
    }
    socklen_t alen = sizeof(addr);
    ::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&addr), &alen);
    listen_port_ = ntohs(addr.sin_port);
    wait_for_client_ = wait_for_client;

    std::cout << name() << ": listening on 127.0.0.1:" << listen_port_
              << (wait_for_client ? " (waiting for client before sim start)" : "")
              << '\n';
}

void uart_host_bridge::replay_file(const std::string& path, sc_core::sc_time start_delay)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        SC_REPORT_ERROR(name(), ("cannot open RX replay file: " + path).c_str());
        return;
    }
    replay_.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    replay_delay_ = start_delay;
    std::cout << name() << ": RX replay of " << replay_.size()
              << " bytes from " << path << '\n';
}

void uart_host_bridge::inject(const unsigned char* data, std::size_t len)
{
    // One byte per byte_interval so every write lands in its own evaluation
    // (rx_out drives an sc_buffer: back-to-back same-delta writes would merge).
    for (std::size_t i = 0; i < len; ++i) {
        rx_out.write(data[i]);
        wait(byte_interval_);
    }
}

void uart_host_bridge::accept_client(bool blocking)
{
    if (blocking) {
        // Deliberately stalls the simulation (wall clock) at the current sim
        // time until the host tool connects.
        int flags = ::fcntl(listen_fd_, F_GETFL, 0);
        ::fcntl(listen_fd_, F_SETFL, flags & ~O_NONBLOCK);
    } else {
        set_nonblocking(listen_fd_);
    }
    int fd = ::accept(listen_fd_, nullptr, nullptr);
    if (fd >= 0) {
        set_nonblocking(fd);
        client_fd_ = fd;
        std::cout << name() << ": client connected\n";
    }
}

void uart_host_bridge::drop_client()
{
    if (client_fd_ >= 0) {
        ::close(client_fd_);
        client_fd_ = -1;
    }
}

void uart_host_bridge::rx_thread()
{
    if (!replay_.empty()) {
        if (replay_delay_ != sc_core::SC_ZERO_TIME) wait(replay_delay_);
        inject(replay_.data(), replay_.size());
    }

    if (listen_fd_ < 0) return; // no TCP backend: nothing left to do

    if (wait_for_client_) accept_client(/*blocking=*/true);

    unsigned char buf[256];
    while (true) {
        wait(poll_interval_);
        if (client_fd_ < 0) {
            accept_client(/*blocking=*/false);
            continue;
        }
        ssize_t n = ::recv(client_fd_, buf, sizeof(buf), 0);
        if (n > 0) {
            inject(buf, static_cast<std::size_t>(n));
        } else if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
            std::cout << name() << ": client disconnected\n";
            drop_client(); // keep listening for the next client
        }
    }
}

void uart_host_bridge::tx_method()
{
    if (client_fd_ < 0) return;
    const unsigned char b = tx_in.read();
    // Best-effort, non-blocking: a stalled client must not stall the sim.
    if (::send(client_fd_, &b, 1, MSG_NOSIGNAL | MSG_DONTWAIT) < 0 &&
        errno != EAGAIN && errno != EWOULDBLOCK) {
        std::cout << name() << ": client send failed, dropping connection\n";
        drop_client();
    }
}

} // namespace cdc::components
