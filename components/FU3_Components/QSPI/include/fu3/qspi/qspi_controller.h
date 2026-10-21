#pragma once
#include "registers.h"
#include "flash_transaction.h"
#include <array>
#include "qspi_register_model.h"
#include "qspi_tx_rx_fifo.h"
#include "qspi_dma_interface.h"
#include "qspi_interrupt.h"
#include "qspi_timing_model.h"
#include "ahb_transaction.h"
#include <vector>
#include <systemc>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

namespace fu3::qspi {
struct Config {
    unsigned tx_fifo_words=32, rx_fifo_words=32;
    unsigned bus_timeout_cycles=5000;
    unsigned mem_cache_count=8, prefetch_length=8;
    unsigned apb_async_fifo_depth=4, ahb_async_fifo_depth=8;
    void validate() const;
};

class QspiController : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket_optional<QspiController> ap_qspic{"ap_qspic"};
    tlm_utils::simple_target_socket_optional<QspiController> xip{"xip"};
    tlm_utils::simple_initiator_socket_optional<QspiController> flash{"flash"};
    sc_core::sc_in<bool> clk{"clk"}, pclk{"pclk"}, hclk{"hclk"}, dma_clk{"dma_clk"};
    sc_core::sc_in<bool> reset_n{"reset_n"}, preset_n{"preset_n"}, hreset_n{"hreset_n"};
    sc_core::sc_in<bool> dma_reset_n{"dma_reset_n"};
    sc_core::sc_in<bool> dma_rx_clr{"dma_rx_clr"}, dma_tx_clr{"dma_tx_clr"};
    sc_core::sc_out<bool> irq{"irq"}, dma_rx_req{"dma_rx_req"}, dma_tx_req{"dma_tx_req"};
    SC_HAS_PROCESS(QspiController);
    explicit QspiController(sc_core::sc_module_name, Config = {});
private:
    enum class Operation { None, Write, Read, Poll, Xip };
    Config cfg_;
    QspiRegisterModel r_;
    QspiTxRxFifo fifo_;
    QspiDmaInterface dma_;
    QspiInterrupt interrupt_;
    QspiTimingModel timing_;
    std::array<unsigned char,4> chunk_{};
    Operation op_=Operation::None;
    bool busy_=false, serial_done_=false, instruction_sent_=false;
    bool frame_open_=false, chunk_pending_=false, backend_called_=false;
    bool abort_pending_=false, completion_pending_=false, dma_inhibited_=false;
    bool cache_fill_enabled_=false, xip_start_=false;
    std::uint64_t abort_cycle_=0;
    std::vector<std::uint64_t> byte_boundaries_;
    struct CacheLine { std::uint32_t address=0, value=0; bool valid=false; };
    std::vector<CacheLine> cache_;
    std::array<bool,2> rx_sync_{},tx_sync_{};
    bool rx_clear_pending_=false,tx_clear_pending_=false;
    bool rx_clear_ack_=false,tx_clear_ack_=false;
    std::uint64_t dma_generation_=0, dma_seen_generation_=0;
    std::uint64_t transferred_=0, total_=0, accepted_=0;
    std::uint64_t frame_id_=0, p_epoch_=0, h_epoch_=0;
    unsigned chunk_size_=0;
    std::uint32_t address_=0, xip_base_=0, last_status_=0;
    tlm::tlm_generic_payload* ap_pending_=nullptr;
    tlm::tlm_generic_payload* xip_pending_=nullptr;
    sc_core::sc_mutex ap_lock_, xip_lock_;
    sc_core::sc_event ap_done_, xip_done_, changed_;
    void tick();
    void ahb_tick();
    void dma_tick();
    void flush_cache();
    void request_abort();
    void interface_reset();
    void outputs();
    void ap_transport(tlm::tlm_generic_payload&, sc_core::sc_time&);
    void xip_transport(tlm::tlm_generic_payload&, sc_core::sc_time&);
    void transport(tlm::tlm_generic_payload&, sc_core::sc_time&, bool);
    unsigned debug(tlm::tlm_generic_payload&);
    static bool validate(tlm::tlm_generic_payload&, bool);
    void access(tlm::tlm_generic_payload&, bool);
    std::uint32_t read(unsigned, bool);
    void write(unsigned, std::uint32_t);
    unsigned mode() const { return r_.mode(); }
    unsigned data_mode() const { return r_.data_mode(); }
    std::uint64_t flash_size() const;
    std::uint32_t status() const;
    bool legal(bool) const;
    void start(Operation, std::uint32_t);
    void advance();
    void schedule_chunk();
    bool send_chunk(bool cancel=false);
    void complete_chunk();
    void stop(bool abort, bool error=false);
    void finish_xip(tlm::tlm_response_status);
    std::uint64_t phase_cycles(unsigned bytes, unsigned lines, bool ddr) const;
};
}
