#define SC_INCLUDE_DYNAMIC_PROCESSES
#include "fu3_pinmux.h"

#include <stdexcept>

namespace cdc::platforms::fu3_soc {

namespace {

const iopad::die_pad& pad(unsigned die)
{
    if (die < 1 || die > iopad::kDiePads)
        throw std::out_of_range("fu3_pinmux: die pad " + std::to_string(die) + " outside 1-108");
    return iopad::kDiePad[die - 1];
}

bool has_mux(const iopad::die_pad& p)
{
    for (const char* m : p.mux)
        if (m)
            return true;
    return false;
}

} // namespace

fu3_pinmux::fu3_pinmux(sc_core::sc_module_name name) : sc_core::sc_module(name)
{
    events_.push_back(&changed_);
}

bool fu3_pinmux::connected(unsigned die)
{
    // Q06-d: die 7 repeats die 8's mux; kept as metadata, not connected.
    return die != 7 && pad(die).type && std::string(pad(die).type) == "DIG";
}

const char* fu3_pinmux::function(unsigned die) const
{
    const auto& p = pad(die);
    if (!connected(die))
        return nullptr;
    return has_mux(p) ? p.mux[select_[die - 1]] : p.fixed;
}

void fu3_pinmux::chip_output(const std::string& function, level_fn data, level_fn oe,
                             std::vector<const sc_core::sc_event*> events, bool open_drain)
{
    outputs_[function] = {std::move(data), std::move(oe), open_drain};
    events_.insert(events_.end(), events.begin(), events.end());
}

void fu3_pinmux::chip_input(const std::string& function, sink_fn sink, bool idle)
{
    inputs_[function] = {std::move(sink), idle};
}

void fu3_pinmux::board_level(unsigned die, bool level)
{
    pad(die);
    auto& b = board_[die - 1];
    b.kind = drive::level;
    b.value = level;
    changed_.notify(sc_core::SC_ZERO_TIME);
}

void fu3_pinmux::board_signal(unsigned die, const sc_core::sc_signal_in_if<bool>& source, bool attached)
{
    pad(die);
    auto& b = board_[die - 1];
    b.source = &source;
    if (attached)
        b.kind = drive::signal;
    events_.push_back(&source.value_changed_event());
}

void fu3_pinmux::board_attach(unsigned die, bool on)
{
    auto& b = board_[pad(die).die - 1];
    if (!b.source)
        throw std::logic_error("fu3_pinmux: no board signal registered on die pad " + std::to_string(die));
    b.kind = on ? drive::signal : drive::none;
    changed_.notify(sc_core::SC_ZERO_TIME);
}

void fu3_pinmux::board_release(unsigned die)
{
    pad(die);
    board_[die - 1].kind = drive::none;
    changed_.notify(sc_core::SC_ZERO_TIME);
}

void fu3_pinmux::board_link(unsigned from, unsigned to, bool on)
{
    pad(from);
    auto& b = board_[pad(to).die - 1];
    if (on && b.kind != drive::link) {
        b.saved_kind = b.kind;
        b.saved_value = b.value;
        b.saved_from = b.from;
        b.kind = drive::link;
        b.from = from;
    } else if (!on && b.kind == drive::link) {
        b.kind = b.saved_kind;
        b.value = b.saved_value;
        b.from = b.saved_from;
    }
    changed_.notify(sc_core::SC_ZERO_TIME);
}

void fu3_pinmux::board_pull(unsigned die, pull p)
{
    pad(die);
    board_[die - 1].p = p;
    changed_.notify(sc_core::SC_ZERO_TIME);
}

void fu3_pinmux::board_observe(unsigned die, sc_core::sc_signal<bool>& level)
{
    pad(die);
    observers_.emplace_back(die, &level);
}

void fu3_pinmux::set_select(unsigned die, unsigned select)
{
    pad(die);
    select_[die - 1] = select & 3;
    changed_.notify(sc_core::SC_ZERO_TIME);
}

void fu3_pinmux::before_end_of_elaboration()
{
    sc_core::sc_spawn_options options;
    options.spawn_method();
    for (const auto* e : events_)
        options.set_sensitivity(e);
    sc_core::sc_spawn([this] { route(); }, "route", &options);
}

void fu3_pinmux::route()
{
    // Chip drive of every die (push-pull level, open-drain low).
    std::array<signed char, kDiePads> chip{};   // -1 none, 0/1 push-pull level, 2 open-drain low
    chip.fill(-1);
    for (unsigned die = 1; die <= kDiePads; ++die) {
        const char* fn = function(die);
        if (!fn)
            continue;
        const auto it = outputs_.find(fn);
        if (it == outputs_.end() || !it->second.oe())
            continue;
        chip[die - 1] = it->second.open_drain ? 2 : it->second.data();
    }
    auto board_value = [&](unsigned die, auto&& self, unsigned depth) -> int {
        const auto& b = board_[die - 1];
        switch (b.kind) {
        case drive::level:
            return b.value;
        case drive::signal:
            return b.source->read();
        case drive::link:
            if (depth > 2)
                return -1;
            if (chip[b.from - 1] == 0 || chip[b.from - 1] == 1)
                return chip[b.from - 1];
            if (chip[b.from - 1] == 2)
                return 0;
            {
                const int v = self(b.from, self, depth + 1);
                return v >= 0 ? v : board_[b.from - 1].p == pull::up;
            }
        case drive::none:
            break;
        }
        return -1;
    };
    auto contention = [this](unsigned die) {
        ++contentions_;
        if (!reported_[die - 1]) {
            reported_[die - 1] = true;
            SC_REPORT_WARNING("fu3_soc/pad_contention",
                              ("die pad " + std::to_string(die) + ": board drives against the chip output").c_str());
        }
    };
    for (unsigned die = 1; die <= kDiePads; ++die) {
        const int c = chip[die - 1];
        const int b = board_value(die, board_value, 0);
        bool v;
        if (c == 0 || c == 1) {
            v = c;   // the chip output wins against a board driver
            if (b >= 0 && b != c)
                contention(die);
        } else if (c == 2) {
            v = false;   // open drain pulling low; a board push-pull high opposes it
            if (b == 1)
                contention(die);
        } else if (b >= 0) {
            v = b;
        } else {
            v = board_[die - 1].p == pull::up;
        }
        level_[die - 1] = v;
    }
    // Chip inputs: the lowest die selecting the function, else idle.
    for (auto& [name, in] : inputs_) {
        bool v = in.idle;
        for (unsigned die = 1; die <= kDiePads; ++die) {
            const char* fn = function(die);
            if (fn && name == fn) {
                v = level_[die - 1];
                break;
            }
        }
        in.sink(v);
    }
    for (auto& f : flushes_)
        f();
    for (auto& [die, sig] : observers_)
        sig->write(level_[die - 1]);
}

} // namespace cdc::platforms::fu3_soc
