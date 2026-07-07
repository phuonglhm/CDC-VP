#include <systemc>

#include <cstdint>
#include <iostream>
#include <vector>

#include "mem.h"
#include "mem_instance.h"
#include "mem_types.h"
#include "xk265_mem_map.h"

static int g_failures = 0;

#define CHECK(expr)                                                       \
    do {                                                                  \
        if (!(expr)) {                                                     \
            ++g_failures;                                                  \
            std::cerr << "[FAIL] " << #expr << std::endl;                 \
        } else {                                                           \
            std::cout << "[PASS] " << #expr << std::endl;                 \
        }                                                                 \
    } while (0)

static void test_basic_read_write()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] MEM basic read/write\n";

    mem_config config;
    config.name = "ram_sp_256x32";
    config.depth = 256;
    config.width_bits = 32;
    config.port_kind = mem_port_kind::single_port;

    mem_instance ram(config);

    CHECK(ram.valid());
    CHECK(ram.depth() == 256);
    CHECK(ram.width_bits() == 32);
    CHECK(ram.width_bytes() == 4);

    std::vector<std::uint8_t> write_data = {0x11, 0x22, 0x33, 0x44};
    std::vector<std::uint8_t> read_data;

    CHECK(ram.write(3, write_data) == mem_status::ok);
    CHECK(ram.read(3, read_data) == mem_status::ok);
    CHECK(read_data == write_data);
}

static void test_unwritten_read_zero()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] MEM unwritten read returns zero\n";

    mem_config config;
    config.name = "ram_zero";
    config.depth = 16;
    config.width_bits = 32;

    mem_instance ram(config);

    std::vector<std::uint8_t> read_data;

    CHECK(ram.read(0, read_data) == mem_status::ok);
    CHECK(read_data.size() == 4);
    CHECK(read_data[0] == 0);
    CHECK(read_data[1] == 0);
    CHECK(read_data[2] == 0);
    CHECK(read_data[3] == 0);
}

static void test_out_of_bound()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] MEM out-of-bound access\n";

    mem_config config;
    config.name = "small_ram";
    config.depth = 4;
    config.width_bits = 32;

    mem_instance ram(config);

    std::vector<std::uint8_t> data = {1, 2, 3, 4};
    std::vector<std::uint8_t> out;

    CHECK(ram.write(4, data) == mem_status::out_of_range);
    CHECK(ram.read(4, out) == mem_status::out_of_range);
}

static void test_width_mismatch()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] MEM width mismatch\n";

    mem_config config;
    config.name = "ram32";
    config.depth = 8;
    config.width_bits = 32;

    mem_instance ram(config);

    std::vector<std::uint8_t> too_short = {1, 2};
    std::vector<std::uint8_t> too_long = {1, 2, 3, 4, 5};

    CHECK(ram.write(0, too_short) == mem_status::width_mismatch);
    CHECK(ram.write(0, too_long) == mem_status::width_mismatch);
}

static void test_non_byte_aligned_width()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] MEM non-byte-aligned width\n";

    mem_config config;
    config.name = "ime_mv_ram_sp_64x13";
    config.depth = 64;
    config.width_bits = 13;

    mem_instance ram(config);

    CHECK(ram.width_bytes() == 2);

    std::vector<std::uint8_t> write_data = {0xff, 0xff};
    std::vector<std::uint8_t> read_data;

    CHECK(ram.write(0, write_data) == mem_status::ok);
    CHECK(ram.read(0, read_data) == mem_status::ok);

    CHECK(read_data.size() == 2);
    CHECK(read_data[0] == 0xff);

    // 13-bit RAM uses 5 bits in the last byte.
    CHECK(read_data[1] == 0x1f);
}

static void test_byte_enable_write()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] MEM byte-enable write\n";

    mem_config config;
    config.name = "ram_sp_be_128x64";
    config.depth = 128;
    config.width_bits = 64;
    config.byte_enable = true;

    mem_instance ram(config);

    std::vector<std::uint8_t> all_ff = {
        0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff
    };

    std::vector<std::uint8_t> patch = {
        0x10, 0x20, 0x30, 0x40,
        0x50, 0x60, 0x70, 0x80
    };

    std::vector<bool> be = {
        false, true, false, true,
        false, true, false, true
    };

    std::vector<std::uint8_t> read_data;

    CHECK(ram.write(5, all_ff) == mem_status::ok);
    CHECK(ram.write_be(5, patch, be) == mem_status::ok);
    CHECK(ram.read(5, read_data) == mem_status::ok);

    CHECK(read_data[0] == 0xff);
    CHECK(read_data[1] == 0x20);
    CHECK(read_data[2] == 0xff);
    CHECK(read_data[3] == 0x40);
    CHECK(read_data[4] == 0xff);
    CHECK(read_data[5] == 0x60);
    CHECK(read_data[6] == 0xff);
    CHECK(read_data[7] == 0x80);
}

