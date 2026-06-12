#include <iostream>
#include <cstring>
#include "uart.h"
using namespace sc_core;

SC_HAS_PROCESS(UartTLM);

UartTLM::UartTLM(sc_module_name name) : sc_module(name)
{
    SC_THREAD(busThread);
    SC_METHOD(rxMethod);
    sensitive << rx;
    dont_initialize();
    bus.register_b_transport(this, &UartTLM::busReadWrite);
    // clear regs
    bzero((void *)&regs, sizeof(regs));
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
            wait(txReceived);
        }
        unsigned char data = tx_hold.front();
        tx_hold.pop();
        clr(regs.uartfr, UART_TXFE);
        tx.write(data);
        wait(SC_ZERO_TIME);
    }
}

void UartTLM::rxMethod()
{
    if (rx.event())
    {
        unsigned char data = rx.read();
        if (rx_buffer.size() >= 16)
            return;
        rx_buffer.push(data);
        clr(regs.uartfr, UART_RXFE);
        if (rx_buffer.size() >= 16)
        {
            set(regs.uartfr, UART_RXFF);
        }
        // interrupt if half full
        if (rx_buffer.size() >= 8)
        {
            set(regs.uartris, UART_RXRIS);
            genIntr(UART_RXRIS); // raw inerrupt status 0x10 but i cant find a flag for it
        }
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
            if (rx_buffer.empty()) set(regs.uartfr, UART_RXFE);
            clr(regs.uartfr, UART_RXFF);
            if (rx_buffer.size() < 8) {
                clr(regs.uartris, UART_RXRIS);
                setIntrFlags();
            }
        }
        break;
    case UARTFR: {
        res = regs.uartfr;
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
        if (tx_hold.size() < 16)
        {
            tx_hold.push(wdata);
            clr(regs.uartfr, UART_TXFE);
            if (tx_hold.size() >= 16) set(regs.uartfr, UART_TXFF);
            txReceived.notify();
        }
        break;
    }
    case UARTIMSC: {
        regs.uartimsc = wdata; // enable/disable interrupts
        setIntrFlags();
        break;
    }
    case UARTICR: {
        if (wdata & UART_RXRIS)
        {
            clr(regs.uartris, wdata);
            setIntrFlags();
        }
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
        //pl011 is 32bit, copy result to 4byte payload
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
    if (regs.uartmis != 0) {
        irq_event.notify(SC_ZERO_TIME);
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
