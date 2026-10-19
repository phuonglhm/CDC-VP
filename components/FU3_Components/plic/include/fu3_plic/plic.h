#pragma once
// FU3 AP_PLIC (ftel_plic): APB4, 32 nguồn ngắt, 1 target (CVA6 irq_i[0]), 8 mức priority
// Bus (PERIBUS_1, 0x4010_0000) đưa vào offset cục bộ
#include <cstdint>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include "fu3_plic/registers.h"

namespace fu3_plic {

constexpr unsigned kSources = FU3_PLIC_SOURCES;
constexpr unsigned kMaxPending = FU3_PLIC_MAX_PENDING_COUNT;

class Plic : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<Plic> socket{"socket"};
    sc_core::sc_in<bool> reset_n{"reset_n"};

    // i_plic_int_src[31:0]: bit k = Source ID k+1
    // Nguồn không dùng nối mức 0.
    sc_core::sc_vector<sc_core::sc_in<bool>> irq_sources{"irq_sources", kSources};

    // o_plic_irq[0] -> CVA6 irq_i[0] (M-mode external, cause 11)
    sc_core::sc_out<bool> irq{"irq"};

    SC_HAS_PROCESS(Plic);
    explicit Plic(sc_core::sc_module_name name);

    // Đọc trạng thái không side effect: đọc ID_0 ở đây KHÔNG claim
    std::uint32_t read_register(std::uint32_t offset) const;
    std::uint32_t pending() const;   // bit k = Source ID k+1 đang pending

private:
    std::uint32_t el_ = 0, ie_ = 0, threshold_ = 0;
    std::uint32_t priority_[4] = {};
    std::uint32_t in_service_ = 0;          // Đã claim, chưa complete
    std::uint32_t level_ip_ = 0;            // Pending đã chốt của nguồn level (giữ tới khi claim)
    unsigned edge_count_[kSources] = {};    // Số sườn lên đang chờ (tối đa kMaxPending)
    bool last_level_[kSources] = {};
    sc_core::sc_event changed_;

    unsigned priority_of(unsigned bit) const;
    unsigned best() const;                  // ID tốt nhất có thể claim: 0 = không có
    unsigned claim();
    void complete(std::uint32_t id);
    void reset_state();
    void sample();                          // Theo dõi nguồn
    void update();                          // Process duy nhất lái irq
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay);
    unsigned transport_dbg(tlm::tlm_generic_payload& tx);
    void access(tlm::tlm_generic_payload& tx, bool backdoor);
};

}  // namespace fu3_plic
