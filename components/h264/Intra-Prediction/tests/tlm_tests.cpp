#include <h264/intra/intra_tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <iostream>
#include <stdexcept>
using namespace h264::intra;
using namespace sc_core;
struct Bench : sc_module {
    tlm_utils::simple_initiator_socket<Bench> driver{"driver"};
    sc_signal<bool,SC_MANY_WRITERS> reset_n{"reset_n"}, frame_enable{"frame_enable"};
    IntraTlm dut;
    sc_event reset_request, contender_request, external_reset_request;
    h264::ResetDomain external_domain;
    bool frame_cancel=false, completed=false, passed=false, contender_done=false;
    unsigned checks=0;
    unsigned reset_delay_ns=2;
    SC_HAS_PROCESS(Bench);
    explicit Bench(sc_module_name name) : sc_module(name),dut("intra",options()) {
        driver.bind(dut.target_socket);
        // Contention is exercised by two threads through the same initiator.
        reset_n.write(false); frame_enable.write(false);
        dut.reset_n(reset_n); dut.frame_enable(frame_enable);
        SC_THREAD(run); SC_THREAD(reset_thread); SC_THREAD(contend); SC_THREAD(watchdog);
        SC_THREAD(external_reset_thread);
    }
    static Options options() { Options o; o.coded_width=48; o.coded_height=32; return o; }
    void require(bool ok, const char* what) {
        ++checks; if (!ok) throw std::runtime_error(what);
    }
    tlm::tlm_response_status transact(Operation operation, std::vector<uint8_t>& data,
            Extension& ext, sc_time incoming=SC_ZERO_TIME,
            tlm::tlm_command forced=tlm::TLM_IGNORE_COMMAND, bool enables=false,
            unsigned stream=0, h264::EpochExtension* epoch=nullptr) {
        tlm::tlm_generic_payload tx;
        tx.set_command(forced==tlm::TLM_IGNORE_COMMAND ?
            (operation==Operation::Replay ? tlm::TLM_READ_COMMAND : tlm::TLM_WRITE_COMMAND) : forced);
        tx.set_address(uint64_t(operation)); tx.set_data_ptr(data.data());
        tx.set_data_length(unsigned(data.size())); tx.set_streaming_width(stream ? stream : unsigned(data.size()));
        uint8_t mask=0xff;
        if (enables) { tx.set_byte_enable_ptr(&mask); tx.set_byte_enable_length(1); }
        tx.set_extension(&ext);
        if (epoch) tx.set_extension(epoch);
        driver->b_transport(tx,incoming);
        tx.clear_extension<Extension>();
        if (epoch) tx.clear_extension<h264::EpochExtension>();
        return tx.get_response_status();
    }
    void reset_thread() {
        while (true) {
            wait(reset_request); wait(reset_delay_ns,SC_NS);
            if (frame_cancel) {
                frame_enable.write(false); wait(1,SC_NS); frame_enable.write(true);
            } else reset_n.write(false);
        }
    }
    void contend() {
        while (true) {
            wait(contender_request); wait(1,SC_NS);
            Extension ext; std::vector<uint8_t> pixels(16,9);
            const auto response=transact(Operation::Evaluate,pixels,ext);
            if (response!=tlm::TLM_GENERIC_ERROR_RESPONSE) { SC_REPORT_ERROR("intra","concurrent request accepted"); }
            contender_done=true;
        }
    }
    void external_reset_thread() {
        while (true) { wait(external_reset_request); wait(12,SC_NS); external_domain.assert_reset(); }
    }
    void start() {
        frame_enable.write(false); wait(1,SC_NS);
        reset_n.write(true); wait(1,SC_NS);
        frame_enable.write(true); wait(1,SC_NS);
    }
    void watchdog() { wait(10,SC_US); if (!completed) { std::cerr<<"FAIL watchdog\n"; sc_stop(); } }
    void run() {
        try {
            wait(1,SC_NS); start();
            Extension ext;
            std::vector<uint8_t> source(16,255), replay(16,0), decoded(16,17);
            auto begin=sc_time_stamp();
            require(transact(Operation::Evaluate,source,ext,sc_time(7,SC_NS))==tlm::TLM_OK_RESPONSE,"evaluate response");
            require(sc_time_stamp()-begin==sc_time(22,SC_NS),"reference/candidate/compare and incoming delays");
            require(ext.decision && ext.decision->mode==2 && ext.decision->predictor[0]==128,"origin winner");
            require(dut.ready() && !dut.block_done() && dut.core().busy(),"winner ready is not block done");
            const auto token=ext.token;
            require(transact(Operation::Evaluate,source,ext)==tlm::TLM_GENERIC_ERROR_RESPONSE,"pending block rejects overlap");
            ext.token=token;
            require(transact(Operation::Reconstruct,decoded,ext)==tlm::TLM_GENERIC_ERROR_RESPONSE,"feedback before acceptance rejected");
            wait(50,SC_NS);
            require(transact(Operation::Replay,replay,ext)==tlm::TLM_OK_RESPONSE && replay[0]==128,"replay after downstream stall");
            const auto snapshot=*ext.decision;
            wait(70,SC_NS);
            require(transact(Operation::Replay,replay,ext)==tlm::TLM_OK_RESPONSE,"repeat replay");
            require(ext.decision->predictor==snapshot.predictor && ext.decision->mode==snapshot.mode,"stalled replay stable");
            require(!dut.block_done(),"stall never fabricates done");
            begin=sc_time_stamp();
            require(transact(Operation::Reconstruct,decoded,ext)==tlm::TLM_OK_RESPONSE,"feedback accepted");
            require(sc_time_stamp()-begin==sc_time(10,SC_NS) && ext.block_done && dut.block_done(),"done after neighbor RAM latency");
            require(dut.core().committed_mode(Block{})==2,"committed mode");
            ext.block={Plane::Y,Kind::Luma4x4,4,0}; source.assign(16,17);
            require(transact(Operation::Evaluate,source,ext)==tlm::TLM_OK_RESPONSE,"next block");
            require(ext.decision->mode==1 && ext.decision->references.left[0]==17,"ITQ pixels feed next prediction");
            const auto old=ext.token;
            start();
            ext.token=old;
            require(transact(Operation::Replay,replay,ext)==tlm::TLM_GENERIC_ERROR_RESPONSE,"frame invalidates replay token");
            require(!dut.core().committed_mode(Block{}),"frame clears mode RAM");
            const auto generation=dut.core().generation();
            frame_enable.write(true); wait(5,SC_NS);
            require(dut.core().generation()==generation,"stale high enable never restarts frame");
            ext=Extension{};
            require(transact(Operation::Evaluate,source,ext,SC_ZERO_TIME,tlm::TLM_READ_COMMAND)==tlm::TLM_COMMAND_ERROR_RESPONSE,"command validation");
            require(transact(Operation::Evaluate,source,ext,SC_ZERO_TIME,tlm::TLM_IGNORE_COMMAND,true)==tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,"byte enable validation");
            require(transact(Operation::Evaluate,source,ext,SC_ZERO_TIME,tlm::TLM_IGNORE_COMMAND,false,1)==tlm::TLM_BURST_ERROR_RESPONSE,"streaming validation");
            std::vector<uint8_t> short_block(15);
            require(transact(Operation::Evaluate,short_block,ext)==tlm::TLM_BURST_ERROR_RESPONSE,"length validation");
            require(transact(static_cast<Operation>(99),source,ext)==tlm::TLM_ADDRESS_ERROR_RESPONSE,"address validation");
            h264::ResetDomain domain; domain.release(); h264::EpochExtension stale(domain); domain.assert_reset();
            require(transact(Operation::Evaluate,source,ext,SC_ZERO_TIME,tlm::TLM_IGNORE_COMMAND,false,0,&stale)==tlm::TLM_GENERIC_ERROR_RESPONSE,"shared epoch cancellation");
            // Cancel at reference fetch, evaluation, replay and reconstruction waits.
            for (unsigned stage=0;stage<4;++stage) {
                start(); ext=Extension{};
                if (stage>=2) {
                    require(transact(Operation::Evaluate,source,ext)==tlm::TLM_OK_RESPONSE,"reset setup evaluation");
                    if (stage==3) require(transact(Operation::Replay,replay,ext)==tlm::TLM_OK_RESPONSE,"reset setup replay");
                }
                frame_cancel=(stage%2)==1;
                reset_delay_ns=stage==1 ? 12 : 2;
                reset_request.notify(SC_ZERO_TIME);
                const auto operation=stage<2 ? Operation::Evaluate : stage==2 ? Operation::Replay : Operation::Reconstruct;
                auto& data=stage<2 ? source : stage==2 ? replay : decoded;
                const auto response=transact(operation,data,ext);
                require(response==tlm::TLM_GENERIC_ERROR_RESPONSE,"reset cancels in-flight response");
                require(!dut.block_done() && !dut.ready() && !dut.core().busy(),"no stale completion after cancellation");
                wait(2,SC_NS);
            }
            start(); ext=Extension{};
            external_domain.release(); h264::EpochExtension live_epoch(external_domain);
            external_reset_request.notify(SC_ZERO_TIME);
            require(transact(Operation::Evaluate,source,ext,SC_ZERO_TIME,tlm::TLM_IGNORE_COMMAND,
                             false,0,&live_epoch)==tlm::TLM_GENERIC_ERROR_RESPONSE,"external reset during candidate evaluation");
            require(!dut.core().busy() && !dut.ready() && !dut.block_done(),"external reset clears partial decision");
            start(); ext=Extension{};
            frame_cancel=false; reset_delay_ns=2; reset_request.notify(SC_ZERO_TIME);
            require(transact(Operation::Evaluate,source,ext,sc_time(20,SC_NS))==tlm::TLM_GENERIC_ERROR_RESPONSE,"reset during incoming delay");
            require(!dut.core().busy(),"incoming cancellation has no partial block");
            start(); ext=Extension{};
            contender_done=false; contender_request.notify(SC_ZERO_TIME);
            require(transact(Operation::Evaluate,source,ext)==tlm::TLM_OK_RESPONSE,"owner survives contention");
            require(contender_done && dut.ready(),"concurrent response isolated");
            require(transact(Operation::Replay,replay,ext)==tlm::TLM_OK_RESPONSE,"contention replay");
            require(transact(Operation::Reconstruct,decoded,ext)==tlm::TLM_OK_RESPONSE,"contention feedback");
            // Import a decoded top row and check full-width upper-right in TLM.
            ext=Extension{}; ext.block={Plane::Y,Kind::Luma16x16,16,0}; std::vector<uint8_t> large(256,33);
            require(transact(Operation::ImportReconstructed,large,ext)==tlm::TLM_OK_RESPONSE,"decoded reference import");
            ext.block.x=32; large.assign(256,44);
            require(transact(Operation::ImportReconstructed,large,ext)==tlm::TLM_OK_RESPONSE,"right decoded reference import");
            ext.block={Plane::Y,Kind::Luma4x4,28,16}; source.assign(16,33);
            require(transact(Operation::Evaluate,source,ext)==tlm::TLM_OK_RESPONSE,"full width candidate");
            require(ext.decision->references.has_upper_right && ext.decision->references.top[4]==44,"TLM full-width reference map");
            require(ext.decision->candidates.size()==4,"latency follows legal candidate count");
            require(transact(Operation::Replay,replay,ext)==tlm::TLM_OK_RESPONSE,"full width replay");
            require(ext.decision->references.top[4]==44,"evaluate/replay same upper-right");
            for (const auto block : {Block{Plane::Y,Kind::Luma16x16,0,0},
                                     Block{Plane::U,Kind::Chroma8x8,0,0},
                                     Block{Plane::V,Kind::Chroma8x8,0,0}}) {
                start(); ext=Extension{}; ext.block=block;
                std::vector<uint8_t> pixels(block.size()*block.size(),128),output(pixels.size());
                require(transact(Operation::Evaluate,pixels,ext)==tlm::TLM_OK_RESPONSE,"large plane TLM evaluation");
                require(ext.decision->mode==(block.plane==Plane::Y ? 2u : 0u) && ext.decision->cost==0,"large plane DC golden");
                require(transact(Operation::Replay,output,ext)==tlm::TLM_OK_RESPONSE && output==pixels,"large plane replay bytes");
                require(transact(Operation::Reconstruct,pixels,ext)==tlm::TLM_OK_RESPONSE && ext.block_done,"large plane feedback");
            }
            passed=true; std::cout<<"PASS intra TLM: "<<checks<<" checks at "<<sc_time_stamp()<<"\n";
        } catch (const std::exception& e) { std::cerr<<"FAIL intra TLM: "<<e.what()<<"\n"; }
        completed=true; sc_stop();
    }
};
int sc_main(int,char**) {
    Bench bench("bench"); sc_start(); return bench.passed ? 0 : 1;
}
