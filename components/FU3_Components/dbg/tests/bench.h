#pragma once
// Test JTAG DTM, dmcontrol/dmstatus 2 hart, halt/resume, abstract Access Register, cmderr, ndmreset, dmactive, SBA
// Dùng chung cho behavior.cpp (SBA nối thẳng bộ nhớ) và integration.cpp (SBA qua cổng DBG của bus FU3, định nghĩa FU3_DBG_WITH_BUS)
#include <fu3_dbg/dtm.h>

#include <cstring>
#include <iostream>
#include <string>
#include <tlm_utils/simple_target_socket.h>

#ifdef FU3_DBG_WITH_BUS
    #include <fu3_bus/bus_system.h>
#endif

using namespace fu3_dbg;

// Hart giả định : halt ngay khi có yêu cầu, giữ GPR/CSR. Hart thật (CVA6/Ibex) cài DebugHart ở wrapper
struct FakeHart : DebugHart {
    bool request = false, is_halted = false;
    unsigned resumes = 0;
    std::uint32_t gpr[32] = {}, csr[4096] = {};

    void set_halt_request(bool r) override {
        request = r;
        if (r) 
            is_halted = true;
    }
    void resume() override { is_halted = false; ++resumes; }
    bool halted() const override { return is_halted; }
    bool access_register(std::uint16_t regno, bool write, std::uint32_t& value) override {
        std::uint32_t* reg = nullptr;
        if (regno < 0x1000) 
            reg = &csr[regno];
        else if (regno < 0x1020) 
            reg = &gpr[regno - 0x1000];
        else 
            return false;   // FPR... không có
        if (write) {
            if (reg != &gpr[0]) *reg = value;
        } else {
            value = *reg;
        }
        return true;
    }
};

// Bộ nhớ 64 KiB tại DSRAM. Offset 0xFFF0 trả lỗi slave
struct Memory : sc_core::sc_module {
    tlm_utils::simple_target_socket<Memory> socket{"socket"};
    unsigned char mem[0x10000] = {};
    bool absolute = true;   // Nối thẳng: địa chỉ tuyệt đối 
                            // Qua bus: offset cục bộ

    explicit Memory(sc_core::sc_module_name n) : sc_module(n) { 
            socket.register_b_transport(this, &Memory::b_transport); 
    }

    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time&) {
        std::uint64_t a = tx.get_address();
        if (absolute) {
            if (a < 0x0100'0000 || a >= 0x0101'0000) {
                tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE); 
                return; 
            }
            a -= 0x0100'0000;
        }
        if (a == 0xFFF0) { 
            tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE); 
            return; 
        }
        if (tx.is_write()) 
            std::memcpy(&mem[a], tx.get_data_ptr(), tx.get_data_length());
        else 
            std::memcpy(tx.get_data_ptr(), &mem[a], tx.get_data_length());
        tx.set_response_status(tlm::TLM_OK_RESPONSE);
    }
    std::uint32_t word(std::uint32_t off) const {
        std::uint32_t v;
        std::memcpy(&v, &mem[off], 4);
        return v;
    }
};

constexpr std::uint32_t kDsram = 0x0100'0000;

struct Bench : sc_core::sc_module {
    FakeHart cva6, ibex;
    DebugModule dm;
    Dtm dtm;
    Memory memory{"memory"};
    sc_core::sc_signal<bool> ndmreset{"ndmreset"};
    unsigned checks = 0, errors = 0;
    bool via_bus;

#ifdef FU3_DBG_WITH_BUS
    fu3_bus::BusSystem* fabric = nullptr;
#endif

    SC_HAS_PROCESS(Bench);
    Bench(sc_core::sc_module_name n, bool bus)
        : sc_module(n), dm("dm", {&cva6, &ibex}), dtm(dm), via_bus(bus) {
        dm.ndmreset(ndmreset);

#ifdef FU3_DBG_WITH_BUS
        if (bus) {
            auto cfg = fu3_bus::BusConfig::fu3();
            for (auto& t : cfg.targets)
                t.enabled = t.name == "DSRAM" || t.name == "ROM";
            fabric = new fu3_bus::BusSystem("fabric", cfg);
            dm.sba.bind(fabric->initiator("DBG"));
            fabric->target("DSRAM").bind(memory.socket);
            fabric->target("ROM").bind(rom_sink.socket);
            memory.absolute = false;
        } else
#endif

        {
            dm.sba.bind(memory.socket);
        }
        SC_THREAD(run);
    }

#ifdef FU3_DBG_WITH_BUS
    Memory rom_sink{"rom_sink"};
#endif

