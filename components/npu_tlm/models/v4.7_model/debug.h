// Toggleable debug/trace output for the unified model.
//
//   Default: OFF (clean, no console spam).
//   Enable:  compile with -DSAURIA_DEBUG=1   (e.g. `make DEBUG=1`)
//
// All module trace prints use DBG_COUT instead of std::cout. When OFF, DBG_COUT routes
// to a null sink that swallows everything (strings, ints, std::endl/std::hex/std::setw,
// sc_bv, ...) and is optimized away -> zero runtime cost and zero spam. Real program
// output (testbench reports) keeps using std::cout and is never gated.

#ifndef SAURIA_UNIFIED_DEBUG_H
#define SAURIA_UNIFIED_DEBUG_H

#include <iostream>

#ifndef SAURIA_DEBUG
#define SAURIA_DEBUG 0
#endif

namespace sauria
{
    struct NullSink
    {
        template <class T>
        NullSink &operator<<(const T &) { return *this; }
        // Stream manipulators that are function pointers:
        NullSink &operator<<(std::ostream &(*)(std::ostream &)) { return *this; } // std::endl, std::flush
        NullSink &operator<<(std::ios_base &(*)(std::ios_base &)) { return *this; } // std::hex, std::dec
    };
    inline NullSink &dbg_sink()
    {
        static NullSink s;
        return s;
    }
}

#if SAURIA_DEBUG
#define DBG_COUT std::cout
#else
#define DBG_COUT ::sauria::dbg_sink()
#endif

#endif // SAURIA_UNIFIED_DEBUG_H
