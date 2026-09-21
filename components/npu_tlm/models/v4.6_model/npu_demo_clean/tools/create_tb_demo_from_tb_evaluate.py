#!/usr/bin/env python3
"""
Create a case-driven demo testbench from the current tb_evaluate.cpp.

tb_demo.cpp keeps the proven NPU drive/verify logic of tb_evaluate.cpp, but:
  - reads all case data from a runtime demo-case folder via NPU_DEMO_* env vars,
  - prints a clear TEST TITLE (NPU_DEMO_TITLE / NPU_DEMO_DESC),
  - prints a PERFORMANCE block (execution cycles + throughput/utilization basis).

Usage from the model root:
  python3 npu_demo_clean/tools/create_tb_demo_from_tb_evaluate.py
Generated: tb_demo.cpp
"""
from pathlib import Path
import re

ROOT = Path.cwd()
SRC = ROOT / "tb_evaluate.cpp"
DST = ROOT / "tb_demo.cpp"

if not SRC.exists():
    raise SystemExit(f"[ERROR] Cannot find {SRC}. Run from the model root.")

s = SRC.read_text()

helper = r'''

// ================================================================
// TB_DEMO path helpers + instrumentation
// ================================================================
// All demo input files are selected at runtime through environment vars:
//   NPU_DEMO_CASE_DIR / NPU_DEMO_INITIAL_DRAM / NPU_DEMO_GOLD_DRAM /
//   NPU_DEMO_GOLDEN_STIMULI / NPU_DEMO_SAURIA_DIR
// Test identity is provided via NPU_DEMO_TITLE / NPU_DEMO_DESC.
static std::string demo_env_or(const char *name, const std::string &fallback)
{
    const char *v = std::getenv(name);
    if (v && std::string(v).size() > 0)
        return std::string(v);
    return fallback;
}

static std::string demo_join(const std::string &a, const std::string &b)
{
    if (a.empty())
        return b;
    if (a.back() == '/')
        return a + b;
    return a + "/" + b;
}

static std::string demo_case_dir()
{
    return demo_env_or("NPU_DEMO_CASE_DIR", ".");
}

static std::string demo_initial_dram_path()
{
    return demo_env_or("NPU_DEMO_INITIAL_DRAM", demo_join(demo_join(demo_case_dir(), "stimuli"), "initial_dram.txt"));
}

static std::string demo_gold_dram_path()
{
    return demo_env_or("NPU_DEMO_GOLD_DRAM", demo_join(demo_join(demo_case_dir(), "stimuli"), "gold_dram.txt"));
}

static std::string demo_golden_stimuli_path()
{
    return demo_env_or("NPU_DEMO_GOLDEN_STIMULI", demo_join(demo_join(demo_case_dir(), "stimuli"), "GoldenStimuli.txt"));
}

static std::string demo_sauria_dir()
{
    return demo_env_or("NPU_DEMO_SAURIA_DIR", demo_join(demo_case_dir(), "sauria_tmp"));
}

static std::string demo_sauria_path(const std::string &fname)
{
    return demo_join(demo_sauria_dir(), fname);
}

static void demo_print_runtime_paths()
{
    std::cout << "\n=========================================\n";
    std::cout << "TB_DEMO RUNTIME PATHS\n";
    std::cout << "=========================================\n";
    std::cout << "case_dir       : " << demo_case_dir() << "\n";
    std::cout << "initial_dram   : " << demo_initial_dram_path() << "\n";
    std::cout << "gold_dram      : " << demo_gold_dram_path() << "\n";
    std::cout << "GoldenStimuli  : " << demo_golden_stimuli_path() << "\n";
    std::cout << "sauria_tmp_dir : " << demo_sauria_dir() << "\n";
    std::cout << "=========================================\n\n";
}

// ---- test title + cycle/throughput/utilization instrumentation ----
static long g_demo_exec_cycles = -1;   // SystemC cycles from start pulse to i_done_std
static long g_demo_mvm_k       = -1;   // contraction depth K (MACs per output element)

static void demo_print_test_title()
{
    const char *title = std::getenv("NPU_DEMO_TITLE");
    const char *desc  = std::getenv("NPU_DEMO_DESC");
    std::cout << "\n##########################################################################\n";
    std::cout << "# TEST : " << ((title && title[0]) ? title : demo_case_dir()) << "\n";
    if (desc && desc[0])
        std::cout << "# WHAT : " << desc << "\n";
    std::cout << "##########################################################################\n";
}

static void demo_print_perf(long exec_cycles, long mvm_k,
                            unsigned long out_elems, int arr_x, int arr_y)
{
    long peak = (long)arr_x * (long)arr_y;
    std::cout << "\n==================== PERFORMANCE (cycle basis) =======================\n";
    std::cout << "  Execution cycles (start -> done) : " << exec_cycles << "\n";
    std::cout << "  Output elements produced         : " << out_elems << "\n";
    std::cout << "  PE array (X x Y)                 : " << arr_x << " x " << arr_y
              << "   (peak " << peak << " MAC/cycle)\n";
    if (mvm_k > 0)
    {
        double macs = (double)out_elems * (double)mvm_k;
        std::cout << "  Contraction depth K (MAC/elem)   : " << mvm_k << "\n";
        std::cout << "  Total MACs                       : " << (long long)macs << "\n";
        if (exec_cycles > 0)
        {
            double mpc = macs / (double)exec_cycles;
            std::cout << "  Throughput (MAC / cycle)         : " << mpc << "\n";
            std::cout << "  Array utilization                : "
                      << (peak > 0 ? 100.0 * mpc / (double)peak : 0.0)
                      << " %   (MAC-per-cycle / peak)\n";
        }
    }
    if (exec_cycles > 0)
        std::cout << "  Output throughput (elem / cycle) : "
                  << ((double)out_elems / (double)exec_cycles) << "\n";
    std::cout << "  NOTE: cycles = clock ticks from start pulse to i_done_std (compute\n";
    std::cout << "        window). Baseline for throughput/utilization; refine later by\n";
    std::cout << "        modelling DMA / config / preload overlap.\n";
    std::cout << "======================================================================\n";
}
'''

