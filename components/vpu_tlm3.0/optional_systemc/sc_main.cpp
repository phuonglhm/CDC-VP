#include "vpu_tlm_mmio.hpp"

#include "hevc/yuv420.hpp"

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Options {
    std::string input;
    std::string output = "output.h265";
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t frames = 0;
    std::uint32_t qp = 26;
    std::uint32_t mode = model::vpu_reg::MODE_PCM;
    std::uint32_t fifo_depth = 4;
};

Options parse_options(int argc, char** argv) {
    Options result;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&]() -> const char* {
            if (++i >= argc) throw std::invalid_argument("missing value after " + arg);
            return argv[i];
        };
        if (arg == "-i" || arg == "--input") result.input = next();
        else if (arg == "-o" || arg == "--output") result.output = next();
        else if (arg == "-w" || arg == "--width") result.width = std::stoul(next());
        else if (arg == "-h" || arg == "--height") result.height = std::stoul(next());
        else if (arg == "-n" || arg == "--frames") result.frames = std::stoul(next());
        else if (arg == "--qp") result.qp = std::stoul(next());
        else if (arg == "--fifo-depth") {
            result.fifo_depth = std::stoul(next());
            if (result.fifo_depth == 0 || result.fifo_depth > 255) {
                throw std::invalid_argument("--fifo-depth must be 1..255");
            }
        }
        else if (arg == "--mode") {
            const std::string mode = next();
            if (mode == "pcm") result.mode = model::vpu_reg::MODE_PCM;
            else if (mode == "intra-dc") result.mode = model::vpu_reg::MODE_INTRA_DC;
            else if (mode == "hybrid-dc") result.mode = model::vpu_reg::MODE_HYBRID_DC;
            else if (mode == "intra-dc-tq") result.mode = model::vpu_reg::MODE_INTRA_DC_TQ;
            else if (mode == "intra-full-tq") result.mode = model::vpu_reg::MODE_INTRA_FULL_TQ;
            else if (mode == "intra-full-tq16") result.mode = model::vpu_reg::MODE_INTRA_FULL_TQ16;
            else if (mode == "intra-adaptive-tq") result.mode = model::vpu_reg::MODE_INTRA_ADAPTIVE_TQ;
            else if (mode == "intra-directional-tq") result.mode = model::vpu_reg::MODE_INTRA_DIRECTIONAL_TQ;
            else throw std::invalid_argument(
                "--mode must be pcm, intra-dc, hybrid-dc, intra-dc-tq, "
                "intra-full-tq, intra-full-tq16, intra-adaptive-tq or "
                "intra-directional-tq");
        }
        else throw std::invalid_argument("unknown option: " + arg);
    }
    if (result.input.empty() || !result.width || !result.height) {
        throw std::invalid_argument("-i, -w and -h are required");
    }
    return result;
}

std::vector<std::uint8_t> read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open input YUV");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

class TlmMemory final : public sc_core::sc_module, public model::MemoryInterface {
public:
    tlm_utils::simple_target_socket<TlmMemory> target_socket{"target_socket"};

    TlmMemory(sc_core::sc_module_name name, std::size_t size)
        : sc_module(name), bytes_(size) {
        target_socket.register_b_transport(this, &TlmMemory::b_transport);
    }

    bool read(std::uint64_t address,
              std::span<std::uint8_t> destination) override {
        if (address > bytes_.size() || destination.size() > bytes_.size() - address) {
            return false;
        }
        std::copy_n(bytes_.begin() + static_cast<std::ptrdiff_t>(address),
                    destination.size(), destination.begin());
        return true;
    }

