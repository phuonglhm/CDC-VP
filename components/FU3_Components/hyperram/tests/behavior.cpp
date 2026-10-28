#define SC_INCLUDE_DYNAMIC_PROCESSES
#include <fu3_hyperram/controller.h>
#include <fu3_bus/bus_system.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <iostream>
#include <memory>
using namespace sc_core;
using namespace fu3_hyperram;
struct Bench:sc_module {
    Controller dut{"dut"};
    tlm_utils::simple_initiator_socket<Bench> mem{"mem"},apb{"apb"};
    sc_signal<bool> reset_n{"reset_n"},device_reset_n{"device_reset_n"},irq{"irq"};
    std::unique_ptr<fu3_bus::BusSystem> bus;
    unsigned checks=0,failures=0;bool finished=false,integrated;
    SC_HAS_PROCESS(Bench);
    Bench(sc_module_name name,bool integration):sc_module(name),integrated(integration) {
        dut.reset_n(reset_n);dut.device_reset_n(device_reset_n);dut.irq(irq);
        if(integrated){auto cfg=fu3_bus::BusConfig::fu3();Controller::configure_bus(cfg);
            for(auto& t:cfg.targets)t.enabled=t.name=="HYPERBUS"||t.name=="AP_HBCTRL";
            // Exercise the endpoint's four slots per direction without a smaller CPU cap.
            for(auto& m:cfg.initiators)if(m.name=="IBEX"){m.outstanding=8;m.separate_read_write=true;}
            bus=std::make_unique<fu3_bus::BusSystem>("bus",cfg);
            mem.bind(bus->initiator("IBEX"));apb.bind(bus->initiator("DBG"));
            bus->target("HYPERBUS").bind(dut.memory);bus->target("AP_HBCTRL").bind(dut.csr);
        }else {mem.bind(dut.memory);apb.bind(dut.csr);}
        SC_THREAD(run);SC_THREAD(watchdog);
    }
    void expect(bool ok,const char* what){++checks;if(!ok){++failures;std::cerr<<sc_time_stamp()<<": "<<what<<'\n';}}
    void settle(){for(unsigned i=0;i<5;++i)wait(SC_ZERO_TIME);}
    unsigned csr(unsigned offset,bool write=false,unsigned value=0,unsigned char* enables=nullptr){
        unsigned char bytes[4];for(unsigned i=0;i<4;++i)bytes[i]=static_cast<unsigned char>(value>>(8*i));
        tlm::tlm_generic_payload tx;tx.set_address((integrated?0x500b0000:0)+offset);tx.set_data_ptr(bytes);tx.set_data_length(4);tx.set_streaming_width(4);
        tx.set_command(write?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND);fu3_bus::ScopedAccess metadata(tx,4);
        tx.set_byte_enable_ptr(enables);tx.set_byte_enable_length(enables?4:0);
        auto delay=sc_time(2,SC_NS);apb->b_transport(tx,delay);settle();expect(tx.is_response_ok()&&delay==SC_ZERO_TIME,"CSR transport");
        value=0;for(unsigned i=0;i<4;++i)value|=unsigned(bytes[i])<<(8*i);return value;
    }
    void wr(unsigned off,unsigned value){csr(off,true,value);}
    unsigned rd(unsigned off){return csr(off);}
    void complete(){for(unsigned i=0;i<1000;++i){if(!(rd(12)&2))return;wait(10,SC_NS);}expect(false,"command watchdog");}
    void init(unsigned latency=0x107){
        wr(8,0);wr(16,latency);wr(20,200);wr(24,4);wr(28,4);wr(32,8);wr(36,1);complete();wr(8,1);
        expect(rd(12)&1,"firmware configuration enables memory");
    }
    unsigned device_read(unsigned address){wr(48,address);wr(56,1);complete();return rd(60);}
    void device_write(unsigned address,unsigned value){wr(48,address);wr(52,value);wr(56,2);complete();}
    bool transfer(bool write,unsigned address,std::vector<unsigned char>& bytes,tlm::tlm_response_status expected=tlm::TLM_OK_RESPONSE,
                  unsigned char* be=nullptr,Fault* injection=nullptr,unsigned id=0,unsigned beat=4,fu3_bus::Burst burst=fu3_bus::Burst::Increment,bool instruction=false){
        tlm::tlm_generic_payload tx;tx.set_address((integrated?0x60000000ULL:0)+address);tx.set_data_ptr(bytes.data());
        tx.set_data_length(unsigned(bytes.size()));tx.set_streaming_width(unsigned(bytes.size()));tx.set_command(write?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND);
        tx.set_byte_enable_ptr(be);tx.set_byte_enable_length(be?4:0);fu3_bus::ScopedAccess metadata(tx,beat,burst,instruction,id);
        if(injection)tx.set_extension(injection);
        auto delay=sc_time(3,SC_NS);mem->b_transport(tx,delay);if(injection)tx.clear_extension<Fault>();
        expect(tx.get_response_status()==expected,"memory response");expect(delay==SC_ZERO_TIME&&!tx.is_dmi_allowed(),"delay consumed and DMI disabled");return tx.is_response_ok();
    }
    void run(){
        reset_n=false;device_reset_n=false;wait(20,SC_NS);reset_n=true;device_reset_n=true;wait(20,SC_NS);
        expect(rd(0)==0x48525650&&rd(4)==1,"explicit provisional ABI identity");
        for(unsigned bits=0;bits<16;++bits){
            wr(16,0xa55a);unsigned char enables[4];unsigned mask=0;
            for(unsigned i=0;i<4;++i){enables[i]=(bits&(1u<<i))?255:0;if(enables[i])mask|=255u<<(8*i);}
            csr(16,true,0x1234,enables);
            expect(rd(16)==((0xa55au&~mask)|(0x1234u&mask)),"all AP_HBCTRL shadow strobe masks merge locally");
        }
        expect(device_read(0)==0xe76&&device_read(1)==9,"S80KS2564 identification");
        expect(device_read(2)==0x8f2f&&device_read(3)==0xffc2,"device POR register defaults");
        std::vector<unsigned char> data(64);for(unsigned i=0;i<data.size();++i)data[i]=static_cast<unsigned char>(i*7+3);
        transfer(true,0,data,tlm::TLM_GENERIC_ERROR_RESPONSE);expect(rd(64)&4,"not-ready error before init");wr(68,255);
        // Fabric NX denial is a slave error (Q02-f); the endpoint's own check reports a command error.
        transfer(false,0,data,integrated?tlm::TLM_GENERIC_ERROR_RESPONSE:tlm::TLM_COMMAND_ERROR_RESPONSE,
                 nullptr,nullptr,0,4,fu3_bus::Burst::Increment,true);
        wr(68,255); // Direct and fabric paths reject NX without launching a memory read.
        init();auto before=rd(100);transfer(true,0,data);expect(rd(100)==before+1&&rd(104)==16,"one burst stays one device transaction");
        std::vector<unsigned char> readback(64,0);transfer(false,0,readback);expect(readback==data,"burst data roundtrip");
        unsigned char sparse[]{255,0,255,0};std::vector<unsigned char> word{0xaa,0xbb,0xcc,0xdd};transfer(true,0,word,tlm::TLM_OK_RESPONSE,sparse);
        std::vector<unsigned char> result(4);transfer(false,0,result);expect(result[0]==0xaa&&result[1]==data[1]&&result[2]==0xcc&&result[3]==data[3],"byte masks preserve untouched lanes");
        unsigned char zero[]{0,0,0,0};transfer(true,0,word,tlm::TLM_OK_RESPONSE,zero);std::vector<unsigned char> same(4);transfer(false,0,same);expect(same==result,"all-zero mask is a valid no-op write");
        std::vector<unsigned char> large(1024,0x55);before=rd(100);transfer(true,1024,large,tlm::TLM_GENERIC_ERROR_RESPONSE);
        expect(rd(100)==before&&(rd(64)&8),"whole burst tCSM rejection without launch or splitting");
        transfer(true,4092,data,tlm::TLM_BURST_ERROR_RESPONSE);
        transfer(true,0,word,tlm::TLM_BURST_ERROR_RESPONSE,nullptr,nullptr,0,2);
        transfer(true,0,word,tlm::TLM_BURST_ERROR_RESPONSE,nullptr,nullptr,0,4,fu3_bus::Burst::Fixed);
        transfer(true,memory_size-4,data,tlm::TLM_ADDRESS_ERROR_RESPONSE);
        wr(68,255);wr(72,255);Fault fault;fault.fail_after_words=2;
        transfer(true,256,data,tlm::TLM_GENERIC_ERROR_RESPONSE,nullptr,&fault);settle();expect(irq&&rd(80)==2&&rd(108)==1,"partial write diagnostic and IRQ");
        readback.assign(64,0xee);transfer(false,256,readback);expect(readback[0]==data[0]&&readback[7]==data[7]&&readback[8]==0,"partial write retained without rollback/replay");
        wr(68,255);expect(!irq,"error clear deasserts IRQ");readback.assign(64,0xee);
        transfer(false,0,readback,tlm::TLM_GENERIC_ERROR_RESPONSE,nullptr,&fault);
        expect(readback[0]==result[0]&&readback[8]==0&&(rd(64)&64),"read fault preserves prefix and zero-fills error suffix");wr(68,255);
        wr(8,0);device_write(2,0x8f27);expect(!(rd(12)&1),"device timing change requires matching controller apply");init(7);
        auto t=sc_time_stamp();transfer(false,0,result);auto single=sc_time_stamp()-t;
        Fault refresh;refresh.refresh=true;t=sc_time_stamp();transfer(false,0,result,tlm::TLM_OK_RESPONSE,nullptr,&refresh);
        expect(sc_time_stamp()-t>single,"variable latency refresh adds delay");
        wr(84,3);wr(88,11);wr(92,1);complete();expect(rd(96)==0xb03,"independent virtual lane delay codes");
        wr(84,32);wr(92,1);complete();expect((rd(12)&64)&&rd(96)==0xb03,"invalid delay rejected without changing active codes");wr(68,255);
        wr(16,3);wr(36,1);complete();expect((rd(12)&64)&&rd(40)==7,"invalid LC at 200 MHz preserves active profile");
        sc_join join;
        for(unsigned i=0;i<8;++i)join.add_process(sc_spawn([&,i]{std::vector<unsigned char> d(64,static_cast<unsigned char>(i));transfer(i<4,4096+i*64,d,tlm::TLM_OK_RESPONSE,nullptr,nullptr,i);}));
        join.wait();expect(dut.peak_reads()==4&&dut.peak_writes()==4,"four read and four write admission slots");
        reset_n=false;wait(10,SC_NS);reset_n=true;wait(10,SC_NS);init(7);
        transfer(false,0,result);expect(result==same,"warm controller reset retains device memory/configuration");
        sc_join resetting;resetting.add_process(sc_spawn([&]{std::vector<unsigned char> d(64,0x77);transfer(true,8192,d,tlm::TLM_GENERIC_ERROR_RESPONSE);}));
        wait(100,SC_NS);reset_n=false;wait(10,SC_NS);reset_n=true;
        if(resetting.process_count())resetting.wait();
        device_reset_n=false;wait(10,SC_NS);device_reset_n=true;wait(10,SC_NS);init();
        transfer(false,0,result);expect(result==std::vector<unsigned char>(4,0),"device reset invalidates memory with deterministic zero fill");
        wr(8,0);expect(rd(12)&8,"quiesced after disabling and draining");
        finished=true;sc_stop();
    }
    void watchdog(){wait(1,SC_MS);expect(false,"simulation watchdog");sc_stop();}
};
int sc_main(int argc,char** argv){Bench b{"bench",argc>1&&std::string(argv[1])=="bus"};sc_start();
    std::cout<<"HyperRAM: "<<b.checks<<" checks, "<<b.failures<<" failures\n";return b.failures||!b.finished?1:0;}
