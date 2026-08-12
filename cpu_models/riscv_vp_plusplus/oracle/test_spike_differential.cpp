// SPDX-License-Identifier: Apache-2.0
//
// The Phase 2 differential gate: one RV32GCV image, two independent models,
// one signature block compared word by word.
//
// Decision record D3 keeps Spike as a golden reference only. It is never linked
// into the platform or into a portable package, and it is not linked into this
// test either — it runs as a child process, so the two models share no
// allocator, no build flags and, most importantly, no copy of Berkeley
// SoftFloat's process-global rounding state (audit finding F5).
//
// ── verdicts ────────────────────────────────────────────────────────────────
//
//   equal                            pass
//   differs, and the field is in     XFAIL — a defect this project has decided
//   `kKnownDifferences`              to carry, named with its upstream commit
//   differs, and it is not           FAIL — an undocumented divergence
//   equal, but the field is in       XPASS — FAIL, because a known difference
//   `kKnownDifferences`              that vanished means one of the two pins
//                                    moved and the audit is now fiction
//
// The XPASS rule is the reason this is a table and not an allowlist. An
// allowlist rots quietly; a table notices when the ground moves.
//
// Usage: test_spike_differential <rvv_sig.elf> <path-to-spike>
// Exit codes: 0 pass, 1 fail, 77 skip (image or oracle not built).

#include <sys/wait.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <cdc/cpu/cpu_base.h>

#include "riscv_vp_plusplus_wrapper.h"

extern "C" {
#include "sig_layout.h"
}

namespace {

// Must match `link_dram.ld` and the `-m` argument handed to Spike. The
// mid-vector fault probe runs off the end of memory, so "the end" has to be in
// the same place on both models or the probe measures two different things.
constexpr std::uint64_t kRamBase = 0x8000'0000ull;
constexpr std::uint64_t kRamSize = 1024 * 1024;
constexpr int kSkip = 77;

const char* const kFieldNames[] = {
#define TPU_V3_SIG_NAME(name, note) #name,
    TPU_V3_SIG_FIELDS(TPU_V3_SIG_NAME)
#undef TPU_V3_SIG_NAME
};

const char* const kFieldNotes[] = {
#define TPU_V3_SIG_NOTE(name, note) note,
    TPU_V3_SIG_FIELDS(TPU_V3_SIG_NOTE)
#undef TPU_V3_SIG_NOTE
};

enum class disposition {
    // Decided in `TPU_V3_DECISION_RECORD.md` D9: the upstream fix is not
    // backported, and the divergence is carried deliberately.
    expected_d9,
    // A real deviation from the specification that has been reproduced and
    // documented but not yet decided. Reported separately and loudly; Phase 2
    // is not signed off while any of these are open.
    open_finding,
};

struct known_difference {
    int field;
    disposition how;
    const char* reference;
    const char* why;
    // True when the deviation is "the model under test did not take a trap the
    // oracle took". `trap_total` counts traps across the whole run, so without
    // this the same defect would be reported twice: once at its own field and
    // once as an unexplained count. See the derived check at the end.
    bool suppresses_trap = false;
};

// Every entry was derived by reading the pinned VP++ source, not by recording
// whatever the first run happened to print. The distinction matters: a table
// filled in from observed output cannot fail, because it agrees with the model
// by construction.
const known_difference kKnownDifferences[] = {
    // ── 63524fbb, "fixed fmax/fmin for F,D,Zfh and NAN check" ──────────────
    //
    // Before v2.2 of the spec, fmin/fmax returned NaN when either operand was
    // NaN; since v2.2 they return the non-NaN operand. The pinned VP++ picks
    // `rs2` whenever `rs1` is not smaller, so it hands back the NaN. Only the
    // rs2-is-NaN ordering diverges, which is why the corpus uses that order.
    {TPU_V3_SIG_fmin_s_qnan, disposition::expected_d9, "63524fbb",
     "fmin.s with a quiet NaN in rs2 returns the NaN instead of the operand"},
    {TPU_V3_SIG_fmax_s_qnan, disposition::expected_d9, "63524fbb",
     "fmax.s with a quiet NaN in rs2 returns the NaN instead of the operand"},
    {TPU_V3_SIG_fmin_d_qnan_hi, disposition::expected_d9, "63524fbb",
     "fmin.d with a quiet NaN in rs2 returns the NaN instead of the operand"},
    {TPU_V3_SIG_fmin_s_snan, disposition::expected_d9, "63524fbb",
     "fmin.s with a signalling NaN in rs2 returns the NaN instead of the operand"},

    // ── 14e7fff5, "fixed load/store for F,D,Zfh" ───────────────────────────
    //
    // The pinned VP++ omits `fp_prepare_instr()` and `fp_set_dirty()` from the
    // F and D load/store cases (`vp/src/core/rv32/iss_ctemplate.cpp`), so a
    // float load neither traps with `mstatus.FS = Off` nor marks the state
    // dirty.
    {TPU_V3_SIG_mstatus_fs_after_fp_ls, disposition::expected_d9, "14e7fff5",
     "flw does not set mstatus.FS to Dirty"},
    {TPU_V3_SIG_fp_ls_off_trapped, disposition::expected_d9, "14e7fff5",
     "flw with mstatus.FS = Off does not raise an illegal instruction", true},
    {TPU_V3_SIG_fp_ls_off_mcause, disposition::expected_d9, "14e7fff5",
     "consequence of the above: no trap, so no cause"},

    // F12 and F13 are **not** here, and that is the point.
    //
    // Both were found by this corpus as real deviations from the
    // specification, and both were then fixed under decision records D12 and
    // D13 rather than accepted as carried differences. The fields they used to
    // occupy — `eew64_trapped`, `eew64_mcause`, `fault_mcause` — are now plain
    // matches, and the XPASS rule below is what enforces that: putting them
    // back in this table would make the gate demand a difference that the
    // patches removed.
};

const known_difference* lookup(int field)
{
    for (const auto& entry : kKnownDifferences) {
        if (entry.field == field) {
            return &entry;
        }
    }
    return nullptr;
}

/// Probe memory for the VP++ run. Deliberately the same size as the region
/// handed to Spike, and deliberately failing outside it: the mid-vector fault
/// probe depends on an access past the end being an error on both models.
class probe_memory : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<probe_memory> tsock;
    std::vector<unsigned char> storage;

