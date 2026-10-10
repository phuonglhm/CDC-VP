#include <h264/intra/intra_tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace h264::intra;
using namespace sc_core;

struct BoundaryBench : sc_module {
    tlm_utils::simple_initiator_socket<BoundaryBench> driver{"driver"};
    sc_signal<bool> reset_n{"reset_n"},enable{"enable"};
    IntraTlm dut;
    h264::ResetDomain domain;
    sc_event arm,reset_finished;
    sc_time reset_delay;
    bool zero,returned=false,reset_done=false,reset_before_return=false,passed=false,finished=false;
    unsigned checks=0,cases=0,ready_events=0,done_events=0,equal_cancelled=0,equal_accepted=0;
    SC_HAS_PROCESS(BoundaryBench);
    static Options options(bool instant) {
        Options o; o.coded_width=32; o.coded_height=32;
        o.reference_latency=o.candidate_latency=o.compare_latency=o.replay_latency=o.feedback_latency=
            sc_time(instant ? 0:20,SC_NS);
        return o;
    }
    BoundaryBench(sc_module_name name,bool instant):sc_module(name),dut("dut",options(instant)),zero(instant) {
        driver.bind(dut.target_socket); dut.reset_n(reset_n); dut.frame_enable(enable);
        SC_THREAD(run); SC_THREAD(resetter); SC_THREAD(events); SC_THREAD(watchdog);
    }
    void require(bool ok,const char* why) { ++checks; if(!ok) throw std::runtime_error(why); }
    void resetter() {
        while(true) {
            wait(arm); wait(reset_delay);
            reset_before_return=!returned;
            domain.assert_reset(); reset_done=true; reset_finished.notify(SC_ZERO_TIME);
        }
    }
    void events() {
        while(true) {
            wait(dut.predictor_ready | dut.block_completed);
            if(dut.predictor_ready.triggered()) ++ready_events;
            if(dut.block_completed.triggered()) ++done_events;
        }
    }
    void watchdog() { wait(100,SC_US); if(!finished) { std::cerr<<"FAIL boundary watchdog\n"; sc_stop(); } }
    void start() {
        enable.write(false); reset_n.write(true); wait(1,SC_NS);
        enable.write(true); wait(1,SC_NS); domain.release();
    }
    tlm::tlm_response_status job(Operation op,Extension& ext,h264::EpochExtension& epoch,
                                   sc_time incoming=SC_ZERO_TIME) {
        std::vector<uint8_t> data(16,17);
        tlm::tlm_generic_payload tx;
        tx.set_command(op==Operation::Replay ? tlm::TLM_READ_COMMAND:tlm::TLM_WRITE_COMMAND);
        tx.set_address(uint64_t(op)); tx.set_data_ptr(data.data());
        tx.set_data_length(16); tx.set_streaming_width(16);
        tx.set_extension(&ext); tx.set_extension(&epoch);
        driver->b_transport(tx,incoming);
        tx.clear_extension<Extension>(); tx.clear_extension<h264::EpochExtension>();
        require(!dut.transport_active(),"boundary response drains transport");
        if(tx.is_response_ok() && op==Operation::Replay)
            require(data==std::vector<uint8_t>(16,128),"accepted boundary Replay retains every predictor byte");
        return tx.get_response_status();
    }
    void one(unsigned site,int offset,bool register_reset_first) {
        start(); Extension ext; h264::EpochExtension epoch(domain);
        Operation op=site==4 ? Operation::Replay:site==5 ? Operation::Reconstruct:
                     site==6 ? Operation::ImportReconstructed:Operation::Evaluate;
        if(site==4 || site==5) {
            require(job(Operation::Evaluate,ext,epoch)==tlm::TLM_OK_RESPONSE,"boundary owner setup");
            if(site==5) require(job(Operation::Replay,ext,epoch)==tlm::TLM_OK_RESPONSE,"boundary feedback setup");
        }
        wait(SC_ZERO_TIME); wait(SC_ZERO_TIME);
        const auto old_ready=ready_events,old_done=done_events;
        const auto old_generation=dut.core().generation(); const Token old_token=ext.token;
        const bool old_busy=dut.core().busy(),old_flag=dut.ready();
        // Reference/candidate/compare, incoming, Replay, feedback and Import.
        const sc_time incoming=site==3 ? sc_time(20,SC_NS):SC_ZERO_TIME;
        const sc_time deadline=zero ? incoming:sc_time(site<=2 ? 20*(site+1):20,SC_NS);
        const sc_time service=incoming+(zero ? SC_ZERO_TIME:sc_time(site<=3 ? 60:20,SC_NS));
        const sc_time tick=sc_get_time_resolution();
        reset_delay=offset<0 ? deadline-tick:offset>0 ? deadline+tick:deadline;
        returned=false; reset_done=false; reset_before_return=false;
        arm.notify(SC_ZERO_TIME);
        // Queue reset's timed wakeup before or after transport's timeout. The
        // contract orders cancellation against acceptance, not by timestamp alone.
        if(register_reset_first) { wait(SC_ZERO_TIME); wait(SC_ZERO_TIME); }
        if(zero && register_reset_first && deadline==SC_ZERO_TIME)
            while(!reset_done) wait(reset_finished);
        const bool stale_entry=!epoch.valid();
        const auto begin=sc_time_stamp();
        const auto response=job(op,ext,epoch,incoming); returned=true;
        const auto elapsed=sc_time_stamp()-begin;
        if(!reset_done) wait(reset_finished);
        wait(SC_ZERO_TIME); wait(SC_ZERO_TIME);
        const bool cancelled=reset_before_return;
        require(response==(cancelled ? tlm::TLM_GENERIC_ERROR_RESPONSE:tlm::TLM_OK_RESPONSE),
            "boundary response agrees with reset before or after acceptance");
        if(cancelled) {
            require(!ext.decision && !ext.block_done,"cancelled boundary request exposes no result or completion");
            if(stale_entry) {
                require(dut.core().generation()==old_generation && dut.core().frame_active()
                    && dut.core().busy()==old_busy && dut.ready()==old_flag,
                    "zero-latency stale entry preserves existing valid local owner");
            } else {
                require(!dut.core().frame_active() && !dut.core().busy() && !dut.ready() && !dut.block_done(),
                    "live cancellation invalidates winner and reconstructed frame before response");
                require(elapsed==reset_delay,"boundary cancellation wakes at exact reset time");
            }
        } else {
            require(elapsed==service,"accepted boundary request keeps configured duration");
            if(op==Operation::Evaluate) require(dut.ready() && dut.core().busy() && !dut.block_done(),"accepted Evaluate is ready, not done");
            if(op==Operation::Replay) require(dut.ready() && !dut.block_done(),"accepted Replay is not reconstruction done");
            if(op==Operation::Reconstruct) require(ext.block_done && dut.block_done()
                && dut.core().committed_mode(Block{})==2,"accepted feedback commits winning mode before done");
            if(op==Operation::Reconstruct || op==Operation::ImportReconstructed)
                require(dut.core().resolve({Plane::Y,Kind::Luma4x4,4,0}).left[0]==17,
                    "accepted feedback/import owns decoded neighbor bytes");
        }
        require(ready_events==old_ready+unsigned(!cancelled && op==Operation::Evaluate),"no obsolete predictor-ready event");
        require(done_events==old_done+unsigned(!cancelled && op==Operation::Reconstruct),"no obsolete block-completed event");
        if(offset==0) { if(cancelled) ++equal_cancelled; else ++equal_accepted; }
        // Shared reset after an idle acceptance needs the documented local
        // frame cooperation. Verify recovery and rejection of previous tokens.
        start(); h264::EpochExtension fresh(domain); ext.token=old_token;
        require(job(Operation::Replay,ext,fresh)==tlm::TLM_GENERIC_ERROR_RESPONSE,"new frame rejects old boundary token");
        ext=Extension{};
        require(job(Operation::Evaluate,ext,fresh)==tlm::TLM_OK_RESPONSE,"boundary recovery Evaluate");
        require(job(Operation::Replay,ext,fresh)==tlm::TLM_OK_RESPONSE,"boundary recovery Replay");
        require(job(Operation::Reconstruct,ext,fresh)==tlm::TLM_OK_RESPONSE && dut.block_done(),"boundary recovery feedback");
        ++cases;
    }
    void run() {
        try {
            for(unsigned site=0;site<7;++site) for(bool first:{false,true}) {
                if(zero) one(site,0,first);
                else for(int offset:{-1,0,1}) one(site,offset,first);
            }
            require(equal_cancelled>0 && equal_accepted>0,"equal-timestamp suite exercises cancellation and accepted work");
            passed=true;
            std::cout<<"PASS reset boundaries (zero="<<zero<<"): "<<checks<<" checks, "<<cases
                <<" cases, equal cancelled="<<equal_cancelled<<", accepted="<<equal_accepted
                <<", resolution="<<sc_get_time_resolution()<<" at "<<sc_time_stamp()<<'\n';
        } catch(const std::exception& e) { std::cerr<<"FAIL reset boundaries: "<<e.what()<<'\n'; }
        finished=true; sc_stop();
    }
};
int sc_main(int argc,char** argv) {
    BoundaryBench bench("bench",argc>1 && std::string(argv[1])=="zero"); sc_start(); return bench.passed ? 0:1;
}
