#pragma once
// Test cầu JTAG2AXI4 (CMD_IDCODE/ADDR/DATA/STATUS/BYPASS), truy cập hệ thống, Debug Module tại 0x1A11_0000:
// dmcontrol/dmstatus 2 hart, halt/resume, abstract Access Register, cmderr, ndmreset, dmactive
// Dùng chung cho behavior.cpp (router nối thẳng) và integration.cpp (qua cổng DBG của bus FU3, định nghĩa FU3_DBG_WITH_BUS)
#include <fu3_dbg/dm.h>
#include <fu3_dbg/jtag2axi.h>

#include <cstring>
#include <iostream>
#include <string>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#ifdef FU3_DBG_WITH_BUS
    #include <fu3_bus/bus_system.h>
#endif

using namespace fu3_dbg;

// Hart giả định: halt ngay khi có yêu cầu, giữ GPR/CSR. Hart thật (CVA6/Ibex) cài DebugHart ở wrapper
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

constexpr std::uint32_t kDsram = 0x0100'0000;

// Bộ nhớ 64 KiB tại DSRAM (offset cục bộ). Offset 0xFFF0 trả lỗi slave
struct Memory : sc_core::sc_module {
    tlm_utils::simple_target_socket<Memory> socket{"socket"};
    unsigned char mem[0x10000] = {};

