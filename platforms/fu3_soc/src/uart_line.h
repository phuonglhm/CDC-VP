#pragma once

#include <cstdint>
#include <deque>
#include <ostream>
#include <string>

#include <systemc>

#include <fu3_uart/uart.h>

namespace cdc::platforms::fu3_soc {

// Board-side peer of one UART (console, decision Q07-f tie-offs elsewhere):
// decodes the UART's TXD into characters and drives its RXD (idle high) with
// queued bytes. The line timing and frame format come from the UART itself
// (bit_time(), LCR), as a terminal set to the same settings would.
class uart_line : public sc_core::sc_module {
public:
    sc_core::sc_in<bool> txd{"txd"};
    sc_core::sc_out<bool> rxd{"rxd"};

    SC_HAS_PROCESS(uart_line);
    uart_line(sc_core::sc_module_name name, const fu3_uart::Uart& uart) : sc_core::sc_module(name), uart_(uart)
    {
        SC_THREAD(decode);
        SC_THREAD(drive);
    }
    // Received characters are appended here; with `echo`, also printed with a prefix.
    void set_echo(std::ostream* out, std::string prefix)
    {
        echo_ = out;
        prefix_ = std::move(prefix);
    }
    const std::string& received() const { return received_; }
    std::uint64_t framing_errors() const { return framing_errors_; }
    // Bytes to send to the UART's RXD, in the UART's current format.
    void send(const std::string& bytes)
    {
        for (unsigned char c : bytes)
            queue_.push_back(c);
        queued_.notify(sc_core::SC_ZERO_TIME);
    }
    bool sending() const { return !queue_.empty() || busy_; }

private:
    const fu3_uart::Uart& uart_;
    std::ostream* echo_ = nullptr;
    std::string prefix_, received_, line_;
    std::uint64_t framing_errors_ = 0;
    std::deque<unsigned char> queue_;
    bool busy_ = false;
    sc_core::sc_event queued_;

    static unsigned data_bits(std::uint8_t lcr) { return 5 + (lcr & 3); }
    static bool parity_bit(unsigned value, std::uint8_t lcr)
    {
        if (lcr & 0x20)
            return !(lcr & 0x10);   // stick parity
        bool odd = false;
        for (unsigned i = 0; i < data_bits(lcr); ++i)
            odd ^= (value >> i) & 1;
        return (lcr & 0x10) ? odd : !odd;   // even / odd parity bit value
    }

    void decode()
    {
        while (true) {
            wait(txd.negedge_event());
            const sc_core::sc_time bit = uart_.bit_time();
            const std::uint8_t lcr = uart_.line_control();
            if (bit == sc_core::SC_ZERO_TIME)
                continue;
            wait(bit / 2);
            if (txd.read())
                continue;   // glitch, not a start bit
            unsigned value = 0;
            for (unsigned i = 0; i < data_bits(lcr); ++i) {
                wait(bit);
                value |= unsigned(txd.read()) << i;
            }
            if (lcr & 0x08)
                wait(bit);   // parity bit (not checked against the sender's intent)
            wait(bit);
            if (!txd.read()) {
                ++framing_errors_;
                continue;
            }
            const char c = static_cast<char>(value);
            received_ += c;
            if (echo_) {
                if (c == '\n') {
                    *echo_ << prefix_ << line_ << std::endl;
                    line_.clear();
                } else if (c != '\r') {
                    line_ += c;
                }
            }
        }
    }

    void drive()
    {
        rxd.write(true);
        while (true) {
            while (queue_.empty())
                wait(queued_);
            const sc_core::sc_time bit = uart_.bit_time();
            const std::uint8_t lcr = uart_.line_control();
            if (bit == sc_core::SC_ZERO_TIME) {
                wait(1, sc_core::SC_US);   // receiver not configured yet
                continue;
            }
            busy_ = true;
            const unsigned value = queue_.front();
            queue_.pop_front();
            rxd.write(false);   // start bit
            wait(bit);
            for (unsigned i = 0; i < data_bits(lcr); ++i) {
                rxd.write((value >> i) & 1);
                wait(bit);
            }
            if (lcr & 0x08) {
                rxd.write(parity_bit(value, lcr));
                wait(bit);
            }
            rxd.write(true);   // stop bit(s)
            wait(bit * ((lcr & 0x04) ? 2.0 : 1.0));
            busy_ = false;
        }
    }
};

} // namespace cdc::platforms::fu3_soc
