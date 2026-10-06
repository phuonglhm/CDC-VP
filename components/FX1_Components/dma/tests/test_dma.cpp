#include "dma/dma.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <string>

using namespace sc_core;
using namespace fx1::dma;
using namespace fx1::dma::reg;

namespace {
void put(std::vector<unsigned char>& data, unsigned address, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) data.at(address+i) = static_cast<unsigned char>(value >> (8*i));
}
struct Record { bool read; std::uint64_t address; unsigned length; AxiExtension ext; sc_time at; };
class Memory : public sc_module {
public:
    tlm_utils::simple_target_socket<Memory> target{"target"};
    std::vector<unsigned char> data = std::vector<unsigned char>(0x20000, 0);
    std::vector<unsigned char> rx_data, tx_data;
    std::vector<Record> log;
    unsigned rx_cursor = 0, pending_read = 0, pending_write = 0, max_read = 0, max_write = 0;
    bool overlap = false, reorder = false, annotated = false;
    unsigned latency_ns = 12, write_latency_ns = 0;
    std::uint64_t error_address = ~std::uint64_t(0);
    tlm::tlm_response_status error = tlm::TLM_ADDRESS_ERROR_RESPONSE;
    SC_HAS_PROCESS(Memory);
    explicit Memory(sc_module_name name) : sc_module(name) {
        target.register_b_transport(this, &Memory::transport);
    }
    void transport(tlm::tlm_generic_payload& tx, sc_time& delay) {
        auto* ext = tx.get_extension<AxiExtension>();
        if (!ext) SC_REPORT_FATAL("test", "DMA did not attach AXI metadata");
        log.push_back({tx.is_read(), tx.get_address(), tx.get_data_length(), *ext, sc_time_stamp()});
        auto& pending = tx.is_read() ? pending_read : pending_write;
        auto& maximum = tx.is_read() ? max_read : max_write;
        ++pending; maximum = std::max(maximum, pending);
        overlap |= pending_read && pending_write;
        const unsigned latency = reorder && log.size() == 1 ? 40 :
            !tx.is_read() && write_latency_ns ? write_latency_ns : latency_ns;
        if (annotated) delay += sc_time(latency, SC_NS); else wait(latency, SC_NS);
        --pending;
        const auto address = tx.get_address();
        const unsigned bytes = tx.get_data_length();
        if (address == error_address) { tx.set_response_status(error); return; }
        if (address == 0x18000 && tx.is_write()) {
            tx_data.insert(tx_data.end(), tx.get_data_ptr(), tx.get_data_ptr()+bytes);
        } else if (address == 0x18010 && tx.is_read()) {
            if (bytes > rx_data.size() - rx_cursor) { tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE); return; }
            std::copy_n(rx_data.data() + rx_cursor, bytes, tx.get_data_ptr()); rx_cursor += bytes;
        } else if (address >= data.size() || bytes > data.size() - address) {
            tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE); return;
        } else if (tx.is_read()) std::copy_n(data.data()+address, bytes, tx.get_data_ptr());
        else std::copy_n(tx.get_data_ptr(), bytes, data.data()+address);
        tx.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

