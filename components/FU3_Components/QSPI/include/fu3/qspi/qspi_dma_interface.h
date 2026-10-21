#pragma once
#include "registers.h"

namespace fu3::qspi {
class QspiDmaInterface {
public:
    struct Input {
        std::uint32_t cr=0,dmacr=0;
        unsigned mode=0,data_mode=0,rx_bytes=0,tx_free=0;
        bool rx_clear=false,tx_clear=false;
        std::uint64_t total=0,accepted=0;
    };
    void reset() { rx_=tx_=armed_=false; }
    void arm(bool value=true) { armed_=value; }
    bool rx_request() const { return rx_; }
    bool tx_request() const { return tx_; }
    void tick(const Input& in) {
        const unsigned rx_threshold=((in.cr>>3)&31)+1,tx_threshold=((in.cr>>8)&31)+1;
        // HDS 6.4.4 / DEC-029: request only a complete burst. A final partial
        // RX word is a valid, zero-padded word; a shorter TX residue uses CPU DR.
        const auto remaining=in.total>in.accepted ? in.total-in.accepted : 0;
        rx_=armed_ && !(in.cr&reg::ABORT) && in.mode==1 && (in.dmacr&reg::RXDMAEN) &&
            (in.rx_bytes+3)/4>=rx_threshold && !in.rx_clear;
        tx_=armed_ && !(in.cr&reg::ABORT) && in.mode==0 && (in.dmacr&reg::TXDMAEN) && in.data_mode &&
            in.tx_free/4>=tx_threshold && !in.tx_clear && (remaining+3)/4>=tx_threshold;
    }
private:
    bool rx_=false,tx_=false,armed_=false;
};
}
