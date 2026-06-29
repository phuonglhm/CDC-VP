#include <iostream>
#include <cstring>
#include "uart.h"
using namespace sc_core;

SC_HAS_PROCESS(UartTLM);

UartTLM::UartTLM(sc_module_name name, sc_core::sc_time rx_timeout)
    : sc_module(name), rx_timeout_period(rx_timeout)
{
    SC_THREAD(busThread);
    SC_METHOD(rxMethod);
    sensitive << rx;
    dont_initialize();
    // Receive-timeout interrupt: fires when RX data sits idle for rx_timeout.
    SC_METHOD(rxTimeout);
    sensitive << rx_timeout_evt;
    dont_initialize();
    // Drive the PLIC IRQ line. Initialized (runs at t=0) so the output starts
    // at a defined low level, then re-evaluated whenever the masked interrupt
    // status changes.
    SC_METHOD(updateIrq);
    sensitive << irq_event;
    bus.register_b_transport(this, &UartTLM::busReadWrite);
    // clear regs
    bzero((void *)&regs, sizeof(regs));
    regs.uartifls = UART_IFLS_RESET; // 1/2,1/2 trigger levels at reset
    set(regs.uartfr, UART_TXFE);
    set(regs.uartfr, UART_RXFE);
}

void UartTLM::busThread()
{
    while (true)
    {
        if (tx_hold.empty())
        {
            set(regs.uartfr, UART_TXFE);
            clr(regs.uartfr, UART_TXFF);
            updateTxIntr(); // empty FIFO is at/below trigger -> TX int asserts
            wait(txReceived);
        }
        unsigned char data = tx_hold.front();
        tx_hold.pop();
        clr(regs.uartfr, UART_TXFE);
        clr(regs.uartfr, UART_TXFF);
        updateTxIntr(); // FIFO drained one entry; re-evaluate TX trigger
        tx.write(data);
        wait(SC_ZERO_TIME);
    }
}

void UartTLM::rxMethod()
{
    if (rx.event())
    {
        unsigned char data = rx.read();
        if (rx_buffer.size() >= UART_FIFO_DEPTH)
        {
            // Overrun: FIFO full, the new character is discarded and the
            // overrun flag/interrupt is raised.
            set(regs.uartrsr, UART_RSR_OE);
            genIntr(UART_OERIS);
            return;
        }
        rx_buffer.push(data);
        clr(regs.uartfr, UART_RXFE);
        if (rx_buffer.size() >= UART_FIFO_DEPTH)
        {
            set(regs.uartfr, UART_RXFF);
        }
        // RX interrupt once the FIFO reaches the programmed trigger level.
        if (rx_buffer.size() >= rxTrigEntries())
        {
            genIntr(UART_RXRIS);
        }
        // (Re)arm the receive-timeout: it fires only if no further data
        // arrives within rx_timeout_period while the FIFO is non-empty.
        rx_timeout_evt.notify(rx_timeout_period);
    }
}

uint32_t UartTLM::busRead(uint32_t uaddr)
{
    uint32_t res = 0;
    switch (uaddr)
    {
    case UARTDR:
        if (!rx_buffer.empty())
        {
            res = rx_buffer.front();
            rx_buffer.pop();
            clr(regs.uartfr, UART_RXFF);
            if (rx_buffer.size() < rxTrigEntries()) {
                clr(regs.uartris, UART_RXRIS);
            }
            if (rx_buffer.empty()) {
                set(regs.uartfr, UART_RXFE);
                // Draining the FIFO clears the receive-timeout condition.
                clr(regs.uartris, UART_RTRIS);
                rx_timeout_evt.cancel();
            }
            setIntrFlags();
        }
        break;
    case UARTRSR: { // 0x004 read side: receive status error flags
        res = regs.uartrsr;
        break;
    }
    case UARTFR: {
        res = regs.uartfr;
        break;
    }
    case UARTIFLS: {
        res = regs.uartifls;
        break;
    }
    case UARTIMSC: {
        res = regs.uartimsc;
        break;
    }
    case UARTMIS: {
        res = regs.uartmis;
        break;
    }
    case UARTRIS: {
        res = regs.uartris;
        break;
    }
    default: break;
    }
    return res;
}