    /// Address of `tohost`, taken from the ELF. A write here is the exit
    /// trigger — the same trigger fesvr uses, so the two runs end at the same
    /// instruction rather than at two different conventions.
    std::uint64_t tohost_address = 0;
    bool exited = false;
    unsigned long out_of_range_accesses = 0;

    probe_memory(sc_core::sc_module_name name, std::size_t bytes)
        : sc_core::sc_module(name)
        , tsock("tsock")
        , storage(bytes, 0)
    {
        tsock.register_b_transport(this, &probe_memory::b_transport);
        tsock.register_transport_dbg(this, &probe_memory::transport_dbg);
    }

    bool contains(std::uint64_t address, std::uint64_t length) const
    {
        return address >= kRamBase && address - kRamBase < storage.size()
            && length <= storage.size() - (address - kRamBase);
    }

    std::uint32_t load_word(std::uint64_t address) const
    {
        std::uint32_t value = 0;
        std::memcpy(&value, storage.data() + (address - kRamBase), sizeof(value));
        return value;
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time&)
    {
        const auto address = trans.get_address();
        const auto length = trans.get_data_length();

        if (!contains(address, length)) {
            ++out_of_range_accesses;
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }

        const auto offset = address - kRamBase;
        if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            std::memcpy(storage.data() + offset, trans.get_data_ptr(), length);
            if (tohost_address != 0 && address == tohost_address
                && load_word(tohost_address) != 0) {
                exited = true;
                sc_core::sc_stop();
            }
        } else {
            std::memcpy(trans.get_data_ptr(), storage.data() + offset, length);
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        const auto address = trans.get_address();
        const auto length = trans.get_data_length();
        if (!contains(address, length)) {
            return 0;
        }
        const auto offset = address - kRamBase;
        if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            std::memcpy(storage.data() + offset, trans.get_data_ptr(), length);
        } else {
            std::memcpy(trans.get_data_ptr(), storage.data() + offset, length);
        }
        return length;
    }
};

