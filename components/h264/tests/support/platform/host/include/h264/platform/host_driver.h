#pragma once
#include <h264/control/register_map.h>
#include <h264/types.h>
#include <tlm_utils/simple_initiator_socket.h>
namespace h264 {
struct ActivationResult {
    bool timed_out=false, quiescent=false;
    uint32_t status=0, words=0;
    std::array<uint32_t,11> registers{};
    std::vector<unsigned char> output;
    std::string diagnostic;
};
class HostDriver : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<HostDriver> registers{"registers"};
    tlm_utils::simple_initiator_socket<HostDriver> memory{"memory"};
    // Methods take PDF register offsets; only the platform supplies a physical base.
    explicit HostDriver(sc_core::sc_module_name name,uint32_t csr_base=0)
        : sc_module(name),csr_base_(csr_base) {
        if (csr_base%4 || uint64_t(csr_base)+reg::STM_LEN>0xffffffffULL)
            throw std::invalid_argument("invalid CSR base");
    }
    SequenceParameters programming;
    uint32_t read(uint32_t address);
    tlm::tlm_response_status write(uint32_t address,uint32_t value,unsigned mask=15);
    void transfer(bool write,uint32_t address,std::vector<unsigned char>& data);
    void configure(unsigned width,unsigned height,uint32_t cmb,uint32_t refm,uint32_t nal);
    void program_sequence();
    ActivationResult wait_for_completion(const sc_core::sc_signal_in_if<bool>& irq,
        sc_core::sc_time timeout,sc_core::sc_time recovery_grace=sc_core::sc_time(1,sc_core::SC_US));
    ActivationResult last_result;
    void save_diagnostics(const std::string& directory) const;
private:
    uint32_t csr_base_;
};
}
