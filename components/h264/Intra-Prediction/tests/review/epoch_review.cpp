#include <h264/intra/intra_tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <iostream>
#include <string>
using namespace h264::intra;
using namespace sc_core;

// Review regressions: ordinary assertions, deliberately not WILL_FAIL.
struct Review : sc_module {
    tlm_utils::simple_initiator_socket<Review> driver{"driver"};
    sc_signal<bool> reset_n{"reset_n"}, enable{"enable"};
    IntraTlm dut;
    h264::ResetDomain domain;
    sc_event trigger;
    bool incoming, passed=false;
    SC_HAS_PROCESS(Review);
    static Options options(bool incoming) {
        Options o;
        if (!incoming) o.reference_latency=sc_time(1,SC_US);
        return o;
    }
    Review(sc_module_name name,bool test_incoming)
        : sc_module(name),dut("dut",options(test_incoming)),incoming(test_incoming) {
        driver.bind(dut.target_socket);
        dut.reset_n(reset_n); dut.frame_enable(enable);
        SC_THREAD(run); SC_THREAD(resetter);
    }
    void resetter() { wait(trigger); wait(12,SC_NS); domain.assert_reset(); }
    tlm::tlm_response_status job(Operation op,Extension& ext,h264::EpochExtension* epoch,
                                  sc_time delay=SC_ZERO_TIME) {
        std::vector<uint8_t> pixels(16,128);
        tlm::tlm_generic_payload tx;
        tx.set_command(op==Operation::Replay ? tlm::TLM_READ_COMMAND : tlm::TLM_WRITE_COMMAND);
        tx.set_address(uint64_t(op)); tx.set_data_ptr(pixels.data());
        tx.set_data_length(16); tx.set_streaming_width(16); tx.set_extension(&ext);
        if (epoch) tx.set_extension(epoch);
        driver->b_transport(tx,delay);
        tx.clear_extension<Extension>();
        if (epoch) tx.clear_extension<h264::EpochExtension>();
        return tx.get_response_status();
    }
    void run() {
        reset_n.write(false); enable.write(false); wait(1,SC_NS);
        reset_n.write(true); wait(1,SC_NS); enable.write(true); wait(1,SC_NS);
        domain.release(); h264::EpochExtension epoch(domain); Extension ext;
        if (incoming && job(Operation::Evaluate,ext,&epoch)!=tlm::TLM_OK_RESPONSE) {
            std::cerr<<"FAIL setup\n"; sc_stop(); return;
        }
        const Token saved=ext.token;
        trigger.notify(SC_ZERO_TIME);
        const auto begin=sc_time_stamp();
        const auto status=job(incoming ? Operation::Replay : Operation::Evaluate,
                              ext,&epoch,incoming ? sc_time(100,SC_NS) : SC_ZERO_TIME);
        const auto elapsed=sc_time_stamp()-begin;
        const bool pending=dut.core().busy(), ready=dut.ready();
        bool stale_accepted=false, obsolete_committed=false;
        if (incoming) {
            // Simulate a caller attaching a fresh shared-domain epoch after reset.
            // The saved intra token must still be obsolete without a new frame.
            domain.release(); h264::EpochExtension fresh(domain); ext.token=saved;
            stale_accepted=job(Operation::Replay,ext,&fresh)==tlm::TLM_OK_RESPONSE;
            if (stale_accepted)
                obsolete_committed=job(Operation::Reconstruct,ext,&fresh)==tlm::TLM_OK_RESPONSE;
            passed=status==tlm::TLM_GENERIC_ERROR_RESPONSE && !pending && !ready
                && !stale_accepted && !obsolete_committed;
        } else {
            // Allow a delta to run the ResetDomain changed notification.
            passed=status==tlm::TLM_GENERIC_ERROR_RESPONSE && elapsed==sc_time(12,SC_NS);
        }
        std::cout<<"case="<<(incoming ? "incoming" : "wakeup")
                 <<" response="<<status<<" elapsed="<<elapsed
                 <<" busy="<<pending<<" ready="<<ready
                 <<" stale_replay_accepted="<<stale_accepted
                 <<" obsolete_feedback_committed="<<obsolete_committed<<'\n';
        std::cout<<(passed ? "PASS" : "FAIL")<<" shared epoch reset contract\n";
        sc_stop();
    }
};
int sc_main(int argc,char** argv) {
    Review review("review",argc>1 && std::string(argv[1])=="incoming");
    sc_start(); return review.passed ? 0 : 1;
}
