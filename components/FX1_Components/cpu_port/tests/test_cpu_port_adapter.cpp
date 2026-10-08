#include "fx1/cpu_port_adapter.h"
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include <iostream>
#include <string>

namespace {
unsigned checks = 0, failures = 0;
void expect(bool ok, const std::string& what) {
    ++checks;
    if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; }
}

// Answers with `reply`; `silent` leaves the payload untouched (forgets to answer).
struct Responder : sc_core::sc_module {
    tlm_utils::simple_target_socket<Responder> socket{"socket"};
    tlm::tlm_response_status reply = tlm::TLM_OK_RESPONSE;
    bool silent = false;
    unsigned calls = 0, seen_width = 0, dbg_width = 0;
    explicit Responder(sc_core::sc_module_name n) : sc_module(n) {
        socket.register_b_transport(this, &Responder::b_transport);
        socket.register_transport_dbg(this, &Responder::transport_dbg);
    }
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time&) {
        ++calls;
        seen_width = tx.get_streaming_width();
        if (!silent) tx.set_response_status(reply);
    }
    unsigned transport_dbg(tlm::tlm_generic_payload& tx) {
        dbg_width = tx.get_streaming_width();
        return tx.get_data_length();
    }
};

struct Result {
    tlm::tlm_response_status status;
    bool integration_error;
    std::string message;
};

struct Tb : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<Tb> cpu{"cpu"};
    fx1::CpuPortAdapter adapter{"adapter"};
    Responder target{"target"};
    SC_HAS_PROCESS(Tb);
    explicit Tb(sc_core::sc_module_name n) : sc_module(n) {
        cpu.bind(adapter.target);
        adapter.out.bind(target.socket);
        SC_THREAD(run);
    }
    // A payload the way VP++ builds it: response pre-set to OK, width 0.
    Result send(tlm::tlm_response_status reply, unsigned length = 4,
                tlm::tlm_command command = tlm::TLM_READ_COMMAND, bool null_data = false,
                unsigned width = 0, unsigned char* enables = nullptr) {
        target.reply = reply;
        unsigned char data[8] = {};
        tlm::tlm_generic_payload tx;
        tx.set_command(command);
        tx.set_address(0x10010000);
        tx.set_data_ptr(null_data ? nullptr : data);
        tx.set_data_length(length);
        tx.set_streaming_width(width);
        tx.set_byte_enable_ptr(enables);
        tx.set_byte_enable_length(enables ? 1 : 0);
        tx.set_response_status(tlm::TLM_OK_RESPONSE);
        sc_core::sc_time delay;
        try {
            cpu->b_transport(tx, delay);
        } catch (const sc_core::sc_report& report) {
            return {tx.get_response_status(), true, report.what()};
        }
        return {tx.get_response_status(), false, {}};
    }
    void run() {
        // Valid guest accesses.
        auto r = send(tlm::TLM_OK_RESPONSE, 2);
        expect(r.status == tlm::TLM_OK_RESPONSE && !r.integration_error && target.seen_width == 2,
               "valid access: streaming width completed, OK kept");
        r = send(tlm::TLM_OK_RESPONSE, 4, tlm::TLM_WRITE_COMMAND, false, 8);
        expect(r.status == tlm::TLM_OK_RESPONSE && target.seen_width == 8, "a set, sufficient width is kept");
        expect(send(tlm::TLM_ADDRESS_ERROR_RESPONSE).status == tlm::TLM_ADDRESS_ERROR_RESPONSE,
               "address error passed through");
        expect(send(tlm::TLM_GENERIC_ERROR_RESPONSE).status == tlm::TLM_GENERIC_ERROR_RESPONSE,
               "generic error passed through");
        r = send(tlm::TLM_BURST_ERROR_RESPONSE);
        expect(r.status == tlm::TLM_GENERIC_ERROR_RESPONSE && !r.integration_error,
               "valid access refused with BURST -> guest fault");
        r = send(tlm::TLM_COMMAND_ERROR_RESPONSE, 4, tlm::TLM_WRITE_COMMAND);
        expect(r.status == tlm::TLM_GENERIC_ERROR_RESPONSE && !r.integration_error,
               "valid write refused with COMMAND -> guest fault");
        expect(adapter.mapped_errors() == 2 && adapter.integration_errors() == 0, "two mappings, no defects");

        // Integration errors from the target side (reviewer G2-R2/R3).
        target.silent = true;
        r = send(tlm::TLM_OK_RESPONSE);
        expect(r.integration_error && r.message.find("no target set a response") != std::string::npos,
               "silent target with VP++'s pre-set OK is an integration error");
        target.silent = false;
        r = send(tlm::TLM_INCOMPLETE_RESPONSE);
        expect(r.integration_error, "explicit INCOMPLETE is an integration error");
        r = send(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        expect(r.integration_error && r.message.find("byte-enable") != std::string::npos,
               "byte-enable error for a payload without byte enables is a model defect");

        // Malformed CPU payloads: rejected before the target sees them.
        const unsigned before = target.calls;
        unsigned char enable = 0xFF;
        expect(send(tlm::TLM_OK_RESPONSE, 4, tlm::TLM_IGNORE_COMMAND).integration_error, "IGNORE command");
        expect(send(tlm::TLM_OK_RESPONSE, 4, tlm::TLM_READ_COMMAND, true).integration_error, "null data");
        expect(send(tlm::TLM_OK_RESPONSE, 3).integration_error, "length 3");
        expect(send(tlm::TLM_OK_RESPONSE, 0).integration_error, "length 0");
        expect(send(tlm::TLM_OK_RESPONSE, 4, tlm::TLM_READ_COMMAND, false, 0, &enable).integration_error,
               "byte enables from the CPU side");
        expect(send(tlm::TLM_OK_RESPONSE, 4, tlm::TLM_READ_COMMAND, false, 1).integration_error,
               "streaming width below the length");
        expect(target.calls == before, "malformed payloads never reach the target");
        expect(adapter.integration_errors() == 9 && adapter.mapped_errors() == 2, "defects counted, not mapped");

        unsigned char data[4] = {};
        tlm::tlm_generic_payload dbg;
        dbg.set_command(tlm::TLM_READ_COMMAND);
        dbg.set_data_ptr(data);
        dbg.set_data_length(4);
        expect(cpu->transport_dbg(dbg) == 4 && target.dbg_width == 4, "debug payload completed too");
        sc_core::sc_stop();
    }
};
} // namespace

int sc_main(int, char**) {
    // Integration errors must throw (the default); keep the log quiet here.
    sc_core::sc_report_handler::set_actions(sc_core::SC_ERROR, sc_core::SC_THROW);
    Tb tb("tb");
    sc_core::sc_start();
    std::cout << "fx1_cpu_port: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
