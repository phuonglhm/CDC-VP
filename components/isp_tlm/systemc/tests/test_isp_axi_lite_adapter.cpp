#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <string>

#include "axi/isp_axi_lite_adapter.h"
#include "registers/isp_register_map.h"
#include "tlm/isp_control_target.h"

namespace {

namespace axi = isp_tlm::axi;
namespace reg = isp_tlm::registers;
namespace control = isp_tlm::tlm_frontend;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

class AxiLiteTester : public sc_core::sc_module {
public:
    sc_core::sc_clock clock;
    sc_core::sc_signal<bool> reset_n;

    sc_core::sc_signal<sc_dt::sc_uint<32>> awaddr;
    sc_core::sc_signal<sc_dt::sc_uint<3>> awprot;
    sc_core::sc_signal<bool> awvalid;
    sc_core::sc_signal<bool> awready;
    sc_core::sc_signal<sc_dt::sc_uint<32>> wdata;
    sc_core::sc_signal<sc_dt::sc_uint<4>> wstrb;
    sc_core::sc_signal<bool> wvalid;
    sc_core::sc_signal<bool> wready;
    sc_core::sc_signal<sc_dt::sc_uint<2>> bresp;
    sc_core::sc_signal<bool> bvalid;
    sc_core::sc_signal<bool> bready;

    sc_core::sc_signal<sc_dt::sc_uint<32>> araddr;
    sc_core::sc_signal<sc_dt::sc_uint<3>> arprot;
    sc_core::sc_signal<bool> arvalid;
    sc_core::sc_signal<bool> arready;
    sc_core::sc_signal<sc_dt::sc_uint<32>> rdata;
    sc_core::sc_signal<sc_dt::sc_uint<2>> rresp;
    sc_core::sc_signal<bool> rvalid;
    sc_core::sc_signal<bool> rready;

    reg::IspRegisterBank bank;
    axi::IspAxiLiteAdapter adapter;
    control::IspControlTarget control_target;
    tlm_utils::simple_initiator_socket<AxiLiteTester> control_socket;