/// Minimal ELF32 symbol lookup. The harness resolves `begin_signature`,
/// `end_signature` and `tohost` exactly the way fesvr does, so neither side
/// carries an address constant that could drift from the linker script.
bool read_symbols(const std::string& path,
                  const std::vector<std::string>& wanted,
                  std::vector<std::uint64_t>& values)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    const std::vector<std::uint8_t> image((std::istreambuf_iterator<char>(file)),
                                          std::istreambuf_iterator<char>());
    if (image.size() < 52 || image[0] != 0x7f || image[1] != 'E') {
        return false;
    }
    const auto rd16 = [&](std::size_t off) {
        return static_cast<std::uint32_t>(image[off])
            | (static_cast<std::uint32_t>(image[off + 1]) << 8);
    };
    const auto rd32 = [&](std::size_t off) {
        return static_cast<std::uint32_t>(image[off])
            | (static_cast<std::uint32_t>(image[off + 1]) << 8)
            | (static_cast<std::uint32_t>(image[off + 2]) << 16)
            | (static_cast<std::uint32_t>(image[off + 3]) << 24);
    };

    const std::uint32_t shoff = rd32(0x20);
    const std::uint32_t shentsize = rd16(0x2e);
    const std::uint32_t shnum = rd16(0x30);

    values.assign(wanted.size(), 0);
    std::vector<bool> found(wanted.size(), false);

    for (std::uint32_t i = 0; i < shnum; ++i) {
        const std::size_t sh = shoff + i * shentsize;
        if (sh + 40 > image.size() || rd32(sh + 4) != 2u) {  // SHT_SYMTAB
            continue;
        }
        const std::uint32_t symoff = rd32(sh + 16);
        const std::uint32_t symsize = rd32(sh + 20);
        // sh_link, not sh_info: the string table for a symbol table lives in
        // sh_link. Reading sh_info here silently resolves every name to
        // garbage, which is exactly the bug this comment exists to prevent
        // recurring.
        const std::uint32_t stroff = rd32(shoff + rd32(sh + 24) * shentsize + 16);

        for (std::uint32_t off = 0; off + 16 <= symsize; off += 16) {
            const std::size_t sym = symoff + off;
            const char* name =
                reinterpret_cast<const char*>(image.data() + stroff + rd32(sym));
            for (std::size_t w = 0; w < wanted.size(); ++w) {
                if (!found[w] && wanted[w] == name) {
                    values[w] = rd32(sym + 4);
                    found[w] = true;
                }
            }
        }
    }
    for (bool ok : found) {
        if (!ok) {
            return false;
        }
    }
    return true;
}

/// fesvr's `+signature` format: one line per granularity unit, bytes printed
/// most significant first, so a 4-byte line is the plain hex of the
/// little-endian word at that offset.
void write_signature(const std::string& path,
                     const std::vector<std::uint32_t>& words)
{
    std::ofstream out(path);
    out << std::setfill('0') << std::hex;
    for (std::uint32_t word : words) {
        out << std::setw(8) << word << '\n';
    }
}

bool read_signature(const std::string& path, std::vector<std::uint32_t>& words)
{
    std::ifstream in(path);
    if (!in) {
        return false;
    }
    words.clear();
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        words.push_back(static_cast<std::uint32_t>(std::stoul(line, nullptr, 16)));
    }
    return true;
}

/// Runs Spike as a child process. `fork`/`execv` rather than `system()`: no
/// shell means no quoting rules to get wrong on a path, and the exit status is
/// the child's rather than a shell's interpretation of it.
int run_spike(const std::string& spike, const std::string& elf,
              const std::string& signature_path)
{
    const std::string mem_arg =
        "-m0x80000000:0x" + [] {
            std::ostringstream oss;
            oss << std::hex << kRamSize;
            return oss.str();
        }();
    const std::string sig_arg = "+signature=" + signature_path;

    std::vector<std::string> args = {spike,
                                     "--isa=rv32gcv_zvl512b",
                                     mem_arg,
                                     sig_arg,
                                     "+signature-granularity=4",
                                     elf};

    std::cout << "oracle:";
    for (const auto& a : args) {
        std::cout << ' ' << a;
    }
    std::cout << '\n';

    std::vector<char*> argv;
    for (auto& a : args) {
        argv.push_back(const_cast<char*>(a.c_str()));
    }
    argv.push_back(nullptr);

    const pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        execv(spike.c_str(), argv.data());
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        return -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

bool file_exists(const std::string& path)
{
    std::ifstream probe(path, std::ios::binary);
    return static_cast<bool>(probe);
}

}  // namespace

