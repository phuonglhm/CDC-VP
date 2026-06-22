//Author: trangnm20
#ifndef CLKMGR_H
#define CLKMGR_H

#include <array>
#include <cstdint>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

namespace mubi4 {
    static const uint8_t True  = 0x6;
    static const uint8_t False = 0x9;

    inline bool test_true_strict(uint8_t val) {
        return val == True;
    }
}

class Clkmgr : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<Clkmgr> socket;
    sc_core::sc_vector<sc_core::sc_in<bool>> idle_i;
    sc_core::sc_out<bool> io_clk_byp_req_o;
    sc_core::sc_in<bool> io_clk_byp_ack_i;

    enum class LcState {
        Prod,
        Test,
        Dev,
        Rma,
    };

    SC_HAS_PROCESS(Clkmgr);
    explicit Clkmgr(sc_core::sc_module_name name);

    void b_transport(tlm::tlm_generic_payload& trans,
                     sc_core::sc_time& delay);

    void set_lc_state(LcState state) {
        lc_state_ = state;
    }

private:
    enum RegOffset : sc_dt::uint64 {
        REG_EXTCLK_CTRL        = 0x8,
        REG_EXTCLK_STATUS      = 0xc,
        REG_CLK_ENABLES        = 0x18,
        REG_CLK_HINTS          = 0x1c,
        REG_CLK_HINTS_STATUS   = 0x20,
        REG_EXTCLK_CTRL_REGWEN = 0x4,
    };

    // Default reset state is production mode
    LcState lc_state_ = LcState::Prod;

    uint8_t extclk_ctrl_sel_      = mubi4::False;
    uint8_t extclk_ctrl_hispeed_  = mubi4::False;
    uint8_t extclk_status_ack_    = mubi4::False;
    uint8_t extclk_ctrl_regwen_   = 0x1;

    uint8_t clk_enables_      = 0xf;
    uint8_t clk_hints_        = 0xf;
    uint8_t clk_hints_status_ = 0xf;

    bool lc_debug_enabled() const {
        return lc_state_ == LcState::Test ||
               lc_state_ == LcState::Dev  ||
               lc_state_ == LcState::Rma;
    }

    uint32_t read_reg(sc_dt::uint64 addr);
    void write_reg(sc_dt::uint64 addr, uint32_t data);

    void handle_extclk_ctrl_write(uint32_t data);
    void handle_clk_hints_write(uint32_t data);
    void on_ast_ack_change();
    void idle_qualifier_thread();
    void recompute_hints_status();

    std::array<unsigned, 4> idle_high_counts_ = {};
    std::array<bool, 4> qualified_idle_ = {};
};

#endif
