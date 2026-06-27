#include "mini_soc_top.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <bus_router.h>
#include <i2c.h>
#include <memory_tlm.h>
#include <uart.h>

namespace cdc::platforms::mini_soc {
    namespace {
        constexpr std::uint64_t kUartBase = 0x1000'0000;
        constexpr std::uint64_t ki2cBase = 0x1001'0000;
        constexpr std::uint64_t kRamBase = 0x8000'0000;
        constexpr std::uint64_t kRegionSize = 0x1000;
        constexpr std::uint64_t kRegionSize_ram = 0x10000;

    class cpu_stub : public sc_core::sc_module {
        public:
            tlm_utils::simple_initiator_socket<cpu_stub> bus_socket;
            sc_core::sc_in<bool> i2c_irq;

            explicit cpu_stub(sc_core::sc_module_name name)
                : sc_core::sc_module(name)
                , bus_socket("bus_socket")
                , i2c_irq("i2c_irq")
            {
                SC_HAS_PROCESS(cpu_stub);
                SC_THREAD(run);
                sensitive << i2c_irq.pos();
            }
        private:
            void run()
            {
                write_uart("Hello from mini SoC \n");        
                 // Prove the RAM path via the bus_router: write a pattern, read it back.
                constexpr std::uint32_t kPattern = 0xCAFEBABEU;
                write32(kRamBase, kPattern);
                if (read32(kRamBase) != kPattern) {
                    throw std::runtime_error("RAM read-back mismatch");
                }

                write32(ki2cBase + 0x4C, 0x1U);  // IIER  : bật interrupt enable cho TRANSFER_DONE (bit0)
                write32(ki2cBase + 0x00, 0x1U);  // IER   : bật nguồn IP
                write32(ki2cBase + 0x04, 0x1U);  // ICTLR : START -> serviceController() set TRANSFER_DONE
                // sau write này hasInterrupt()==true -> I2C kéo irq lên
                wait(i2c_irq.posedge_event());
                write_uart("I2C IRQ fired\n");   // sửa luôn chữ "at 10ms" (I2C không có 10ms)


              
            }
            void write_uart(const char* text)
            {
                for (const char* cursor = text; *cursor != '\0'; ++cursor) {
                    const auto ch = static_cast<unsigned char>(*cursor);
                    write8(kUartBase, ch);
                }
            }
            void write8(std::uint64_t addr, std::uint8_t value)
            {
                tlm::tlm_generic_payload trans;
                sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

                trans.set_command(tlm::TLM_WRITE_COMMAND);
                trans.set_address(addr);
                trans.set_data_ptr(&value);
                trans.set_data_length(sizeof(value));
                trans.set_streaming_width(sizeof(value));
                trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

                bus_socket->b_transport(trans, delay);
                if (trans.is_response_error()) {
                    throw std::runtime_error("UART write failed");
                }
            }
            void write32(std::uint64_t addr, std::uint32_t value)
            {
                std::array<unsigned char, sizeof(value)> bytes{};
                std::memcpy(bytes.data(), &value, sizeof(value));

                tlm::tlm_generic_payload trans;
                sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

                trans.set_command(tlm::TLM_WRITE_COMMAND);
                trans.set_address(addr);
                trans.set_data_ptr(bytes.data());
                trans.set_data_length(bytes.size());
                trans.set_streaming_width(bytes.size());
                trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

                bus_socket->b_transport(trans, delay);
                if (trans.is_response_error()) {
                    throw std::runtime_error("word write failed");
                }
            }
            std::uint32_t read32(std::uint64_t addr)
            {
                std::array<unsigned char, sizeof(std::uint32_t)> bytes{};

                tlm::tlm_generic_payload trans;
                sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

                trans.set_command(tlm::TLM_READ_COMMAND);
                trans.set_address(addr);
                trans.set_data_ptr(bytes.data());
                trans.set_data_length(bytes.size());
                trans.set_streaming_width(bytes.size());
                trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

                bus_socket->b_transport(trans, delay);
                if (trans.is_response_error()) {
                    throw std::runtime_error("word read failed");
                }

                std::uint32_t value = 0;
                std::memcpy(&value, bytes.data(), sizeof(value));
                return value;
            }
    };
    } //namespace 
    struct mini_soc_top::impl : public sc_core::sc_module {
    cpu_stub cpu;
    cdc::components::bus_router bus;
    UartTLM uart;
   sc_core::sc_buffer<unsigned char> uart_tx;
   sc_core::sc_signal<bool> uart_irq;
    i2c i2c0;
    cdc::components::memory_tlm ram;
    sc_core::sc_signal<bool> i2c_irq; // sợi dây vật lý

    impl(sc_core::sc_module_name name, const std::string& config_path)
        : sc_core::sc_module(name)
        , cpu("cpu")
        , bus("bus", 3)
        , uart("uart")
       , uart_tx("uart_tx")
       , uart_irq("uart_irq")
        , i2c0("i2c0")
        , ram("ram", kRegionSize_ram)
        , i2c_irq("i2c_irq")
    {
        // duyptt note:  Nối ouput của CPU vào Input của BUS Từ giờ CPU gọi bus_socket->b_transport(...) thì giao dịch chui vào bus_router.
        cpu.bus_socket.bind(bus.target_socket); 

        // duyptt note: nối day và memory map
        bus.add_target(kUartBase, kRegionSize).bind(uart.bus);
      uart.tx(uart_tx);
      uart.irq(uart_irq); // địa chỉ bắt đầu, độ dài , và dành 1 socket cho uart 
        bus.add_target(ki2cBase, kRegionSize).bind(i2c0.socket); // địa chỉ bắt đầu, độ dài , và dành 1 socket cho i2c
        bus.add_target(kRamBase, kRegionSize_ram).bind(ram.socket); // địa chỉ bắt đầu, độ dài , và dành 1 socket cho ram 

        // dây tín hiệu IRQ 
        i2c0.irq(i2c_irq); // đầu ra của i2c hàn vào dây.
        cpu.i2c_irq(i2c_irq); // đầu vào của CPU hàn vào cùng dây đó

        if (!config_path.empty()) {
            std::cout << "mini_soc config: " << config_path << '\n';
        }
    }
};

mini_soc_top::mini_soc_top(sc_core::sc_module_name name, std::string config_path)
    : sc_core::sc_module(name)
    , impl_(std::make_unique<impl>("impl", config_path))
{
}

mini_soc_top::~mini_soc_top() = default;
}
