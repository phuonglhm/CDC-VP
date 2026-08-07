#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "pipeline/isp_top.h"
#include "registers/isp_register_map.h"

namespace {

namespace reg = isp_tlm::registers;

constexpr unsigned kWidth = 2592;
constexpr unsigned kHeight = 1536;
constexpr std::size_t kRawSize =
    static_cast<std::size_t>(kWidth) * kHeight * 2U;
constexpr std::size_t kYSize =
    static_cast<std::size_t>(kWidth) * kHeight;
constexpr std::size_t kChromaSize = kYSize / 4U;
constexpr std::uint64_t kRawAddress = 0x10000000ULL;
constexpr std::uint64_t kYAddress = 0x20000000ULL;
constexpr std::uint64_t kUAddress = 0x21000000ULL;
constexpr std::uint64_t kVAddress = 0x22000000ULL;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

std::uint64_t fnv1a64(const std::vector<unsigned char>& bytes)
{
    std::uint64_t hash = 14695981039346656037ULL;
    for (const unsigned char byte : bytes) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    return hash;
}

class FixtureMemory : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<FixtureMemory> socket{"socket"};
    std::vector<unsigned char> raw;
    std::vector<unsigned char> y;
    std::vector<unsigned char> u;
    std::vector<unsigned char> v;

    FixtureMemory(sc_core::sc_module_name name, const std::string& raw_path)
        : sc_core::sc_module(name)
        , y(kYSize, 0)
        , u(kChromaSize, 0)
        , v(kChromaSize, 0)
    {
        socket.register_b_transport(this, &FixtureMemory::b_transport);
        std::ifstream input(raw_path, std::ios::binary | std::ios::ate);
        if (!input) {
            load_error_ = "cannot open RAW fixture: " + raw_path;
            return;
        }
        const std::streamoff file_size = input.tellg();
        if (file_size < 0 ||
            static_cast<std::uint64_t>(file_size) != kRawSize) {
            load_error_ =
                "RAW fixture must be exactly 7,962,624 bytes";
            return;
        }
        raw.resize(kRawSize);
        input.seekg(0, std::ios::beg);
        input.read(reinterpret_cast<char*>(raw.data()),
                   static_cast<std::streamsize>(raw.size()));
        if (!input) {
            raw.clear();
            load_error_ = "failed while reading RAW fixture";
        }
    }

    bool loaded() const { return load_error_.empty(); }
    const std::string& load_error() const { return load_error_; }

private:
    std::string load_error_;

    static bool map_range(std::uint64_t address,
                          unsigned int length,
                          std::uint64_t base,
                          std::size_t size,
                          std::size_t& offset)
    {
        if (address < base) {
            return false;
        }
        const std::uint64_t relative = address - base;
        if (relative > size ||
            length > size - static_cast<std::size_t>(relative)) {
            return false;
        }
        offset = static_cast<std::size_t>(relative);
        return true;
    }

