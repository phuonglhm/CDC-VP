#pragma once

#include <cstdint>

#include "block.h"
#include "fme.h"
#include "fme_result.h"
#include "frame.h"
#include "ime.h"
#include "ime_result.h"
#include "mode_decision.h"
#include "posi.h"
#include "prediction_to_rec_packet.h"
#include "prediction_result.h"
#include "prei.h"
#include "prei_result.h"

namespace cdc::components {

struct video_encoder_tlm_trace {
    prei_result prei_info;
    prediction_result best_intra;
    ime_result ime_info;
    fme_result fme_info;
    prediction_result best_inter;
};

struct video_encoder_tlm_result {
    bool valid = false;

    block region {};
    std::uint32_t qp = INIT_QP;
    mode_decision_result final_decision;
    prediction_result selected_prediction;
    prediction_to_rec_result rec_request;
    video_encoder_tlm_trace trace;

    static video_encoder_tlm_result invalid()
    {
        return video_encoder_tlm_result {};
    }
};

class video_encoder_tlm {
public:
    video_encoder_tlm() = default;

    prediction_result run_intra(const frame& input,
                                const frame& reconstructed,
                                const block& region,
                                std::uint32_t qp = INIT_QP) const;

    fme_result run_inter(const frame& input,
                         const frame& reference,
                         const block& region,
                         std::uint32_t qp = INIT_QP) const;

    video_encoder_tlm_result run_prediction(const frame& input,
                                            const frame& reconstructed,
                                            const frame& reference,
                                            const block& region,
                                            std::uint32_t qp = INIT_QP) const;

private:
    prei prei_;
    posi posi_;
    ime ime_;
    fme fme_;
    mode_decision mode_decision_;
    prediction_to_rec_packet prediction_to_rec_packet_;
};

} // namespace cdc::components
