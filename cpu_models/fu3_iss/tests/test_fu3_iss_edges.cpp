// fu3_iss unit checks on single instructions and CSRs (no bus), with the FU3
// CVA6 and Ibex option profiles. Started from the T04/T05 review probe
// (plan/T04_T05_review_logs/repro/iss_edges.cpp) and extended.

#include <cstring>
#include <functional>
#include <iostream>
#include <string>

#include "core/rv32/iss.h"

using namespace fu3_rv32;

namespace {

struct instr_mem : instr_memory_if {
    uint32_t word = 0x13;
    uint32_t load_instr(uint64_t) override { return word; }
};

struct data_mem : data_memory_if {
    uint32_t value = 0x3f800000;  // 1.0f
    int64_t load_double(uint64_t) override { return 0; }
    int32_t load_word(uint64_t) override { return value; }
    int32_t load_half(uint64_t) override { return 0; }
    int32_t load_byte(uint64_t) override { return 0; }
    uint32_t load_uhalf(uint64_t) override { return 0; }
    uint32_t load_ubyte(uint64_t) override { return 0; }
    void store_double(uint64_t, uint64_t) override {}
    void store_word(uint64_t, uint32_t) override {}
    void store_half(uint64_t, uint16_t) override {}
    void store_byte(uint64_t, uint8_t) override {}
    int32_t atomic_load_word(uint64_t) override { return 0; }
    void atomic_store_word(uint64_t, uint32_t) override {}
    int32_t atomic_load_reserved_word(uint64_t) override { return 0; }
    bool atomic_store_conditional_word(uint64_t, uint32_t) override { return false; }
    void atomic_unlock() override {}
    void flush_tlb() override {}
};

unsigned failures = 0;

void check(bool ok, const std::string& what, long observed)
{
    if (!ok) {
        ++failures;
        std::cout << "FAIL: " << what << " (observed " << observed << ")\n";
    }
}

// Execute one instruction; returns the trap cause or -1.
int step(ISS& cpu, instr_mem& im, uint32_t word)
{
    im.word = word;
    try {
        cpu.exec_step();
    } catch (SimulationTrap& e) {
        return e.reason;
    }
    return -1;
}

core_options cva6_options()
{
    core_options o;
    o.misa_disable = csr_misa::A | csr_misa::N | csr_misa::S | csr_misa::U;
    o.pmp_regions = 8;
    o.cycle_time = sc_core::sc_time(2.5, sc_core::SC_NS);
    return o;
}

core_options ibex_options()
{
    core_options o;
    o.misa_disable = csr_misa::A | csr_misa::F | csr_misa::N | csr_misa::S;
    o.fast_irqs = 15;
    o.pmp_regions = 16;
    o.smepmp = true;
    o.zcb = true;
    o.mtvec_vectored_only = true;
    o.mtvec_reset = 1;
    o.cycle_time = sc_core::sc_time(2.5, sc_core::SC_NS);
    return o;
}

}  // namespace