    void check(bool ok, const std::string& what) {
        ++checks;
        if (!ok) {
            ++errors;
            std::cerr << "FAIL: " << what << '\n';
        }
    }

    // Mọi truy cập DM đi qua JTAG DTM
    std::uint32_t dmi_rd(std::uint8_t a) {
        dtm.scan_ir(FU3_DBG_IR_DMI);
        dtm.scan_dr((std::uint64_t(a) << 34) | FU3_DBG_DMI_OP_READ);
        const std::uint64_t r = dtm.scan_dr(FU3_DBG_DMI_OP_NOP);
        check((r & 3) == 0, "DMI op status success");
        return static_cast<std::uint32_t>(r >> 2);
    }
    void dmi_wr(std::uint8_t a, std::uint32_t v) {
        dtm.scan_ir(FU3_DBG_IR_DMI);
        dtm.scan_dr((std::uint64_t(a) << 34) | (std::uint64_t(v) << 2) | FU3_DBG_DMI_OP_WRITE);
    }
    void select(unsigned hart, std::uint32_t extra = 0) {
        dmi_wr(FU3_DBG_DMCONTROL, FU3_DBG_DMCONTROL_DMACTIVE | FU3_DBG_DMCONTROL_HARTSEL(hart) | extra);
    }
    std::uint32_t cmderr() { return (dmi_rd(FU3_DBG_ABSTRACTCS) >> 8) & 7; }
    void clear_cmderr() { dmi_wr(FU3_DBG_ABSTRACTCS, FU3_DBG_ABSTRACTCS_CMDERR_MASK); }
    void command(std::uint32_t cmd) { dmi_wr(FU3_DBG_COMMAND, cmd); }
    std::uint32_t read_reg(std::uint16_t regno) {
        command(FU3_DBG_CMD_AARSIZE_32 | FU3_DBG_CMD_TRANSFER | regno);
        return dmi_rd(FU3_DBG_DATA0);
    }
    void write_reg(std::uint16_t regno, std::uint32_t v) {
        dmi_wr(FU3_DBG_DATA0, v);
        command(FU3_DBG_CMD_AARSIZE_32 | FU3_DBG_CMD_TRANSFER | FU3_DBG_CMD_WRITE | regno);
    }
    std::uint32_t sberror() { return (dmi_rd(FU3_DBG_SBCS) >> 12) & 7; }
    void clear_sberror() { dmi_wr(FU3_DBG_SBCS, (dmi_rd(FU3_DBG_SBCS) & ~0u) | FU3_DBG_SBCS_SBERROR_MASK); }

