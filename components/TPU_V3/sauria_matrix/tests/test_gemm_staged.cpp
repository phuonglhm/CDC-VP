// SPDX-License-Identifier: Apache-2.0
//
// The engine computes a real GEMM, checked against the pinned golden case.
//
// ## What this proves and what it deliberately leaves out
//
// This test stages the operands into the store *by hand* and reads the results
// back the same way. No fabric, no prefetch controller, no writeback controller.
// That is the point: everything between "the operands are in the staging store"
// and "the results are in the staging store" is the extracted Sauria composition
// plus this repository's configuration translation, and this test says whether
// that part is right. `test_gemm_adapter.cpp` then runs the same job through
// `neo_local_sram_if` and requires the identical answer, so a divergence there
// is in the controllers and nowhere else.
//
// ## The oracle
//
// `demo_gemm_64x64` from the pinned source's `npu_demo_clean/` — the case plan
// §11.4 names. Two of its captured files matter:
//
//   sauria_A_Mat_mvm_flat.txt      A, row-major (M, K) = (64, 256)
//   sauria_B_Mat_mvm_flat.txt      B, row-major (K, N) = (256, 64)
//   sauria_C_compute_mvm_flat.txt  A . B, row-major (M, N)
//
// `C_compute` is used rather than `C_Mat`, and that is a consequence of D17:
// `C_Mat = C_compute + preloads` because the demo runs with `preload_en = 1`,
// and this phase refuses C preload. Checking `C_compute == A . B` exactly is
// what makes it a real oracle — INT8 x INT8 -> INT32 is exact integer
// arithmetic, so there is no tolerance to argue about and no rounding to hide a
// wrong reduction order.
//
// ## The operand layout is transposed, and it is measured rather than assumed
//
// The array's activation address generator walks channel-major: `ACT.CHSTEP` is
// the tile's spatial extent, so activation element `(k, m)` lives at flat index
// `k * M + m` — A transposed. The captured DRAM image confirms it: the bytes at
// the case's A offset equal `A.T` flattened, not `A`. Weights are the other way
// round, `(k, n)` at `k * N + n`, matching B as stored.
//
// The frozen `job` interface says `A[M x K]` row-major, so the transpose is the
// adapter's to do. Here the test does it explicitly, in the open, so that the
// controller's version can be checked against something legible.

#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <systemc>

#include "tpu_v3/sauria/config_loader.h"
#include "tpu_v3/sauria/matrix_composition.h"

namespace sauria_tpu = cdc::components::tpu_v3::sauria;

namespace {

int failures = 0;

void check(bool condition, const std::string& what)
{
    if (!condition) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}

/// Whitespace-separated decimal integers, as the captured files store them.
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

} // namespace

using engine_t = sauria_tpu::matrix_composition<
    sauria_tpu::columns, sauria_tpu::rows, sauria_tpu::activation_t,
    sauria_tpu::weight_t, sauria_tpu::accumulator_t,
    /*SRAMA_CAP=*/1024, /*SRAMB_CAP=*/1024, /*SRAMC_CAP=*/2048>;