    SC_HAS_PROCESS(AxiLiteTester);
    explicit AxiLiteTester(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , clock("clock", sc_core::sc_time(10, sc_core::SC_NS))
        , reset_n("reset_n")
        , awaddr("awaddr")
        , awprot("awprot")
        , awvalid("awvalid")
        , awready("awready")
        , wdata("wdata")
        , wstrb("wstrb")
        , wvalid("wvalid")
        , wready("wready")
        , bresp("bresp")
        , bvalid("bvalid")
        , bready("bready")
        , araddr("araddr")
        , arprot("arprot")
        , arvalid("arvalid")
        , arready("arready")
        , rdata("rdata")
        , rresp("rresp")
        , rvalid("rvalid")
        , rready("rready")
        , bank()
        , adapter("adapter", bank)
        , control_target("control_target", bank)
        , control_socket("control_socket")
    {
        adapter.aclk(clock);
        adapter.aresetn(reset_n);
        adapter.awaddr(awaddr);
        adapter.awprot(awprot);
        adapter.awvalid(awvalid);
        adapter.awready(awready);
        adapter.wdata(wdata);
        adapter.wstrb(wstrb);
        adapter.wvalid(wvalid);
        adapter.wready(wready);
        adapter.bresp(bresp);
        adapter.bvalid(bvalid);
        adapter.bready(bready);
        adapter.araddr(araddr);
        adapter.arprot(arprot);
        adapter.arvalid(arvalid);
        adapter.arready(arready);
        adapter.rdata(rdata);
        adapter.rresp(rresp);
        adapter.rvalid(rvalid);
        adapter.rready(rready);

        control_socket.bind(control_target.socket);
        SC_THREAD(run);
    }

private:
    void drive_idle()
    {
        awaddr.write(0u);
        awprot.write(0u);
        awvalid.write(false);
        wdata.write(0u);
        wstrb.write(0u);
        wvalid.write(false);
        bready.write(false);
        araddr.write(0u);
        arprot.write(0u);
        arvalid.write(false);
        rready.write(false);
    }

    void cycle()
    {
        wait(clock.posedge_event());
        wait(sc_core::SC_ZERO_TIME);
    }

    void reset_and_enable()
    {
        drive_idle();
        reset_n.write(false);
        cycle();
        expect(!awready.read() && !wready.read() && !arready.read() &&
                   !bvalid.read() && !rvalid.read(),
               "synchronous reset clears all AXI handshakes");

        reset_n.write(true);
        cycle();
        expect(awready.read() && wready.read() && arready.read(),
               "adapter advertises empty channel slots after reset release");
    }

    static std::array<unsigned char, 4> little_endian(std::uint32_t value)
    {
        return {
            static_cast<unsigned char>(value & 0xffu),
            static_cast<unsigned char>((value >> 8u) & 0xffu),
            static_cast<unsigned char>((value >> 16u) & 0xffu),
            static_cast<unsigned char>((value >> 24u) & 0xffu),
        };
    }

    std::uint32_t tlm_read(std::uint32_t address, const std::string& label)
    {
        std::array<unsigned char, 4> bytes {0u, 0u, 0u, 0u};
        tlm::tlm_generic_payload transaction;
        transaction.set_command(tlm::TLM_READ_COMMAND);
        transaction.set_address(address);
        transaction.set_data_ptr(bytes.data());
        transaction.set_data_length(bytes.size());
        transaction.set_streaming_width(bytes.size());
        transaction.set_byte_enable_ptr(nullptr);
        transaction.set_byte_enable_length(0u);
        transaction.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        control_socket->b_transport(transaction, delay);
        expect(transaction.get_response_status() == tlm::TLM_OK_RESPONSE,
               label + ": TLM read succeeds");
        return static_cast<std::uint32_t>(bytes[0]) |
               (static_cast<std::uint32_t>(bytes[1]) << 8u) |
               (static_cast<std::uint32_t>(bytes[2]) << 16u) |
               (static_cast<std::uint32_t>(bytes[3]) << 24u);
    }

    void tlm_write(std::uint32_t address,
                   std::uint32_t value,
                   const std::string& label)
    {
        auto bytes = little_endian(value);
        tlm::tlm_generic_payload transaction;
        transaction.set_command(tlm::TLM_WRITE_COMMAND);
        transaction.set_address(address);
        transaction.set_data_ptr(bytes.data());
        transaction.set_data_length(bytes.size());
        transaction.set_streaming_width(bytes.size());
        transaction.set_byte_enable_ptr(nullptr);
        transaction.set_byte_enable_length(0u);
        transaction.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        control_socket->b_transport(transaction, delay);
        expect(transaction.get_response_status() == tlm::TLM_OK_RESPONSE,
               label + ": TLM write succeeds");
    }

    void consume_write_response()
    {
        bready.write(true);
        cycle();
        bready.write(false);
        expect(!bvalid.read(), "B handshake releases the write slot");
        expect(awready.read() && wready.read(),
               "AW and W become ready after B handshake");
    }

    void consume_read_response()
    {
        rready.write(true);
        cycle();
        rready.write(false);
        expect(!rvalid.read(), "R handshake releases the read slot");
        expect(arready.read(), "AR becomes ready after R handshake");
    }

    void simultaneous_write(std::uint32_t address,
                            std::uint32_t value,
                            std::uint8_t strobe,
                            std::uint8_t expected_response,
                            const std::string& label)
    {
        expect(awready.read() && wready.read(),
               label + ": write channels initially ready");
        awaddr.write(address);
        awvalid.write(true);
        wdata.write(value);
        wstrb.write(strobe);
        wvalid.write(true);
        cycle();
        awvalid.write(false);
        wvalid.write(false);
        expect(bvalid.read(), label + ": produces a write response");
        expect(bresp.read().to_uint() == expected_response,
               label + ": expected BRESP");
    }

    void begin_read(std::uint32_t address,
                    std::uint8_t expected_response,
                    const std::string& label)
    {
        expect(arready.read(), label + ": read address channel ready");
        araddr.write(address);
        arvalid.write(true);
        cycle();
        arvalid.write(false);
        expect(rvalid.read(), label + ": produces a read response");
        expect(rresp.read().to_uint() == expected_response,
               label + ": expected RRESP");
    }

    void test_independent_write_channels_and_strobes()
    {
        reset_and_enable();

        awaddr.write(reg::kIspSourceAddress);
        awprot.write(5u);
        awvalid.write(true);
        cycle();
        awvalid.write(false);
        expect(!awready.read() && wready.read() && !bvalid.read(),
               "AW can arrive first while W remains independently ready");

        wdata.write(0x11223344u);
        wstrb.write(0xfu);
        wvalid.write(true);
        cycle();
        wvalid.write(false);
        expect(bvalid.read() && bresp.read().to_uint() == axi::IspAxiLiteAdapter::kOkay,
               "later W completes an address-first write");
        expect(!awready.read() && !wready.read(),
               "one outstanding write blocks both request channels");

        const sc_dt::sc_uint<2> held_bresp = bresp.read();
        awaddr.write(reg::kDpcThreshold);
        wdata.write(0xffffffffu);
        cycle();
        expect(bvalid.read() && bresp.read() == held_bresp,
               "BVALID and BRESP remain stable under backpressure");
        expect(tlm_read(reg::kIspSourceAddress, "address-first visibility") ==
                   0x11223344u,
               "TLM frontend sees the AXI pending write");
        consume_write_response();

        wdata.write(0xaabbccddu);
        wstrb.write(0x5u);
        wvalid.write(true);
        cycle();
        wvalid.write(false);
        expect(awready.read() && !wready.read() && !bvalid.read(),
               "W can arrive first while AW remains independently ready");

        awaddr.write(reg::kIspSourceAddress);
        awvalid.write(true);
        cycle();
        awvalid.write(false);
        expect(bvalid.read() && bresp.read().to_uint() == axi::IspAxiLiteAdapter::kOkay,
               "later AW completes a data-first write");
        expect(tlm_read(reg::kIspSourceAddress, "WSTRB visibility") ==
                   0x11bb33ddu,
               "AXI WSTRB updates only selected register byte lanes");
        consume_write_response();

        std::uint32_t active = 0xffffffffu;
        expect(bank.read_active(reg::kIspSourceAddress, active) ==
                   reg::AccessResult::Ok && active == 0u,
               "AXI writes remain pending before frame commit");
        bank.commit_frame();
        expect(bank.read_active(reg::kIspSourceAddress, active) ==
                   reg::AccessResult::Ok && active == 0x11bb33ddu,
               "shared frame commit activates AXI-written configuration");
    }

    void test_reads_backpressure_and_cross_path_visibility()
    {
        tlm_write(reg::kIspSourceStrideBytes, 0x55667788u,
                  "cross-path source stride");

        arprot.write(3u);
        begin_read(reg::kIspSourceStrideBytes,
                   axi::IspAxiLiteAdapter::kOkay,
                   "AXI read of TLM-written register");
        expect(rdata.read().to_uint() == 0x55667788u,
               "AXI read sees TLM pending state");
        expect(!arready.read(),
               "one outstanding read blocks another AR request");

        tlm_write(reg::kIspSourceStrideBytes, 0xa1b2c3d4u,
                  "pending update during R backpressure");
        araddr.write(reg::kIspSensorWidth);
        cycle();
        expect(rvalid.read() &&
                   rdata.read().to_uint() == 0x55667788u &&
                   rresp.read().to_uint() == axi::IspAxiLiteAdapter::kOkay,
               "RVALID, RDATA, and RRESP remain stable under backpressure");
        consume_read_response();

        begin_read(reg::kIspSourceStrideBytes,
                   axi::IspAxiLiteAdapter::kOkay,
                   "AXI reread after response consumption");
        expect(rdata.read().to_uint() == 0xa1b2c3d4u,
               "new AXI read observes the later TLM write");
        consume_read_response();

        begin_read(reg::kIspSensorWidth,
                   axi::IspAxiLiteAdapter::kOkay,
                   "read-only identification read");
        expect(rdata.read().to_uint() == bank.configuration().sensor_width,
               "read-only identification register is AXI-readable");
        consume_read_response();
    }

    void test_error_responses()
    {
        simultaneous_write(reg::kIspSensorWidth,
                           640u,
                           0xfu,
                           axi::IspAxiLiteAdapter::kSlvErr,
                           "read-only register write");
        consume_write_response();

        simultaneous_write(reg::kOecfLutIndex,
                           4096u,
                           0xfu,
                           axi::IspAxiLiteAdapter::kSlvErr,
                           "invalid mapped register value");
        consume_write_response();

        begin_read(0x20u,
                   axi::IspAxiLiteAdapter::kDecErr,
                   "reserved register read");
        consume_read_response();

        begin_read(reg::kDpcThreshold + 2u,
                   axi::IspAxiLiteAdapter::kDecErr,
                   "misaligned register read");
        consume_read_response();

        begin_read(reg::kAddressSpaceBytes,
                   axi::IspAxiLiteAdapter::kDecErr,
                   "out-of-range register read");
        consume_read_response();
    }

    void test_synchronous_reset()
    {
        simultaneous_write(reg::kIspSourceAddress,
                           0xcafebabeu,
                           0xfu,
                           axi::IspAxiLiteAdapter::kOkay,
                           "write held before reset");
        begin_read(reg::kIspSourceAddress,
                   axi::IspAxiLiteAdapter::kOkay,
                   "read held before reset");
        expect(bvalid.read() && rvalid.read(),
               "write and read responses are both outstanding before reset");

        reset_n.write(false);
        wait(sc_core::sc_time(2, sc_core::SC_NS));
        expect(bvalid.read() && rvalid.read(),
               "active-low reset does not act asynchronously between clocks");
        expect(tlm_read(reg::kIspSourceAddress,
                        "pre-edge reset visibility") == 0xcafebabeu,
               "shared bank is unchanged before reset clock edge");

        cycle();
        expect(!bvalid.read() && !rvalid.read() &&
                   !awready.read() && !wready.read() && !arready.read(),
               "sampled reset clears outstanding AXI state");
        expect(tlm_read(reg::kIspSourceAddress,
                        "post-edge reset visibility") == 0u,
               "sampled AXI reset resets the shared register bank");

        reset_n.write(true);
        cycle();
        expect(awready.read() && wready.read() && arready.read(),
               "adapter resumes after synchronous reset release");
    }

    void run()
    {
        test_independent_write_channels_and_strobes();
        test_reads_backpressure_and_cross_path_visibility();
        test_error_responses();
        test_synchronous_reset();
        sc_core::sc_stop();
    }
};

}  // namespace

int sc_main(int, char**)
{
    AxiLiteTester tester("tester");
    sc_core::sc_start();
    if (failures == 0) {
        std::cout << "PASS: ISP shared-register-bank AXI4-Lite adapter tests\n";
    }
    return failures == 0 ? 0 : 1;
}