class Bench : public sc_module {
public:
    tlm_utils::simple_initiator_socket<Bench> registers{"registers"};
    sc_signal<bool> reset_n{"reset_n"}, irq{"irq"};
    sc_signal<std::uint32_t> rx_request{"rx_request"}, tx_request{"tx_request"};
    sc_signal<std::uint32_t> rx_clear{"rx_clear"}, tx_clear{"tx_clear"};
    unsigned errors = 0;
    SC_HAS_PROCESS(Bench);
    Bench(sc_module_name name, Memory& memory, std::string mode)
        : sc_module(name), memory_(memory), mode_(std::move(mode)) { SC_THREAD(run); }
private:
    Memory& memory_;
    std::string mode_;
    void check(bool condition, const std::string& message) {
        if (!condition) { ++errors; std::cerr << "FAIL @" << sc_time_stamp() << ": " << message << '\n'; }
    }
    tlm::tlm_response_status access(std::uint64_t address, bool write, std::uint32_t& value,
                                  unsigned length = 4, unsigned streaming = 4, bool enables = false,
                                  bool null_data = false, tlm::tlm_command command = tlm::TLM_IGNORE_COMMAND) {
        unsigned char data[8]{};
        for (unsigned i = 0; i < 4; ++i) data[i] = static_cast<unsigned char>(value >> (8*i));
        unsigned char enabled[4] = {255, 255, 255, 255};
        tlm::tlm_generic_payload tx;
        tx.set_command(command == tlm::TLM_IGNORE_COMMAND ? (write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND) : command);
        tx.set_address(address); tx.set_data_ptr(null_data ? nullptr : data);
        tx.set_data_length(length); tx.set_streaming_width(streaming);
        if (enables) { tx.set_byte_enable_ptr(enabled); tx.set_byte_enable_length(4); }
        sc_time delay = SC_ZERO_TIME;
        registers->b_transport(tx, delay);
        if (delay != SC_ZERO_TIME) wait(delay);
        value = std::uint32_t(data[0]) | (std::uint32_t(data[1]) << 8) |
                (std::uint32_t(data[2]) << 16) | (std::uint32_t(data[3]) << 24);
        return tx.get_response_status();
    }
    void wr(std::uint64_t address, std::uint32_t value) {
        check(access(address, true, value) == tlm::TLM_OK_RESPONSE, "register write " + std::to_string(address));
    }
    std::uint32_t rd(std::uint64_t address) {
        std::uint32_t value = 0;
        check(access(address, false, value) == tlm::TLM_OK_RESPONSE, "register read " + std::to_string(address));
        return value;
    }
    std::uint32_t cr(unsigned n, std::uint32_t offset) { return rd(channel(n, offset)); }
    void cw(unsigned n, std::uint32_t offset, std::uint32_t value) { wr(channel(n, offset), value); }
    void reset() {
        reset_n.write(false); wait(2, SC_NS); reset_n.write(true); wait(2, SC_NS);
    }
    void configure(unsigned n, unsigned src, unsigned dst, unsigned size,
                   std::uint32_t read = 0x81010008, std::uint32_t write = 0x81010008,
                   std::uint32_t peripheral = 0, std::uint32_t mode = 0, std::uint32_t control = 3) {
        cw(n, CH_ENABLE, 0); cw(n, CH_INTERRUPT_CLEAR, INTERRUPT_MASK);
        cw(n, CH_CMD_READ_ADDR, src); cw(n, CH_CMD_WRITE_ADDR, dst);
        cw(n, CH_CMD_TRANSFER_SIZE, size); cw(n, CH_CMD_CONTROL, control);
        cw(n, CH_READ_CONFIG, read); cw(n, CH_WRITE_CONFIG, write);
        cw(n, CH_PERIPHERAL_CONFIG, peripheral); cw(n, CH_MODE_CONFIG, mode);
        cw(n, CH_ENABLE, 1);
    }
    void start(unsigned n) { cw(n, CH_START, 1); }
    void done(unsigned n, unsigned limit = 2000) {
        wait(1, SC_NS);
        for (unsigned i = 0; i < limit; ++i) {
            if (!cr(n, CH_ACTIVE_STATUS) && !cr(n, CH_OUTSTANDING_STATUS)) return;
            wait(1, SC_NS);
        }
        check(false, "channel " + std::to_string(n) + " did not stop");
    }
    void pattern(unsigned start, unsigned size) {
        for (unsigned i = 0; i < size; ++i) memory_.data.at(start+i) = static_cast<unsigned char>(i*13 + 7);
    }
    void copied(unsigned source, unsigned dest, unsigned size) {
        check(std::equal(memory_.data.begin()+source, memory_.data.begin()+source+size,
                         memory_.data.begin()+dest), "destination bytes differ");
    }
    void registers_test() {
        const std::map<unsigned, std::uint32_t> reset_values = {
            {0,0},{4,0},{8,0},{12,0},{16,0x84010000},{20,0x84010000},{24,0},{28,0},{32,0},{36,0},
            {44,0},{48,0},{52,0},{56,32},{60,0},{64,1},{68,0},{72,0},{80,0},{160,0},{164,0},{168,0x1fff},{172,0}};
        for (unsigned n = 0; n < 8; ++n) {
            for (unsigned offset = 0; offset < 256; offset += 4) {
                std::uint32_t value = 0;
                const auto response = access(channel(n, offset), false, value);
                const auto found = reset_values.find(offset);
                if (found == reset_values.end()) {
                    check(response == tlm::TLM_ADDRESS_ERROR_RESPONSE, "channel reserved address read");
                    check(access(channel(n, offset), true, value) == tlm::TLM_ADDRESS_ERROR_RESPONSE, "channel reserved address write");
                } else check(response == tlm::TLM_OK_RESPONSE && value == found->second, "reset value at " + std::to_string(offset));
            }
        }
        const std::map<unsigned, std::uint32_t> globals = {{0x1000,0},{0x1030,0},{0x1038,0},{0x1040,0},
            {0x1048,0},{0x1050,0},{0x1054,0},{0x10d0,1},{0x10e0,1},{0x10f0,0x0a602258},{0x10f4,0x0f14}};
        for (unsigned address = 0x800; address < 0x1100; address += 4) {
            std::uint32_t value = 0;
            const auto response = access(address, false, value);
            const auto found = globals.find(address);
            if (found == globals.end()) {
                check(response == tlm::TLM_ADDRESS_ERROR_RESPONSE, "global/reserved decode");
                check(access(address, true, value) == tlm::TLM_ADDRESS_ERROR_RESPONSE, "global/reserved write decode");
            } else check(response == tlm::TLM_OK_RESPONSE && value == found->second, "global reset value");
        }
        for (auto offset : {CH_READ_CONFIG, CH_WRITE_CONFIG}) { cw(0, offset, ~0u); check(cr(0, offset) == 0xcf3f007f, "config mask"); }
        cw(0, CH_CMD_TRANSFER_SIZE, ~0u); check(cr(0, CH_CMD_TRANSFER_SIZE) == 1023, "10-bit byte count");
        cw(0, CH_MODE_CONFIG, ~0u); check(cr(0, CH_MODE_CONFIG) == 0x30000000, "mode mask");
        cw(0, CH_PERIPHERAL_CONFIG, ~0u); check(cr(0, CH_PERIPHERAL_CONFIG) == 0x071f071f, "peripheral mask");
        cw(0, CH_AXI_ATTR_REG, ~0u); check(cr(0, CH_AXI_ATTR_REG) == 0xbfbf, "AXI attribute mask");
        for (auto address : {CORE_JOINT_CONFIG, CORE_CLOCK_DIVIDER, CH_SCHEDULE_CONFIG}) {
            wr(address, ~0u); check(rd(address) == 0, "compatibility RAZ/WI");
        }
        wr(CORE_CAPABILITY_STATUS0, 0); check(rd(CORE_CAPABILITY_STATUS0) == 0x0a602258, "capability read-only");
        cw(7, CH_INTERRUPT_ENABLE, 0); cw(7, CH_INTERRUPT_RAW_STATUS, ~0u);
        check(cr(7, CH_INTERRUPT_RAW_STATUS) == 0x1fff && rd(CORE_STATUS) == 0 && !irq.read(), "raw without mask");
        cw(7, CH_INTERRUPT_ENABLE, 2); wait(SC_ZERO_TIME); wait(SC_ZERO_TIME);
        check(cr(7, CH_INTERRUPT_STATUS) == 2 && rd(CORE_STATUS) == 128 && irq.read(), "channel 7 consolidated IRQ");
        cw(7, CH_INTERRUPT_CLEAR, ~0u); wait(1, SC_NS);
        check(!irq.read() && rd(CORE_STATUS) == 0, "W1C clears IRQ");
        wr(PERIPHERAL_RX_REQUEST, 3); wr(PERIPHERAL_TX_REQUEST, 1);
        check(rd(PERIPHERAL_RX_REQUEST) == 2 && rd(PERIPHERAL_TX_REQUEST) == 0, "peripheral ID 0 reserved");
        std::uint32_t value = 0;
        check(access(1, false, value) == tlm::TLM_ADDRESS_ERROR_RESPONSE, "unaligned MMIO rejected");
        check(access(0x100000000ull, false, value) == tlm::TLM_ADDRESS_ERROR_RESPONSE, "no high address alias");
        check(access(0x1100, false, value) == tlm::TLM_ADDRESS_ERROR_RESPONSE, "outside aperture rejected");
        check(access(0, true, value, 8, 8) == tlm::TLM_BURST_ERROR_RESPONSE, "oversized MMIO rejected");
        check(access(0, true, value, 2, 2) == tlm::TLM_BURST_ERROR_RESPONSE, "narrow APB3 rejected");
        check(access(0, true, value, 4, 4, true) == tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE, "APB3 byte enables rejected");
        check(access(0, false, value, 4, 4, false, true) == tlm::TLM_GENERIC_ERROR_RESPONSE, "null data rejected");
    }
    void memory_test() {
        // HDS page 56's exact software sequence: a 256-byte M2M copy.
        pattern(0x1000, 1023);
        configure(0, 0x1000, 0x5000, 256); start(0); done(0); copied(0x1000, 0x5000, 256);
        check(cr(0, CH_TRANSFER_COUNT) == 0x10001 && cr(0, CH_INTERRUPT_STATUS) == 1, "completion counter and IRQ");
        check(cr(0, CH_CMD_READ_ADDR) == 0x1100 && cr(0, CH_CMD_WRITE_ADDR) == 0x5100, "live addresses");
        check(rd(CORE_IDLE_STATUS) == 1 && memory_.overlap, "R/W overlap and final idle");
        for (const unsigned bytes : {1u, 2u, 3u, 31u, 32u, 33u, 1023u}) {
            configure(0, 0x1001, 0x6003, bytes, 0x8101007f, 0x8101007f); start(0); done(0);
            copied(0x1001, 0x6003, bytes);
        }
        configure(0, 0x1000, 0x6000, 16, 0x81010008, 0x81010008, 0, 0x10000000); start(0); done(0);
        for (unsigned i = 0; i < 16; ++i) check(memory_.data[0x6000+i] == memory_.data[0x1000+(i^1)], "16-bit endian swap");
        configure(0, 0x1000, 0x6000, 16, 0x81010001, 0x81010001, 0, 0x20000000); start(0); done(0);
        for (unsigned i = 0; i < 16; ++i) check(memory_.data[0x6000+i] == memory_.data[0x1000+(i^3)], "32-bit endian swap across byte bursts");
        configure(0, 0x1000, 0x6000, 12, 0x01010004, 0x81010004); start(0); done(0);
        for (unsigned i = 0; i < 12; ++i) check(memory_.data[0x6000+i] == memory_.data[0x1000+i%4], "FIXED source repeats word");
        pattern(0xff0, 48); configure(0, 0xff0, 0x7ff0, 48, 0x8101007f, 0x8101007f); start(0); done(0); copied(0xff0, 0x7ff0, 48);
        for (const auto& r : memory_.log) {
            check(r.ext.id == 4 && r.ext.beats <= 16, "AXI ID and 16-beat limit");
            check((r.address & 4095) + r.length <= 4096, "4K boundary");
        }
        memory_.annotated = true;
        configure(0, 0x1000, 0x9000, 32); const auto at = sc_time_stamp(); start(0); done(0); copied(0x1000,0x9000,32);
        check(sc_time_stamp() - at >= sc_time(24, SC_NS), "annotated target latency consumed");
        configure(0, 0x1000, 0x9000, 0); const auto count = memory_.log.size(); start(0); done(0);
        check(memory_.log.size() == count && (cr(0, CH_TRANSFER_COUNT) & 0xfff) == 1, "zero byte command does no bus work");
    }
    void peripheral_test() {
        pattern(0x1000, 32);
        // Memory can prefetch while a peripheral has no request.
        configure(0, 0x1000, 0x18000, 8, 0xc4010008, 0x44010001, 1u << 16);
        start(0); wait(70, SC_NS);
        check(memory_.tx_data.empty() && cr(0, CH_ACTIVE_STATUS), "TX waits for request");
        check((cr(0, CH_RESTRICTION_STATUS) & 0x160) == 0x20, "M2P read pending independent of write peripheral");
        check((cr(0, CH_FIFO_STATUS) >> 16) == 8, "prefetched bytes visible");
        for (unsigned i = 0; i < 8; ++i) {
            wr(PERIPHERAL_TX_REQUEST, 2);
            for (unsigned k = 0; k < 40 && !(tx_clear.read() & 2); ++k) wait(1, SC_NS);
            check((tx_clear.read() & 2) != 0 && rd(PERIPHERAL_TX_REQUEST) == 0, "TX request acknowledged and software bit cleared");
            wait(2, SC_NS);
        }
        done(0);
        check(std::equal(memory_.tx_data.begin(), memory_.tx_data.end(), memory_.data.begin()+0x1000) && memory_.tx_data.size()==8, "M2P byte data");
        memory_.rx_data = {0x11,0x22,0x33,0x44,0x55,0x66};
        configure(1, 0x18010, 0x8000, 6, 0x44010001, 0xc4010002, 2 | (3u << 8)); start(1);
        wait(3, SC_NS);
        check((cr(1, CH_RESTRICTION_STATUS) & 0x160) == 0x40, "P2M memory write pending enabled independently");
        for (unsigned i = 0; i < 6; ++i) {
            rx_request.write(4);
            for (unsigned k = 0; k < 40 && !(rx_clear.read() & 4); ++k) wait(1, SC_NS);
            check((rx_clear.read() & 4) != 0, "external RX acknowledge");
            if (i == 0) { wait(20, SC_NS); check(memory_.rx_cursor == 1, "held request cannot generate a second credit"); }
            rx_request.write(0); wait(4, SC_NS);
        }
        done(1);
        check(std::equal(memory_.rx_data.begin(), memory_.rx_data.end(), memory_.data.begin()+0x8000), "P2M payload");
        check(cr(1,CH_CMD_READ_ADDR)==0x18010 && cr(1,CH_CMD_WRITE_ADDR)==0x8006, "peripheral fixed, memory incrementing");
        // A FIXED 4-byte service produces two 16-bit transactions to the same FIFO address.
        memory_.tx_data.clear(); configure(2,0x1000,0x18000,4,0x81010004,0x01010002,3u<<16); start(2);
        for(unsigned i=0;i<2;++i) { wr(PERIPHERAL_TX_REQUEST,8); wait(30,SC_NS); }
        done(2); check(memory_.tx_data.size()==4, "16-bit peripheral service");
        memory_.tx_data.clear();
        const auto log_begin=memory_.log.size();
        configure(3,0x1000,0x18000,8,0x81010008,0x01010008,4u<<16); start(3);
        wr(PERIPHERAL_TX_REQUEST,16); done(3);
        check(memory_.tx_data.size()==8 &&
              std::equal(memory_.tx_data.begin(),memory_.tx_data.end(),memory_.data.begin()+0x1000), "FIXED burst byte order");
        unsigned fragments=0;
        for(unsigned i=static_cast<unsigned>(log_begin);i<memory_.log.size();++i) {
            const auto& r=memory_.log[i];
            if(!r.read) {
                check(r.address==0x18000 && r.length==4 && r.ext.fragmented && !r.ext.increment &&
                      r.ext.beats==2 && r.ext.beat_index==fragments,"FIXED multi-beat metadata");
                ++fragments;
            }
        }
        check(fragments==2,"two FIXED beats consume one peripheral credit");
    }
    void chain_test() {
        pattern(0x1000, 64);
        // Four little-endian words: source, destination, byte size, control.
        for (unsigned n = 0; n < 2; ++n) {
            const unsigned base = 0x9000 + n*16;
            put(memory_.data,base,0x1010+n*16); put(memory_.data,base+4,0x6010+n*16);
            put(memory_.data,base+8,16); put(memory_.data,base+12,n==0 ? 0x9011 : 3);
        }
        configure(0,0x1000,0x6000,16,0x81010008,0x81010008,0,0,0x9001); start(0); done(0);
        copied(0x1000,0x6000,48);
        check(cr(0,CH_TRANSFER_COUNT)==0x30003 && cr(0,CH_INTERRUPT_STATUS)==1, "three completed commands and queued events");
        for(unsigned remaining=2;;--remaining) {
            cw(0,CH_INTERRUPT_CLEAR,1);
            check((cr(0,CH_TRANSFER_COUNT)>>16)==remaining, "one W1C acknowledges one completion");
            check(cr(0,CH_INTERRUPT_STATUS)==(remaining?1u:0u), "IRQ persists while completions remain");
            if(!remaining) break;
        }
        check(std::count_if(memory_.log.begin(),memory_.log.end(),[](const Record& r){return r.ext.command_fetch;})==2, "command list fetch count");
        configure(1,0x1000,0x7000,16,0x81010008,0x81010008,0,0,2); start(1); done(1);
        check(cr(1,CH_TRANSFER_COUNT)==1 && cr(1,CH_INTERRUPT_RAW_STATUS)==0, "CMD_SET_INT gates completion events");
        pattern(0x2000,8);
        put(memory_.data,0xffc,0x2004); put(memory_.data,0x1000,0x7004);
        put(memory_.data,0x1004,4); put(memory_.data,0x1008,3);
        const auto before=memory_.log.size();
        configure(3,0x2000,0x7000,4,0x81010004,0x81010004,0,0,0xffd); start(3); done(3);
        copied(0x2000,0x7000,8);
        unsigned fetch_parts=0;
        for(unsigned i=static_cast<unsigned>(before);i<memory_.log.size();++i) {
            const auto& r=memory_.log[i];
            if(r.ext.command_fetch) {
                check((r.address&4095)+r.length<=4096 && r.ext.fragmented,"descriptor 4K split");
                ++fetch_parts;
            }
        }
        check(fetch_parts==2,"descriptor crosses boundary using two reads");
    }
    void pending_test() {
        pattern(0x1000,128); memory_.reorder=true;
        configure(0,0x1000,0x6000,128,0xcf010004,0xcf010004); start(0);
        unsigned peak_rd=0,peak_wr=0;
        for(unsigned i=0;i<600;++i) {
            const auto pending=cr(0,CH_OUTSTANDING_STATUS);
            peak_rd=std::max(peak_rd,pending&15u); peak_wr=std::max(peak_wr,(pending>>8)&15u);
            const auto fifo=cr(0,CH_FIFO_STATUS);
            check((fifo&63)+(fifo>>16)<=32,"FIFO reservation bounded");
            if(i>0 && !cr(0,CH_ACTIVE_STATUS)) break;
            wait(1,SC_NS);
        }
        done(0); copied(0x1000,0x6000,128);
        check(peak_rd==4 && peak_wr==4 && memory_.max_read==4 && memory_.max_write==4, "effective/global pending clamps to four");
        check((cr(0,CH_RESTRICTION_STATUS)&0x60)==0x60,"multi-pending restriction bits");
        memory_.max_read=memory_.max_write=0;
        configure(0,0x1000,0x7000,64,0x80010004,0x80010004); start(0); done(0); copied(0x1000,0x7000,64);
        check(memory_.max_read==1 && memory_.max_write==1,"disabled outstanding limit is one");
        configure(0,0x1000,0x7000,64,0xc0010004,0xc0010004); start(0); done(0);
        check((cr(0,CH_RESTRICTION_STATUS)&0x60)==0,"zero programmed pending count clamps to one");
    }
    void arbitration_test() {
        memory_.latency_ns=10;
        for(unsigned n=0;n<8;++n) { pattern(0x1000+n*256,64); configure(n,0x1000+n*256,0x6000+n*256,64,0xc4030004,0xc4020004); }
        wr(CORE_PRIORITY_CONFIG,0xffff); wr(CORE_CHANNEL_START,0xff);
        for(unsigned n=0;n<8;++n) { done(n); copied(0x1000+n*256,0x6000+n*256,64); }
        check(rd(CORE_STATUS)==0xff,"global bitmap starts all eight channels");
        check(memory_.max_read<=4 && memory_.max_write<=4,"global pending limits across channels");
        std::vector<unsigned> sequence;
        for(const auto& r:memory_.log) if(r.read) sequence.push_back(r.ext.channel);
        check(sequence.size()>6 && sequence[0]==0 && sequence[1]==0 && sequence[2]==0 && sequence[3]==1,
              "three read tokens then release to next channel");
        // Tokens/priority must not permanently starve any other active channel.
        for(unsigned n=0;n<8;++n) check(cr(n,CH_TRANSFER_COUNT)==0x10001,"all channels completed");
    }
    void errors_test() {
        pattern(0x1000,32);
        struct Case { bool read; tlm::tlm_response_status response; std::uint32_t bit; };
        for(const auto c:{Case{true,tlm::TLM_ADDRESS_ERROR_RESPONSE,READ_DECERR},
                          Case{true,tlm::TLM_GENERIC_ERROR_RESPONSE,READ_SLVERR},
                          Case{false,tlm::TLM_ADDRESS_ERROR_RESPONSE,WRITE_DECERR},
                          Case{false,tlm::TLM_GENERIC_ERROR_RESPONSE,WRITE_SLVERR}}) {
            memory_.error_address=c.read?0x1000:0x6000; memory_.error=c.response;
            configure(0,0x1000,0x6000,32); start(0); done(0);
            check(cr(0,CH_INTERRUPT_RAW_STATUS)==c.bit,"AXI error maps to HDS bit");
            check(cr(0,CH_TRANSFER_COUNT)==0,"failed command is not completion");
        }
        memory_.error_address=~std::uint64_t(0);
        configure(0,0x1000,0x6000,32,0x81010000,0x81010008); start(0); done(0);
        check(cr(0,CH_INTERRUPT_STATUS)==READ_DECERR,"zero read burst rejected");
        configure(0,0x1000,0x6000,32,0x81010008,0x81010008,0,0x30000000); start(0); done(0);
        check(cr(0,CH_INTERRUPT_STATUS)==READ_DECERR,"reserved swap encoding rejected");
        configure(0,0x1000,0x6000,32,0x81010008,0x81010008,1|(2u<<16)); start(0); done(0);
        check(cr(0,CH_INTERRUPT_STATUS)==READ_DECERR,"unsupported P2P rejected");
        // A disable prevents new grants while accepted transfers drain.
        configure(0,0x1000,0x6000,32,0xc4010004,0xc4010004); start(0); wait(2,SC_NS);
        cw(0,CH_ENABLE,0); const auto log_size=memory_.log.size(); wait(60,SC_NS);
        check(memory_.log.size()==log_size && cr(0,CH_OUTSTANDING_STATUS)==0,"disable drains existing work and stops new grants");
        check(cr(0,CH_ACTIVE_STATUS)!=0,"disable preserves unfinished command");
        const auto original=cr(0,CH_CMD_READ_ADDR); cw(0,CH_CMD_READ_ADDR,0x9000);
        check(cr(0,CH_CMD_READ_ADDR)==original,"live reprogramming ignored");
        cw(0,CH_ENABLE,1); done(0); copied(0x1000,0x6000,32);
    }
    void reset_test() {
        pattern(0x1000,64); memory_.latency_ns=40;
        configure(0,0x1000,0x6000,64,0xc4010004,0xc4010004); start(0); wait(4,SC_NS);
        check(cr(0,CH_OUTSTANDING_STATUS)!=0,"reset exercised with pending transactions");
        reset(); wait(60,SC_NS);
        check(cr(0,CH_OUTSTANDING_STATUS)==0 && cr(0,CH_ACTIVE_STATUS)==0,"late read responses cannot resurrect reset channel");
        check(cr(0,CH_FIFO_STATUS)==32 && !irq.read(),"reset FIFO and IRQ");
        configure(0,0x1000,0x6000,64); start(0); done(0); copied(0x1000,0x6000,64);
        reset();
        // Reset again while an accepted write is still inside the target.
        memory_.log.clear(); configure(1,0x1000,0x7000,64); start(1);
        for(unsigned i=0;i<100 && !memory_.pending_write;++i) wait(1,SC_NS);
        check(memory_.pending_write!=0,"reset exercised with pending write"); reset(); wait(100,SC_NS);
        check(cr(1,CH_TRANSFER_COUNT)==0 && cr(1,CH_INTERRUPT_RAW_STATUS)==0,"late write response discarded");
    }
    void timing_test() {
        check((rd(CORE_CAPABILITY_STATUS1)&3)==3,"optional timing capability bits");
        configure(3,0x1000,0x6000,16); start(3); wait(1,SC_NS); cw(3,CH_ENABLE,0);
        wait(70,SC_NS);
        check(cr(3,CH_INTERRUPT_RAW_STATUS)==0,"disabled channel is not watched for stalled progress");
        cw(3,CH_ENABLE,1); done(3);
        check(cr(3,CH_INTERRUPT_STATUS)==COMMAND_COMPLETE,"watchdog resumes with a fresh progress deadline");
        configure(0,0x1000,0x18000,4,0x81010004,0x01010001,1u<<16); start(0); done(0);
        check(cr(0,CH_INTERRUPT_STATUS)==WATCHDOG_TIMEOUT,"waiting peripheral watchdog");
        memory_.latency_ns=60;
        configure(1,0x1000,0x6000,4); start(1); done(1);
        check(cr(1,CH_INTERRUPT_STATUS)==READ_DATA_TIMEOUT,"blocking read timeout and late response drain");
        memory_.latency_ns=4; memory_.write_latency_ns=60;
        configure(2,0x1000,0x6000,4); start(2); done(2);
        check(cr(2,CH_INTERRUPT_STATUS)==WRITE_RESPONSE_TIMEOUT,"blocking write response timeout");
    }
    void run() {
        reset();
        if(mode_=="registers") registers_test(); else if(mode_=="memory") memory_test();
        else if(mode_=="peripheral") peripheral_test(); else if(mode_=="chain") chain_test();
        else if(mode_=="pending") pending_test(); else if(mode_=="arbitration") arbitration_test();
        else if(mode_=="errors") errors_test(); else if(mode_=="reset") reset_test();
        else if(mode_=="timing") timing_test(); else check(false,"unknown test case");
        std::cout << "[FX1 DMA] " << mode_ << ": " << (errors?"FAIL":"PASS") << " (" << errors << " errors)\n";
        sc_stop();
    }
};
}

int sc_main(int argc, char** argv) {
    const std::string mode=argc>1?argv[1]:"memory";
    Config config;
    if(mode=="timing") { config.transaction_timeout=sc_time(20,SC_NS); config.watchdog_timeout=sc_time(35,SC_NS); }
    Memory memory{"memory"}; Bench bench{"bench",memory,mode}; Dma dma{"dma",config};
    bench.registers.bind(dma.target_socket); dma.master_socket.bind(memory.target);
    dma.reset_n(bench.reset_n); dma.irq(bench.irq);
    dma.rx_request(bench.rx_request); dma.tx_request(bench.tx_request);
    dma.rx_clear(bench.rx_clear); dma.tx_clear(bench.tx_clear);
    sc_start(1,SC_MS);
    if(!sc_end_of_simulation_invoked()) { std::cerr<<"FAIL: simulation deadline\n"; return 1; }
    return bench.errors?1:0;
}
