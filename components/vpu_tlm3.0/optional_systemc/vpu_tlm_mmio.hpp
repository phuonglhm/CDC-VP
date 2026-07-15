#pragma once

#include "model/vpu_mmio.hpp"

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <cstdint>
#include <limits>
#include <span>

namespace model::systemc_tlm {

// SystemC/TLM-2.0 wrapper for the SystemC-independent VPU register model.
// mmio_socket is a 32-bit little-endian target; dma_socket is a byte-addressed
// initiator used by the VPU to fetch YUV and store Annex-B output.
class VpuTlmMmio final : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(VpuTlmMmio);

    tlm_utils::simple_target_socket<VpuTlmMmio> mmio_socket{"mmio_socket"};
    tlm_utils::simple_initiator_socket<VpuTlmMmio> dma_socket{"dma_socket"};
    sc_core::sc_out<bool> irq{"irq"};

    explicit VpuTlmMmio(sc_core::sc_module_name name)
        : sc_module(name), dma_memory_(dma_socket),
          core_(dma_memory_, [this](bool level) { irq.write(level); }) {
        mmio_socket.register_b_transport(this, &VpuTlmMmio::b_transport);
        irq.initialize(false);
        SC_THREAD(worker);
    }

    [[nodiscard]] VpuMmioDevice& functional_model() { return core_; }
    [[nodiscard]] const VpuMmioDevice& functional_model() const { return core_; }

private:
    class DmaMemory final : public MemoryInterface {
    public:
        explicit DmaMemory(
            tlm_utils::simple_initiator_socket<VpuTlmMmio>& socket)
            : socket_(socket) {}

        bool read(std::uint64_t address,
                  std::span<std::uint8_t> destination) override {
            return transport(tlm::TLM_READ_COMMAND, address,
                             destination.data(), destination.size());
        }

        bool write(std::uint64_t address,
                   std::span<const std::uint8_t> source) override {
            return transport(tlm::TLM_WRITE_COMMAND, address,
                             const_cast<std::uint8_t*>(source.data()),
                             source.size());
        }

    private:
        bool transport(tlm::tlm_command command, std::uint64_t address,
                       std::uint8_t* data, std::size_t size) {
            if (size == 0) return true;
            if (size > std::numeric_limits<unsigned>::max()) return false;

            tlm::tlm_generic_payload transaction;
            transaction.set_command(command);
            transaction.set_address(address);
            transaction.set_data_ptr(data);
            transaction.set_data_length(static_cast<unsigned>(size));
            transaction.set_streaming_width(static_cast<unsigned>(size));
            transaction.set_byte_enable_ptr(nullptr);
            transaction.set_dmi_allowed(false);
            transaction.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            socket_->b_transport(transaction, delay);
            if (delay != sc_core::SC_ZERO_TIME) sc_core::wait(delay);
            return transaction.get_response_status() == tlm::TLM_OK_RESPONSE;
        }

        tlm_utils::simple_initiator_socket<VpuTlmMmio>& socket_;
    };

    static std::uint32_t load_le32(const unsigned char* bytes) {
        return static_cast<std::uint32_t>(bytes[0]) |
               (static_cast<std::uint32_t>(bytes[1]) << 8) |
               (static_cast<std::uint32_t>(bytes[2]) << 16) |
               (static_cast<std::uint32_t>(bytes[3]) << 24);
    }

    static void store_le32(unsigned char* bytes, std::uint32_t value) {
        bytes[0] = static_cast<unsigned char>(value);
        bytes[1] = static_cast<unsigned char>(value >> 8);
        bytes[2] = static_cast<unsigned char>(value >> 16);
        bytes[3] = static_cast<unsigned char>(value >> 24);
    }

    void b_transport(tlm::tlm_generic_payload& transaction,
                     sc_core::sc_time& delay) {
        const auto address = transaction.get_address();
        auto* data = transaction.get_data_ptr();
        if (data == nullptr || transaction.get_data_length() != 4 ||
            transaction.get_streaming_width() < 4 ||
            transaction.get_byte_enable_ptr() != nullptr || (address & 3U) != 0 ||
            address + 4 > vpu_reg::REGISTER_SPACE_BYTES) {
            transaction.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }

        if (transaction.get_command() == tlm::TLM_READ_COMMAND) {
            store_le32(data, core_.read32(static_cast<std::uint32_t>(address)));
        } else if (transaction.get_command() == tlm::TLM_WRITE_COMMAND) {
            const bool was_busy = core_.busy();
            core_.write32(static_cast<std::uint32_t>(address), load_le32(data));
            if (!was_busy && core_.busy()) start_event_.notify(sc_core::SC_ZERO_TIME);
        } else {
            transaction.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return;
        }

        delay += sc_core::sc_time(1, sc_core::SC_NS);
        transaction.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    void worker() {
        for (;;) {
            sc_core::wait(start_event_);
            while (core_.busy()) {
                // One SystemC nanosecond is one VPU architectural cycle.
                // Each tick advances the bounded FIFO pipeline exactly once.
                sc_core::wait(sc_core::sc_time(1, sc_core::SC_NS));
                (void)core_.tick();
            }
        }
    }

    DmaMemory dma_memory_;
    VpuMmioDevice core_;
    sc_core::sc_event start_event_;
};

} // namespace model::systemc_tlm
