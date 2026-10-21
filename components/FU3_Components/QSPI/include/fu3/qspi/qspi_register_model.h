#pragma once
#include "registers.h"
#include <stdexcept>

namespace fu3::qspi {
class QspiController;
// Register storage/access policy. FIFO and command side effects belong to the
// controller; hardware flags are only mutable by that controller.
class QspiRegisterModel {
public:
    void reset() { words_.fill(0); }
    std::uint32_t read(unsigned offset) const {
        check(offset);
        return offset==reg::FCR || offset==reg::DMAFCR ? 0 : words_[offset/4];
    }
    bool write(unsigned offset, std::uint32_t value, bool busy) {
        check(offset);
        if (offset==reg::SR || offset==reg::DMASR || offset==reg::DR) return false;
        if (offset==reg::FCR) { words_[reg::SR/4]&=~(value&0x3f); return false; }
        if (offset==reg::DMAFCR) { words_[reg::DMASR/4]&=~(value&0xc); return false; }
        if (offset==reg::CR) {
            const auto mask=busy ? (0x003f0000u|reg::EN|reg::ABORT) : reg::masks[0];
            const auto abort=words_[0]&reg::ABORT; // Writing zero cannot cancel an abort.
            words_[0]=(words_[0]&~mask)|(value&mask)|abort;
            return true;
        }
        if (offset!=reg::DMACR && (busy || (offset==reg::AR && mode()==3))) return false;
        words_[offset/4]=value&reg::masks[offset/4];
        return true;
    }
    unsigned mode() const { return (words_[reg::CCR/4]>>26)&3; }
    unsigned data_mode() const { return (words_[reg::CCR/4]>>24)&3; }
    std::uint64_t flash_size() const { return std::uint64_t{1}<<(((words_[reg::DCR/4]>>16)&31)+1); }
private:
    friend class QspiController;
    std::array<std::uint32_t,16> words_{};
    std::uint32_t& operator[](unsigned index) { return words_[index]; }
    std::uint32_t operator[](unsigned index) const { return words_[index]; }
    static void check(unsigned offset) {
        if ((offset&3) || offset>reg::DMAFCR) throw std::out_of_range("QSPI register offset");
    }
};
}