    explicit Memory(sc_core::sc_module_name n) : sc_module(n) {
        socket.register_b_transport(this, &Memory::b_transport);
    }

    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time&) {
        const std::uint64_t a = tx.get_address();
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

// Nối thẳng không qua bus: giải mã DM 0x1A11_0000 (4 KiB) và DSRAM (64 KiB), còn lại lỗi địa chỉ
struct Router : sc_core::sc_module {
    tlm_utils::simple_target_socket<Router> in{"in"};
    tlm_utils::simple_initiator_socket<Router> dm{"dm"}, ram{"ram"};

    explicit Router(sc_core::sc_module_name n) : sc_module(n) {
        in.register_b_transport(this, &Router::b_transport);
    }
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& t) {
        const std::uint64_t a = tx.get_address();
        if (a >= FU3_DBG_DM_BASE && a < FU3_DBG_DM_BASE + 0x1000) {
            tx.set_address(a - FU3_DBG_DM_BASE);
            dm->b_transport(tx, t);
        } else if (a >= kDsram && a < kDsram + 0x10000) {
            tx.set_address(a - kDsram);
            ram->b_transport(tx, t);
        } else {
            tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        tx.set_address(a);
    }
};

struct Bench : sc_core::sc_module {
    FakeHart cva6, ibex;
    DebugModule dm;
    Jtag2Axi jtag{"jtag"};
    Memory memory{"memory"};
    sc_core::sc_signal<bool> ndmreset{"ndmreset"};
    unsigned checks = 0, errors = 0;
    bool via_bus;

#ifdef FU3_DBG_WITH_BUS
    fu3_bus::BusSystem* fabric = nullptr;
    Memory rom_sink{"rom_sink"};
#else
    Router router{"router"};
#endif

    SC_HAS_PROCESS(Bench);
    Bench(sc_core::sc_module_name n, bool bus)
        : sc_module(n), dm("dm", {&cva6, &ibex}), via_bus(bus) {
        dm.ndmreset(ndmreset);

#ifdef FU3_DBG_WITH_BUS
        auto cfg = fu3_bus::BusConfig::fu3();
        for (auto& t : cfg.targets)
            t.enabled = t.name == "DSRAM" || t.name == "ROM" || t.name == "IBEX_DBG";
        fabric = new fu3_bus::BusSystem("fabric", cfg);
        jtag.axi.bind(fabric->initiator("DBG"));
        fabric->target("IBEX_DBG").bind(dm.socket);
        fabric->target("DSRAM").bind(memory.socket);
        fabric->target("ROM").bind(rom_sink.socket);
#else
        jtag.axi.bind(router.in);
        router.dm.bind(dm.socket);
        router.ram.bind(memory.socket);
#endif
        SC_THREAD(run);
    }

    void check(bool ok, const std::string& what) {
        ++checks;
        if (!ok) {
            ++errors;
            std::cerr << "FAIL: " << what << '\n';
        }
    }

    // Truy cập AXI qua cầu JTAG2AXI4
    void set_address(std::uint32_t a) {
        jtag.scan_ir(FU3_DBG_IR_CMD_ADDR);
        jtag.scan_dr(a);
    }
    void axi_write(std::uint32_t a, std::uint32_t v) {
        set_address(a);
        jtag.scan_ir(FU3_DBG_IR_CMD_DATA);
        jtag.scan_dr(FU3_DBG_DATA_WRITE | v);
    }
    std::uint32_t axi_read(std::uint32_t a) {
        set_address(a);
        jtag.scan_ir(FU3_DBG_IR_CMD_DATA);
        jtag.scan_dr(FU3_DBG_DATA_READ);
        return static_cast<std::uint32_t>(jtag.scan_dr(0));
    }
    std::uint32_t status(std::uint32_t clear = 0) {
        jtag.scan_ir(FU3_DBG_IR_CMD_STATUS);
        return static_cast<std::uint32_t>(jtag.scan_dr(clear));
    }

    // Mọi truy cập DM đi qua JTAG2AXI4 vào cửa sổ 0x1A11_0000
    std::uint32_t dmi_rd(std::uint8_t a) {
        const std::uint32_t v = axi_read(FU3_DBG_DM_BASE + FU3_DBG_DM_OFFSET(a));
        check(status() == 0, "DM read: no bridge error");
        return v;
    }
    void dmi_wr(std::uint8_t a, std::uint32_t v) {
        axi_write(FU3_DBG_DM_BASE + FU3_DBG_DM_OFFSET(a), v);
    }
    void select(unsigned hart, std::uint32_t extra = 0) {
        dmi_wr(FU3_DBG_DMCONTROL, FU3_DBG_DMCONTROL_DMACTIVE | FU3_DBG_DMCONTROL_HARTSEL(hart) | extra);
    }
    std::uint32_t cmderr() { 
        return (dmi_rd(FU3_DBG_ABSTRACTCS) >> 8) & 7; 
    }
    void clear_cmderr() { 
        dmi_wr(FU3_DBG_ABSTRACTCS, FU3_DBG_ABSTRACTCS_CMDERR_MASK); 
    }
    void command(std::uint32_t cmd) { 
        dmi_wr(FU3_DBG_COMMAND, cmd); 
    }
    std::uint32_t read_reg(std::uint16_t regno) {
        command(FU3_DBG_CMD_AARSIZE_32 | FU3_DBG_CMD_TRANSFER | regno);
        return dmi_rd(FU3_DBG_DATA0);
    }
    void write_reg(std::uint16_t regno, std::uint32_t v) {
        dmi_wr(FU3_DBG_DATA0, v);
        command(FU3_DBG_CMD_AARSIZE_32 | FU3_DBG_CMD_TRANSFER | FU3_DBG_CMD_WRITE | regno);
    }

    void test_jtag() {
        jtag.test_logic_reset();
        check(jtag.dr_length() == 32, "Test-Logic-Reset selects CMD_IDCODE (32-bit DR)");
        const std::uint64_t id = jtag.scan_dr(0);
        check(id == FU3_DBG_IDCODE && (id & 1), "IDCODE value, bit 0 = 1");
        jtag.scan_ir(FU3_DBG_IR_CMD_ADDR);
        check(jtag.dr_length() == 32, "CMD_ADDR DR 32 bits");
        jtag.scan_ir(FU3_DBG_IR_CMD_DATA);
        check(jtag.dr_length() == 34, "CMD_DATA DR 34 bits");
        jtag.scan_ir(FU3_DBG_IR_CMD_STATUS);
        check(jtag.dr_length() == 32, "CMD_STATUS DR 32 bits");
        check(jtag.scan_dr(0) == 0, "STATUS 0 after reset");
        for (std::uint8_t ir : {std::uint8_t(FU3_DBG_IR_CMD_BYPASS), std::uint8_t(0x00), std::uint8_t(0x10)}) {
            jtag.scan_ir(ir);
            check(jtag.dr_length() == 1 && jtag.scan_dr(1) == 0, "IR " + std::to_string(ir) + " is BYPASS, captures 0");
        }
        jtag.scan_ir(FU3_DBG_IR_CMD_ADDR);
        jtag.scan_dr(0x1234'5678);
        check(jtag.scan_dr(0) == 0x1234'5678, "CMD_ADDR captures the loaded address");
        jtag.test_logic_reset();
        jtag.scan_ir(FU3_DBG_IR_CMD_ADDR);
        check(jtag.scan_dr(0) == 0, "Test-Logic-Reset clears the address");
    }

    void test_system_access() {
        axi_write(kDsram + 0x10, 0xDEAD'BEEFu);
        check(memory.word(0x10) == 0xDEAD'BEEFu, "Write to DSRAM through JTAG2AXI4");
        check(axi_read(kDsram + 0x10) == 0xDEAD'BEEFu, "Read back");
        check(status() == 0, "No error");

        // Address tăng 4 sau mỗi truy cập: ghi/đọc liên tiếp
        set_address(kDsram + 0x20);
        jtag.scan_ir(FU3_DBG_IR_CMD_DATA);
        for (std::uint32_t i = 0; i < 3; ++i) 
            jtag.scan_dr(FU3_DBG_DATA_WRITE | (0x100 + i));
        check(memory.word(0x20) == 0x100 && memory.word(0x24) == 0x101 && memory.word(0x28) == 0x102, "Address auto-increments: block write");
        jtag.scan_ir(FU3_DBG_IR_CMD_ADDR);
        check(jtag.scan_dr(kDsram + 0x20) == kDsram + 0x2C, "Address advanced by 4 per access");
        jtag.scan_ir(FU3_DBG_IR_CMD_DATA);
        jtag.scan_dr(FU3_DBG_DATA_READ);
        check(static_cast<std::uint32_t>(jtag.scan_dr(FU3_DBG_DATA_READ)) == 0x100, "Block read word 0");
        check(static_cast<std::uint32_t>(jtag.scan_dr(FU3_DBG_DATA_READ)) == 0x101, "Block read word 1");
        check(static_cast<std::uint32_t>(jtag.scan_dr(0)) == 0x102, "Block read word 2, no access");
        jtag.scan_ir(FU3_DBG_IR_CMD_ADDR);
        check(jtag.scan_dr(0) == kDsram + 0x2C, "A scan without read/write does not access");

        // Lỗi: sticky trong STATUS, ghi 1 để xoá
        axi_write(kDsram + 0x42, 0x55);
        check(status() == FU3_DBG_STATUS_ALIGN, "Misaligned address -> ALIGN, no access");
        check(memory.word(0x40) == 0, "Misaligned write stores nothing");
        status(FU3_DBG_STATUS_ALIGN);
        check(status() == 0, "STATUS W1C clears");
        axi_read(0x0200'0000u);
        check(status() == FU3_DBG_STATUS_DECERR, "Unmapped address -> DECERR");
        status(FU3_DBG_STATUS_DECERR);
        axi_write(kDsram + 0xFFF0, 1);
        check(status() == FU3_DBG_STATUS_SLVERR, "Slave error -> SLVERR");
        status(FU3_DBG_STATUS_SLVERR);
#ifdef FU3_DBG_WITH_BUS
        axi_read(0x0000'0000u);
        check(status() == FU3_DBG_STATUS_SLVERR, "Boot ROM is Ibex-only on the bus -> SLVERR");
        status(FU3_DBG_STATUS_SLVERR);
#endif
        set_address(FU3_DBG_DM_BASE + 0x800);
        jtag.scan_ir(FU3_DBG_IR_CMD_DATA);
        jtag.scan_dr(FU3_DBG_DATA_WRITE | 0xFFFF'FFFFu);
        jtag.scan_dr(FU3_DBG_DATA_READ);
        check(jtag.scan_dr(0) == 0 && status() == 0, "DM window above the DMI registers reads 0");
    }

    void test_activation() {
        check(dmi_rd(FU3_DBG_DMCONTROL) == 0, "Power-on: DMCONTROL 0, DM inactive");
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
        check(dmi_rd(FU3_DBG_DATA0) == 0x77, "Command ignored while cmderr != 0");
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
        select(1);   // Hart 1 đang chạy
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
        check(dmi_rd(FU3_DBG_SBCS) == 0, "SBCS reads 0: no System Bus Access in the DM");
    }

    void run() {
        test_jtag();
        test_system_access();
        test_activation();
        test_havereset();
        test_halt_resume();
        test_abstract();
        test_hartsel();
        test_ndmreset();
        test_deactivate();
        sc_core::sc_stop();
    }
};
