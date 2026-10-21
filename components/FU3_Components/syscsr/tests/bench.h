#pragma once
// Test reset value, mask rw/ro, byte lane/PSTRB, strap chốt lúc nhả reset, sys_ctrl -> CRM, reset_n và lỗi truy cập
// Dùng chung cho behavior.cpp (nối thẳng) và integration.cpp (qua bus FU3, định nghĩa FU3_SYSCSR_WITH_BUS).
#include <fu3_syscsr/syscsr.h>

#include <iostream>
#include <tlm_utils/simple_initiator_socket.h>

#ifdef FU3_SYSCSR_WITH_BUS
    #include <fu3_bus/access.h>
    #include <fu3_bus/bus_system.h>
#endif

using namespace fu3_syscsr;

struct Bench : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<Bench> cpu{"cpu"};
    Syscsr dut{"dut"};
    sc_core::sc_signal<bool> reset_n{"reset_n", true};
    sc_core::sc_signal<bool> boot{"boot"}, mode_0{"mode_0"}, mode_1{"mode_1"};
    sc_core::sc_signal<bool> test_mode{"test_mode"}, test_mode_sel{"test_mode_sel"};
    sc_core::sc_signal<bool> sw_reset_req{"sw_reset_req"}, sw_global_rst_req{"sw_global_rst_req"};
    std::uint64_t base = 0;
    unsigned checks = 0, errors = 0;

#ifdef FU3_SYSCSR_WITH_BUS
    fu3_bus::BusSystem* fabric = nullptr;