int sc_main(int, char**)
{
    tlm::tlm_global_quantum::instance().set(sc_core::sc_time(1, sc_core::SC_US));
    instr_mem im;
    data_mem dm;

    // ---- CVA6: F without U-mode --------------------------------------------
    ISS cva6(0);
    cva6.configure(cva6_options());
    auto reset_cva6 = [&](unsigned fs) {
        cva6.reset(0x10000);
        cva6.init(&im, &dm, nullptr, 0x10000, 0);
        cva6.csrs.mstatus.fields.fs = fs;
    };
    reset_cva6(1);
    check(step(cva6, im, 0x001022f3) == -1, "CVA6 csrr fflags with FS=Initial", 0);   // csrr t0, fflags
    reset_cva6(1);
    check(step(cva6, im, 0x00101073) == -1, "CVA6 csrw fflags with FS=Initial", 0);   // csrw fflags, zero
    check(cva6.csrs.mstatus.fields.fs == 3, "CVA6 FP CSR write dirties FS", cva6.csrs.mstatus.fields.fs);
    reset_cva6(0);
    check(step(cva6, im, 0x001022f3) == 2, "CVA6 csrr fflags with FS=Off is illegal", 0);
    check(step(cva6, im, 0x00002007) == 2, "CVA6 FLW with FS=Off is illegal", 0);     // flw f0, 0(zero)
    check(step(cva6, im, 0x00002027) == 2, "CVA6 FSW with FS=Off is illegal", 0);     // fsw f0, 0(zero)
    reset_cva6(1);
    check(step(cva6, im, 0x00002007) == -1, "CVA6 FLW with FS=Initial", 0);
    check(cva6.csrs.mstatus.fields.fs == 3, "CVA6 FLW dirties FS", cva6.csrs.mstatus.fields.fs);
    // fcvt.w.s t0, f0, rmm with f0 = 2.5: round to nearest, ties away -> 3.
    reset_cva6(1);
    dm.value = 0x40200000;  // 2.5f
    step(cva6, im, 0x00002007);
    const int rmm = step(cva6, im, 0xc00042d3);  // fcvt.w.s t0, f0, rmm
    check(rmm == -1 && cva6.regs[5] == 3, "CVA6 RMM rounding (fcvt.w.s 2.5 -> 3)", cva6.regs[5]);
    check(step(cva6, im, 0xc00052d3) == 2, "CVA6 reserved rm=5 is illegal", 0);
    cva6.csrs.fcsr.fields.frm = 5;
    check(step(cva6, im, 0xc00072d3) == 2, "CVA6 dynamic rm with frm=5 is illegal", 0);
    dm.value = 0x3f800000;
    reset_cva6(0);
    check(step(cva6, im, 0xc00022f3) == -1, "CVA6 reads cycle in M-mode without U", 0);  // csrr t0, cycle
    check(step(cva6, im, 0x30200073) == -1 && cva6.prv == MachineMode, "CVA6 mret stays in M (MPP WARL)",
          cva6.prv);

    // ---- Ibex: U-mode, no F, no N ------------------------------------------
    ISS ibex(1);
    ibex.configure(ibex_options());
    auto reset_ibex = [&] {
        ibex.reset(0x80);
        ibex.init(&im, &dm, nullptr, 0x80, 0);
    };
    reset_ibex();
    ibex.prv = UserMode;
    ibex.csrs.mepc.reg = 0x84;
    check(step(ibex, im, 0x30200073) == 2, "Ibex MRET from U-mode is illegal", ibex.prv);
    check(step(ibex, im, 0x10200073) == 2, "Ibex SRET (no S) is illegal", 0);
    check(step(ibex, im, 0x00200073) == 2, "Ibex URET (no N) is illegal", 0);
    reset_ibex();
    for (uint32_t mpp : {1u, 2u}) {
        ibex.set_csr_value(csr::MSTATUS_ADDR, mpp << 11);
        check(ibex.csrs.mstatus.fields.mpp == MachineMode, "Ibex MPP WARL maps unsupported value to M",
              ibex.csrs.mstatus.fields.mpp);
    }
    ibex.set_csr_value(csr::MSTATUS_ADDR, 0);
    check(ibex.csrs.mstatus.fields.mpp == UserMode, "Ibex MPP keeps U", ibex.csrs.mstatus.fields.mpp);
    reset_ibex();
    ibex.csrs.mstatus.fields.mpp = UserMode;
    ibex.csrs.mstatus.fields.mprv = 1;
    ibex.csrs.mepc.reg = 0x84;
    check(step(ibex, im, 0x30200073) == -1 && ibex.prv == UserMode && ibex.csrs.mstatus.fields.mprv == 0,
          "Ibex MRET to U clears MPRV", ibex.csrs.mstatus.fields.mprv);
    reset_ibex();
    ibex.prv = UserMode;
    ibex.csrs.mstatus.fields.tw = 1;
    ibex.csrs.mie.reg = 0x80;
    ibex.csrs.mip.reg = 0x80;
    check(step(ibex, im, 0x10500073) == 2, "Ibex WFI from U with TW=1 is illegal", 0);
    ibex.csrs.mstatus.fields.tw = 0;
    check(step(ibex, im, 0x10500073) == -1, "Ibex WFI from U with TW=0 (interrupt pending)", 0);
    reset_ibex();
    ibex.prv = UserMode;
    check(step(ibex, im, 0x001022f3) == 2, "Ibex fflags (no F) is illegal", 0);

    // ---- mcycle: writable, both words, inhibit, reset -----------------------
    reset_ibex();
    ibex.set_csr_value(csr::MCYCLE_ADDR, 1234);
    check(ibex.get_csr_value(csr::MCYCLE_ADDR) == 1234, "mcycle reads back the written value",
          ibex.get_csr_value(csr::MCYCLE_ADDR));
    ibex.set_csr_value(csr::MCYCLEH_ADDR, 7);
    check(ibex.get_csr_value(csr::MCYCLEH_ADDR) == 7 && ibex.get_csr_value(csr::MCYCLE_ADDR) == 1234,
          "mcycleh write keeps mcycle", ibex.get_csr_value(csr::MCYCLEH_ADDR));
    ibex.performance_and_sync_update(Opcode::ADD);
    check(ibex.get_csr_value(csr::MCYCLE_ADDR) == 1235, "mcycle counts after a write",
          ibex.get_csr_value(csr::MCYCLE_ADDR));
    ibex.set_csr_value(csr::MCOUNTINHIBIT_ADDR, 1);
    ibex.performance_and_sync_update(Opcode::ADD);
    check(ibex.get_csr_value(csr::MCYCLE_ADDR) == 1235, "mcountinhibit.CY stops mcycle",
          ibex.get_csr_value(csr::MCYCLE_ADDR));
    reset_ibex();
    check(ibex.get_csr_value(csr::MCYCLE_ADDR) == 0 && ibex.get_csr_value(csr::MCYCLEH_ADDR) == 0,
          "reset clears mcycle", ibex.get_csr_value(csr::MCYCLE_ADDR));

    std::cout << "fu3_iss edges: " << (failures ? "FAIL" : "PASS") << " (" << failures << " failures)\n";
    return failures ? 1 : 0;
}