    void test_jtag() {
        dtm.test_logic_reset();
        check(dtm.dr_length() == 32, "Test-Logic-Reset selects IDCODE (32-bit DR)");
        const std::uint64_t id = dtm.scan_dr(0);
        check(id == FU3_DBG_IDCODE && (id & 1), "IDCODE value, bit 0 = 1");
        dtm.scan_ir(FU3_DBG_IR_DTMCS);
        const std::uint32_t cs = static_cast<std::uint32_t>(dtm.scan_dr(0));
        check((cs & 0xF) == 1, "DTMCS.VERSION = 1 (0.13)");
        check(((cs >> 4) & 0x3F) == 7, "DTMCS.ABITS = 7");
        check(((cs >> 10) & 3) == 0, "DTMCS.DMISTAT = 0");
        dtm.scan_ir(FU3_DBG_IR_DMI);
        check(dtm.dr_length() == 41, "DMI DR is abits + 34 = 41 bits");
        for (std::uint8_t ir : {std::uint8_t(0x00), std::uint8_t(0x1F), std::uint8_t(0x05)}) {
            dtm.scan_ir(ir);
            check(dtm.dr_length() == 1, "IR " + std::to_string(ir) + " is BYPASS");
        }

        // Kết quả DMI là của lần scan trước, DMIHARDRESET bỏ nó
        dmi_wr(FU3_DBG_DMCONTROL, FU3_DBG_DMCONTROL_DMACTIVE);
        dtm.scan_dr((std::uint64_t(FU3_DBG_DMCONTROL) << 34) | FU3_DBG_DMI_OP_READ);
        const std::uint64_t prev = dtm.scan_dr((std::uint64_t(FU3_DBG_DMSTATUS) << 34) | FU3_DBG_DMI_OP_READ);
        check(prev == ((std::uint64_t(FU3_DBG_DMCONTROL) << 34) | (std::uint64_t(FU3_DBG_DMCONTROL_DMACTIVE) << 2)),
              "A DMI scan returns the previous access: address and data of the dmcontrol read");
        dtm.scan_ir(FU3_DBG_IR_DTMCS);
        dtm.scan_dr(FU3_DBG_DTMCS_DMIHARDRESET);
        dtm.scan_ir(FU3_DBG_IR_DMI);
        check(dtm.scan_dr(FU3_DBG_DMI_OP_NOP) == 0, "DMIHARDRESET drops the pending DMI result");
        dmi_wr(FU3_DBG_DMCONTROL, 0);
    }

    void test_activation() {
        check(dmi_rd(FU3_DBG_DMCONTROL) == 0, "_ower-on: DMCONTROL 0, DM inactive");
        const std::uint32_t st = dmi_rd(FU3_DBG_DMSTATUS);
        check((st & 0xF) == 2 && (st & FU3_DBG_DMSTATUS_AUTHENTICATED), "DMSTATUS: version 2, authenticated");
        dmi_wr(FU3_DBG_DATA0, 0x1234);
        check(dmi_rd(FU3_DBG_DATA0) == 0, "DM inactive: data0 write ignored");
        dmi_wr(FU3_DBG_DMCONTROL, FU3_DBG_DMCONTROL_DMACTIVE | FU3_DBG_DMCONTROL_HALTREQ);
        check(dmi_rd(FU3_DBG_DMCONTROL) == FU3_DBG_DMCONTROL_DMACTIVE, "DMACTIVE set, haltreq reads 0");
        select(0);
        select(0, FU3_DBG_DMCONTROL_RESUMEREQ);
        const std::uint32_t abs = dmi_rd(FU3_DBG_ABSTRACTCS);
        check((abs & 0xF) == 2 && ((abs >> 24) & 0x1F) == 0, "ABSTRACTCS: datacount 2, progbufsize 0");
    }

    void test_havereset() {
        check(dmi_rd(FU3_DBG_DMSTATUS) & FU3_DBG_DMSTATUS_ALLHAVERESET, "Hart 0 havereset after power-on");
        select(0, FU3_DBG_DMCONTROL_ACKHAVERESET);
        check(!(dmi_rd(FU3_DBG_DMSTATUS) & FU3_DBG_DMSTATUS_ANYHAVERESET), "ACKHAVERESET clears hart 0");
        select(1);
        check(dmi_rd(FU3_DBG_DMSTATUS) & FU3_DBG_DMSTATUS_ALLHAVERESET, "Hart 1 still havereset");
        select(1, FU3_DBG_DMCONTROL_ACKHAVERESET);
        dm.notify_hart_reset(1);
        check(dmi_rd(FU3_DBG_DMSTATUS) & FU3_DBG_DMSTATUS_ALLHAVERESET, "Platform hart reset sets havereset");
        select(1, FU3_DBG_DMCONTROL_ACKHAVERESET);
    }

