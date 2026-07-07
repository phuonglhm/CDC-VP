#include <array>
#include <cstdint>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <flash_nor_tlm.h>
#include <tlm_probe.h>

namespace {

class flash_probe : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<flash_probe> socket;

    explicit flash_probe(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
    }

    tlm::tlm_response_status shift(unsigned char* data, unsigned len)
    {
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        trans.set_command(tlm::TLM_IGNORE_COMMAND);
        trans.set_address(0);
        trans.set_data_ptr(data);
        trans.set_data_length(len);
        trans.set_streaming_width(len);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        socket->b_transport(trans, delay);
        return trans.get_response_status();
    }
};

// Drives the byte-stream SPI face the way spi_tlm does: one 16-bit write
// per frame, low byte = MOSI, MISO returned in the low byte.
class spi_master_probe : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<spi_master_probe> socket;

    explicit spi_master_probe(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
    }

    std::uint8_t xfer(std::uint8_t mosi)
    {
        std::uint16_t frame = mosi;
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        trans.set_write();
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&frame));
        trans.set_data_length(sizeof(frame));
        socket->b_transport(trans, delay);
        CDC_CHECK(trans.is_response_ok());
        return static_cast<std::uint8_t>(frame & 0xff);
    }
};

} // namespace

int sc_main(int, char**)
{
    cdc::components::flash_nor_tlm flash("flash", 256);
    flash_probe probe("probe");
    probe.socket.bind(flash.from_qspi_socket);
    spi_master_probe spi("spi");
    spi.socket.bind(flash.from_spi_socket);

    const std::array<std::uint8_t, 8> image{{0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88}};
    flash.load(image.data(), image.size(), 0x10);

    sc_core::sc_spawn([&] {
        std::array<unsigned char, 4> rdid{{0x9f, 0x00, 0x00, 0x00}};
        CDC_CHECK(probe.shift(rdid.data(), rdid.size()) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(rdid[1] == 0xef);
        CDC_CHECK(rdid[2] == 0x40);
        CDC_CHECK(rdid[3] == 0x18);

        std::array<unsigned char, 8> read{{0x03, 0x00, 0x00, 0x10, 0, 0, 0, 0}};
        CDC_CHECK(probe.shift(read.data(), read.size()) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(read[4] == 0x11);
        CDC_CHECK(read[5] == 0x22);
        CDC_CHECK(read[6] == 0x33);
        CDC_CHECK(read[7] == 0x44);

        std::array<unsigned char, 8> fast_read{{0x0b, 0x00, 0x00, 0x14, 0, 0, 0, 0}};
        CDC_CHECK(probe.shift(fast_read.data(), fast_read.size()) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(fast_read[5] == 0x55);
        CDC_CHECK(fast_read[6] == 0x66);
        CDC_CHECK(fast_read[7] == 0x77);

        // ── Byte-stream SPI face ─────────────────────────────────────────
        // Deselected: MISO tri-stated, frame echoes back (loopback), and
        // nothing is decoded (the 0x03 below must NOT start a command).
        CDC_CHECK(spi.xfer(0x03) == 0x03);
        CDC_CHECK(spi.xfer(0x5a) == 0x5a);

        // READ 0x03 @0x10: CMD + 3 addr bytes clock out 0xff, then data.
        flash.spi_cs(true);
        CDC_CHECK(spi.xfer(0x03) == 0xff);
        CDC_CHECK(spi.xfer(0x00) == 0xff);
        CDC_CHECK(spi.xfer(0x00) == 0xff);
        CDC_CHECK(spi.xfer(0x10) == 0xff);
        CDC_CHECK(spi.xfer(0xff) == 0x11);
        CDC_CHECK(spi.xfer(0xff) == 0x22);
        CDC_CHECK(spi.xfer(0xff) == 0x33);
        flash.spi_cs(false); // CS deassert ends the command mid-stream

        // New command after deassert starts fresh (different address).
        flash.spi_cs(true);
        CDC_CHECK(spi.xfer(0x03) == 0xff);
        CDC_CHECK(spi.xfer(0x00) == 0xff);
        CDC_CHECK(spi.xfer(0x00) == 0xff);
        CDC_CHECK(spi.xfer(0x14) == 0xff);
        CDC_CHECK(spi.xfer(0xff) == 0x55);
        flash.spi_cs(false);

        // Unsupported opcode on this face: dead (0xff) until CS toggles.
        flash.spi_cs(true);
        CDC_CHECK(spi.xfer(0x9f) == 0xff);
        CDC_CHECK(spi.xfer(0xff) == 0xff);
        CDC_CHECK(spi.xfer(0xff) == 0xff);
        flash.spi_cs(false);

        sc_core::sc_stop();
    });

    sc_core::sc_start();
    return cdc::test::failures() == 0 ? 0 : 1;
}