    void b_transport(tlm::tlm_generic_payload& transaction,
                     sc_core::sc_time&)
    {
        unsigned char* const data = transaction.get_data_ptr();
        const unsigned int length = transaction.get_data_length();
        if (data == nullptr) {
            transaction.set_response_status(
                tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }

        std::vector<unsigned char>* region = nullptr;
        std::size_t offset = 0;
        const std::uint64_t address = transaction.get_address();
        if (map_range(address, length, kRawAddress, raw.size(), offset)) {
            region = &raw;
        } else if (map_range(address, length, kYAddress, y.size(), offset)) {
            region = &y;
        } else if (map_range(address, length, kUAddress, u.size(), offset)) {
            region = &u;
        } else if (map_range(address, length, kVAddress, v.size(), offset)) {
            region = &v;
        } else {
            transaction.set_response_status(
                tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }

        if (transaction.get_command() == tlm::TLM_READ_COMMAND) {
            std::copy(region->data() + offset,
                      region->data() + offset + length,
                      data);
        } else if (transaction.get_command() ==
                   tlm::TLM_WRITE_COMMAND) {
            std::copy(data, data + length, region->data() + offset);
        } else {
            transaction.set_response_status(
                tlm::TLM_COMMAND_ERROR_RESPONSE);
            return;
        }
        transaction.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

class ColorCheckerTester : public sc_core::sc_module {
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
    FixtureMemory memory;
    tlm_utils::simple_initiator_socket<ColorCheckerTester> control{
        "control"};

    SC_HAS_PROCESS(ColorCheckerTester);

    ColorCheckerTester(sc_core::sc_module_name name,
                       const std::string& raw_path)
        : sc_core::sc_module(name)
        , memory("memory", raw_path)
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
            static_cast<unsigned char>(value),
            static_cast<unsigned char>(value >> 8U),
            static_cast<unsigned char>(value >> 16U),
            static_cast<unsigned char>(value >> 24U),
        };
        tlm::tlm_generic_payload transaction;
        transaction.set_command(tlm::TLM_WRITE_COMMAND);
        transaction.set_address(address);
        transaction.set_data_ptr(data.data());
        transaction.set_data_length(data.size());
        transaction.set_streaming_width(data.size());
        transaction.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        control->b_transport(transaction, delay);
        expect(transaction.get_response_status() == tlm::TLM_OK_RESPONSE,
               "control write " + std::to_string(address));
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
        transaction.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        control->b_transport(transaction, delay);
        expect(transaction.get_response_status() == tlm::TLM_OK_RESPONSE,
               "control read " + std::to_string(address));
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

    void program_job()
    {
        // Approved deterministic minimal profile.
        write_register(reg::kIspTopEnable,
                       (1U << 8U) | (1U << 11U));
        write_register(reg::kCscConversionStandard,
                       isp_csc<12>::CSC_BT601);
        write_register(reg::kIspSourceAddress,
                       static_cast<std::uint32_t>(kRawAddress));
        write_register(reg::kIspSourceStrideBytes, kWidth * 2U);
        write_register(reg::kIspSourceSizeBytes,
                       static_cast<std::uint32_t>(kRawSize));
        write_register(reg::kIspDestinationYAddress,
                       static_cast<std::uint32_t>(kYAddress));
        write_register(reg::kIspDestinationUAddress,
                       static_cast<std::uint32_t>(kUAddress));
        write_register(reg::kIspDestinationVAddress,
                       static_cast<std::uint32_t>(kVAddress));
        write_register(reg::kIspDestinationYStrideBytes, kWidth);
        write_register(reg::kIspDestinationUvStrideBytes, kWidth / 2U);
        write_register(reg::kIspDestinationYSizeBytes,
                       static_cast<std::uint32_t>(kYSize));
        write_register(reg::kIspDestinationUSizeBytes,
                       static_cast<std::uint32_t>(kChromaSize));
        write_register(reg::kIspDestinationVSizeBytes,
                       static_cast<std::uint32_t>(kChromaSize));
        write_register(reg::kIspJobControl,
                       reg::kJobControlSourceMode |
                           reg::kJobControlStart);
    }

    static bool plane_is_nontrivial(
        const std::vector<unsigned char>& plane)
    {
        if (plane.empty()) {
            return false;
        }
        const auto range =
            std::minmax_element(plane.begin(), plane.end());
        return *range.first != *range.second;
    }

    void run()
    {
        initialize_inputs();
        if (!memory.loaded()) {
            expect(false, memory.load_error());
            sc_core::sc_stop();
            return;
        }

        reset_n.write(false);
        axi_reset_n.write(false);
        cycle();
        cycle();
        reset_n.write(true);
        axi_reset_n.write(true);
        cycle();
        program_job();

        const std::uint64_t timeout_cycles =
            static_cast<std::uint64_t>(kWidth) * kHeight +
            kHeight + 100000U;
        bool done = false;
        for (std::uint64_t elapsed = 0; elapsed < timeout_cycles;
             ++elapsed) {
            cycle();
            if ((elapsed & 0xfffU) == 0U) {
                const std::uint32_t status =
                    read_register(reg::kIspJobStatus);
                if ((status & reg::kJobStatusDone) != 0) {
                    done = true;
                    break;
                }
            }
        }
        if (!done) {
            done = (read_register(reg::kIspJobStatus) &
                    reg::kJobStatusDone) != 0;
        }
        expect(done, "ColorChecker frame reaches JOB_STATUS.DONE");
        const std::uint32_t status =
            read_register(reg::kIspJobStatus);
        expect((status & reg::kJobStatusError) == 0,
               "ColorChecker frame has no model error");
        expect(read_register(reg::kIspLastJobReadBytes) == kRawSize,
               "all RAW12 fixture bytes are read");
        expect(read_register(reg::kIspLastJobWriteBytes) ==
                   kYSize + 2U * kChromaSize,
               "all I420 plane bytes are written");
        expect(read_register(reg::kIspLastJobReadTransactions) ==
                   kHeight,
               "one RAW transaction is issued per source row");
        expect(read_register(reg::kIspLastJobWriteTransactions) ==
                   kHeight + kHeight,
               "one destination transaction is issued per I420 row");
        expect(plane_is_nontrivial(memory.y),
               "ColorChecker Y plane is non-trivial");
        expect(plane_is_nontrivial(memory.u),
               "ColorChecker U plane is non-trivial");
        expect(plane_is_nontrivial(memory.v),
               "ColorChecker V plane is non-trivial");

        const IspPerformanceSnapshot metrics = dut.performance();
        expect(metrics.frames_completed == 1,
               "one complete ColorChecker output frame is observed");
        expect(metrics.last_frame_cycles != 0 &&
                   metrics.last_frame_time != sc_core::SC_ZERO_TIME &&
                   metrics.last_frame_fps > 0.0,
               "ColorChecker reports measured pclk cycles/time/FPS");

        std::cout << "ColorChecker output FNV-1a: Y=0x" << std::hex
                  << fnv1a64(memory.y) << " U=0x" << fnv1a64(memory.u)
                  << " V=0x" << fnv1a64(memory.v) << std::dec << '\n';
        std::cout << "Observed frame: " << metrics.last_frame_cycles
                  << " pclk cycles, " << metrics.last_frame_time
                  << ", " << metrics.last_frame_fps << " FPS\n";
        sc_core::sc_stop();
    }
};

}  // namespace

int sc_main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "usage: " << argv[0]
                  << " /path/to/ColorChecker_2592x1536_12bits_RGGB.raw\n";
        return 2;
    }
    ColorCheckerTester tester{"tester", argv[1]};
    sc_core::sc_start();
    if (failures == 0) {
        std::cout << "PASS: full ColorChecker RAW12 model validation\n";
    }
    return failures == 0 ? 0 : 1;
}