#endif

    SC_HAS_PROCESS(Bench);
    explicit Bench(sc_core::sc_module_name n, bool via_bus) : sc_module(n) {
        dut.reset_n(reset_n);
        dut.boot(boot); 
        dut.mode_0(mode_0); 
        dut.mode_1(mode_1);
        dut.test_mode(test_mode); 
        dut.test_mode_sel(test_mode_sel);
        dut.sw_reset_req(sw_reset_req); 
        dut.sw_global_rst_req(sw_global_rst_req);
        
#ifdef FU3_SYSCSR_WITH_BUS
        if (via_bus) {
            auto cfg = fu3_bus::BusConfig::fu3();

            for (auto& t : cfg.targets) 
                t.enabled = t.name == "SYSCSR";

            fabric = new fu3_bus::BusSystem("fabric", cfg);
            cpu.bind(fabric->initiator("IBEX"));
            fabric->target("SYSCSR").bind(dut.socket);
            base = 0x4401'0000;
        } else
#endif

        {
            (void)via_bus;
            cpu.bind(dut.socket);
        }
        SC_THREAD(run);
    }

    void check(bool ok, const char* what) {
        ++checks;
        if (!ok) { 
            ++errors; 
            std::cerr << "FAIL: " << what << '\n'; 
        }
    }

    tlm::tlm_response_status xfer(bool write, std::uint32_t offset, std::uint32_t& value,
                                  unsigned len = 4, const unsigned char* be = nullptr) {
        unsigned char data[4] = {};
        for (unsigned i = 0; i < len; ++i) data[i] = (value >> (8 * i)) & 0xFF;
        tlm::tlm_generic_payload tx;

        tx.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
        tx.set_address(base + offset);
        tx.set_data_ptr(data);
        tx.set_data_length(len);
        tx.set_streaming_width(len);

        if (be) { 
            tx.set_byte_enable_ptr(const_cast<unsigned char*>(be)); 
            tx.set_byte_enable_length(len); 
        }

        tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

#ifdef FU3_SYSCSR_WITH_BUS
        fu3_bus::ScopedAccess access(tx, len);
#endif

        cpu->b_transport(tx, delay);
        if (!write) {
            value = 0;
            for (unsigned i = 0; i < len; ++i) value |= std::uint32_t(data[i]) << (8 * i);
        }
        return tx.get_response_status();
    }
    std::uint32_t rd(std::uint32_t offset) {
        std::uint32_t v = 0;
        check(xfer(false, offset, v) == tlm::TLM_OK_RESPONSE, "Read OK");
        return v;
    }
    void wr(std::uint32_t offset, std::uint32_t v) {
        check(xfer(true, offset, v) == tlm::TLM_OK_RESPONSE, "Write OK");
    }

    void run() {
        sc_core::wait(1, sc_core::SC_NS);
        struct { 
            std::uint32_t off, reset, wmask; 
        } exp[] = {
            // Đủ 51 thanh ghi, sinh từ FU3_SYSCSR_v1.xlsx (offset, reset, mask rw)
            {0x00, 0x000000, 0x00000000},   // version
            {0x10, 0x000000, 0xFFFFFFFF},   // left_io_cfg
            {0x14, 0x000000, 0xFFFFFFFF},   // bottom_io_cfg
            {0x18, 0x000000, 0xFFFFFFFF},   // right_io_cfg
            {0x1C, 0x000000, 0xFFFFFFFF},   // top_io_cfg
            {0x20, 0xF86C3F, 0x00FFFFFF},   // left_pad_ctrl_0
            {0x24, 0xFFFFFF, 0x00FFFFFF},   // left_pad_ctrl_1
            {0x28, 0x000000, 0x00FFFFFF},   // left_pad_ctrl_2
            {0x2C, 0x000000, 0x00FFFFFF},   // left_pad_ctrl_3
            {0x30, 0x000000, 0x00FFFFFF},   // left_pad_ctrl_4
            {0x34, 0xC5007F, 0x00FFFFFF},   // bottom_pad_ctrl_0
            {0x38, 0xFFFFFF, 0x00FFFFFF},   // bottom_pad_ctrl_1
            {0x3C, 0x000000, 0x00FFFFFF},   // bottom_pad_ctrl_2
            {0x40, 0x000000, 0x00FFFFFF},   // bottom_pad_ctrl_3
            {0x44, 0x000000, 0x00FFFFFF},   // bottom_pad_ctrl_4
            {0x48, 0xF80580, 0x00FFFFFF},   // right_pad_ctrl_0
            {0x4C, 0xFFFFFF, 0x00FFFFFF},   // right_pad_ctrl_1
            {0x50, 0x000000, 0x00FFFFFF},   // right_pad_ctrl_2
            {0x54, 0x000000, 0x00FFFFFF},   // right_pad_ctrl_3
            {0x58, 0x000000, 0x00FFFFFF},   // right_pad_ctrl_4
            {0x5C, 0xE16E0F, 0x00FFFFFF},   // top_pad_ctrl_0
            {0x60, 0xFFFFFF, 0x00FFFFFF},   // top_pad_ctrl_1
            {0x64, 0x000000, 0x00FFFFFF},   // top_pad_ctrl_2
            {0x68, 0x000000, 0x00FFFFFF},   // top_pad_ctrl_3
            {0x6C, 0x000000, 0x00FFFFFF},   // top_pad_ctrl_4
            {0x70, 0x000000, 0x00000000},   // syscsr_status
            {0x74, 0x000000, 0x00001111},   // syscsr_sys_ctrl
            {0x78, 0x000000, 0x00000000},   // syscsr_cpu_ctrl
            {0x7C, 0x000000, 0x00000000},   // syscsr_cpu_systick
            {0x80, 0x000013, 0x0000003F},   // osc_pad_ctrl
            {0x88, 0x000000, 0x00000001},   // rom_ctrl
            {0x8C, 0xF8041F, 0x00FFFFFF},   // left_pad_ctrl_5
            {0x90, 0x000000, 0x00FFFFFF},   // left_pad_ctrl_6
            {0x94, 0x000000, 0x00FFFFFF},   // left_pad_ctrl_7
            {0x98, 0xC7007F, 0x00FFFFFF},   // bottom_pad_ctrl_5
            {0x9C, 0x000000, 0x00FFFFFF},   // bottom_pad_ctrl_6
            {0xA0, 0x000000, 0x00FFFFFF},   // bottom_pad_ctrl_7
            {0xA4, 0xF80180, 0x00FFFFFF},   // right_pad_ctrl_5
            {0xA8, 0x000000, 0x00FFFFFF},   // right_pad_ctrl_6
            {0xAC, 0x000000, 0x00FFFFFF},   // right_pad_ctrl_7
            {0xB0, 0xE1E007, 0x00FFFFFF},   // top_pad_ctrl_5
            {0xB4, 0x000000, 0x00FFFFFF},   // top_pad_ctrl_6
            {0xB8, 0x000000, 0x00FFFFFF},   // top_pad_ctrl_7
            {0xBC, 0x000000, 0x0000FFFF},   // left_io_cfg_ext
            {0xC0, 0x000000, 0x0000FFFF},   // bottom_io_cfg_ext
            {0xC4, 0x000000, 0x0000FFFF},   // right_io_cfg_ext
            {0xC8, 0x000000, 0x0000FFFF},   // top_io_cfg_ext
            {0xCC, 0x07FBE0, 0x00FFFFFF},   // left_pad_ctrl_8
            {0xD0, 0x38FF80, 0x00FFFFFF},   // bottom_pad_ctrl_8
            {0xD4, 0x07FE7F, 0x00FFFFFF},   // right_pad_ctrl_8
            {0xD8, 0x1E1FF8, 0x00FFFFFF},   // top_pad_ctrl_8
        };

        // Reset value
        for (auto& e : exp) 
            check(rd(e.off) == e.reset, "Reset Value");

        // Mask rw/ro: ghi full 1 rồi full 0
        for (auto& e : exp) {
            if (e.off == 0x74) continue;   // sys_ctrl kiểm tra riêng (kéo reset CRM)
            wr(e.off, ~0u);
            check(rd(e.off) == ((e.reset & ~e.wmask) | e.wmask), "Write ones");
            wr(e.off, 0);
            check(rd(e.off) == (e.reset & ~e.wmask), "Write zeros");
        }

        // Byte/halfword lane và PSTRB
        wr(0x10, 0);
        std::uint32_t v = 0xAB;
        check(xfer(true, 0x12, v, 1) == tlm::TLM_OK_RESPONSE, "Byte Write");
        check(rd(0x10) == 0x00AB0000, "Byte Lane 2");

        v = 0xBEEF;
        check(xfer(true, 0x10, v, 2) == tlm::TLM_OK_RESPONSE, "Half Write");
        check(rd(0x10) == 0x00ABBEEF, "Half Lane 0");

        v = 0;
        check(xfer(false, 0x12, v, 2) == tlm::TLM_OK_RESPONSE && v == 0x00AB, "Half Read Lane 2");
        const unsigned char strb[4] = {0xFF, 0x00, 0x00, 0xFF};

        v = 0x11223344;
        check(xfer(true, 0x10, v, 4, strb) == tlm::TLM_OK_RESPONSE, "PSTRB Write");
        check(rd(0x10) == 0x11ABBE44, "PSTRB Lanes 0,3");

        // syscsr_status: strap chốt lúc nhả reset, test_mode live
        check(rd(0x70) == 0, "Status 0");

        boot = true;
        mode_1 = true;
        test_mode_sel = true;
        sc_core::wait(1, sc_core::SC_NS);
        check(rd(0x70) == 0x10, "Straps held after reset release, test_mode_sel live");

        reset_n = false;
        sc_core::wait(1, sc_core::SC_NS);
        check(rd(0x70) == 0x15, "Straps follow the pins while reset_n is low");
        mode_0 = true;
        sc_core::wait(1, sc_core::SC_NS);
        reset_n = true;
        sc_core::wait(1, sc_core::SC_NS);
        check(rd(0x70) == 0x17, "Straps captured at reset release");
        boot = false;
        mode_0 = false;
        mode_1 = false;
        test_mode = true;
        sc_core::wait(1, sc_core::SC_NS);
        check(rd(0x70) == 0x1F, "Captured straps ignore later pin changes, test_mode live");
        test_mode = false;
        sc_core::wait(1, sc_core::SC_NS);

        wr(0x70, 0);
        check(rd(0x70) == 0x17, "Status RO");

        // sys_ctrl -> CRM
        wr(0x74, ~0u);
        sc_core::wait(1, sc_core::SC_NS);
        check(rd(0x74) == 0x1111, "SYS_CTRL Mask");
        check(sw_reset_req.read() && sw_global_rst_req.read(), "Reset REQ High");

        wr(0x74, kJtagmuxSel);
        sc_core::wait(1, sc_core::SC_NS);
        check(!sw_reset_req.read() && !sw_global_rst_req.read(), "Reset REQ Low");

        // reset_n
        wr(0x74, kSwResetReq);
        wr(0x20, 0);
        reset_n = false;
        sc_core::wait(1, sc_core::SC_NS);
        sc_core::wait(1, sc_core::SC_NS);

        check(rd(0x20) == 0xF86C3F && rd(0x74) == 0, "Reset Restores");
        check(!sw_reset_req.read(), "Reset Clears REQ");
        
        wr(0x10, 0x5A);
        check(rd(0x10) == 0, "Write Ignored in Reset");

        reset_n = true;
        sc_core::wait(1, sc_core::SC_NS);

        // Lỗi truy cập
        v = 0;
        check(rd(0x84) == 0, "Unmapped RAZ");
        wr(0x84, ~0u);
        check(rd(0x84) == 0, "Unmapped write ignored");
        if (base == 0) {   // Bus FU3 tự chặn các lỗi này trước khi tới SYSCSR
            check(xfer(false, 0x11, v, 2) == tlm::TLM_ADDRESS_ERROR_RESPONSE, "Misaligned Half");
            check(xfer(false, 0x10, v, 3) == tlm::TLM_BURST_ERROR_RESPONSE, "Length: 3");
            check(xfer(false, 0x10000, v) == tlm::TLM_ADDRESS_ERROR_RESPONSE, "Outside 64 KiB");
        }
        sc_core::sc_stop();
    }
};
