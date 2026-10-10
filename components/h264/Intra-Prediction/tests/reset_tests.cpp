#include <h264/intra/intra_tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <iostream>
#include <stdexcept>
using namespace h264::intra;
using namespace sc_core;

struct ResetBench : sc_module {
    tlm_utils::simple_initiator_socket<ResetBench> driver{"driver"};
    sc_signal<bool,SC_MANY_WRITERS> reset_n{"reset_n"},enable{"enable"};
    IntraTlm dut;
    h264::ResetDomain domain;
    sc_event trigger;
    unsigned action=0,after=5,checks=0;
    bool passed=false,finished=false;
    SC_HAS_PROCESS(ResetBench);
    static Options options() {
        Options o; o.coded_width=32; o.coded_height=32;
        o.reference_latency=o.candidate_latency=o.compare_latency=
            o.replay_latency=o.feedback_latency=sc_time(20,SC_NS);
        return o;
    }
    explicit ResetBench(sc_module_name name):sc_module(name),dut("dut",options()) {
        driver.bind(dut.target_socket); dut.reset_n(reset_n); dut.frame_enable(enable);
        SC_THREAD(run); SC_THREAD(resetter); SC_THREAD(watchdog);
    }
    void require(bool ok,const char* why) { ++checks; if(!ok) throw std::runtime_error(why); }
    void resetter() {
        while(true) {
            wait(trigger); wait(after,SC_NS);
            if(action==0) domain.assert_reset();
            else if(action==1) reset_n.write(false);
            else if(action==2) { enable.write(false); wait(SC_ZERO_TIME); enable.write(true); }
            else domain.release(); // Valid notification must not shorten a wait.
        }
    }
    void start() {
        enable.write(false); reset_n.write(true); wait(1,SC_NS);
        enable.write(true); wait(1,SC_NS); domain.release();
    }
    tlm::tlm_response_status job(Operation op,Extension& ext,h264::EpochExtension* epoch,
                                  sc_time incoming=SC_ZERO_TIME,unsigned length=16) {
        std::vector<uint8_t> data(length,17);
        tlm::tlm_generic_payload tx;
        tx.set_command(op==Operation::Replay ? tlm::TLM_READ_COMMAND:tlm::TLM_WRITE_COMMAND);
        tx.set_address(uint64_t(op)); tx.set_data_ptr(data.data());
        tx.set_data_length(length); tx.set_streaming_width(length); tx.set_extension(&ext);
        if(epoch) tx.set_extension(epoch);
        driver->b_transport(tx,incoming);
        tx.clear_extension<Extension>(); if(epoch) tx.clear_extension<h264::EpochExtension>();
        require(!dut.transport_active(),"owner releases transport after response");
        return tx.get_response_status();
    }
    void watchdog() { wait(100,SC_US); if(!finished) { std::cerr<<"FAIL reset watchdog\n"; sc_stop(); } }
    void run() {
        try {
            // 11 wait sites x shared/local/frame cancellation, and one benign
            // shared-domain notification at each site (44 independent cases).
            for(unsigned kind=0;kind<4;++kind) for(unsigned stage=0;stage<11;++stage) {
                start(); h264::EpochExtension epoch(domain); Extension ext;
                require(job(Operation::ImportReconstructed,ext,&epoch)==tlm::TLM_OK_RESPONSE,"seed previous decoded context");
                ext.block.x=4;
                Operation op=Operation::Evaluate;
                sc_time incoming=SC_ZERO_TIME;
                if(stage==0) incoming=sc_time(100,SC_NS);
                if(stage>=4 && stage<=7) {
                    require(job(Operation::Evaluate,ext,&epoch)==tlm::TLM_OK_RESPONSE,"pending reset setup");
                    op=stage<=5 ? Operation::Replay:Operation::Reconstruct;
                    if(stage>=6) require(job(Operation::Replay,ext,&epoch)==tlm::TLM_OK_RESPONSE,"accepted replay reset setup");
                    if(stage==4 || stage==6) incoming=sc_time(100,SC_NS);
                }
                if(stage>=8) {
                    op=Operation::ImportReconstructed;
                    if(stage==8) incoming=sc_time(100,SC_NS);
                }
                // stage10 specifically cancels Evaluate of a large block.
                if(stage==10) { op=Operation::Evaluate; ext.block={Plane::Y,Kind::Luma16x16,16,16}; }
                const unsigned length=stage==10 ? 256:16;
                const Token old=ext.token;
                action=kind; after=stage==2 ? 25:stage==3 ? 45:5;
                trigger.notify(SC_ZERO_TIME);
                const auto begin=sc_time_stamp();
                const auto response=job(op,ext,&epoch,incoming,length);
                const auto elapsed=sc_time_stamp()-begin;
                if(kind<3) {
                    require(response==tlm::TLM_GENERIC_ERROR_RESPONSE,"reset returns cancelled response");
                    require(elapsed==sc_time(after,SC_NS),"reset interrupts each wait immediately");
                    require(!dut.ready() && !dut.block_done() && !dut.core().busy(),"no obsolete winner or done");
                    if(dut.core().frame_active())
                        require(!dut.core().resolve({Plane::Y,Kind::Luma4x4,4,0}).has_left,"new frame clears previous decoded context");
                    domain.release(); h264::EpochExtension fresh(domain); ext.token=old;
                    require(job(Operation::Replay,ext,&fresh)==tlm::TLM_GENERIC_ERROR_RESPONSE,"old token rejected with new epoch");
                    start(); fresh=h264::EpochExtension(domain); ext=Extension{};
                    require(job(Operation::Evaluate,ext,&fresh)==tlm::TLM_OK_RESPONSE,"recovery evaluation");
                    require(job(Operation::Replay,ext,&fresh)==tlm::TLM_OK_RESPONSE,"recovery replay");
                    require(job(Operation::Reconstruct,ext,&fresh)==tlm::TLM_OK_RESPONSE && dut.block_done(),"recovery completion");
                } else {
                    require(response==tlm::TLM_OK_RESPONSE,"benign notification keeps valid work");
                    unsigned service=20;
                    if(op==Operation::Evaluate) service=stage==10 ? 60:140;
                    require(elapsed==incoming+sc_time(service,SC_NS),"benign wakeup preserves complete stage latency");
                }
            }
            // Stale epoch / invalid token / malformed Replay must never cancel
            // the pending owner just because validation runs before a wait.
            start(); h264::EpochExtension live(domain); Extension ext;
            require(job(Operation::Evaluate,ext,&live)==tlm::TLM_OK_RESPONSE,"isolation setup");
            const auto saved=ext.token;
            h264::ResetDomain other; other.release(); h264::EpochExtension stale(other); other.assert_reset();
            require(job(Operation::Replay,ext,&stale)==tlm::TLM_GENERIC_ERROR_RESPONSE,"stale request rejected");
            require(dut.ready() && dut.core().replay(saved).token==saved,"stale request preserves winner");
            ++ext.token.sequence;
            require(job(Operation::Replay,ext,&live)==tlm::TLM_GENERIC_ERROR_RESPONSE,"wrong token rejected");
            ext.token=saved;
            require(job(Operation::Replay,ext,&live,SC_ZERO_TIME,15)==tlm::TLM_BURST_ERROR_RESPONSE,"malformed replay rejected");
            require(dut.ready() && dut.core().busy(),"malformed request preserves pending owner");
            require(job(Operation::Replay,ext,&live)==tlm::TLM_OK_RESPONSE,"valid replay after rejects");
            require(job(Operation::Reconstruct,ext,&live)==tlm::TLM_OK_RESPONSE,"valid feedback after rejects");
            passed=true; std::cout<<"PASS reset matrix: "<<checks<<" checks at "<<sc_time_stamp()<<'\n';
        } catch(const std::exception& e) { std::cerr<<"FAIL reset matrix: "<<e.what()<<'\n'; }
        finished=true; sc_stop();
    }
};
int sc_main(int,char**) { ResetBench bench("bench"); sc_start(); return bench.passed ? 0:1; }
