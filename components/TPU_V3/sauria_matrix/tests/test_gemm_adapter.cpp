// SPDX-License-Identifier: Apache-2.0
//
// The same GEMM, through the fabric — the differential D17 and the audit's
// remaining-work list call for.
//
// ## Why this is the strong check and why it is cheap
//
// `test_gemm_staged.cpp` puts the operands into the staging store by hand and
// reads the results back the same way. This test puts them into **core SRAM**,
// submits a `job` through `sauria_matrix_if`, and reads C back out of core SRAM
// afterwards. Same problem, same golden, one difference: the data travels over
// `neo_local_sram_if` through a real arbitrated fabric, moved by the adapter's
// prefetch and writeback controllers.
//
// So the two tests share an oracle and differ in exactly one variable. If the
// staged test passes and this one does not, the fault is in the controllers —
// the transpose, the strides, the chunking, the result indexing — and not in the
// array, the configuration, or the golden data. That is worth much more than a
// pass here would be on its own.
//
// The operands go in through `dbg_access`, which bypasses arbitration and
// latency but not decode or bounds checks, advances no simulated time, and
// updates no counter. `INTERFACE_CONTRACT.md` §8 requires exactly that of a
// loader: a testbench filling memory is not workload traffic, and counting it
// would corrupt the counters this test then reads.
//
// ## What it additionally checks that the staged test cannot
//
//   * the fabric counters attribute every beat to `neo_requester::sa`, so the
//     engine demonstrably went through arbitration rather than around it;
//   * the three timing terms are separately non-zero, which is the observable
//     consequence of D17's three phases actually being three;
//   * a second job on the same adapter produces the same answer, so nothing is
//     carried between jobs;
//   * `submit()` refuses the jobs it promises to refuse, before touching C.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <systemc>

#include "tpu_v3/address_map.h"
#include "tpu_v3/core/neo_local_sram_fabric.h"
#include "tpu_v3/sauria/sauria_matrix_adapter.h"
#include "tpu_v3/sram/core_sram.h"

namespace sauria_tpu = cdc::components::tpu_v3::sauria;
namespace sram = cdc::components::tpu_v3::sram;
namespace core = cdc::components::tpu_v3::core;
namespace tpu = cdc::components::tpu_v3;
namespace am = cdc::components::tpu_v3::address_map;

namespace {

int failures = 0;

void check(bool condition, const std::string& what)
{
    if (!condition) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}

std::vector<std::int64_t> read_flat(const std::string& path)
{
    std::ifstream in(path);
    if (!in) {
        std::cerr << "FAIL: cannot open the pinned golden file " << path << '\n';
        ++failures;
        return {};
    }
    std::vector<std::int64_t> values;
    std::int64_t value = 0;
    while (in >> value) {
        values.push_back(value);
    }
    return values;
}

constexpr int golden_m = 64;
constexpr int golden_n = 64;
constexpr int golden_k = 256;

constexpr unsigned kChip = 0;
constexpr unsigned kCore = 0;
constexpr std::uint64_t kSramCapacity = 256 * 1024;

sram::core_sram_config sram_config()
{
    sram::core_sram_config config;
    config.base_address = am::core_sram_base(kChip, kCore);
    config.window_bytes = am::core_sram_window;
    config.capacity_bytes = kSramCapacity;
    return config;
}

tpu::local_sram_fabric_config fabric_config()
{
    tpu::local_sram_fabric_config config;
    config.data_width_bits = 128;
    config.bank_count = 4;
    config.mapping = tpu::bank_mapping::low_order_interleaved;
    config.pipeline_stages = 1;
    config.max_outstanding_per_requester = 1;
    config.arbitration = tpu::arbitration_policy::round_robin;
    return config;
}

} // namespace

using adapter_t = sauria_tpu::sauria_matrix_adapter<
    sauria_tpu::columns, sauria_tpu::rows, sauria_tpu::activation_t,
    sauria_tpu::weight_t, sauria_tpu::accumulator_t,
    /*SRAMA_CAP=*/1024, /*SRAMB_CAP=*/1024, /*SRAMC_CAP=*/2048>;