    void test_halt_resume() {
        select(0);
        check(dmi_rd(FU3_DBG_DMSTATUS) & FU3_DBG_DMSTATUS_ALLRUNNING, "Hart 0 running");
        select(0, FU3_DBG_DMCONTROL_HALTREQ);
        check(cva6.request, "Haltreq drives hart 0 debug request");
        check(dmi_rd(FU3_DBG_DMSTATUS) & FU3_DBG_DMSTATUS_ALLHALTED, "Hart 0 halted");
        check(!ibex.request && !ibex.is_halted, "Hart 1 untouched");
        check(dmi_rd(FU3_DBG_HALTSUM0) == 0x1, "HALTSUM0 = hart 0");
        select(0);
        check(!cva6.request, "Haltreq 0 clears the request");
        select(1, FU3_DBG_DMCONTROL_HALTREQ);
        select(1);
        check(dmi_rd(FU3_DBG_HALTSUM0) == 0x3, "HALTSUM0 = both harts");

        select(1, FU3_DBG_DMCONTROL_HALTREQ | FU3_DBG_DMCONTROL_RESUMEREQ);
        check(ibex.is_halted && ibex.resumes == 0, "Resumereq ignored while haltreq is set");
        select(1, FU3_DBG_DMCONTROL_RESUMEREQ);
        const std::uint32_t st = dmi_rd(FU3_DBG_DMSTATUS);
        check(!ibex.is_halted && (st & FU3_DBG_DMSTATUS_ALLRUNNING), "Resumereq resumes hart 1");
        check(st & FU3_DBG_DMSTATUS_ALLRESUMEACK, "Resumeack after resume");
        check(dmi_rd(FU3_DBG_HALTSUM0) == 0x1, "Haltsum0 back to hart 0");
        select(1, FU3_DBG_DMCONTROL_RESUMEREQ);
        check(ibex.resumes == 1, "Resumereq on a running hart does nothing");
    }

