#pragma once

#include <cstdint>

#include <cva6_wrapper.h>
#include <fu3_dbg/dm.h>

namespace cdc::platforms::fu3_soc {

// The view the FU3 Debug Module (fu3_dbg) has of one CPU: debug_req_i, the
// halted state (Debug Mode), resume (a DRET, single step per dcsr.step) and
// abstract register access. Forwards to the CPU wrapper's debug control.
//
// It is also the arbiter between the Debug Module and the GDB stub (plan T19,
// decision Q04-d): both drive this one hart control, one owner at a time.
//   - The DM owns the hart from its halt request until the hart runs again
//     (or is in reset) with no request left; GDB cannot attach meanwhile.
//   - GDB owns the hart from attach to detach; meanwhile the DM sees the hart
//     unavailable (DMSTATUS), its halt requests and resumes do not reach the
//     hart and its register accesses fail (cmderr). Its last halt request
//     level applies again at detach.
class cpu_debug_hart : public fu3_dbg::DebugHart {
public:
    enum class owner { none, dm, gdb };

    explicit cpu_debug_hart(cdc::cpu::cva6_wrapper& cpu) : cpu_(cpu) {}

    // Debug Module side.
    void set_halt_request(bool request) override
    {
        dm_request_ = request;
        if (owner_ == owner::gdb)
            return;
        if (request)
            owner_ = owner::dm;
        cpu_.set_debug_request(request);
        release_dm_if_idle();
    }
    void resume() override
    {
        if (owner_ == owner::gdb)
            return;
        cpu_.debug_resume();
        release_dm_if_idle();
    }
    bool halted() const override { return owner_ != owner::gdb && cpu_.debug_halted(); }
    bool access_register(std::uint16_t regno, bool write, std::uint32_t& value) override
    {
        return owner_ != owner::gdb && cpu_.debug_access_register(regno, write, value);
    }
    // A hart held in reset (CRM, ndmreset, FCLK_SW for CVA6) is unavailable
    // (Q04-c), and so is a hart GDB owns.
    bool unavailable() const override { return owner_ == owner::gdb || cpu_.in_reset(); }

    // GDB side. Attach fails while the DM owns the hart.
    bool gdb_attach()
    {
        release_dm_if_idle();
        if (owner_ == owner::dm)
            return false;
        owner_ = owner::gdb;
        return true;
    }
    void gdb_detach()
    {
        if (owner_ != owner::gdb)
            return;
        owner_ = owner::none;
        reset_halt_ = false;
        cpu_.set_debug_request(false);
        cpu_.debug_resume();
        if (dm_request_)
            set_halt_request(true);
    }
    void gdb_halt_request(bool request)
    {
        if (owner_ == owner::gdb)
            cpu_.set_debug_request(request);
    }
    void gdb_resume()
    {
        if (owner_ == owner::gdb)
            cpu_.debug_resume();
    }
    bool gdb_halted() const { return owner_ == owner::gdb && cpu_.debug_halted(); }
    bool gdb_access_register(std::uint16_t regno, bool write, std::uint32_t& value)
    {
        return owner_ == owner::gdb && cpu_.debug_access_register(regno, write, value);
    }

    // The platform reports every reset of the hart. dcsr and the triggers are
    // back at their reset values: with GDB attached the hart halts on its
    // first instruction after the reset (the request survives reset) so that
    // the stub re-arms them (reset_halt_pending).
    void notify_reset()
    {
        ++resets_;
        release_dm_if_idle();
        if (owner_ == owner::gdb) {
            cpu_.set_debug_request(true);
            reset_halt_ = true;
        }
    }
    unsigned resets() const { return resets_; }
    bool reset_halt_pending() const { return reset_halt_; }
    void clear_reset_halt() { reset_halt_ = false; }

    owner current_owner() const { return owner_; }
    bool in_reset() const { return cpu_.in_reset(); }
    cdc::cpu::cva6_wrapper& cpu() { return cpu_; }

private:
    cdc::cpu::cva6_wrapper& cpu_;
    owner owner_ = owner::none;
    bool dm_request_ = false, reset_halt_ = false;
    unsigned resets_ = 0;

    void release_dm_if_idle()
    {
        if (owner_ == owner::dm && !dm_request_ && (!cpu_.debug_halted() || cpu_.in_reset()))
            owner_ = owner::none;
    }
};

} // namespace cdc::platforms::fu3_soc