    bool write(std::uint64_t address,
               std::span<const std::uint8_t> source) override {
        if (address > bytes_.size() || source.size() > bytes_.size() - address) {
            return false;
        }
        std::copy(source.begin(), source.end(),
                  bytes_.begin() + static_cast<std::ptrdiff_t>(address));
        return true;
    }

private:
    void b_transport(tlm::tlm_generic_payload& transaction,
                     sc_core::sc_time& delay) {
        auto* data = transaction.get_data_ptr();
        const auto size = transaction.get_data_length();
        const auto address = transaction.get_address();
        if (data == nullptr || transaction.get_byte_enable_ptr() != nullptr ||
            transaction.get_streaming_width() < size ||
            address > bytes_.size() || size > bytes_.size() - address) {
            transaction.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        if (transaction.get_command() == tlm::TLM_READ_COMMAND) {
            std::copy_n(bytes_.begin() + static_cast<std::ptrdiff_t>(address),
                        size, data);
        } else if (transaction.get_command() == tlm::TLM_WRITE_COMMAND) {
            std::copy_n(data, size,
                        bytes_.begin() + static_cast<std::ptrdiff_t>(address));
        } else {
            transaction.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return;
        }
        delay += sc_core::sc_time((size + 15) / 16, sc_core::SC_NS);
        transaction.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    std::vector<std::uint8_t> bytes_;
};

class CpuDriver final : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(CpuDriver);

    tlm_utils::simple_initiator_socket<CpuDriver> mmio_socket{"mmio_socket"};
    sc_core::sc_in<bool> irq{"irq"};

    CpuDriver(sc_core::sc_module_name name, TlmMemory& memory, Options options,
              std::vector<std::uint8_t> raw, std::uint64_t src,
              std::uint64_t dst, std::uint32_t capacity)
        : sc_module(name), memory_(memory), options_(std::move(options)),
          raw_(std::move(raw)), src_(src), dst_(dst), capacity_(capacity) {
        SC_THREAD(run);
    }

private:
    static void store_le32(unsigned char* bytes, std::uint32_t value) {
        bytes[0] = static_cast<unsigned char>(value);
        bytes[1] = static_cast<unsigned char>(value >> 8);
        bytes[2] = static_cast<unsigned char>(value >> 16);
        bytes[3] = static_cast<unsigned char>(value >> 24);
    }

    static std::uint32_t load_le32(const unsigned char* bytes) {
        return static_cast<std::uint32_t>(bytes[0]) |
               (static_cast<std::uint32_t>(bytes[1]) << 8) |
               (static_cast<std::uint32_t>(bytes[2]) << 16) |
               (static_cast<std::uint32_t>(bytes[3]) << 24);
    }

    void transfer(tlm::tlm_command command, std::uint32_t address,
                  unsigned char* bytes) {
        tlm::tlm_generic_payload transaction;
        transaction.set_command(command);
        transaction.set_address(address);
        transaction.set_data_ptr(bytes);
        transaction.set_data_length(4);
        transaction.set_streaming_width(4);
        transaction.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        mmio_socket->b_transport(transaction, delay);
        if (delay != sc_core::SC_ZERO_TIME) sc_core::wait(delay);
        if (transaction.get_response_status() != tlm::TLM_OK_RESPONSE) {
            throw std::runtime_error("MMIO transaction failed");
        }
    }

    void write32(std::uint32_t address, std::uint32_t value) {
        unsigned char bytes[4];
        store_le32(bytes, value);
        transfer(tlm::TLM_WRITE_COMMAND, address, bytes);
    }

    std::uint32_t read32(std::uint32_t address) {
        unsigned char bytes[4]{};
        transfer(tlm::TLM_READ_COMMAND, address, bytes);
        return load_le32(bytes);
    }

    void write64(std::uint32_t lo, std::uint32_t hi, std::uint64_t value) {
        write32(lo, static_cast<std::uint32_t>(value));
        write32(hi, static_cast<std::uint32_t>(value >> 32));
    }

    void run() {
        try {
            if (!memory_.write(src_, raw_)) {
                throw std::runtime_error("cannot stage source DMA buffer");
            }

            using namespace model::vpu_reg;
            write64(SRC_ADDR_LO, SRC_ADDR_HI, src_);
            write64(DST_ADDR_LO, DST_ADDR_HI, dst_);
            write32(DST_CAPACITY, capacity_);
            write32(WIDTH, options_.width);
            write32(HEIGHT, options_.height);
            write32(STRIDE_Y, options_.width);
            write32(FRAME_COUNT, options_.frames);
            write32(QP, options_.qp);
            write32(INPUT_FORMAT, FORMAT_YUV420P8);
            write32(ENCODER_MODE, options_.mode);
            const auto fifo_config = options_.fifo_depth |
                (options_.fifo_depth << 8) | (options_.fifo_depth << 16) |
                (options_.fifo_depth << 24);
            write32(FIFO_CONFIG, fifo_config);
            write32(CONTROL, CONTROL_IRQ_ENABLE | CONTROL_START);

            if (!irq.read()) sc_core::wait(irq.posedge_event());
            const auto status = read32(STATUS);
            if ((status & STATUS_DONE) == 0 || (status & STATUS_ERROR) != 0) {
                throw std::runtime_error("VPU error code " +
                                         std::to_string(read32(ERROR_CODE)));
            }

            const auto output_size = read32(BITSTREAM_BYTES);
            std::vector<std::uint8_t> bitstream(output_size);
            if (!memory_.read(dst_, bitstream)) {
                throw std::runtime_error("cannot read destination DMA buffer");
            }
            std::ofstream output(options_.output, std::ios::binary);
            output.write(reinterpret_cast<const char*>(bitstream.data()),
                         static_cast<std::streamsize>(bitstream.size()));
            if (!output) throw std::runtime_error("cannot write output bitstream");

            const auto cycles =
                (static_cast<std::uint64_t>(read32(CYCLES_HI)) << 32) |
                read32(CYCLES_LO);
            std::cout << "TLM MMIO encode complete: frames=" << read32(FRAMES_DONE)
                      << ", bytes=" << output_size << ", model_cycles=" << cycles
                      << ", sim_time=" << sc_core::sc_time_stamp() << '\n';
            write32(IRQ_STATUS, IRQ_DONE | IRQ_ERROR);
        } catch (const std::exception& error) {
            SC_REPORT_ERROR("CpuDriver", error.what());
        }
        sc_core::sc_stop();
    }

    TlmMemory& memory_;
    Options options_;
    std::vector<std::uint8_t> raw_;
    std::uint64_t src_;
    std::uint64_t dst_;
    std::uint32_t capacity_;
};

} // namespace