int sc_main(int argc, char* argv[])
{
    if (argc != 3) {
        std::cerr << "usage: test_spike_differential <rvv_sig.elf> <spike>\n";
        return 2;
    }
    const std::string elf_path = argv[1];
    const std::string spike_path = argv[2];

    if (!file_exists(elf_path)) {
        std::cerr << "SKIP: " << elf_path << " was not built\n";
        return kSkip;
    }
    if (!file_exists(spike_path)) {
        std::cerr << "SKIP: no Spike oracle at " << spike_path
                  << "\n       run cpu_models/riscv_vp_plusplus/oracle/fetch_spike.sh\n";
        return kSkip;
    }

    std::vector<std::uint64_t> symbols;
    if (!read_symbols(elf_path, {"begin_signature", "end_signature", "tohost"},
                      symbols)) {
        std::cerr << "FAIL: " << elf_path
                  << " is missing begin_signature, end_signature or tohost\n";
        return 1;
    }
    const std::uint64_t sig_begin = symbols[0];
    const std::uint64_t sig_end = symbols[1];
    const std::uint64_t tohost = symbols[2];
    const std::size_t sig_words = (sig_end - sig_begin) / 4;

    if (sig_words != TPU_V3_SIG_WORD_COUNT) {
        std::cerr << "FAIL: the image's signature block holds " << sig_words
                  << " words but sig_layout.h declares " << TPU_V3_SIG_WORD_COUNT
                  << "; the firmware and the harness were built from different "
                     "headers\n";
        return 1;
    }

    // ── the model under test ────────────────────────────────────────────────
    probe_memory mem("mem", kRamSize);
    mem.tohost_address = tohost;

    cdc::cpu::cpu_config config;
    config.hart_id = 0;
    cdc::cpu::riscv_vp_plusplus_cpu cpu("core", config);
    cpu.data_bus().bind(mem.tsock);
    cpu.load_elf(elf_path);

    sc_core::sc_start(sc_core::sc_time(2, sc_core::SC_MS));

    std::cout << "riscv-vp-plusplus\n"
              << "  instructions retired : " << cpu.get_instret() << '\n'
              << "  out-of-range accesses: " << mem.out_of_range_accesses
              << "  (the mid-vector fault probe needs at least one)\n"
              << "  exited via tohost    : " << (mem.exited ? "yes" : "no")
              << '\n';

    if (!mem.exited) {
        std::cerr << "FAIL: the image never wrote tohost; PC 0x" << std::hex
                  << cpu.get_pc() << std::dec << '\n';
        return 1;
    }

    std::vector<std::uint32_t> vpp(sig_words);
    for (std::size_t i = 0; i < sig_words; ++i) {
        vpp[i] = mem.load_word(sig_begin + 4 * i);
    }

    const std::string vpp_sig_path = "vpp.sig";
    const std::string spike_sig_path = "spike.sig";
    write_signature(vpp_sig_path, vpp);

    // ── the oracle ──────────────────────────────────────────────────────────
    const int spike_status = run_spike(spike_path, elf_path, spike_sig_path);
    if (spike_status != 0) {
        std::cerr << "FAIL: Spike exited with status " << spike_status
                  << ". A non-zero HTIF exit code is the firmware's own status "
                     "word, so the oracle run itself failed a self-check.\n";
        return 1;
    }

    std::vector<std::uint32_t> golden;
    if (!read_signature(spike_sig_path, golden)) {
        std::cerr << "FAIL: Spike produced no signature file\n";
        return 1;
    }
    if (golden.size() != sig_words) {
        std::cerr << "FAIL: Spike dumped " << golden.size() << " words, expected "
                  << sig_words << '\n';
        return 1;
    }

    // ── sanity on the oracle before trusting it ─────────────────────────────
    //
    // Without this, two models that both aborted in phase 1 would agree
    // perfectly and the gate would report a pass having tested nothing.
    bool oracle_ok = true;
    if (golden[TPU_V3_SIG_magic] != TPU_V3_SIG_MAGIC) {
        std::cerr << "FAIL: the oracle's signature block has the wrong magic\n";
        oracle_ok = false;
    }
    if (golden[TPU_V3_SIG_status] != 0 || golden[TPU_V3_SIG_termination] != 1) {
        std::cerr << "FAIL: the oracle run did not complete cleanly (status "
                  << golden[TPU_V3_SIG_status] << ", termination "
                  << golden[TPU_V3_SIG_termination] << ", phase "
                  << golden[TPU_V3_SIG_phase_reached] << ")\n";
        oracle_ok = false;
    }
    if (!oracle_ok) {
        return 1;
    }

    if (vpp[TPU_V3_SIG_termination] != 1) {
        std::cerr << "FAIL: the model under test aborted in a trap at phase "
                  << vpp[TPU_V3_SIG_phase_reached] << " (status "
                  << vpp[TPU_V3_SIG_status] << ")\n";
    }

    // ── compare ─────────────────────────────────────────────────────────────
    int failures = 0;
    int xfail = 0;
    int open_findings = 0;

    std::cout << "\ndifferential comparison, " << sig_words << " fields\n";

    // How many traps the model under test failed to take, counted from the
    // fields that record a suppressed trap. `trap_total` is then checked
    // *exactly* against that, rather than being excused: an extra or missing
    // trap that no field explains is still a failure.
    unsigned suppressed_traps = 0;
    for (const auto& entry : kKnownDifferences) {
        if (entry.suppresses_trap && vpp[entry.field] == 0
            && golden[entry.field] != 0) {
            ++suppressed_traps;
        }
    }

    for (std::size_t i = 0; i < sig_words; ++i) {
        if (i == TPU_V3_SIG_trap_total) {
            const std::uint32_t expected = golden[i] - suppressed_traps;
            if (vpp[i] == expected) {
                if (suppressed_traps != 0) {
                    std::cout << "  DERIV trap_total  vp++ " << vpp[i]
                              << "  spike " << golden[i] << "  (" << suppressed_traps
                              << " trap(s) suppressed by the differences above)\n";
                }
            } else {
                ++failures;
                std::cout << "  FAIL  trap_total  vp++ " << vpp[i] << "  spike "
                          << golden[i] << "\n         expected " << expected
                          << " after accounting for " << suppressed_traps
                          << " known suppressed trap(s); the remainder is "
                             "unexplained\n";
            }
            continue;
        }

        const known_difference* known = lookup(static_cast<int>(i));
        const bool differs = vpp[i] != golden[i];

        if (!differs && known == nullptr) {
            continue;
        }

        std::ostringstream values;
        values << std::hex << std::setfill('0') << "vp++ 0x" << std::setw(8)
               << vpp[i] << "  spike 0x" << std::setw(8) << golden[i];

        if (differs && known != nullptr) {
            if (known->how == disposition::expected_d9) {
                ++xfail;
                std::cout << "  XFAIL " << kFieldNames[i] << "  " << values.str()
                          << "\n         " << known->reference << ": "
                          << known->why << '\n';
            } else {
                ++open_findings;
                std::cout << "  OPEN  " << kFieldNames[i] << "  " << values.str()
                          << "\n         " << known->reference << ": "
                          << known->why << '\n';
            }
        } else if (differs) {
            ++failures;
            std::cout << "  FAIL  " << kFieldNames[i] << "  " << values.str()
                      << "\n         " << kFieldNotes[i] << '\n';
        } else {
            // XPASS. The difference this project decided to carry is gone,
            // which means a pin moved: either VP++ picked up the fix or Spike
            // changed its mind. Either way the audit, the manifest and this
            // table are now describing something that no longer exists.
            ++failures;
            std::cout << "  XPASS " << kFieldNames[i] << "  " << values.str()
                      << "\n         " << known->reference
                      << " no longer differs; re-check the pins and this table\n";
        }
    }

    std::cout << "\n  matched      : " << (sig_words - xfail - open_findings - failures)
              << "\n  XFAIL (D9)   : " << xfail
              << "\n  OPEN         : " << open_findings
              << "\n  FAIL         : " << failures << '\n';

    if (open_findings != 0) {
        std::cout << "\nOPEN findings are reproduced and documented deviations "
                     "with no decision yet.\nPhase 2 is not signed off while any "
                     "remain open; see TPU_V3_PHASE2_AUDIT.md.\n";
    }

    // OPEN findings block. The banner above says Phase 2 is not signed off
    // while any remain, and a gate that prints that and then exits 0 is
    // advisory, not a gate. There are none today; this is here so the next one
    // stops the build instead of scrolling past.
    if (failures != 0 || open_findings != 0
        || vpp[TPU_V3_SIG_termination] != 1) {
        std::cerr << '\n'
                  << failures << " undocumented difference(s), " << open_findings
                  << " open finding(s)\n";
        return 1;
    }

    std::cout << "\ntest_spike_differential: every difference between "
                 "riscv-vp-plusplus and Spike is accounted for\n";
    return 0;
}