void UartTLM::busWrite(uint32_t uaddr, uint32_t wdata)
{
    switch (uaddr)
    {
    case UARTDR: {
        if (tx_hold.size() < UART_FIFO_DEPTH)
        {
            tx_hold.push(wdata);
            clr(regs.uartfr, UART_TXFE);
            if (tx_hold.size() >= UART_FIFO_DEPTH) set(regs.uartfr, UART_TXFF);
            // Filling above the trigger level deasserts the TX interrupt.
            updateTxIntr();
            txReceived.notify();
        }
        break;
    }
    case UARTECR: { // 0x004 write side: clear receive-status error flags
        regs.uartrsr = 0;
        break;
    }
    case UARTIFLS: {
        regs.uartifls = wdata & 0x3F; // TXIFLSEL[2:0], RXIFLSEL[5:3]
        // Trigger levels changed; re-evaluate level-based interrupts.
        if (rx_buffer.size() >= rxTrigEntries())
            set(regs.uartris, UART_RXRIS);
        else
            clr(regs.uartris, UART_RXRIS);
        updateTxIntr();
        break;
    }
    case UARTIMSC: {
        regs.uartimsc = wdata; // enable/disable interrupts
        setIntrFlags();
        break;
    }
    case UARTICR: {
        // Write-1-to-clear any raw interrupt bit (UARTICR).
        clr(regs.uartris, wdata & UART_INT_ALL);
        setIntrFlags();
        break;
    }
    case UARTCR: {
        regs.uartcr = wdata;
        break;
    }
    case UARTLCR_H: {
        regs.uartlcr_h = wdata;
        break;
    }
    default: break;
    }
}

void UartTLM::busReadWrite(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay)
{
    sc_dt::uint64 addr = trans.get_address();
    unsigned char *maskPtr = trans.get_byte_enable_ptr();
    unsigned char *dataPtr = trans.get_data_ptr();
    tlm::tlm_command cmd = trans.get_command();
    uint32_t uaddr = static_cast<uint32_t>(addr);
    switch (cmd)
    {
        // registers are 32-bit; copy result to 4-byte payload
    case tlm::TLM_READ_COMMAND: {
        uint32_t val = busRead(uaddr);
        memcpy(dataPtr, &val, 4);
        break;
    }
    case tlm::TLM_WRITE_COMMAND: {
        uint32_t val;
        memcpy(&val, dataPtr, 4);
        busWrite(uaddr, val);
        break;
    }
    case tlm::TLM_IGNORE_COMMAND: {
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    default: break;
    }
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
    delay += sc_time(10, SC_NS);
}
//calculate masked interrupt
void UartTLM::setIntrFlags() {
    regs.uartmis = regs.uartris & regs.uartimsc;
    // Notify on every change so the IRQ line follows both assertion and
    // deassertion (level-sensitive UARTINTR).
    irq_event.notify(SC_ZERO_TIME);
}

// Drive the combined interrupt as a level: high while any masked
// interrupt source is pending.
void UartTLM::updateIrq() {
    irq.write(regs.uartmis != 0);
}

// Transmit interrupt: asserted while the TX FIFO level is at or below the
// programmed trigger level (level-sensitive in FIFO mode).
void UartTLM::updateTxIntr() {
    if (tx_hold.size() <= txTrigEntries())
        set(regs.uartris, UART_TXRIS);
    else
        clr(regs.uartris, UART_TXRIS);
    setIntrFlags();
}

// Map a UARTIFLS 3-bit field select to a FIFO fill level in entries, for a
// 16-deep FIFO: 1/8, 1/4, 1/2, 3/4, 7/8.
static unsigned ifls_entries(uint32_t sel) {
    static const unsigned tbl[5] = {UART_FIFO_DEPTH / 8, UART_FIFO_DEPTH / 4,
                                    UART_FIFO_DEPTH / 2, (UART_FIFO_DEPTH * 3) / 4,
                                    (UART_FIFO_DEPTH * 7) / 8};
    if (sel > 4) sel = 4; // reserved encodings behave as 7/8
    return tbl[sel];
}

unsigned UartTLM::rxTrigEntries() const {
    return ifls_entries((regs.uartifls >> 3) & 0x7); // RXIFLSEL[5:3]
}

unsigned UartTLM::txTrigEntries() const {
    return ifls_entries(regs.uartifls & 0x7); // TXIFLSEL[2:0]
}

// Receive-timeout interrupt: the timer expired with RX data still in the
// FIFO and no new data received in the meantime.
void UartTLM::rxTimeout() {
    if (!rx_buffer.empty()) {
        set(regs.uartris, UART_RTRIS);
        setIntrFlags();
    }
}

void UartTLM::genIntr(uint32_t ierFlag) 
{
    set(regs.uartris, ierFlag);
    setIntrFlags();
}
void UartTLM::clrIntr(uint32_t ierFlag)
{
    clr(regs.uartris, ierFlag);
    setIntrFlags();
}
// flag handle
void UartTLM::set(uint32_t &reg, uint32_t flag)
{
    reg |= flag;
}
void UartTLM::clr(uint32_t &reg, uint32_t flag)
{
    reg &= ~flag;
}
// are bits set in register
bool UartTLM::isSet(uint32_t reg, uint32_t flag)
{
    return flag == (reg & flag);
}
// no bits in register
bool UartTLM::isClr(uint32_t reg, uint32_t flag)
{
    return flag != (reg & flag);
}