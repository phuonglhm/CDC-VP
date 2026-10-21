#pragma once
#include <tlm>

namespace fu3::qspi {
// Optional AHB control for a single TLM word. Without it, use selected NONSEQ.
// A fabric/AXI bridge attaches SEQ explicitly; address adjacency alone is not SEQ.
struct AhbTransaction : tlm::tlm_extension<AhbTransaction> {
    enum Transfer { Idle=0, Busy=1, Nonseq=2, Seq=3 };
    Transfer transfer=Nonseq;
    bool selected=true;
    tlm_extension_base* clone() const override { return new AhbTransaction(*this); }
    void copy_from(const tlm_extension_base& e) override { *this=static_cast<const AhbTransaction&>(e); }
};
}
