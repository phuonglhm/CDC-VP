#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <systemc>

#include "fu3_iopad_map.h"

namespace cdc::platforms::fu3_soc {

// FU3 pad multiplexing per Common v2.1 IOPAD MAP (decision Q06-a: die pads
// 1-108 are the identifiers; table in fu3_iopad_map.h). Each die pad carries
// the chip function csr_io_cfg[1:0] selects (or its fixed function) and the
// board wiring attached to that die.
//
// Chip side, by IOPAD MAP function name: an output gives data and output
// enable (open drain: the pad is pulled low while enabled); an input receives
// the level of the die pad that selects the function, or its idle level when
// no pad does (the lowest die wins if several do). Functions the platform
// models at transaction level (JTAG, QSPI, HyperBus) or does not have yet
// (PWM, CAN) are not bound: their pads are not driven by the chip.
//
// Board side, by die: a static level, a signal (peer model), a wire from
// another die (loopback) and a pull. Pad level: the chip's push-pull output
// when enabled; else low if an open-drain output pulls it low; else the board
// driver; else the pull (none: low). A board driver opposing an enabled chip
// output is counted as contention and reported once per die.
//
// SYSCSR csr_io_cfg is not connected (decision Q06-b: field -> die mapping
// pending): selects are set by the host (set_select), reset value b00. The
// test-mode columns are empty in v2.1, so test mode does not change routing.
// Die 7 (PIN_OSC_IN) repeats die 8's TDI/GPIOA_0 mux (decision Q06-d,
// pending): its mux is kept as metadata and not connected.
class fu3_pinmux : public sc_core::sc_module {
public:
    static constexpr unsigned kDiePads = iopad::kDiePads;
    using level_fn = std::function<bool()>;
    using sink_fn = std::function<void(bool)>;

    explicit fu3_pinmux(sc_core::sc_module_name name);

    // Chip side (elaboration only). `events` wake the routing when data/oe change.
    void chip_output(const std::string& function, level_fn data, level_fn oe, std::vector<const sc_core::sc_event*> events,
                     bool open_drain = false);
    void chip_input(const std::string& function, sink_fn sink, bool idle);
    // Called after every routing pass (writers of vector signals such as GPIO inputs).
    void after_inputs(std::function<void()> flush) { flushes_.push_back(std::move(flush)); }

    // Board side.
    void board_level(unsigned die, bool level);
    // A board peer's output (registered during elaboration; `attached`: drives now).
    void board_signal(unsigned die, const sc_core::sc_signal_in_if<bool>& source, bool attached = true);
    void board_attach(unsigned die, bool on);   // that peer drives the die or not
    void board_release(unsigned die);
    // Board wire: `to` is driven with the level of `from` (replaces its driver while on).
    void board_link(unsigned from, unsigned to, bool on);
    enum class pull { none, up, down };
    void board_pull(unsigned die, pull p);
    void board_observe(unsigned die, sc_core::sc_signal<bool>& level);   // elaboration only

    // csr_io_cfg[1:0] of a die pad (0-3; ignored on a pad without mux).
    void set_select(unsigned die, unsigned select);
    unsigned select(unsigned die) const { return select_.at(die - 1); }
    bool level(unsigned die) const { return level_.at(die - 1); }
    // Function the die pad carries now (nullptr: none).
    const char* function(unsigned die) const;
    std::uint64_t contentions() const { return contentions_; }

private:
    struct output {
        level_fn data, oe;
        bool open_drain = false;
    };
    struct input {
        sink_fn sink;
        bool idle = false;
    };
    enum class drive { none, level, signal, link };
    struct board {
        drive kind = drive::none, saved_kind = drive::none;
        bool value = false, saved_value = false;
        const sc_core::sc_signal_in_if<bool>* source = nullptr;
        unsigned from = 0, saved_from = 0;
        pull p = pull::none;
    };

    std::map<std::string, output> outputs_;
    std::map<std::string, input> inputs_;
    std::vector<std::function<void()>> flushes_;
    std::vector<const sc_core::sc_event*> events_;
    std::vector<std::pair<unsigned, sc_core::sc_signal<bool>*>> observers_;
    std::array<unsigned, kDiePads> select_{};
    std::array<bool, kDiePads> level_{}, reported_{};
    std::array<board, kDiePads> board_{};
    std::uint64_t contentions_ = 0;
    sc_core::sc_event changed_;

    static bool connected(unsigned die);
    void before_end_of_elaboration() override;
    void route();
};

} // namespace cdc::platforms::fu3_soc
