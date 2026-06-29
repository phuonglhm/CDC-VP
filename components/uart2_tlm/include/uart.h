#ifndef UART_H
#define UART_H

#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_target_socket.h"
#include <queue>

//offsets
#define UARTDR 0x000 //data register
#define UARTRSR 0x004 //receive status (read) / UARTECR error clear (write)
#define UARTECR 0x004 //error clear (write side of 0x004)
#define UARTFR 0x018 //flag reg
#define UARTIBRD 0x024 //integer baudrate reg
#define UARTFBRD 0x028 //fractional baudrate reg
#define UARTLCR_H 0x02C //line control reg
#define UARTCR 0x030 //control reg
#define UARTIFLS 0x034 //interrupt FIFO level select reg
#define UARTIMSC 0x038 //interrupt mask set/clear reg
#define UARTRIS 0x03C //raw interrupt status reg
#define UARTMIS 0x040 //masked interrupt status reg
#define UARTICR 0x044 //interrupt clear reg

// UARTFR (flag register) bits
#define UART_TXFE 0x80
#define UART_TXFF 0x20
#define UART_DSR 0x04
#define UART_RXFE 0x10
#define UART_RXFF 0x40

// UARTRIS/UARTMIS/UARTIMSC/UARTICR interrupt bits (PL011)
#define UART_RXRIS 0x10  //bit4 receive interrupt
#define UART_TXRIS 0x20  //bit5 transmit interrupt
#define UART_RTRIS 0x40  //bit6 receive-timeout interrupt
#define UART_FERIS 0x80  //bit7 framing-error interrupt
#define UART_PERIS 0x100 //bit8 parity-error interrupt
#define UART_BERIS 0x200 //bit9 break-error interrupt
#define UART_OERIS 0x400 //bit10 overrun-error interrupt
#define UART_INT_ALL (UART_RXRIS | UART_TXRIS | UART_RTRIS | \
                      UART_FERIS | UART_PERIS | UART_BERIS | UART_OERIS)

// UARTRSR/UARTDR[11:8] receive-status error bits.
#define UART_RSR_FE 0x01 //framing error
#define UART_RSR_PE 0x02 //parity error
#define UART_RSR_BE 0x04 //break error
#define UART_RSR_OE 0x08 //overrun error

// UARTIFLS field encodings: TXIFLSEL[2:0], RXIFLSEL[5:3]. Reset 0x12 = 1/2,1/2.
#define UART_IFLS_RESET 0x12
#define UART_FIFO_DEPTH 16

class UartTLM : public sc_core::sc_module {
    public:
    // rx_timeout models the PL011 receive-timeout period (real hardware uses
    // 32 baud clocks). It is abstract here (no baud model) and configurable.
    UartTLM (sc_core::sc_module_name name,
             sc_core::sc_time rx_timeout = sc_core::sc_time(1, sc_core::SC_MS));
    tlm_utils::simple_target_socket<UartTLM> bus;
    sc_core::sc_buffer<unsigned char> rx;
    sc_core::sc_out<unsigned char> tx;
    // PL011 UARTINTR: level-sensitive combined interrupt to the PLIC.
    // High whenever any masked interrupt is pending (UARTMIS != 0).
    sc_core::sc_out<bool> irq;

    protected:
    virtual void busThread();
    //blocking transports
    virtual void busReadWrite( tlm::tlm_generic_payload &payload, sc_core::sc_time &delay);
    virtual void busWrite(uint32_t uaddr, uint32_t wdata);
    //interrupt handle
    void setIntrFlags();
    void updateIrq();
    void updateTxIntr();   // re-evaluate TX FIFO-level interrupt
    void rxTimeout();      // receive-timeout expiry
    unsigned rxTrigEntries() const; // RX trigger level in FIFO entries (UARTIFLS)
    unsigned txTrigEntries() const; // TX trigger level in FIFO entries (UARTIFLS)
    virtual void genIntr(uint32_t ierFlag);
    virtual void clrIntr(uint32_t ierFlag);
    //flag handle
    void set(uint32_t &reg, uint32_t flag);
    void clr(uint32_t &reg, uint32_t flag );
    bool isSet(uint32_t reg, uint32_t flag);
    bool isClr(uint32_t reg, uint32_t flag);

    sc_core::sc_event  txReceived;
    struct {
    uint32_t uartfr; // flag reg
    uint32_t uartibrd; //interger baudrate
    uint32_t uartfbrd; //fractional baudrate
    uint32_t uartlcr_h; //line control
    uint32_t uartcr; //control reg
    uint32_t uartifls; //interrupt FIFO level select
    uint32_t uartimsc; //interrupt mask
    uint32_t uartris; //raw interrupt status
    uint32_t uartmis; //masked interrupt status
    uint32_t uartrsr; //receive status (error flags of last RX char)
    } regs;

    std::queue<unsigned char> rx_buffer;
    std::queue<unsigned char> tx_hold;
    sc_core::sc_event irq_event;
    sc_core::sc_event rx_timeout_evt;
    sc_core::sc_time rx_timeout_period;

    private:
    void rxMethod();
    uint32_t busRead(uint32_t uaddr);
};

#endif