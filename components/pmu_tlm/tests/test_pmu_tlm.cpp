#include "pmu.h"
#include <tlm_utils/simple_initiator_socket.h>
#include <iostream>

using namespace sc_core;
using namespace pwrmgr_reg;

namespace cdc::components {
    class Testbench : public sc_module {
    public:
        bool ok = false; // set true at end of run() if all checks passed
        tlm_utils::simple_initiator_socket<Testbench> tl_socket;

        sc_signal<bool> por_rst_n;
        sc_signal<bool> core_sleeping;
        sc_signal<bool> otp_done;
        sc_signal<bool> lc_done;
        sc_signal<bool> rom_done;
        sc_signal<bool> rom_good;
        sc_signal<bool> flash_idle;
        sc_signal<bool> lc_test_state;
        sc_signal<bool> main_pok;

        sc_signal<sc_dt::sc_bv<NUM_WAKEUPS> > wakeups;
        sc_signal<sc_dt::sc_bv<NUM_RESET_REQS> > rstreqs;
        sc_signal<bool> ndmreset_req;
        sc_signal<bool> sw_rst_req;
        sc_signal<bool> esc_rx;
        sc_signal<bool> esc_clk_alive;

        sc_signal<bool> ast_main_pd_n;
        sc_signal<bool> rst_lc_n;
        sc_signal<bool> clk_en_2nd;
        sc_signal<bool> fetch_en;
        sc_signal<bool> strap_o;
        sc_signal<bool> low_power_o;
        sc_signal<bool> sys_rst_n;
        sc_signal<bool> wakeup_irq;

        SC_HAS_PROCESS(Testbench);
        explicit Testbench(const sc_module_name& name)
            : sc_module(name),
            tl_socket("tl_socket"),
            por_rst_n("por_rst_n"),
            core_sleeping("core_sleeping"),
            otp_done("otp_done"),
            lc_done("lc_done"),
            rom_done("rom_done"),
            rom_good("rom_good"),
            flash_idle("flash_idle"),
            lc_test_state("lc_test_state"),
            main_pok("main_pok"),
            wakeups("wakeups"),
            rstreqs("rstreqs"),
            ndmreset_req("ndmreset_req"),
            sw_rst_req("sw_rst_req"),
            esc_rx("esc_rx"),
            esc_clk_alive("esc_clk_alive"),
            ast_main_pd_n("ast_main_pd_n"),
            rst_lc_n("rst_lc_n"),
            clk_en_2nd("clk_en_2nd"),
            fetch_en("fetch_en"),
            strap_o("strap_o"),
            low_power_o("low_power_o"),
            sys_rst_n("sys_rst_n"),
            wakeup_irq("wakeup_irq")
        {
            SC_THREAD(run);
        }

        void write_reg(uint64_t addr, uint32_t data)
        {
            tlm::tlm_generic_payload trans;
            sc_time delay = SC_ZERO_TIME;
            unsigned char buf[4];
            std::memcpy(buf, &data, 4);

            trans.set_command(tlm::TLM_WRITE_COMMAND);
            trans.set_address(addr);
            trans.set_data_ptr(buf);
            trans.set_data_length(4);
            trans.set_streaming_width(4);
            trans.set_byte_enable_ptr(nullptr);
            trans.set_dmi_allowed(false);
            trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

            tl_socket->b_transport(trans, delay);

            if (trans.is_response_error()) {
                std::cout << "write_reg ERROR at addr 0x" << std::hex << addr << std::dec << std::endl;
            }
        }

        uint32_t read_reg(uint64_t addr)
        {
            tlm::tlm_generic_payload trans;
            sc_time delay = SC_ZERO_TIME;
            unsigned char buf[4] = {0, 0, 0, 0};

            trans.set_command(tlm::TLM_READ_COMMAND);
            trans.set_address(addr);
            trans.set_data_ptr(buf);
            trans.set_data_length(4);
            trans.set_streaming_width(4);
            trans.set_byte_enable_ptr(nullptr);
            trans.set_dmi_allowed(false);
            trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

            tl_socket->b_transport(trans, delay);

            if (trans.is_response_error()) {
                std::cout << "read_reg ERROR at addr 0x" << std::hex << addr << std::dec << std::endl;
            }

            uint32_t val = 0;
            std::memcpy(&val, buf, 4);
            return val;
        }