# Insert helper after the include block.
include_matches = list(re.finditer(r'^#include\s+[^\n]+$', s, flags=re.MULTILINE))
if not include_matches:
    raise SystemExit("[ERROR] Cannot find include block in tb_evaluate.cpp")
insert_pos = include_matches[-1].end()
s = s[:insert_pos] + helper + s[insert_pos:]

# Replace hard-coded input paths with runtime functions.
replacements = {
    '"stimuli/initial_dram.txt"': 'demo_initial_dram_path()',
    '"stimuli/gold_dram.txt"': 'demo_gold_dram_path()',
    '"stimuli/GoldenStimuli.txt"': 'demo_golden_stimuli_path()',
    '"/tmp/sauria_A_Mat_mvm_shape.txt"': 'demo_sauria_path("sauria_A_Mat_mvm_shape.txt")',
    '"/tmp/sauria_A_Mat_mvm_flat.txt"': 'demo_sauria_path("sauria_A_Mat_mvm_flat.txt")',
    '"/tmp/sauria_B_Mat_mvm_shape.txt"': 'demo_sauria_path("sauria_B_Mat_mvm_shape.txt")',
    '"/tmp/sauria_B_Mat_mvm_flat.txt"': 'demo_sauria_path("sauria_B_Mat_mvm_flat.txt")',
    '"/tmp/sauria_C_compute_mvm_shape.txt"': 'demo_sauria_path("sauria_C_compute_mvm_shape.txt")',
    '"/tmp/sauria_C_compute_mvm_flat.txt"': 'demo_sauria_path("sauria_C_compute_mvm_flat.txt")',
    '"/tmp/sauria_C_Mat_mvm_shape.txt"': 'demo_sauria_path("sauria_C_Mat_mvm_shape.txt")',
    '"/tmp/sauria_C_Mat_mvm_flat.txt"': 'demo_sauria_path("sauria_C_Mat_mvm_flat.txt")',
    '"/tmp/sauria_preloads_mvm_shape.txt"': 'demo_sauria_path("sauria_preloads_mvm_shape.txt")',
    '"/tmp/sauria_preloads_mvm_flat.txt"': 'demo_sauria_path("sauria_preloads_mvm_flat.txt")',
}
for a, b in replacements.items():
    s = s.replace(a, b)

# Capture execution cycles at the STD-done print (anchor before the [TB] rename).
s = s.replace(
    'std::cout << "[TB] STD NPU done at cycle " << cycle << std::endl;',
    'g_demo_exec_cycles = cycle;\n                std::cout << "[TB] STD NPU done at cycle " << cycle << std::endl;'
)

# Capture contraction depth K wherever mvm_k is driven onto the core.
s = s.replace('o_mvm_k.write(mvm_k);', 'o_mvm_k.write(mvm_k); g_demo_mvm_k = (long)mvm_k;')

# Inject the PERF summary right before the single-shot PASS result.
perf_anchor = '        if (errors_std == 0)\n        {\n            std::cout << "[RESULT] TEST PASSED: NPU Exact Final'
perf_inject = (
    '        demo_print_perf(g_demo_exec_cycles, g_demo_mvm_k,\n'
    '                        (unsigned long)total_c_elements, EVAL_X, EVAL_Y);\n\n'
    + perf_anchor
)
if perf_anchor not in s:
    raise SystemExit("[ERROR] perf anchor (single-shot PASS) not found")
s = s.replace(perf_anchor, perf_inject, 1)

# Print title + runtime paths at the start of test_process.
s = s.replace('''    void test_process()\n    {''',
              '''    void test_process()\n    {\n        demo_print_test_title();\n        demo_print_runtime_paths();''')

# Rename log identity lightly without touching proven logic.
s = s.replace('Evaluation Testbench', 'Runtime Demo Testbench')
s = s.replace('[TB]', '[TB_DEMO]')
s = s.replace('[TB ERROR]', '[TB_DEMO ERROR]')
s = s.replace('[TB WARNING]', '[TB_DEMO WARNING]')

banner = """// ================================================================\n// AUTO-GENERATED FILE: tb_demo.cpp\n// Generated from tb_evaluate.cpp by create_tb_demo_from_tb_evaluate.py.\n// Do not edit directly; re-generate after major tb_evaluate.cpp changes.\n// ================================================================\n\n"""
DST.write_text(banner + s)
print(f"[OK] Generated {DST}")
