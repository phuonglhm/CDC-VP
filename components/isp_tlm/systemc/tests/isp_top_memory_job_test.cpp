#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "pipeline/isp_top.h"
#include "registers/isp_register_map.h"

namespace {

namespace reg = isp_tlm::registers;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

class TestMemory : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<TestMemory> socket{"socket"};
    std::vector<unsigned char> bytes;

    explicit TestMemory(sc_core::sc_module_name name,
                        std::size_t size = 0x10000)
        : sc_core::sc_module(name)
        , bytes(size, 0)
    {
        socket.register_b_transport(this, &TestMemory::b_transport);
    }

private:
    void b_transport(tlm::tlm_generic_payload& transaction,
                     sc_core::sc_time&)
    {
        const std::uint64_t address = transaction.get_address();
        const unsigned int length = transaction.get_data_length();
        unsigned char* const data = transaction.get_data_ptr();
        if (data == nullptr || address > bytes.size() ||
            length > bytes.size() - static_cast<std::size_t>(address)) {
            transaction.set_response_status(
                tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        unsigned char* const storage =
            bytes.data() + static_cast<std::size_t>(address);
        if (transaction.get_command() == tlm::TLM_READ_COMMAND) {
            std::copy(storage, storage + length, data);
        } else if (transaction.get_command() ==
                   tlm::TLM_WRITE_COMMAND) {
            std::copy(data, data + length, storage);
        } else {
            transaction.set_response_status(
                tlm::TLM_COMMAND_ERROR_RESPONSE);
            return;
        }
        transaction.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

class TopTester : public sc_core::sc_module {
    static constexpr unsigned kWidth = 8;
    static constexpr unsigned kHeight = 4;
    static constexpr std::uint32_t kRawAddress = 0x1000;
    static constexpr std::uint32_t kYAddress = 0x2000;
    static constexpr std::uint32_t kUAddress = 0x3000;
    static constexpr std::uint32_t kVAddress = 0x4000;

public:
    sc_core::sc_clock pclk{"pclk", sc_core::sc_time(10, sc_core::SC_NS)};
    sc_core::sc_clock aclk{"aclk", sc_core::sc_time(14, sc_core::SC_NS)};
    sc_core::sc_signal<bool> reset_n{"reset_n"};
    sc_core::sc_signal<bool> axi_reset_n{"axi_reset_n"};

    sc_core::sc_signal<bool> in_href{"in_href"};
    sc_core::sc_signal<bool> in_vsync{"in_vsync"};
    sc_core::sc_signal<sc_dt::sc_uint<12>> in_raw{"in_raw"};
    sc_core::sc_signal<bool> in_href_rgb{"in_href_rgb"};
    sc_core::sc_signal<bool> in_vsync_rgb{"in_vsync_rgb"};
    sc_core::sc_signal<sc_dt::sc_uint<12>> in_r{"in_r"};
    sc_core::sc_signal<sc_dt::sc_uint<12>> in_g{"in_g"};
    sc_core::sc_signal<sc_dt::sc_uint<12>> in_b{"in_b"};

    sc_core::sc_signal<bool> out_gamma_href{"out_gamma_href"};
    sc_core::sc_signal<bool> out_gamma_vsync{"out_gamma_vsync"};
    sc_core::sc_signal<sc_dt::sc_uint<12>> out_gamma_r{"out_gamma_r"};
    sc_core::sc_signal<sc_dt::sc_uint<12>> out_gamma_g{"out_gamma_g"};
    sc_core::sc_signal<sc_dt::sc_uint<12>> out_gamma_b{"out_gamma_b"};
    sc_core::sc_signal<bool> out_href{"out_href"};
    sc_core::sc_signal<bool> out_vsync{"out_vsync"};
    sc_core::sc_signal<std::uint8_t> out_y{"out_y"};
    sc_core::sc_signal<std::uint8_t> out_u{"out_u"};
    sc_core::sc_signal<std::uint8_t> out_v{"out_v"};
    sc_core::sc_signal<bool> irq{"irq"};

    sc_core::sc_signal<sc_dt::sc_uint<32>> awaddr{"awaddr"};
    sc_core::sc_signal<sc_dt::sc_uint<3>> awprot{"awprot"};
    sc_core::sc_signal<bool> awvalid{"awvalid"};
    sc_core::sc_signal<bool> awready{"awready"};
    sc_core::sc_signal<sc_dt::sc_uint<32>> wdata{"wdata"};
    sc_core::sc_signal<sc_dt::sc_uint<4>> wstrb{"wstrb"};
    sc_core::sc_signal<bool> wvalid{"wvalid"};
    sc_core::sc_signal<bool> wready{"wready"};
    sc_core::sc_signal<sc_dt::sc_uint<2>> bresp{"bresp"};
    sc_core::sc_signal<bool> bvalid{"bvalid"};
    sc_core::sc_signal<bool> bready{"bready"};
    sc_core::sc_signal<sc_dt::sc_uint<32>> araddr{"araddr"};
    sc_core::sc_signal<sc_dt::sc_uint<3>> arprot{"arprot"};
    sc_core::sc_signal<bool> arvalid{"arvalid"};
    sc_core::sc_signal<bool> arready{"arready"};
    sc_core::sc_signal<sc_dt::sc_uint<32>> rdata{"rdata"};
    sc_core::sc_signal<sc_dt::sc_uint<2>> rresp{"rresp"};
    sc_core::sc_signal<bool> rvalid{"rvalid"};
    sc_core::sc_signal<bool> rready{"rready"};

    isp_top<12, BayerPattern::RGGB, kWidth, kHeight> dut{"dut"};
    TestMemory memory{"memory"};
    tlm_utils::simple_initiator_socket<TopTester> control{"control"};

    SC_HAS_PROCESS(TopTester);

    explicit TopTester(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
    {
        bind_top();
        control.bind(dut.control_socket);
        dut.memory_socket.bind(memory.socket);
        SC_THREAD(run);
    }

private:
    void bind_top()
    {
        dut.pclk(pclk);
        dut.rst_n(reset_n);
        dut.in_href(in_href);
        dut.in_vsync(in_vsync);
        dut.in_raw(in_raw);
        dut.in_href_rgb(in_href_rgb);
        dut.in_vsync_rgb(in_vsync_rgb);
        dut.in_r(in_r);
        dut.in_g(in_g);
        dut.in_b(in_b);
        dut.out_gamma_href(out_gamma_href);
        dut.out_gamma_vsync(out_gamma_vsync);
        dut.out_gamma_r(out_gamma_r);
        dut.out_gamma_g(out_gamma_g);
        dut.out_gamma_b(out_gamma_b);
        dut.out_href(out_href);
        dut.out_vsync(out_vsync);
        dut.out_y(out_y);
        dut.out_u(out_u);
        dut.out_v(out_v);
        dut.irq(irq);

        dut.aclk(aclk);
        dut.aresetn(axi_reset_n);
        dut.s_axi_awaddr(awaddr);
        dut.s_axi_awprot(awprot);
        dut.s_axi_awvalid(awvalid);
        dut.s_axi_awready(awready);
        dut.s_axi_wdata(wdata);
        dut.s_axi_wstrb(wstrb);
        dut.s_axi_wvalid(wvalid);
        dut.s_axi_wready(wready);
        dut.s_axi_bresp(bresp);
        dut.s_axi_bvalid(bvalid);
        dut.s_axi_bready(bready);
        dut.s_axi_araddr(araddr);
        dut.s_axi_arprot(arprot);
        dut.s_axi_arvalid(arvalid);
        dut.s_axi_arready(arready);
        dut.s_axi_rdata(rdata);
        dut.s_axi_rresp(rresp);
        dut.s_axi_rvalid(rvalid);
        dut.s_axi_rready(rready);
    }

    void cycle()
    {
        wait(pclk.posedge_event());
        wait(sc_core::SC_ZERO_TIME);
    }

    void write_register(std::uint32_t address, std::uint32_t value)
    {
        std::array<unsigned char, 4> data{
            static_cast<unsigned char>(value & 0xffU),
            static_cast<unsigned char>((value >> 8U) & 0xffU),
            static_cast<unsigned char>((value >> 16U) & 0xffU),
            static_cast<unsigned char>((value >> 24U) & 0xffU),
        };
        tlm::tlm_generic_payload transaction;
        transaction.set_command(tlm::TLM_WRITE_COMMAND);
        transaction.set_address(address);
        transaction.set_data_ptr(data.data());
        transaction.set_data_length(data.size());
        transaction.set_streaming_width(data.size());
        transaction.set_byte_enable_ptr(nullptr);
        transaction.set_byte_enable_length(0);
        transaction.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        control->b_transport(transaction, delay);
        expect(transaction.get_response_status() == tlm::TLM_OK_RESPONSE,
               "control write at " + std::to_string(address));
        if (delay != sc_core::SC_ZERO_TIME) {
            wait(delay);
        }
    }

    std::uint32_t read_register(std::uint32_t address)
    {
        std::array<unsigned char, 4> data{0, 0, 0, 0};
        tlm::tlm_generic_payload transaction;
        transaction.set_command(tlm::TLM_READ_COMMAND);
        transaction.set_address(address);
        transaction.set_data_ptr(data.data());
        transaction.set_data_length(data.size());
        transaction.set_streaming_width(data.size());
        transaction.set_byte_enable_ptr(nullptr);
        transaction.set_byte_enable_length(0);
        transaction.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        control->b_transport(transaction, delay);
        expect(transaction.get_response_status() == tlm::TLM_OK_RESPONSE,
               "control read at " + std::to_string(address));
        if (delay != sc_core::SC_ZERO_TIME) {
            wait(delay);
        }
        return static_cast<std::uint32_t>(data[0]) |
               (static_cast<std::uint32_t>(data[1]) << 8U) |
               (static_cast<std::uint32_t>(data[2]) << 16U) |
               (static_cast<std::uint32_t>(data[3]) << 24U);
    }

    void initialize_inputs()
    {
        in_href.write(false);
        in_vsync.write(true);
        in_raw.write(0);
        in_href_rgb.write(false);
        in_vsync_rgb.write(true);
        in_r.write(0);
        in_g.write(0);
        in_b.write(0);

        awaddr.write(0);
        awprot.write(0);
        awvalid.write(false);
        wdata.write(0);
        wstrb.write(0);
        wvalid.write(false);
        bready.write(false);
        araddr.write(0);
        arprot.write(0);
        arvalid.write(false);
        rready.write(false);
    }

    void initialize_raw_memory()
    {
        for (unsigned row = 0; row < kHeight; ++row) {
            for (unsigned column = 0; column < kWidth; ++column) {
                const std::uint16_t value = static_cast<std::uint16_t>(
                    512U + row * 320U + column * 64U);
                const std::size_t offset =
                    kRawAddress + (row * kWidth + column) * 2U;
                memory.bytes[offset] =
                    static_cast<unsigned char>(value & 0xffU);
                memory.bytes[offset + 1U] =
                    static_cast<unsigned char>(value >> 8U);
            }
        }
    }

    void program_memory_job()
    {
        // Deterministic minimal pipeline: demosaic and CSC enabled; optional
        // autonomous/tuning blocks disabled.
        write_register(reg::kIspTopEnable,
                       (1U << 8U) | (1U << 11U));
        write_register(reg::kCscConversionStandard,
                       isp_csc<12>::CSC_BT601);
        write_register(reg::kIspSourceAddress, kRawAddress);
        write_register(reg::kIspSourceStrideBytes, kWidth * 2U);
        write_register(reg::kIspSourceSizeBytes,
                       kWidth * kHeight * 2U);
        write_register(reg::kIspDestinationYAddress, kYAddress);
        write_register(reg::kIspDestinationUAddress, kUAddress);
        write_register(reg::kIspDestinationVAddress, kVAddress);
        write_register(reg::kIspDestinationYStrideBytes, kWidth);
        write_register(reg::kIspDestinationUvStrideBytes, kWidth / 2U);
        write_register(reg::kIspDestinationYSizeBytes,
                       kWidth * kHeight);
        write_register(reg::kIspDestinationUSizeBytes,
                       kWidth * kHeight / 4U);
        write_register(reg::kIspDestinationVSizeBytes,
                       kWidth * kHeight / 4U);
        write_register(reg::kIspJobControl,
                       reg::kJobControlSourceMode |
                           reg::kJobControlStart);
    }

    void program_direct_rgb_stream_job()
    {
        write_register(reg::kIspJobStatus,
                       reg::kJobStatusDone | reg::kJobStatusError);
        write_register(reg::kIspTopEnable, 1U << 11U);
        write_register(reg::kCscConversionStandard,
                       isp_csc<12>::CSC_BT601);
        write_register(reg::kIspDestinationYAddress, 0x5000);
        write_register(reg::kIspDestinationUAddress, 0x6000);
        write_register(reg::kIspDestinationVAddress, 0x7000);
        write_register(reg::kIspDestinationYStrideBytes, kWidth);
        write_register(reg::kIspDestinationUvStrideBytes, kWidth / 2U);
        write_register(reg::kIspDestinationYSizeBytes,
                       kWidth * kHeight);
        write_register(reg::kIspDestinationUSizeBytes,
                       kWidth * kHeight / 4U);
        write_register(reg::kIspDestinationVSizeBytes,
                       kWidth * kHeight / 4U);
        write_register(reg::kIspJobControl,
                       reg::kJobControlDirectRgbInput |
                           reg::kJobControlStart);
    }

    void drive_solid_red_rgb_frame()
    {
        in_href_rgb.write(false);
        in_vsync_rgb.write(true);
        cycle();
        in_vsync_rgb.write(false);
        cycle();
        for (unsigned row = 0; row < kHeight; ++row) {
            in_href_rgb.write(true);
            in_r.write(4095);
            in_g.write(0);
            in_b.write(0);
            for (unsigned column = 0; column < kWidth; ++column) {
                cycle();
            }
            in_href_rgb.write(false);
            cycle();
        }
        in_vsync_rgb.write(true);
        cycle();
    }

    bool wait_for_done(unsigned timeout_cycles)
    {
        for (unsigned timeout = 0; timeout < timeout_cycles; ++timeout) {
            cycle();
            if ((read_register(reg::kIspJobStatus) &
                 reg::kJobStatusDone) != 0) {
                return true;
            }
        }
        return false;
    }

    void run()
    {
        initialize_inputs();
        initialize_raw_memory();
        reset_n.write(false);
        axi_reset_n.write(false);
        cycle();
        cycle();
        reset_n.write(true);
        axi_reset_n.write(true);
        cycle();

        program_memory_job();
        const bool finished = wait_for_done(2000);
        expect(finished, "memory job reaches DONE before timeout");
        expect((read_register(reg::kIspJobStatus) &
                reg::kJobStatusError) == 0,
               "memory job completes without ERROR");

        expect(read_register(reg::kIspLastJobReadBytes) ==
                   kWidth * kHeight * 2U,
               "RAW read byte counter");
        expect(read_register(reg::kIspLastJobWriteBytes) ==
                   kWidth * kHeight * 3U / 2U,
               "I420 write byte counter");
        expect(read_register(reg::kIspLastJobReadTransactions) ==
                   kHeight,
               "one RAW read transaction per row");
        expect(read_register(reg::kIspLastJobWriteTransactions) ==
                   kHeight + kHeight,
               "one I420 write transaction per plane row");

        bool nonzero_y = false;
        for (std::size_t index = 0; index < kWidth * kHeight; ++index) {
            nonzero_y = nonzero_y || memory.bytes[kYAddress + index] != 0;
        }
        expect(nonzero_y, "memory job writes non-trivial Y output");

        IspPerformanceSnapshot metrics = dut.performance();
        expect(metrics.frames_completed == 1,
               "one observed output frame is measured");
        expect(metrics.last_frame_cycles != 0 &&
                   metrics.last_frame_time != sc_core::SC_ZERO_TIME &&
                   metrics.last_frame_fps > 0.0,
               "performance reports observed cycles/time/FPS only");

        program_direct_rgb_stream_job();
        cycle();
        expect((read_register(reg::kIspJobStatus) &
                reg::kJobStatusBusy) != 0,
               "stream START becomes BUSY while waiting for the next frame");
        drive_solid_red_rgb_frame();
        const bool stream_finished = wait_for_done(2000);
        expect(stream_finished,
               "direct-RGB stream job reaches DONE before timeout");
        expect((read_register(reg::kIspJobStatus) &
                reg::kJobStatusError) == 0,
               "direct-RGB stream job completes without ERROR");
        expect(read_register(reg::kIspLastJobReadBytes) == 0,
               "stream job performs no source-memory reads");
        expect(read_register(reg::kIspLastJobWriteBytes) ==
                   kWidth * kHeight * 3U / 2U,
               "stream job writes one I420 frame");

        bool exact_y = true;
        for (std::size_t index = 0; index < kWidth * kHeight; ++index) {
            exact_y = exact_y && memory.bytes[0x5000 + index] == 77;
        }
        bool exact_u = true;
        bool exact_v = true;
        for (std::size_t index = 0;
             index < kWidth * kHeight / 4U; ++index) {
            exact_u = exact_u && memory.bytes[0x6000 + index] == 85;
            exact_v = exact_v && memory.bytes[0x7000 + index] == 255;
        }
        expect(exact_y && exact_u && exact_v,
               "direct red RGB produces exact BT.601 I420 planes");

        metrics = dut.performance();
        expect(metrics.frames_completed == 2,
               "both memory and stream frames are measured");
        expect(read_register(reg::kIspFrameCount) == 2,
               "hardware frame counter counts completed jobs once");

        write_register(reg::kIspJobStatus,
                       reg::kJobStatusDone | reg::kJobStatusError);
        write_register(reg::kIspTopEnable, 0);
        write_register(reg::kIspJobControl,
                       reg::kJobControlDirectRgbInput |
                           reg::kJobControlStart);
        expect(wait_for_done(20),
               "a stream job without CSC terminates instead of hanging");
        expect((read_register(reg::kIspJobStatus) &
                reg::kJobStatusError) != 0,
               "a stream job without CSC reports ERROR");
        expect(read_register(reg::kIspJobErrorCode) ==
                   reg::kJobErrorUnsupportedConfiguration,
               "a stream job without CSC reports unsupported configuration");
        metrics = dut.performance();
        expect(metrics.frames_completed == 2,
               "rejected configuration does not fabricate an output frame");
        expect(read_register(reg::kIspFrameCount) == 3,
               "frame counter includes terminal job attempts");

        sc_core::sc_stop();
    }
};

}  // namespace

int sc_main(int, char**)
{
    TopTester tester{"tester"};
    sc_core::sc_start();
    if (failures == 0) {
        std::cout
            << "PASS: register-driven ISP top memory and stream jobs\n";
    }
    return failures == 0 ? 0 : 1;
}