        void run()
        {
            por_rst_n.write(false);
            core_sleeping.write(false);
            otp_done.write(false);
            lc_done.write(false);
            rom_done.write(false);
            rom_good.write(true);
            flash_idle.write(true);
            lc_test_state.write(true);
            main_pok.write(true);
            wakeups.write(0);
            rstreqs.write(0);
            ndmreset_req.write(false);
            sw_rst_req.write(false);
            esc_rx.write(false);
            esc_clk_alive.write(true);

            wait(20, SC_NS);
            por_rst_n.write(true);

            wait(50, SC_NS);
            otp_done.write(true);
            wait(10, SC_NS);
            lc_done.write(true);
            wait(40, SC_NS);
            rom_done.write(true);

            wait(200, SC_NS);

            uint32_t ctrl_before = read_reg(CONTROL);
            std::cout << "CONTROL before = 0x" << std::hex << ctrl_before << std::dec << std::endl;

            write_reg(WAKEUP_EN, 0x1);
            uint32_t wakeup_en = read_reg(WAKEUP_EN);
            std::cout << "WAKEUP_EN after write = 0x" << std::hex << wakeup_en << std::dec << std::endl;

            bool pass = true;

            if (fetch_en.read() != true) {
                std::cout << "FAIL: fetch_en expected true after ROM check" << std::endl;
                pass = false;
            }

            if (wakeup_en != 0x1) {
                std::cout << "FAIL: WAKEUP_EN expected 0x1, got 0x" << std::hex << wakeup_en << std::dec << std::endl;
                pass = false;
            }

            if (ctrl_before != CONTROL_RESET_DEFAULT) {
                std::cout << "FAIL: CONTROL expected reset default 0x" << std::hex
                        << CONTROL_RESET_DEFAULT << ", got 0x" << ctrl_before << std::dec << std::endl;
                pass = false;
            }

            ok = pass;
            if (pass) {
                std::cout << "TEST PASSED" << std::endl;
            } else {
                std::cout << "TEST FAILED" << std::endl;
            }

            sc_stop();
        }
    };
}

int sc_main(int argc, char* argv[])
{
    cdc::components::Pwrmgr pwrmgr("pwrmgr");
    cdc::components::Testbench tb("tb");

    tb.tl_socket.bind(pwrmgr.tl_socket);

    pwrmgr.por_rst_n(tb.por_rst_n);
    pwrmgr.core_sleeping(tb.core_sleeping);
    pwrmgr.otp_done(tb.otp_done);
    pwrmgr.lc_done(tb.lc_done);
    pwrmgr.rom_done(tb.rom_done);
    pwrmgr.rom_good(tb.rom_good);
    pwrmgr.flash_idle(tb.flash_idle);
    pwrmgr.lc_test_state(tb.lc_test_state);
    pwrmgr.main_pok(tb.main_pok);
    pwrmgr.wakeups(tb.wakeups);
    pwrmgr.rstreqs(tb.rstreqs);
    pwrmgr.ndmreset_req(tb.ndmreset_req);
    pwrmgr.sw_rst_req(tb.sw_rst_req);
    pwrmgr.esc_rx(tb.esc_rx);
    pwrmgr.esc_clk_alive(tb.esc_clk_alive);

    pwrmgr.ast_main_pd_n(tb.ast_main_pd_n);
    pwrmgr.rst_lc_n(tb.rst_lc_n);
    pwrmgr.clk_en_2nd(tb.clk_en_2nd);
    pwrmgr.fetch_en(tb.fetch_en);
    pwrmgr.strap_o(tb.strap_o);
    pwrmgr.low_power_o(tb.low_power_o);
    pwrmgr.sys_rst_n(tb.sys_rst_n);
    pwrmgr.wakeup_irq(tb.wakeup_irq);

    sc_start();

    return tb.ok ? 0 : 1;
}