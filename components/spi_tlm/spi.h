#define SC_INCLUDE_DYNAMIC_PROCESSES

#include "systemc"
using namespace sc_core;
using namespace sc_dt;
using namespace std;

#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"

constexpr int FIFO_SIZE = 8; // 8-location deep, as specified by arm

SC_MODULE(spi_controller) {

   tlm_utils::simple_target_socket<spi_controller> from_apb_socket;
   tlm_utils::simple_initiator_socket<spi_controller> to_peri_socket;

   SC_CTOR(spi_controller);

   void b_transport(tlm::tlm_generic_payload & trans, sc_time & delay);

private:
   const uint64_t base_addr = 0x0;
   const uint32_t clk = 100 * 1000000; // anh duy bao the, 100 Mhz

   uint16_t reg_cr0;
   uint16_t reg_cr1;
   uint16_t reg_dr;
   uint16_t reg_sr;
   uint16_t reg_cpsr;
   uint16_t reg_imsc;
   uint16_t reg_ris;
   uint16_t reg_mis;
   uint16_t reg_icr;
   uint16_t reg_dmacr;

   // readonly IDs
   const uint8_t reg_periph_id0 = 0x22;
   const uint8_t reg_periph_id1 = 0x10;
   const uint8_t reg_periph_id2 = 0x34;
   const uint8_t reg_periph_id3 = 0x00;

   // readonly IDs
   const uint8_t reg_cell_id0 = 0x0d;
   const uint8_t reg_cell_id1 = 0xf0;
   const uint8_t reg_cell_id2 = 0x05;
   const uint8_t reg_cell_id3 = 0xb1;

   sc_fifo<uint16_t> rx_fifo;
   sc_fifo<uint16_t> tx_fifo;

   sc_event transmission_event;
   void transmit(); // bound to a thread to be called whenever SPI receives a payload

   bool write_reg(uint8_t offset, uint16_t value);
   uint16_t read_reg(uint16_t offset);

   sc_event possible_intr_event;
   void update_intr();

   sc_event status_event;
   void update_status_reg();
   // this is 1 if the simulated hardware is currently transmitting bits to peripheral, 0 otherwise.
   bool is_transmitting = false;

   void handle_reset();

public:
   sc_out<bool> intr;

   sc_in<bool> reset;
};