    void test_abstract() {
        select(0);   // hart 0 đang halted
        cva6.gpr[5] = 0xA5A5'0005u;
        cva6.csr[0x7B1] = 0x0001'0040u;
        check(read_reg(FU3_DBG_REGNO_GPR(5)) == 0xA5A5'0005u, "Read x5");
        check(read_reg(FU3_DBG_REGNO_CSR(0x7B1)) == 0x0001'0040u, "Read DPC (CSR 0x7B1)");
        write_reg(FU3_DBG_REGNO_GPR(6), 0x6666'0006u);
        check(cva6.gpr[6] == 0x6666'0006u, "Write x6");
        write_reg(FU3_DBG_REGNO_CSR(0x7B1), 0x0001'0100u);
        check(cva6.csr[0x7B1] == 0x0001'0100u, "Write DPC");
        check(ibex.gpr[6] == 0, "Hart 1 registers untouched");
        check(cmderr() == 0, "No cmderr");
        command(FU3_DBG_CMD_AARSIZE_32 | FU3_DBG_REGNO_GPR(7));
        check(cmderr() == 0, "Transfer = 0: no-op success");

        command((3u << 20) | FU3_DBG_CMD_TRANSFER | FU3_DBG_REGNO_GPR(5));
        check(cmderr() == FU3_DBG_CMDERR_NOTSUPPORTED, "Aarsize 64-bit -> cmderr 2");
        dmi_wr(FU3_DBG_DATA0, 0x77);
        command(FU3_DBG_CMD_AARSIZE_32 | FU3_DBG_CMD_TRANSFER | FU3_DBG_REGNO_GPR(5));
        check(dmi_rd(FU3_DBG_DATA0) == 0x77, "COmmand ignored while cmderr != 0");
        dmi_wr(FU3_DBG_ABSTRACTCS, 1u << 8);
        check(cmderr() == FU3_DBG_CMDERR_NOTSUPPORTED, "Cmderr W1C: other bit keeps it");
        clear_cmderr();
        check(cmderr() == 0, "Cmderr W1C clears");
        command(FU3_DBG_CMD_AARSIZE_32 | FU3_DBG_CMD_POSTEXEC | FU3_DBG_REGNO_GPR(5));
        check(cmderr() == FU3_DBG_CMDERR_NOTSUPPORTED, "Postexec without program buffer -> cmderr 2");
        clear_cmderr();
        command(1u << 24);
        check(cmderr() == FU3_DBG_CMDERR_NOTSUPPORTED, "Cmdtype 1 (Quick Access) -> cmderr 2");
        clear_cmderr();
        command(FU3_DBG_CMD_AARSIZE_32 | FU3_DBG_CMD_TRANSFER | 0x1020);
        check(cmderr() == FU3_DBG_CMDERR_EXCEPTION, "FPR regno -> cmderr 3");
        clear_cmderr();
        select(1);   // hart 1 đang chạy
        command(FU3_DBG_CMD_AARSIZE_32 | FU3_DBG_CMD_TRANSFER | FU3_DBG_REGNO_GPR(5));
        check(cmderr() == FU3_DBG_CMDERR_HALTRESUME, "Running hart -> cmderr 4");
        clear_cmderr();
    }

    void test_hartsel() {
        select(5);
        const std::uint32_t st = dmi_rd(FU3_DBG_DMSTATUS);
        check((st & FU3_DBG_DMSTATUS_ALLNONEXISTENT) && !(st & FU3_DBG_DMSTATUS_ANYHALTED) && !(st & FU3_DBG_DMSTATUS_ANYRUNNING), "hart 5 nonexistent");
        command(FU3_DBG_CMD_AARSIZE_32 | FU3_DBG_CMD_TRANSFER | FU3_DBG_REGNO_GPR(1));
        check(cmderr() == FU3_DBG_CMDERR_HALTRESUME, "Command to a nonexistent hart -> cmderr 4");
        clear_cmderr();
        select(0xFFFFF);
        check((dmi_rd(FU3_DBG_DMCONTROL) & 0x03FF'FFC0u) == FU3_DBG_DMCONTROL_HARTSEL(0xFFFFFu), "hartsello/hi hold 20 bits");
        select(0);
    }

    void test_ndmreset() {
        select(0, FU3_DBG_DMCONTROL_NDMRESET);
        sc_core::wait(1, sc_core::SC_NS);
        check(ndmreset.read(), "NDMRESET output high");
        check(dmi_rd(FU3_DBG_DMSTATUS) & FU3_DBG_DMSTATUS_ALLHAVERESET, "NDMRESET sets havereset on hart 0");
        select(1, FU3_DBG_DMCONTROL_NDMRESET);
        check(dmi_rd(FU3_DBG_DMSTATUS) & FU3_DBG_DMSTATUS_ALLHAVERESET, "NDMRESET sets havereset on hart 1");
        select(0);
        sc_core::wait(1, sc_core::SC_NS);
        check(!ndmreset.read(), "NDMRESET output low again");
        select(0, FU3_DBG_DMCONTROL_ACKHAVERESET);
        select(1, FU3_DBG_DMCONTROL_ACKHAVERESET);
    }

    void test_sba() {
        select(0);
        check(dmi_rd(FU3_DBG_SBCS) == 0x2004'0404u, "SBCS: version 1, sbaccess 2, sbasize 32, 32-bit access only");
        dmi_wr(FU3_DBG_SBADDRESS0, kDsram + 0x10);
        dmi_wr(FU3_DBG_SBDATA0, 0xDEAD'BEEFu);
        check(memory.word(0x10) == 0xDEAD'BEEFu, "SBA write to DSRAM");
        check(dmi_rd(FU3_DBG_SBADDRESS0) == kDsram + 0x10, "No autoincrement by default");

        dmi_wr(FU3_DBG_SBCS, FU3_DBG_SBCS_SBACCESS(2) | FU3_DBG_SBCS_SBAUTOINCREMENT);
        dmi_wr(FU3_DBG_SBADDRESS0, kDsram + 0x20);
        for (std::uint32_t i = 0; i < 3; ++i) dmi_wr(FU3_DBG_SBDATA0, 0x100 + i);
        check(memory.word(0x20) == 0x100 && memory.word(0x24) == 0x101 && memory.word(0x28) == 0x102, "Autoincrement write burst");
        check(dmi_rd(FU3_DBG_SBADDRESS0) == kDsram + 0x2C, "Address advanced by 4 per access");

        dmi_wr(FU3_DBG_SBCS, FU3_DBG_SBCS_SBACCESS(2) | FU3_DBG_SBCS_SBREADONADDR | FU3_DBG_SBCS_SBAUTOINCREMENT | FU3_DBG_SBCS_SBREADONDATA);
        dmi_wr(FU3_DBG_SBADDRESS0, kDsram + 0x20);
        check(dmi_rd(FU3_DBG_SBDATA0) == 0x100, "Readonaddr then sbdata0");
        check(dmi_rd(FU3_DBG_SBDATA0) == 0x101, "Readondata fetches the next word");
        check(dmi_rd(FU3_DBG_SBDATA0) == 0x102, "Readondata block read");
        check(sberror() == 0, "No sberror");

        dmi_wr(FU3_DBG_SBCS, FU3_DBG_SBCS_SBACCESS(0));
        dmi_wr(FU3_DBG_SBADDRESS0, kDsram + 0x40);
        dmi_wr(FU3_DBG_SBDATA0, 0x55);
        check(sberror() == FU3_DBG_SBERROR_SIZE, "8-bit access -> sberror 4 (DBG port is 32-bit only)");
        check(memory.word(0x40) == 0, "Rejected access writes nothing");
        dmi_wr(FU3_DBG_SBCS, FU3_DBG_SBCS_SBACCESS(2));
        dmi_wr(FU3_DBG_SBDATA0, 0x66);
        check(memory.word(0x40) == 0, "Accesses blocked while sberror != 0");
        clear_sberror();
        check(sberror() == 0, "SBERROR W1C clears");

        dmi_wr(FU3_DBG_SBCS, FU3_DBG_SBCS_SBACCESS(2) | FU3_DBG_SBCS_SBREADONADDR);
        dmi_wr(FU3_DBG_SBADDRESS0, kDsram + 0x42);
        check(sberror() == FU3_DBG_SBERROR_ALIGNMENT, "Misaligned -> sberror 3");
        clear_sberror();
        dmi_wr(FU3_DBG_SBADDRESS0, 0x0200'0000u);
        check(sberror() == FU3_DBG_SBERROR_BADADDR, "Unmapped address -> sberror 2");
        clear_sberror();
        dmi_wr(FU3_DBG_SBADDRESS0, kDsram + 0xFFF0);
        check(sberror() == FU3_DBG_SBERROR_OTHER, "Slave error -> sberror 7");
        clear_sberror();
#ifdef FU3_DBG_WITH_BUS
        dmi_wr(FU3_DBG_SBADDRESS0, 0x0000'0000u);
        check(sberror() == FU3_DBG_SBERROR_OTHER, "Boot ROM is Ibex-only on the bus -> sberror 7");
        clear_sberror();
#endif
        dmi_wr(FU3_DBG_SBCS, FU3_DBG_SBCS_SBACCESS(2));
    }

    void test_deactivate() {
        select(0, FU3_DBG_DMCONTROL_HALTREQ | FU3_DBG_DMCONTROL_NDMRESET);
        dmi_wr(FU3_DBG_DATA0, 0x99);
        clear_cmderr();
        command((3u << 20) | FU3_DBG_CMD_TRANSFER);
        dmi_wr(FU3_DBG_DMCONTROL, FU3_DBG_DMCONTROL_HALTREQ);   // dmactive = 0
        sc_core::wait(1, sc_core::SC_NS);
        check(dmi_rd(FU3_DBG_DMCONTROL) == 0, "DMACTIVE 0: dmcontrol reset");
        check(!ndmreset.read(), "DMACTIVE 0 drops ndmreset");
        check(!cva6.request && !ibex.request, "DMACTIVE 0 clears halt requests");
        check(dmi_rd(FU3_DBG_DATA0) == 0 && cmderr() == 0, "DMACTIVE 0 resets data0 and cmderr");
        check(dmi_rd(FU3_DBG_SBCS) == 0x2004'0404u, "DMACTIVE 0 resets sbcs");
    }

    void run() {
        test_jtag();
        test_activation();
        test_havereset();
        test_halt_resume();
        test_abstract();
        test_hartsel();
        test_ndmreset();
        test_sba();
        test_deactivate();
        sc_core::sc_stop();
    }
};