/// Drives the composition's host port and start, and nothing else.
///
/// Deliberately independent of `sauria_matrix_adapter`: this test is one side of
/// a differential and must not share the code it is differentiating against.
SC_MODULE(host_driver)
{
    sc_core::sc_in<bool> i_clk{"i_clk"};
    sc_core::sc_out<std::uint32_t> o_addr{"o_addr"};
    sc_core::sc_out<bool> o_wren{"o_wren"};
    sc_core::sc_out<::sauria::host_data_t> o_wdata{"o_wdata"};
    sc_core::sc_out<::sauria::host_mask_t> o_wmask{"o_wmask"};

    std::vector<sauria_tpu::config_write> program;
    bool finished = false;

    SC_CTOR(host_driver) { SC_THREAD(run); sensitive << i_clk.pos(); }

    void run()
    {
        for (const auto& write : program) {
            emit(write);
        }
        // The control register: lane 0 non-zero raises `r_start`, which
        // `ConfigRegs` auto-clears on the next clock, so this is a one-cycle
        // start pulse and not a level that would re-arm the engine.
        sauria_tpu::config_write start{};
        start.address = 0x00;
        start.value = 1;
        start.name = "CON.START";
        emit(start);
        finished = true;
    }

private:
    void emit(const sauria_tpu::config_write& write)
    {
        ::sauria::host_data_t data;
        ::sauria::host_mask_t mask;
        if (write.how == sauria_tpu::config_write::encoding::byte_spread) {
            for (int lane = 0; lane < 4; ++lane) {
                data[lane] =
                    static_cast<double>((write.value >> (lane * 8)) & 0xFFu);
                mask[lane] = true;
            }
        } else {
            data[0] = static_cast<double>(write.value);
            mask[0] = true;
        }

        wait();
        o_addr.write(write.address);
        o_wdata.write(data);
        o_wmask.write(mask);
        o_wren.write(true);
        wait();
        o_wren.write(false);
        o_wmask.write(::sauria::host_mask_t());
        wait();
    }
};

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

    check(a_flat.size() == std::size_t(golden_m) * golden_k,
          "the captured A does not have M*K elements");
    check(b_flat.size() == std::size_t(golden_k) * golden_n,
          "the captured B does not have K*N elements");
    check(c_flat.size() == std::size_t(golden_m) * golden_n,
          "the captured C does not have M*N elements");
    if (failures != 0) {
        return 1;
    }

    // The oracle checks itself first. If `C_compute` is not exactly `A . B` then
    // the case files are not what this test believes they are, and every
    // comparison below would be against the wrong thing.
    {
        bool exact = true;
        for (int m = 0; m < golden_m && exact; ++m) {
            for (int n = 0; n < golden_n && exact; ++n) {
                std::int64_t sum = 0;
                for (int k = 0; k < golden_k; ++k) {
                    sum += a_flat[std::size_t(m) * golden_k + k]
                           * b_flat[std::size_t(k) * golden_n + n];
                }
                exact = sum == c_flat[std::size_t(m) * golden_n + n];
            }
        }
        check(exact,
              "the captured C_compute is not exactly A . B, so it cannot be "
              "used as an oracle");
        if (failures != 0) {
            return 1;
        }
    }

    engine_t engine("engine");
    host_driver driver("driver");

    sc_core::sc_clock clock("clock", 10, sc_core::SC_NS);
    sc_core::sc_signal<bool> rstn{"rstn"}, soft_reset{"soft_reset"};
    sc_core::sc_signal<bool> start{"start"};
    sc_core::sc_signal<bool> done{"done"}, deadlock{"deadlock"};
    sc_core::sc_signal<bool> host_wren{"host_wren"}, host_rden{"host_rden"};
    sc_core::sc_signal<std::uint32_t> host_addr{"host_addr"};
    sc_core::sc_signal<std::uint32_t> mvm_k{"mvm_k"};
    sc_core::sc_signal<std::uint32_t> total_contexts{"total_contexts"};
    sc_core::sc_signal<::sauria::host_data_t> host_wdata{"host_wdata"};
    sc_core::sc_signal<::sauria::host_mask_t> host_wmask{"host_wmask"};
    sc_core::sc_signal<float> threshold{"threshold"};

    engine.i_clk(clock);
    engine.i_rstn(rstn);
    engine.i_soft_reset(soft_reset);
    engine.i_start(start);
    engine.o_done(done);
    engine.o_deadlock(deadlock);
    engine.i_host_addr(host_addr);
    engine.i_host_wren(host_wren);
    engine.i_host_rden(host_rden);
    engine.i_host_wdata(host_wdata);
    engine.i_host_wmask(host_wmask);
    engine.i_threshold(threshold);
    engine.i_mvm_k(mvm_k);
    engine.i_total_contexts(total_contexts);

    driver.i_clk(clock);
    driver.o_addr(host_addr);
    driver.o_wren(host_wren);
    driver.o_wdata(host_wdata);
    driver.o_wmask(host_wmask);

    // ── stage the operands ───────────────────────────────────────────────────
    //
    // Lane 0 only. The configuration sets `NSPLIT` to the full row count, which
    // routes every array row to lane A; filling lane 1 as well would make that
    // routing unobservable, and a wrong `NSPLIT` would then still produce the
    // right answer.
    {
        using act_vector = engine_t::store_t::act_vector;
        using wei_vector = engine_t::store_t::wei_vector;
        constexpr int y = sauria_tpu::rows;
        constexpr int x = sauria_tpu::columns;

        const std::size_t act_elements = std::size_t(golden_k) * golden_m;
        for (std::size_t vi = 0; vi * y < act_elements; ++vi) {
            act_vector vec;
            for (int c = 0; c < y; ++c) {
                const std::size_t e = vi * y + c;
                if (e >= act_elements) {
                    break;
                }
                const std::size_t k = e / golden_m;
                const std::size_t m = e % golden_m;
                vec[c] = static_cast<sauria_tpu::activation_t>(
                    a_flat[m * golden_k + k]);
            }
            engine.store.store_activation(0, vi, vec);
        }

        const std::size_t wei_elements = std::size_t(golden_k) * golden_n;
        for (std::size_t vi = 0; vi * x < wei_elements; ++vi) {
            wei_vector vec;
            for (int c = 0; c < x; ++c) {
                const std::size_t e = vi * x + c;
                if (e >= wei_elements) {
                    break;
                }
                vec[c] = static_cast<sauria_tpu::weight_t>(b_flat[e]);
            }
            engine.store.store_weight(0, vi, vec);
        }
    }

    // ── the job, and the configuration it translates to ──────────────────────
    sauria_tpu::job work;
    work.m = golden_m;
    work.n = golden_n;
    work.k = golden_k;
    work.a_address = 0;
    work.b_address = 0x8000;
    work.c_address = 0x10000;

    driver.program =
        sauria_tpu::build_config_writes(work, sauria_tpu::rows,
                                        sauria_tpu::columns);
    std::cout << "configuration for " << golden_m << 'x' << golden_n << 'x'
              << golden_k << ":\n"
              << sauria_tpu::describe_config_writes(driver.program);

    // `mvm_k` is the activation read count per context, which for a 1x1 kernel
    // is K. `total_contexts` is one because a single-tile GEMM has one context.
    mvm_k.write(golden_k);
    total_contexts.write(1);
    // 0.5 is the source's INT8 sparsity threshold: no non-zero INT8 product has
    // magnitude below 1, so it means "skip exact zeros only".
    threshold.write(0.5f);
    start.write(false);
    soft_reset.write(false);
    host_rden.write(false);

    rstn.write(false);
    sc_core::sc_start(100, sc_core::SC_NS);
    rstn.write(true);

    // Let the driver finish the register sequence, then wait for completion
    // under a bound. A hang here is a real result and must not look like one.
    const int watchdog_ns = 4000000;
    int elapsed = 0;
    while (!done.read() && elapsed < watchdog_ns) {
        sc_core::sc_start(1000, sc_core::SC_NS);
        elapsed += 1000;
        if (deadlock.read()) {
            break;
        }
    }

    check(driver.finished, "the host driver did not finish its register writes");
    check(!deadlock.read(), "the engine reported a feeder deadlock");
    check(done.read(), "the engine never reported done");
    std::cout << "done=" << done.read() << " deadlock=" << deadlock.read()
              << " at " << sc_core::sc_time_stamp() << '\n';

    if (!done.read()) {
        return 1;
    }

    // Let the last writeback settle before reading the store.
    sc_core::sc_start(2000, sc_core::SC_NS);

    // ── compare ──────────────────────────────────────────────────────────────
    //
    // The output vector index is the array column, which carries the output
    // channel; the component within the vector is the array row, which carries
    // the output position. So `C[m][n]` is component `m` of vector `n`. The
    // opposite orientation is checked too, and reported rather than silently
    // accepted: at a square 64x64 both have the same shape, and a test that only
    // tried one would pass a transposed engine.
    std::size_t match_column_major = 0;
    std::size_t match_row_major = 0;
    for (int m = 0; m < golden_m; ++m) {
        for (int n = 0; n < golden_n; ++n) {
            const std::int64_t expected = c_flat[std::size_t(m) * golden_n + n];
            if (static_cast<std::int64_t>(engine.store.load_result(0, n)[m])
                == expected) {
                ++match_column_major;
            }
            if (static_cast<std::int64_t>(engine.store.load_result(0, m)[n])
                == expected) {
                ++match_row_major;
            }
        }
    }

    const std::size_t total = std::size_t(golden_m) * golden_n;
    std::cout << "C[m][n] == result[n][m] : " << match_column_major << '/'
              << total << '\n'
              << "C[m][n] == result[m][n] : " << match_row_major << '/' << total
              << '\n';

    check(match_column_major == total,
          "the engine's result does not equal A . B; " + std::to_string(total - match_column_major)
              + " of " + std::to_string(total) + " elements differ");

    if (match_column_major != total) {
        std::cerr << "first differences (m, n, expected, got):\n";
        int shown = 0;
        for (int m = 0; m < golden_m && shown < 8; ++m) {
            for (int n = 0; n < golden_n && shown < 8; ++n) {
                const std::int64_t expected =
                    c_flat[std::size_t(m) * golden_n + n];
                const std::int64_t got = static_cast<std::int64_t>(
                    engine.store.load_result(0, n)[m]);
                if (got != expected) {
                    std::cerr << "  (" << m << ", " << n << ") " << expected
                              << " vs " << got << '\n';
                    ++shown;
                }
            }
        }
    }

    if (failures != 0) {
        std::cerr << "staged GEMM: " << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "staged GEMM 64x64x256 against demo_gemm_64x64: PASS\n";
    return 0;
}
