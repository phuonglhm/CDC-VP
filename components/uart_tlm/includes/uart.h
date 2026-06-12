#ifndef UART_H
#define UART_H

#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_target_socket.h"
#include <queue>

//offsets
#define UARTDR 0x000 //data register
#define UARTECR 0x004 //error clear
#define UARTFR 0x018 //flag reg
#define UARTIBRD 0x024 //integer baudrate reg
#define UARTFBRD 0x028 //fractional baudrate reg
#define UARTLCR_H 0x02C //line control reg
#define UARTCR 0x030 //control reg
#define UARTIMSC 0x038 //interrupt mask set/clear reg
#define UARTRIS 0x03C //raw interrupt status reg
#define UARTMIS 0x040 //masked interrupt status reg
#define UARTICR 0x044 //interrupt clear reg

#define UART_TXFE 0x80
#define UART_TXFF 0x20
#define UART_DSR 0x04
#define UART_RXFE 0x10
#define UART_RXFF 0x40
#define UART_RXRIS 0x10 //receive interrupt status

class UartTLM : public sc_core::sc_module {
    public:
    UartTLM (sc_core::sc_module_name name);
    tlm_utils::simple_target_socket<UartTLM> bus;
    sc_core::sc_buffer<unsigned char> rx;
    sc_core::sc_out<unsigned char> tx;

    protected:
    virtual void busThread();
    //blocking transports
    virtual void busReadWrite( tlm::tlm_generic_payload &payload, sc_core::sc_time &delay);
    virtual void busWrite(uint32_t uaddr, uint32_t wdata);
    //interrupt handle
    void setIntrFlags();
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
    uint32_t uartimsc; //interrupt mask
    uint32_t uartris; //raw interrupt status
    uint32_t uartmis; //masked interrupt status
    } regs;

    std::queue<unsigned char> rx_buffer;
    std::queue<unsigned char> tx_hold;
    sc_core::sc_event irq_event;

    private:
    void rxMethod();
    uint32_t busRead(uint32_t uaddr);
};

#endif