static void test_byte_enable_unsupported()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] MEM byte-enable unsupported\n";

    mem_config config;
    config.name = "ram_no_be";
    config.depth = 16;
    config.width_bits = 32;
    config.byte_enable = false;

    mem_instance ram(config);

    std::vector<std::uint8_t> data = {1, 2, 3, 4};
    std::vector<bool> be = {true, true, true, true};

    CHECK(ram.write_be(0, data, be) == mem_status::byte_enable_unsupported);
}

static void test_u64_helpers()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] MEM u64 helpers\n";

    mem_config config;
    config.name = "ram64";
    config.depth = 16;
    config.width_bits = 64;

    mem_instance ram(config);

    std::uint64_t value = 0;

    CHECK(ram.write_u64(1, 0x1122334455667788ull) == mem_status::ok);
    CHECK(ram.read_u64(1, value) == mem_status::ok);
    CHECK(value == 0x1122334455667788ull);
}

static void test_mem_container()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] MEM subsystem container\n";

    mem memory;

    mem_config config;
    config.name = "prei_md_ram_sp_85x6";
    config.depth = 85;
    config.width_bits = 6;

    CHECK(memory.add_instance(config));
    CHECK(memory.has("prei_md_ram_sp_85x6"));
    CHECK(!memory.has("missing_ram"));

    std::vector<std::uint8_t> data = {0x3f};
    std::vector<std::uint8_t> out;

    CHECK(memory.write("prei_md_ram_sp_85x6", 10, data) == mem_status::ok);
    CHECK(memory.read("prei_md_ram_sp_85x6", 10, out) == mem_status::ok);
    CHECK(out == data);

    CHECK(memory.read("missing_ram", 0, out) == mem_status::instance_not_found);
}

static void test_xk265_default_map()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] MEM xk265 default memory map\n";

    mem memory = make_xk265_mem();

    CHECK(memory.has("prei_md_ram_sp_85x6"));
    CHECK(memory.has("posi_md_ram_sp_64x6"));
    CHECK(memory.has("ime_mv_ram_sp_64x13"));
    CHECK(memory.has("fme_mv_ram_dp_64x20"));
    CHECK(memory.has("fetch_rf_1p_128x512"));
    CHECK(memory.has("ram_sp_be_192x512"));

    const mem_instance* ime_mv = memory.get("ime_mv_ram_sp_64x13");
    CHECK(ime_mv != nullptr);
    CHECK(ime_mv->depth() == 64);
    CHECK(ime_mv->width_bits() == 13);

    const mem_instance* fme_mv = memory.get("fme_mv_ram_dp_64x20");
    CHECK(fme_mv != nullptr);
    CHECK(fme_mv->depth() == 64);
    CHECK(fme_mv->width_bits() == 20);
}

static void test_reset_all()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] MEM reset all\n";

    mem memory = make_xk265_mem();

    std::vector<std::uint8_t> data = {0xaa, 0xbb};
    std::vector<std::uint8_t> out;

    CHECK(memory.write("ime_mv_ram_sp_64x13", 0, data) == mem_status::ok);
    CHECK(memory.read("ime_mv_ram_sp_64x13", 0, out) == mem_status::ok);
    CHECK(out[0] == 0xaa);

    CHECK(memory.reset_all() == mem_status::ok);
    CHECK(memory.read("ime_mv_ram_sp_64x13", 0, out) == mem_status::ok);
    CHECK(out[0] == 0x00);
    CHECK(out[1] == 0x00);
}

int sc_main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    std::cout << "========================================\n";
    std::cout << " MEM Full Functional Unit Test\n";
    std::cout << "========================================\n";

    test_basic_read_write();
    test_unwritten_read_zero();
    test_out_of_bound();
    test_width_mismatch();
    test_non_byte_aligned_width();
    test_byte_enable_write();
    test_byte_enable_unsupported();
    test_u64_helpers();
    test_mem_container();
    test_xk265_default_map();
    test_reset_all();

    if (g_failures == 0) {
        std::cout << "\nMEM full functional test PASSED\n";
    } else {
        std::cout << "\nMEM full functional test FAILED, failures = "
                  << g_failures << "\n";
    }

    return g_failures == 0 ? 0 : 1;
}
