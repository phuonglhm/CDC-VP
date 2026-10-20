#pragma once
#include <fu3_i2c/i2c.h>
#include <fu3_bus/bus_system.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <iostream>
using namespace sc_core;
using namespace fu3_i2c;
struct Bench:sc_module {
    static fu3_bus::BusConfig bus_config(){auto c=fu3_bus::BusConfig::fu3();
        for(auto& t:c.targets)t.enabled=t.name=="I2C";return c;}
    I2c dut{"dut"};
    tlm_utils::simple_initiator_socket<Bench> cpu{"cpu"};
#ifdef I2C_INTEGRATION
    fu3_bus::BusSystem bus{"bus",bus_config()};
#endif
    sc_signal<bool> reset_n{"reset_n"},scl{"scl"},sda{"sda"},scl_oe{"scl_oe"},sda_oe{"sda_oe"},irq{"irq"};
    sc_signal<bool> plic_source14{"plic_source14"},ibex_fast2{"ibex_fast2"};
    sc_signal<bool,SC_UNCHECKED_WRITERS> peer_scl{"peer_scl",true},peer_sda{"peer_sda",true};
    unsigned checks=0,failures=0;bool finished=false;sc_event peer_go;
    unsigned peer_mode=0,received_address=0,received_data=0,read_ack=0;
    SC_HAS_PROCESS(Bench);
    explicit Bench(sc_module_name n):sc_module(n){
        dut.reset_n(reset_n);dut.scl_i(scl);dut.sda_i(sda);dut.scl_oe_n(scl_oe);dut.sda_oe_n(sda_oe);dut.irq(irq);
#ifdef I2C_INTEGRATION
        cpu.bind(bus.initiator("IBEX"));bus.target("I2C").bind(dut.socket);
#else
        cpu.bind(dut.socket);
#endif
        SC_METHOD(resolve);sensitive<<scl_oe<<sda_oe<<peer_scl<<peer_sda;
        SC_METHOD(route_irq);sensitive<<irq;
        SC_THREAD(run);SC_THREAD(fake_slave);SC_THREAD(watchdog);
    }
    void resolve(){scl=scl_oe.read()&&peer_scl.read();sda=sda_oe.read()&&peer_sda.read();}
    void route_irq(){plic_source14=irq.read();ibex_fast2=irq.read();}
    void expect(bool ok,const char* message){++checks;if(!ok){++failures;std::cerr<<sc_time_stamp()<<": "<<message<<"\n";}}
    void settle(){for(unsigned i=0;i<6;++i)wait(SC_ZERO_TIME);}
    unsigned access(unsigned offset,bool write=false,unsigned value=0,tlm::tlm_response_status expected=tlm::TLM_OK_RESPONSE,
                    unsigned length=4,unsigned char* enables=nullptr){
        unsigned char data[4]{};for(unsigned i=0;i<4;++i)data[i]=static_cast<unsigned char>(value>>(8*i));
        tlm::tlm_generic_payload tx;tx.set_address(offset);
#ifdef I2C_INTEGRATION
        tx.set_address(0x50080000ULL+offset);
#endif
        tx.set_command(write?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND);tx.set_data_ptr(data);tx.set_data_length(length);
        tx.set_streaming_width(length);tx.set_byte_enable_ptr(enables);tx.set_byte_enable_length(enables?4:0);
        fu3_bus::ScopedAccess meta(tx,length);auto delay=sc_time(3,SC_NS);
        cpu->b_transport(tx,delay);settle();expect(tx.get_response_status()==expected,"MMIO response");
        expect(delay==SC_ZERO_TIME,"annotated delay consumed once");
        value=0;for(unsigned i=0;i<4;++i)value|=unsigned(data[i])<<(8*i);return value;
    }
    unsigned rd(unsigned o){return access(o);}void wr(unsigned o,unsigned v){access(o,true,v);}
    void reset(){reset_n=false;peer_scl=true;peer_sda=true;wait(100,SC_NS);reset_n=true;wait(100,SC_NS);}
    void configure(bool slave=false){
        wr(4,0);wr(0,19);
        for(unsigned o=24;o<=48;o+=4)wr(o,29);
        wr(52,1);wr(56,0x52);wr(60,slave?1:0);wr(4,0xe0);wr(20,1);
    }
    unsigned done(){for(unsigned i=0;i<1000;++i){auto v=rd(12);if(!(v&2))return v;wait(100,SC_NS);}
        expect(false,"master completion watchdog");return rd(12);}
    void launch(unsigned cmd,unsigned data=0){wr(16,data);wr(20,cmd|1);}
    void rise(){wait(scl.posedge_event());}void fall(){wait(scl.negedge_event());}
    void find_start(){while(true){wait(sda.negedge_event());if(scl.read())return;}}
    unsigned receive(){unsigned v=0;for(unsigned i=0;i<8;++i){rise();v=(v<<1)|unsigned(sda.read());}
        fall();peer_sda=false;rise();fall();peer_sda=true;return v;}
    bool send(unsigned v){for(unsigned i=0;i<8;++i){peer_sda=bool(v&(0x80u>>i));rise();fall();}
        peer_sda=true;rise();const bool nack=sda.read();fall();return nack;}
    void fake_slave(){while(true){wait(peer_go);
        if(peer_mode==1){find_start();received_address=receive();received_data=receive();
            find_start();received_address=(received_address<<8)|receive();read_ack=send(0xa6);read_ack=(read_ack<<1)|send(0x39);}
        else if(peer_mode==2){find_start();fall();peer_scl=false;wait(4,SC_US);peer_scl=true;received_address=receive();}
        else if(peer_mode==3){find_start();fall();peer_sda=false;wait(3,SC_US);peer_sda=true;}
    }}
    void external_start(){peer_sda=true;peer_scl=true;wait(400,SC_NS);peer_sda=false;wait(400,SC_NS);peer_scl=false;wait(400,SC_NS);}
    void external_stop(){peer_scl=false;peer_sda=false;wait(400,SC_NS);peer_scl=true;wait(400,SC_NS);peer_sda=true;wait(400,SC_NS);}
    bool external_write(unsigned value,unsigned clear_register=~0u){
        for(unsigned i=0;i<8;++i){peer_sda=bool(value&(0x80u>>i));wait(400,SC_NS);peer_scl=true;
            if(i==7&&clear_register!=~0u){wait(30,SC_NS);wr(clear_register,clear_register==20?1:128);wait(370,SC_NS);}
            else wait(400,SC_NS);
            peer_scl=false;wait(400,SC_NS);}
        peer_sda=true;wait(400,SC_NS);peer_scl=true;wait(400,SC_NS);const bool ack=!sda.read();
        peer_scl=false;wait(400,SC_NS);return ack;
    }
    unsigned external_read(bool nack,unsigned refill=~0u){unsigned v=0;peer_sda=true;
        for(unsigned i=0;i<8;++i){wait(400,SC_NS);peer_scl=true;wait(400,SC_NS);v=(v<<1)|unsigned(sda.read());peer_scl=false;wait(400,SC_NS);
            if(i==3&&refill!=~0u)wr(16,refill);}
        peer_sda=nack;wait(400,SC_NS);peer_scl=true;wait(400,SC_NS);peer_scl=false;wait(400,SC_NS);peer_sda=true;return v;
    }
    void run(){
        reset();for(unsigned o=0;o<=64;o+=4)expect(rd(o)==0,"reset CSR value");
        wr(16,0x123456ab);expect(rd(16)==0xab,"TX byte mask");expect(rd(0x90)==0xab,"upper-address alias");
        wr(68,0xffffffff);expect(rd(68)==0,"reserved read-zero write-ignore");
        wr(4,31);expect(rd(4)==0,"reserved CTRL bits read zero");
        wr(20,0x90);expect(rd(20)==0&&rd(12)==0,"disabled commands ignored");
        unsigned char sparse[]{255,0,255,255};
        access(16,true,0,tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,4,sparse);
        expect(rd(16)==0xab,"rejected strobes do not modify TX");
        configure();peer_mode=1;peer_go.notify();wait(100,SC_NS);
        launch(0x90,0xa4);expect(!(done()&0xa0),"master address ACK without arbitration loss");
        expect(irq.read()&&plic_source14&&ibex_fast2,"master IRQ reaches fake PLIC and Ibex routes");wr(20,1);expect(!irq.read(),"IACK clears IRQ");
        launch(0x10,0x75);expect(!(done()&128),"master data ACK");
        launch(0x90,0xa5);done();launch(0x20);done();expect(rd(8)==0xa6,"master read first byte");
        launch(0x68);done();expect(rd(8)==0x39,"master read final byte");wait(1,SC_US);
        expect(received_address==0xa4a5&&received_data==0x75,"fake peer verifies repeated START data flow");
        expect(read_ack==1,"master sends ACK then final NACK");expect(!(rd(12)&64)&&scl&&sda,"STOP frees bus");
        launch(0xd0,0x88);expect(done()&128,"unaddressed peer returns NACK");
        reset();configure();peer_mode=2;peer_go.notify();wait(100,SC_NS);launch(0xd0,0xa4);
        wait(2,SC_US);expect((rd(12)&3)==2,"stretch prevents premature completion");done();
        expect(received_address==0xa4,"stretch preserves transmitted data");
        reset();configure();peer_mode=3;peer_go.notify();wait(100,SC_NS);launch(0x90,0xff);
        expect(done()&32,"arbitration loss reported");expect(scl_oe&&sda_oe,"loser releases both lines");
        wr(20,1);expect((rd(12)&33)==32,"IACK preserves AL");wait(4,SC_US);
        reset();configure(true);external_start();expect(external_write(0xa4),"slave matches 7-bit write address");
        expect(external_write(0x12),"slave ACKs RX");expect(external_write(0x34),"slave ACKs RX overwrite");
        expect(rd(8)==0x34&&(rd(64)&0x8a)==0x8a,"latest slave RX and sticky/live status");
        expect(irq,"slave receive IRQ");wr(64,128);expect(irq,"W1C does not clear IF");wr(20,1);expect(!irq,"slave IACK");
#ifndef I2C_INTEGRATION
        expect(external_write(0x56,64),"RX event coincides with W1C");
        expect(rd(64)&128,"new RX_VALID wins over W1C on sampling boundary");
        expect(external_write(0x78,20),"RX event coincides with IACK");
        expect(irq,"new IF wins over IACK on sampling boundary");
#endif
        external_stop();expect((rd(64)&40)==32,"STOP sticky and ACTIVE cleared");
        wr(64,255);wr(16,0x5a);external_start();expect(external_write(0xa5),"slave read address ACK");
        expect(external_read(false,0xc3)==0x5a,"mid-byte refill does not change latched TX byte");
        expect(external_read(false)==0xc3,"refill becomes next TX byte");
        expect(external_read(true)==0xc3,"slave repeats TX without refill");external_stop();
        expect((rd(64)&0x76)==0x76,"TX_DONE, NACK, STOP, direction and match flags");
        external_start();expect(!external_write(0xa6),"slave rejects address mismatch");external_stop();
        external_start();expect(!external_write(0),"general call unsupported");external_stop();
        wr(4,0);expect(scl_oe&&sda_oe,"disable releases lines");wr(20,1);expect(rd(12)&1,"disabled IACK ignored");
        wr(4,0xe0);wr(20,1);expect(!irq,"IACK after enabling follows corrected init sequence");
        reset();configure();
        external_start();launch(0xd0,0xff);wait(1,SC_US);
        expect((rd(12)&0x22)==2&&scl_oe&&sda_oe,"master waits without driving an occupied bus");
        external_stop();expect(!(done()&32),"another master's STOP permits START without false arbitration loss");
        launch(0xd0,0xff);wait(1,SC_US);wr(20,0xb1);
        expect(rd(20)==0xd0,"busy CMD write cannot replace accepted transfer");
        done();expect(scl&&sda,"original STOP retained despite competing command");
        wr(20,0xb0);expect(!(rd(12)&2),"RD and WR together ignored");
        wr(20,0x80);done();expect(rd(12)&64,"START-only owns bus");
        wr(20,0x40);done();expect(!(rd(12)&64),"STOP-only terminates owned bus");
        launch(0x90,0x55);wait(1,SC_US);wr(4,0);expect(!(rd(12)&2)&&scl_oe&&sda_oe,"disable aborts active transfer");
        reset();configure(true);wr(4,0);wr(52,7);wr(4,0xe0);
        peer_sda=false;wait(30,SC_NS);peer_sda=true;wait(200,SC_NS);
        expect(!(rd(12)&64)&&rd(64)==0,"programmable filter rejects short START pulse");
        reset();configure();wr(4,0);wr(4,0xc0);launch(0xd0,0xaa);
        expect((done()&0xa1)==0x81,"legacy timing completes NACK transfer without false arbitration loss");
        wr(4,0x80);expect(!irq&&(rd(12)&1),"interrupt mask preserves pending IF");wr(4,0xc0);expect(irq,"unmask exposes pending IF");
        reset();configure();launch(0x90,0xaa);wait(1,SC_US);
        reset_n=false;wait(1,SC_NS);settle();expect(scl_oe&&sda_oe&&!irq,"asynchronous reset releases outputs");
        finished=true;sc_stop();
    }
    void watchdog(){wait(2,SC_MS);expect(false,"simulation watchdog");sc_stop();}
};
