#include "fu3_bus/bus_system.h"
#include "support/probe_target.h"
#include "support/test_helpers.h"
#include <functional>
#include <memory>
#include <array>

// Independent transcription of HAS v2.0 pp.46-47, not generated from BusConfig.
struct Expected { const char* name; std::uint64_t base,size; fu3_bus::TargetPath path; };
using P=fu3_bus::TargetPath;
const Expected golden[] = {
    {"ROM",0,0x1000,P::SysBus1Axi}, {"CVA6_ISRAM",0x10000,0x10000,P::SysBus1Axi},
    {"IBEX_ISRAM",0x20000,0x10000,P::SysBus1Axi}, {"DSRAM",0x01000000,0x10000,P::SysBus1Axi},
    {"IBEX_DBG",0x1a110000,0x1000,P::SysBus1Axi}, {"QSPI_XIP",0x30000000,0x20000,P::SysBus0Axi},
    {"EDHOC_OSCORE",0x30100000,0x10000,P::SysBus0Axi}, {"AES",0x40000000,0x10000,P::SysBus0Axi},
    {"HYPERBUS",0x60000000,0x2000000,P::SysBus1Axi}, {"AP_PLIC",0x40100000,0x10000,P::Peribus1Apb},
    {"AP_QSPIC",0x44000000,0x10000,P::Peribus0Apb}, {"SYSCSR",0x44010000,0x10000,P::Peribus0Apb},
    {"AP_DMA",0x44020000,0x10000,P::Peribus0Apb}, {"RNG",0x44030000,0x10000,P::Peribus0Apb},
    {"MAILBOX_IBEX",0x44040000,0x10000,P::Peribus0Apb}, {"WDT0",0x44050000,0x10000,P::Peribus0Apb},
    {"TIMER0",0x44060000,0x10000,P::Peribus0Apb}, {"TIMER1",0x44070000,0x10000,P::Peribus0Apb},
    {"WDT1",0x44080000,0x10000,P::Peribus0Apb}, {"SPI1",0x440b0000,0x10000,P::Peribus0Apb},
    {"SPI0",0x440c0000,0x10000,P::Peribus0Apb}, {"UART0",0x440d0000,0x10000,P::Peribus0Apb},
    {"UART1",0x440e0000,0x10000,P::Peribus0Apb}, {"GPIOC",0x440f0000,0x10000,P::Peribus0Apb},
    {"CRM",0x50010000,0x10000,P::Peribus1Apb}, {"MAILBOX_CVA6",0x50020000,0x10000,P::Peribus1Apb},
    {"GPIOA",0x50030000,0x10000,P::Peribus1Apb}, {"GPIOB",0x50040000,0x10000,P::Peribus1Apb},
    {"UART2",0x50050000,0x10000,P::Peribus1Apb}, {"TIMER2",0x50060000,0x10000,P::Peribus1Apb},
    {"TIMER3",0x50070000,0x10000,P::Peribus1Apb}, {"I2C",0x50080000,0x10000,P::Peribus1Apb},
    {"SPI2",0x50090000,0x10000,P::Peribus1Apb}, {"ADC",0x500a0000,0x10000,P::Peribus1Apb},
    {"AP_HBCTRL",0x500b0000,0x10000,P::Peribus1Apb}, {"SPI3",0x500c0000,0x10000,P::Peribus1Apb}
};
struct Worker : sc_core::sc_module {
    std::function<void()> body;
    SC_HAS_PROCESS(Worker);
    explicit Worker(sc_core::sc_module_name n):sc_module(n){SC_THREAD(run);}
    void run(){body();}
};
struct Test : sc_core::sc_module, Checks {
    fu3_bus::BusConfig cfg=fu3_bus::BusConfig::fu3();
    fu3_bus::BusSystem fabric{"fabric",cfg};
    sc_core::sc_vector<Master> masters{"masters",4};
    sc_core::sc_vector<Worker> workers{"workers",12};
    std::vector<std::unique_ptr<fu3_bus::test::ProbeTarget>> targets;
    sc_core::sc_event start,done;
    unsigned completed=0,phase=0;
    bool finished=false, independent_ids=true, secondary=false, overlap=false;
    SC_HAS_PROCESS(Test);
    explicit Test(sc_core::sc_module_name n):sc_module(n) {
        for(unsigned i=0;i<4;++i) masters[i].socket.bind(fabric.initiator(cfg.initiators[i].name));
        for(const auto& row:golden) {
            auto ip=std::make_unique<fu3_bus::test::ProbeTarget>(sc_core::sc_gen_unique_name("target"),row.size);
            fabric.target(row.name).bind(ip->socket);targets.push_back(std::move(ip));
        }
        for(unsigned i=0;i<workers.size();++i) workers[i].body=[this,i]{work(i);};
        SC_THREAD(run);SC_THREAD(watchdog);
    }
    void transfer(unsigned master, tlm::tlm_generic_payload& tx, tlm::tlm_response_status expected) {
        auto a=tx.get_address();auto* d=tx.get_data_ptr();auto n=tx.get_data_length();
        sc_core::sc_time delay=sc_core::SC_ZERO_TIME;
        masters[master].socket->b_transport(tx,delay);
        expect(tx.get_response_status()==expected,"contract response");
        expect(tx.get_address()==a && tx.get_data_ptr()==d && tx.get_data_length()==n &&
               delay==sc_core::SC_ZERO_TIME && !tx.is_dmi_allowed(),"contract restores request");
    }
    void work(unsigned i) {
        while(true) {
            wait(start);
            unsigned char bytes[4]={};tlm::tlm_generic_payload tx;
            const bool write=phase==3 && i>=6;
            payload(tx,write?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND,
                    secondary?(write?0x30000000:0x40000000):(write?0x01000000:(phase==0?0x00010000:0x00020000)),bytes,4);
            fu3_bus::ScopedAccess access(tx,4,fu3_bus::Burst::Increment,false,independent_ids?i:0);
            transfer(phase,tx,tlm::TLM_OK_RESPONSE);
            ++completed;done.notify(sc_core::SC_ZERO_TIME);
        }
    }
    void run() {
        expect(cfg.targets.size()==std::size(golden),"exact number of physical windows");
        unsigned char bytes[80]={};tlm::tlm_generic_payload tx;
        for(unsigned i=0;i<std::size(golden);++i) {
            const auto& row=golden[i];
            auto it=std::find_if(cfg.targets.begin(),cfg.targets.end(),[&](const auto& t){return t.name==row.name;});
            expect(it!=cfg.targets.end() && it->base==row.base && it->size==row.size && it->path==row.path,
                   std::string("HAS map: ")+row.name);
            for(auto offset:{std::uint64_t{0},row.size-4}) {
                payload(tx,tlm::TLM_READ_COMMAND,row.base+offset,bytes,4);
                fu3_bus::ScopedAccess access(tx,4,fu3_bus::Burst::Increment,false,0,true);transfer(row.name==std::string("IBEX_DBG")?2:1,tx,tlm::TLM_OK_RESPONSE);
                expect(targets[i]->visits.back().address==offset,"independent target boundary");
            }
        }
        payload(tx,tlm::TLM_READ_COMMAND,0x20000,bytes,4);
        for(unsigned m=0;m<4;++m) transfer(m,tx,tlm::TLM_GENERIC_ERROR_RESPONSE);
        { fu3_bus::ScopedAccess fetch(tx,4,fu3_bus::Burst::Increment,true);
          tx.set_address(0x01000000);transfer(0,tx,tlm::TLM_COMMAND_ERROR_RESPONSE); }
        // Fixed narrow requests to the final halfword of an APB aperture.
        auto& fifo=*targets[10];
        const auto count=fifo.visits.size();
        for(unsigned i=0;i<10;++i)bytes[i]=static_cast<unsigned char>(i+1);
        payload(tx,tlm::TLM_WRITE_COMMAND,0x4400fffe,bytes,10);
        {fu3_bus::ScopedAccess access(tx,2,fu3_bus::Burst::Fixed);transfer(3,tx,tlm::TLM_OK_RESPONSE);}
        expect(fifo.visits.size()==count+5,"five halfword FIFO beats");
        for(unsigned i=0;i<5;++i) expect(fifo.visits[count+i].address==0xfffe &&
            fifo.visits[count+i].length==2 && fifo.visits[count+i].value==2*i+1,"fixed address and exact beat size");
        {fu3_bus::ScopedAccess access(tx,2);transfer(3,tx,tlm::TLM_ADDRESS_ERROR_RESPONSE);}
        expect(fifo.visits.size()==count+5,"whole increment footprint rejected before side effects");
        payload(tx,tlm::TLM_WRITE_COMMAND,0x44000100,bytes,3);
        {fu3_bus::ScopedAccess access(tx,1);transfer(3,tx,tlm::TLM_OK_RESPONSE);}
        expect(fifo.visits.back().address==0x102 && fifo.visits.back().length==1,"three incrementing byte beats");
        payload(tx,tlm::TLM_WRITE_COMMAND,0x44000100,bytes,68);
        {fu3_bus::ScopedAccess access(tx,4);transfer(3,tx,tlm::TLM_BURST_ERROR_RESPONSE);}
        payload(tx,tlm::TLM_WRITE_COMMAND,0x44000200,bytes,10);
        unsigned char enables[3]={0xff,0,0xff};
        tx.set_byte_enable_ptr(enables);tx.set_byte_enable_length(3);
        {fu3_bus::ScopedAccess access(tx,2);transfer(3,tx,tlm::TLM_OK_RESPONSE);}
        tx.set_byte_enable_ptr(nullptr);tx.set_byte_enable_length(0);
        unsigned char observed[10]={};
        payload(tx,tlm::TLM_READ_COMMAND,0x44000200,observed,10);
        {fu3_bus::ScopedAccess access(tx,2);transfer(3,tx,tlm::TLM_OK_RESPONSE);}
        for(unsigned i=0;i<10;++i)expect(observed[i]==(i%3==1?0:bytes[i]),"subword strobes survive beat boundaries");
        payload(tx,tlm::TLM_READ_COMMAND,0x20000,bytes,4);
        for(unsigned invalid:{0U,3U,8U}) {
            fu3_bus::ScopedAccess access(tx,invalid);transfer(0,tx,tlm::TLM_BURST_ERROR_RESPONSE);
        }
        {fu3_bus::ScopedAccess access(tx,4,static_cast<fu3_bus::Burst>(99));transfer(0,tx,tlm::TLM_BURST_ERROR_RESPONSE);}
        payload(tx,tlm::TLM_WRITE_COMMAND,0x20000,bytes,4);
        {fu3_bus::ScopedAccess access(tx,4,fu3_bus::Burst::Increment,true);transfer(0,tx,tlm::TLM_COMMAND_ERROR_RESPONSE);}
        // DBG's explicitly specified interface rejects narrow and fixed requests.
        payload(tx,tlm::TLM_READ_COMMAND,0x20000,bytes,4);
        {fu3_bus::ScopedAccess access(tx,2);transfer(2,tx,tlm::TLM_BURST_ERROR_RESPONSE);}
        {fu3_bus::ScopedAccess access(tx,4,fu3_bus::Burst::Fixed);transfer(2,tx,tlm::TLM_BURST_ERROR_RESPONSE);}
        // Narrow DMA writes reach a word-only SRAM as aligned lane strobes.
        payload(tx,tlm::TLM_WRITE_COMMAND,0x20001,bytes,3);
        {fu3_bus::ScopedAccess access(tx,1);transfer(3,tx,tlm::TLM_OK_RESPONSE);}
        expect(targets[2]->visits.back().length==4 && targets[2]->visits.back().address==0,
               "word-only target gets aligned word beats");
        unsigned char word[4]={};
        payload(tx,tlm::TLM_READ_COMMAND,0x20000,word,4);
        {fu3_bus::ScopedAccess access(tx,4);transfer(1,tx,tlm::TLM_OK_RESPONSE);}
        expect(word[0]==0 && word[1]==bytes[0] && word[2]==bytes[1] && word[3]==bytes[2],
               "lane conversion preserves untouched byte without RMW");
        payload(tx,tlm::TLM_READ_COMMAND,0x20001,word,3);
        {fu3_bus::ScopedAccess access(tx,1);transfer(3,tx,tlm::TLM_OK_RESPONSE);}
        expect(word[0]==bytes[0] && word[1]==bytes[1] && word[2]==bytes[2],"narrow read extraction");
        // The QSPI word/SINGLE boundary receives normalized metadata, even for
        // repeated narrow functional transfers. Disabled read lanes stay intact.
        auto qspi_before=targets[5]->visits.size();
        payload(tx,tlm::TLM_WRITE_COMMAND,0x30000002,bytes,4);
        {fu3_bus::ScopedAccess access(tx,2,fu3_bus::Burst::Fixed);transfer(3,tx,tlm::TLM_OK_RESPONSE);}
        expect(targets[5]->visits.size()==qspi_before+2,"QSPI decomposes into two singles");
        for(unsigned i=0;i<2;++i) {
            const auto& v=targets[5]->visits[qspi_before+i];
            expect(v.address==0 && v.length==4 && v.beat_bytes==4 && v.increment,
                   "QSPI receives aligned word SINGLE with increment metadata");
        }
        unsigned char narrow[2]={0xee,0xee}, mask[2]={0xff,0};
        payload(tx,tlm::TLM_READ_COMMAND,0x30000002,narrow,2);
        tx.set_byte_enable_ptr(mask);tx.set_byte_enable_length(2);
        {fu3_bus::ScopedAccess access(tx,2);transfer(3,tx,tlm::TLM_OK_RESPONSE);}
        expect(narrow[0]==bytes[2] && narrow[1]==0xee,"widened read honors disabled lanes");
        tx.set_byte_enable_ptr(nullptr);tx.set_byte_enable_length(0);
        // Measure real target occupancy, not merely admission counters.
        for(auto& target:targets) target->latency_ns=100;
        targets[2]->on_visit=[this](const auto&){if(targets[3]->active)overlap=true;};
        targets[3]->on_visit=[this](const auto&){if(targets[2]->active)overlap=true;};
        // Exercise admission through real ports, including independent DMA engines.
        for(phase=0;phase<4;++phase) {
            completed=0;for(auto& t:targets)t->peak=0;start.notify(sc_core::SC_ZERO_TIME);
            wait(1,sc_core::SC_NS);
            if(phase==1) {
                bool blocked=false;try{fabric.finish_staging();}catch(const std::logic_error&){blocked=true;}
                expect(blocked,"staging handoff refuses pending IBEX requests");
            }
            if(phase==2) {
                bool blocked=false;try{fabric.reset();}catch(const std::logic_error&){blocked=true;}
                expect(blocked,"reset refuses in-flight traffic");
            }
            while(completed<12)wait(done);
            const unsigned limit=phase<2?2:phase==2?1:4;
            expect(fabric.peak_outstanding(cfg.initiators[phase].name)==limit,"bounded read/shared admission");
            expect(targets[phase==0?1:2]->peak==limit,"actual concurrent requests at each CPU's permitted SRAM obey master limit");
            if(phase==3) {
                expect(fabric.peak_outstanding("DMA0",true)==4,"independent DMA write admission");
                expect(targets[3]->peak==4 && overlap,"four DMA reads and writes overlap at IPs");
            }
        }
        // Same ordering ID remains serialized, even with four admission slots.
        phase=3;independent_ids=false;completed=0;
        for(auto& t:targets)t->peak=0;
        start.notify(sc_core::SC_ZERO_TIME);while(completed<12)wait(done);
        expect(targets[2]->peak==1 && targets[3]->peak==1,"same-ID ordering preserved");
        // Both independent targets behind AP_SB0 must overlap; QSPI remains single outstanding.
        independent_ids=true;secondary=true;overlap=false;completed=0;
        targets[7]->on_visit=[this](const auto&){if(targets[5]->active)overlap=true;};
        targets[5]->on_visit=[this](const auto&){if(targets[7]->active)overlap=true;};
        start.notify(sc_core::SC_ZERO_TIME);while(completed<12)wait(done);
        expect(targets[7]->peak==4 && targets[5]->peak==1 && overlap,
               "SYSBUS0 link overlaps AES and single-outstanding QSPI");
        fabric.finish_staging();
        payload(tx,tlm::TLM_WRITE_COMMAND,0x10000,bytes,4);
        {fu3_bus::ScopedAccess access(tx,4);transfer(1,tx,tlm::TLM_COMMAND_ERROR_RESPONSE);}
        fabric.reset();
        {fu3_bus::ScopedAccess access(tx,4);transfer(1,tx,tlm::TLM_OK_RESPONSE);}
        finished=true;sc_core::sc_stop();
    }
    void watchdog(){wait(1,sc_core::SC_MS);expect(false,"contract watchdog");sc_core::sc_stop();}
};
int sc_main(int,char**) {
    Test t{"test"};sc_core::sc_start();
    std::cout<<"Contract: "<<t.checks<<" checks, "<<t.failures<<" failures\n";
    return t.finished&&!t.failures?0:1;
}