int sc_main(int argc, char** argv) {
    try {
        auto options = parse_options(argc, argv);
        auto raw = read_file(options.input);
        const auto frame_bytes =
            hevc::yuv420_frame_bytes(options.width, options.height);
        if (frame_bytes == 0) throw std::runtime_error("invalid frame dimensions");
        if (options.frames == 0) {
            options.frames = static_cast<std::uint32_t>(raw.size() / frame_bytes);
        }
        const auto expected = frame_bytes * options.frames;
        if (options.frames == 0 || raw.size() < expected) {
            throw std::runtime_error("raw input is truncated or contains no full frame");
        }
        raw.resize(expected);

        constexpr std::uint64_t src = 0x00100000;
        const std::uint64_t dst = (src + expected + 0xfffU) & ~0xfffULL;
        const auto capacity64 = expected * 2ULL + 1024ULL * 1024ULL;
        if (capacity64 > std::numeric_limits<std::uint32_t>::max()) {
            throw std::runtime_error("testbench output buffer exceeds 32-bit capacity");
        }
        const auto capacity = static_cast<std::uint32_t>(capacity64);
        const auto memory_size = static_cast<std::size_t>(dst + capacity + 4096);

        TlmMemory memory("memory", memory_size);
        model::systemc_tlm::VpuTlmMmio vpu("vpu");
        CpuDriver cpu("cpu", memory, options, std::move(raw), src, dst, capacity);
        sc_core::sc_signal<bool> irq_signal("irq_signal");

        cpu.mmio_socket.bind(vpu.mmio_socket);
        vpu.dma_socket.bind(memory.target_socket);
        vpu.irq(irq_signal);
        cpu.irq(irq_signal);

        sc_core::sc_start();
        return sc_core::sc_report_handler::get_count(sc_core::SC_ERROR) == 0 ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
