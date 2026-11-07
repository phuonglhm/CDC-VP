// neo_rev.h -- which commit of the neo_lite RTL the neo_lite knobs of the model follow (HAS_NEO_REV, read here and nowhere else).
//   0 = neo_lite RTL revision 0 (default), 1 = neo_lite RTL revision 1; a larger value is taken as the newest known revision.
// It acts only together with the neo_lite knob of the part concerned (for example HAS_GVU_REGS_NEO for the register file of the vector unit).
// The RTL ports (rtl_ref_*) do not read the environment: they take the revision as a constructor parameter, and the has:: code passes neo_rev().
#ifndef HAS_NEO_REV_H
#define HAS_NEO_REV_H

#include <cstdlib>

namespace has
{
    enum NeoRev : unsigned { NEO_REV_9B704BB = 0, NEO_REV_1FB7978 = 1, NEO_REV_MAX = 1 };

    inline unsigned neo_rev()
    {
        static const unsigned v = [] {
            const char *e = std::getenv("HAS_NEO_REV");
            const int r = e ? std::atoi(e) : 0;
            return r <= 0 ? static_cast<unsigned>(NEO_REV_9B704BB) : (r >= static_cast<int>(NEO_REV_MAX) ? static_cast<unsigned>(NEO_REV_MAX) : static_cast<unsigned>(r));
        }();
        return v;
    }
    inline bool neo_rev_at_least(unsigned r) { return neo_rev() >= r; }
}

#endif // HAS_NEO_REV_H