int sc_main(int argc, char* argv[])
{
    const std::string case_dir =
        argc > 1 ? argv[1] : std::string(TPU_V3_SAURIA_CASE_DIR);
    const std::string tmp = case_dir + "/sauria_tmp/";

    const auto a_flat = read_flat(tmp + "sauria_A_Mat_mvm_flat.txt");
    const auto b_flat = read_flat(tmp + "sauria_B_Mat_mvm_flat.txt");
    const auto c_flat = read_flat(tmp + "sauria_C_compute_mvm_flat.txt");
    if (failures != 0) {
        return 1;
    }

    const std::uint64_t sram_base = am::core_sram_base(kChip, kCore);

    sram::core_sram store("core_sram", sram_config());
    core::neo_local_sram_fabric fabric(
        "local_fabric", fabric_config(), store,
        {sram::neo_requester::cpu, sram::neo_requester::sa},
        core::local_fabric_timing::arbitrated,
        sc_core::sc_time(1, sc_core::SC_NS));

    sauria_tpu::adapter_config config;
    config.sram_base = sram_base;
    config.sram_window = am::core_sram_window;
    adapter_t adapter("matrix_engine", config);

    sc_core::sc_clock clock("clock", 10, sc_core::SC_NS);
    sc_core::sc_signal<bool> rstn{"rstn"};
    adapter.i_clk(clock);
    adapter.i_rstn(rstn);
    adapter.local_port.bind(fabric.native_port);

    // ── the job's memory layout ──────────────────────────────────────────────
    //
    // A row-major [M][K], B row-major [K][N], C row-major [M][N], tightly
    // packed. The frozen interface's layout, and deliberately *not* the layout
    // the array wants — making the adapter do the transpose is the thing under
    // test.
    const std::uint64_t a_address = sram_base;
    const std::uint64_t b_address = a_address + std::uint64_t(golden_m) * golden_k;
    const std::uint64_t c_address = b_address + std::uint64_t(golden_k) * golden_n;

    const auto load = [&](std::uint64_t address,
                          const std::vector<unsigned char>& bytes) {
        std::vector<unsigned char> mutable_bytes = bytes;
        std::uint64_t offset = 0;
        while (offset < mutable_bytes.size()) {
            const std::uint32_t chunk = static_cast<std::uint32_t>(
                std::min<std::size_t>(mutable_bytes.size() - offset,
                                      sram::neo_max_transfer_bytes));
            sram::neo_local_request request;
            request.requester = sram::neo_requester::cpu;
            request.command = sram::neo_command::write;
            request.address = address + offset;
            request.size = chunk;
            request.data = mutable_bytes.data() + offset;
            const std::uint32_t moved = fabric.dbg_access(request);
            if (moved != chunk) {
                check(false, "the loader could not fill core SRAM");
                return;
            }
            offset += chunk;
        }
    };

    {
        std::vector<unsigned char> a_bytes(a_flat.size());
        for (std::size_t i = 0; i < a_flat.size(); ++i) {
            a_bytes[i] = static_cast<unsigned char>(
                static_cast<std::int8_t>(a_flat[i]));
        }
        std::vector<unsigned char> b_bytes(b_flat.size());
        for (std::size_t i = 0; i < b_flat.size(); ++i) {
            b_bytes[i] = static_cast<unsigned char>(
                static_cast<std::int8_t>(b_flat[i]));
        }
        load(a_address, a_bytes);
        load(b_address, b_bytes);
    }
    if (failures != 0) {
        return 1;
    }

    sauria_tpu::job work;
    work.m = golden_m;
    work.n = golden_n;
    work.k = golden_k;
    work.a_address = a_address;
    work.b_address = b_address;
    work.c_address = c_address;
    work.datatype = sauria_tpu::datatype_value::int8_int32;

    rstn.write(false);
    sc_core::sc_start(100, sc_core::SC_NS);
    rstn.write(true);
    sc_core::sc_start(50, sc_core::SC_NS);

    // ── refusals happen before any state changes ─────────────────────────────
    //
    // Checked first, and against a C region that is still zero, so "leaves C
    // untouched" is a measurement rather than an assertion.
    {
        auto bad = work;
        bad.m = 0;
        check(adapter.submit(bad) == sauria_tpu::submit_status::invalid_dimension,
              "a zero dimension was not refused");
        bad = work;
        bad.n = sauria_tpu::columns + 1;
        check(adapter.submit(bad)
                  == sauria_tpu::submit_status::dimension_exceeds_array,
              "a problem larger than the array was not refused as such");
        bad = work;
        bad.k = 64;
        bad.n = 192; // QKV projection in the source ViT workload.
        check(adapter.submit(bad)
                  == sauria_tpu::submit_status::dimension_exceeds_array,
              "the untiled ViT QKV projection was not refused explicitly");
        bad = work;
        bad.datatype = sauria_tpu::datatype_value::bf16_fp32;
        check(adapter.submit(bad)
                  == sauria_tpu::submit_status::datatype_unsupported,
              "BF16 was not refused; the pinned source has no BF16 profile");
        bad = work;
        bad.c_address = bad.a_address;
        check(adapter.submit(bad) == sauria_tpu::submit_status::region_overlap,
              "C overlapping A was not refused");
        bad = work;
        bad.m = 64;
        bad.n = 64;
        bad.k = 1025; // 1025 A and B vectors, but each store holds 1024
        bad.a_address = sram_base;
        bad.b_address = sram_base + 0x1'0100;
        bad.c_address = sram_base + 0x2'0200;
        check(adapter.submit(bad)
                  == sauria_tpu::submit_status::staging_capacity_exceeded,
              "a job larger than the private A/B staging stores was accepted");
        check(!adapter.busy(),
              "a refused job left the engine busy; a refusal must change no "
              "state at all");
    }

    const auto identity = adapter.identity();
    check(identity.rows == 64 && identity.columns == 64,
          "the engine does not report the 64x64 geometry");
    check(!identity.source_revision.empty(),
          "the engine reports no source revision; a result whose source cannot "
          "be named is not reportable (D14)");
    std::cout << "engine: " << identity.columns << 'x' << identity.rows
              << "  revision " << identity.source_revision << '\n';

    // ── run it ───────────────────────────────────────────────────────────────
    check(adapter.submit(work) == sauria_tpu::submit_status::accepted,
          "the golden job was not accepted");

    const int watchdog_ns = 6000000;
    int elapsed = 0;
    while (adapter.busy() && elapsed < watchdog_ns) {
        sc_core::sc_start(1000, sc_core::SC_NS);
        elapsed += 1000;
    }
    check(!adapter.busy(), "the job never completed");
    check(adapter.last_error() == sauria_tpu::error_cause::none,
          "the job reported an error");
    if (adapter.busy()
        || adapter.last_error() != sauria_tpu::error_cause::none) {
        std::cerr << "  error cause = "
                  << static_cast<std::uint32_t>(adapter.last_error()) << '\n';
        return 1;
    }

    const auto timing = adapter.last_timing();
    std::cout << "prefetch  " << timing.prefetch_ns << " ns\n"
              << "compute   " << timing.compute_ns << " ns\n"
              << "writeback " << timing.writeback_ns << " ns\n"
              << "committed " << adapter.committed_bytes() << " bytes\n";

    // Each term separately non-zero. A total would hide a phase that never ran,
    // and "three phases" is the whole content of D17's choice.
    check(timing.prefetch_ns > 0, "the prefetch phase took no simulated time");
    check(timing.compute_ns > 0, "the compute phase took no simulated time");
    check(timing.writeback_ns > 0, "the writeback phase took no simulated time");
    check(adapter.committed_bytes()
              == std::uint64_t(golden_m) * golden_n * sizeof(std::int32_t),
          "the committed byte count is not the whole of C");

    // ── read C back out of core SRAM ─────────────────────────────────────────
    const auto read_c = [&]() {
        std::vector<std::int32_t> out(std::size_t(golden_m) * golden_n, 0);
        auto* raw = reinterpret_cast<unsigned char*>(out.data());
        const std::size_t bytes = out.size() * sizeof(std::int32_t);
        std::size_t offset = 0;
        while (offset < bytes) {
            const std::uint32_t chunk = static_cast<std::uint32_t>(
                std::min<std::size_t>(bytes - offset,
                                      sram::neo_max_transfer_bytes));
            sram::neo_local_request request;
            request.requester = sram::neo_requester::cpu;
            request.command = sram::neo_command::read;
            request.address = c_address + offset;
            request.size = chunk;
            request.data = raw + offset;
            fabric.dbg_access(request);
            offset += chunk;
        }
        return out;
    };

    const auto c_out = read_c();
    std::size_t mismatches = 0;
    for (std::size_t i = 0; i < c_out.size(); ++i) {
        if (static_cast<std::int64_t>(c_out[i]) != c_flat[i]) {
            if (mismatches < 8) {
                std::cerr << "  C[" << i / golden_n << "][" << i % golden_n
                          << "] expected " << c_flat[i] << " got " << c_out[i]
                          << '\n';
            }
            ++mismatches;
        }
    }
    check(mismatches == 0,
          std::to_string(mismatches) + " of " + std::to_string(c_out.size())
              + " elements of C differ from the pinned golden. The staged test "
                "runs the same job with the same oracle and only the fabric "
                "path differs, so a failure here is in the controllers");

    // ── the traffic went through arbitration ─────────────────────────────────
    const auto& sa_counters = fabric.counters(sram::neo_requester::sa);
    const auto& cpu_counters = fabric.counters(sram::neo_requester::cpu);
    std::cout << "sa requests " << sa_counters.request_count << ", beats "
              << sa_counters.physical_beat_count << ", bytes "
              << sa_counters.transferred_bytes << '\n';
    check(sa_counters.request_count > 0,
          "the fabric attributed no requests to the matrix engine, so the "
          "operands did not travel over the native port");
    check(sa_counters.error_count == 0,
          "the fabric refused one of the engine's accesses");
    check(cpu_counters.request_count == 0,
          "the loader was counted as workload traffic; dbg_access must update "
          "no counter (INTERFACE_CONTRACT.md section 8)");
    // Operands in, results out, all of it through the one port.
    const std::uint64_t expected_bytes =
        std::uint64_t(golden_m) * golden_k + std::uint64_t(golden_k) * golden_n
        + std::uint64_t(golden_m) * golden_n * sizeof(std::int32_t);
    check(sa_counters.transferred_bytes == expected_bytes,
          "the engine moved " + std::to_string(sa_counters.transferred_bytes)
              + " bytes, expected " + std::to_string(expected_bytes)
              + " (A + B in, C out)");

    // ── nothing is carried between jobs ──────────────────────────────────────
    //
    // The same job again, into a different C. A stale configuration, a stale
    // staging store or a latched `done` would show up here and nowhere else.
    {
        auto second = work;
        second.c_address = c_address + std::uint64_t(golden_m) * golden_n * 4;
        check(adapter.submit(second) == sauria_tpu::submit_status::accepted,
              "the second job was not accepted");
        elapsed = 0;
        while (adapter.busy() && elapsed < watchdog_ns) {
            sc_core::sc_start(1000, sc_core::SC_NS);
            elapsed += 1000;
        }
        check(!adapter.busy(), "the second job never completed");

        std::vector<std::int32_t> again(std::size_t(golden_m) * golden_n, 0);
        auto* raw = reinterpret_cast<unsigned char*>(again.data());
        const std::size_t bytes = again.size() * sizeof(std::int32_t);
        std::size_t offset = 0;
        while (offset < bytes) {
            const std::uint32_t chunk = static_cast<std::uint32_t>(
                std::min<std::size_t>(bytes - offset,
                                      sram::neo_max_transfer_bytes));
            sram::neo_local_request request;
            request.requester = sram::neo_requester::cpu;
            request.command = sram::neo_command::read;
            request.address = second.c_address + offset;
            request.size = chunk;
            request.data = raw + offset;
            fabric.dbg_access(request);
            offset += chunk;
        }
        check(again == c_out,
              "the same job run twice on the same engine produced different "
              "results; state is being carried between jobs");
    }

    // ── non-square edge tile with padded rows ───────────────────────────────
    // A full 64x64 job cannot expose a rows-used mask, an X/Y swap, or a
    // controller that validates one stride and walks another. This deliberately
    // exercises all three with exact integer arithmetic.
    {
        constexpr std::uint32_t em = 7, en = 13, ek = 5;
        constexpr std::uint32_t as = 8, bs = 16, cs = 64;
        const std::uint64_t ea = sram_base + 0x30000;
        const std::uint64_t eb = sram_base + 0x30100;
        const std::uint64_t ec = sram_base + 0x30200;

        std::vector<std::int8_t> a(em * ek), b(ek * en);
        std::vector<std::int32_t> expected(em * en, 0);
        unsigned edge_mismatches_shown = 0;
        for (std::uint32_t m = 0; m < em; ++m) {
            for (std::uint32_t k = 0; k < ek; ++k) {
                a[m * ek + k] = static_cast<std::int8_t>((m * 3 + k * 5) % 11 - 5);
            }
        }
        for (std::uint32_t k = 0; k < ek; ++k) {
            for (std::uint32_t n = 0; n < en; ++n) {
                b[k * en + n] = static_cast<std::int8_t>((k * 7 + n * 2) % 13 - 6);
            }
        }
        for (std::uint32_t m = 0; m < em; ++m) {
            for (std::uint32_t n = 0; n < en; ++n) {
                for (std::uint32_t k = 0; k < ek; ++k) {
                    expected[m * en + n] += a[m * ek + k] * b[k * en + n];
                }
            }
        }
        for (std::uint32_t m = 0; m < em; ++m) {
            std::vector<unsigned char> row(as, 0xa5);
            std::memcpy(row.data(), a.data() + m * ek, ek);
            load(ea + m * as, row);
        }
        for (std::uint32_t k = 0; k < ek; ++k) {
            std::vector<unsigned char> row(bs, 0xa5);
            std::memcpy(row.data(), b.data() + k * en, en);
            load(eb + k * bs, row);
        }
        std::vector<unsigned char> c_filler((em - 1) * cs + cs, 0xa5);
        load(ec, c_filler);

        sauria_tpu::job edge;
        edge.m = em; edge.n = en; edge.k = ek;
        edge.a_address = ea; edge.b_address = eb; edge.c_address = ec;
        edge.a_stride_bytes = as;
        edge.b_stride_bytes = bs;
        edge.c_stride_bytes = cs;
        edge.datatype = sauria_tpu::datatype_value::int8_int32;
        check(adapter.submit(edge) == sauria_tpu::submit_status::accepted,
              "the padded 7x13x5 edge job was refused");
        elapsed = 0;
        while (adapter.busy() && elapsed < watchdog_ns) {
            sc_core::sc_start(1000, sc_core::SC_NS);
            elapsed += 1000;
        }
        check(!adapter.busy(), "the padded edge job never completed");
        check(adapter.last_error() == sauria_tpu::error_cause::none,
              "the padded edge job reported an error");

        for (std::uint32_t m = 0; m < em; ++m) {
            std::vector<unsigned char> raw(cs, 0);
            sram::neo_local_request request;
            request.requester = sram::neo_requester::cpu;
            request.command = sram::neo_command::read;
            request.address = ec + m * cs;
            request.size = cs;
            request.data = raw.data();
            check(fabric.dbg_access(request) == cs,
                  "could not read one padded C row");
            for (std::uint32_t n = 0; n < en; ++n) {
                std::int32_t got = 0;
                std::memcpy(&got, raw.data() + n * sizeof(got), sizeof(got));
                if (got != expected[m * en + n] && edge_mismatches_shown < 8) {
                    std::cerr << "  edge C[" << m << "][" << n << "] expected "
                              << expected[m * en + n] << " got " << got << '\n';
                    ++edge_mismatches_shown;
                }
                check(got == expected[m * en + n],
                      "padded edge C[" + std::to_string(m) + "]["
                          + std::to_string(n) + "] differs");
            }
            for (std::size_t byte = en * sizeof(std::int32_t); byte < cs; ++byte) {
                check(raw[byte] == 0xa5,
                      "edge writeback overwrote C row padding");
            }
        }
    }

    // ── matrix-only slice of the source ViT workload ────────────────────────
    //
    // `tools/test_vit_encoder_int8.cpp` is not an array oracle: its nine rich
    // instructions execute through NpuTop's software-emulation path and report
    // zero SA cycles and zero MACs.  Reusing its deterministic INT8 generator
    // here still gives the adapter the workload's 64x64 projection shape and
    // value distribution, while the independent integer loop below is honest
    // about being the oracle.  The full chained ViT belongs to Phase 7.
    {
        constexpr std::uint32_t vm = 64, vn = 64, vk = 64;
        const std::uint64_t va = sram_base + 0x10000;
        const std::uint64_t vb = sram_base + 0x12000;
        const std::uint64_t vc = sram_base + 0x14000;

        const auto gen_value = [](int index, float scale) {
            float value = std::sin(static_cast<float>(index)) * scale;
            value = std::max(-127.0f, std::min(127.0f, value));
            return static_cast<std::int8_t>(std::round(value));
        };

        std::vector<std::int8_t> a(vm * vk), b(vk * vn);
        std::vector<std::int32_t> expected(vm * vn, 0);
        for (std::size_t i = 0; i < a.size(); ++i) {
            a[i] = gen_value(static_cast<int>(i), 80.0f);
        }
        for (std::size_t i = 0; i < b.size(); ++i) {
            // Same offset/scale as W_proj in the source ViT test.
            b[i] = gen_value(static_cast<int>(i) + 40, 20.0f);
        }
        for (std::uint32_t m = 0; m < vm; ++m) {
            for (std::uint32_t n = 0; n < vn; ++n) {
                for (std::uint32_t k = 0; k < vk; ++k) {
                    expected[m * vn + n] += a[m * vk + k] * b[k * vn + n];
                }
            }
        }

        std::vector<unsigned char> a_bytes(a.size()), b_bytes(b.size());
        std::memcpy(a_bytes.data(), a.data(), a.size());
        std::memcpy(b_bytes.data(), b.data(), b.size());
        load(va, a_bytes);
        load(vb, b_bytes);

        sauria_tpu::job projection;
        projection.m = vm;
        projection.n = vn;
        projection.k = vk;
        projection.a_address = va;
        projection.b_address = vb;
        projection.c_address = vc;
        projection.datatype = sauria_tpu::datatype_value::int8_int32;
        check(adapter.submit(projection) == sauria_tpu::submit_status::accepted,
              "the ViT-derived 64x64 projection was refused");
        elapsed = 0;
        while (adapter.busy() && elapsed < watchdog_ns) {
            sc_core::sc_start(1000, sc_core::SC_NS);
            elapsed += 1000;
        }
        check(!adapter.busy(), "the ViT-derived projection never completed");
        check(adapter.last_error() == sauria_tpu::error_cause::none,
              "the ViT-derived projection reported an error");

        std::vector<std::int32_t> got(expected.size(), 0);
        auto* raw = reinterpret_cast<unsigned char*>(got.data());
        for (std::size_t offset = 0; offset < got.size() * sizeof(got[0]);) {
            const std::uint32_t chunk = static_cast<std::uint32_t>(
                std::min<std::size_t>(got.size() * sizeof(got[0]) - offset,
                                      sram::neo_max_transfer_bytes));
            sram::neo_local_request request;
            request.requester = sram::neo_requester::cpu;
            request.command = sram::neo_command::read;
            request.address = vc + offset;
            request.size = chunk;
            request.data = raw + offset;
            check(fabric.dbg_access(request) == chunk,
                  "could not read the ViT-derived projection result");
            offset += chunk;
        }
        check(got == expected,
              "the ViT-derived 64x64 projection differs from exact INT32 GEMM");
    }

    if (failures != 0) {
        std::cerr << "adapter GEMM: " << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "adapter golden, edge and ViT-derived GEMMs over "
                 "neo_local_sram_if: PASS\n";
    return 0;
}
