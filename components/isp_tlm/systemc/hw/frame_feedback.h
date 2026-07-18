#ifndef ISP_FRAME_FEEDBACK_H
#define ISP_FRAME_FEEDBACK_H

#include <systemc>

#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace isp_tlm {

struct frame_feedback {
    std::uint64_t applies_to_frame = 0;
    float awb_r_gain = 1.0f;
    float awb_b_gain = 1.0f;
    std::int32_t aec_feedback = 0;
    std::uint32_t dg_gain = 1;
};

class frame_feedback_latch {
public:
    static constexpr std::size_t record_capacity = 8;

    explicit frame_feedback_latch(bool enable_awb = true, bool enable_aec = true,
                                  bool consume_awb = true, bool consume_aec = true)
        : m_enabled_mask(static_cast<std::uint8_t>((enable_awb ? awb_bit : 0) |
                                                    (enable_aec ? aec_bit : 0))),
          m_consumer_mask(static_cast<std::uint8_t>(
              ((consume_awb && enable_awb) ? awb_bit : 0) |
              ((consume_aec && enable_aec) ? aec_bit : 0))) {
        m_records[0].used = true;
        m_records[0].ready = true;
        m_records[0].consumed_mask = m_consumer_mask;
        m_records[0].value = frame_feedback{};
        m_records[0].value.applies_to_frame = 0;
    }

    frame_feedback_latch(const frame_feedback_latch&) = delete;
    frame_feedback_latch& operator=(const frame_feedback_latch&) = delete;

    bool ready(std::uint64_t frame_id) const noexcept {
        const record* item = find(frame_id);
        return item != nullptr && item->ready;
    }

    frame_feedback snapshot(std::uint64_t frame_id) const {
        const record* item = find(frame_id);
        if (item == nullptr || !item->ready) {
            throw std::logic_error("feedback is not ready for this frame");
        }
        return item->value;
    }

    void consume_awb(std::uint64_t frame_id) {
        consume(frame_id, awb_bit);
    }

    void consume_aec(std::uint64_t frame_id) {
        consume(frame_id, aec_bit);
    }

    void commit_awb(std::uint64_t frame_id, float r_gain, float b_gain,
                    std::uint32_t dg_gain = 1) {
        if ((m_enabled_mask & awb_bit) == 0) {
            throw std::logic_error("AWB feedback producer is disabled");
        }
        record& item = record_for_source(frame_id);
        if ((item.committed_mask & awb_bit) != 0) {
            throw std::logic_error("AWB feedback already committed");
        }
        item.value.awb_r_gain = r_gain;
        item.value.awb_b_gain = b_gain;
        item.value.dg_gain = dg_gain;
        item.committed_mask = static_cast<std::uint8_t>(item.committed_mask | awb_bit);
        complete_if_ready(item);
    }

    void commit_aec(std::uint64_t frame_id, std::int32_t feedback) {
        commit_aec_impl(frame_id, feedback, nullptr);
    }

    void commit_aec(std::uint64_t frame_id, std::int32_t feedback,
                    std::uint32_t dg_gain) {
        commit_aec_impl(frame_id, feedback, &dg_gain);
    }

    sc_core::sc_event& ready_event() noexcept { return m_ready_event; }
    const sc_core::sc_event& ready_event() const noexcept { return m_ready_event; }

    std::uint8_t enabled_producers() const noexcept { return m_enabled_mask; }

private:
    static constexpr std::uint8_t awb_bit = 1u;
    static constexpr std::uint8_t aec_bit = 2u;

    struct record {
        frame_feedback value{};
        std::uint8_t committed_mask = 0;
        std::uint8_t consumed_mask = 0;
        bool used = false;
        bool ready = false;
    };

    void commit_aec_impl(std::uint64_t frame_id, std::int32_t feedback,
                         const std::uint32_t* dg_gain) {
        if ((m_enabled_mask & aec_bit) == 0) {
            throw std::logic_error("AEC feedback producer is disabled");
        }
        record& item = record_for_source(frame_id);
        if ((item.committed_mask & aec_bit) != 0) {
            throw std::logic_error("AEC feedback already committed");
        }
        item.value.aec_feedback = feedback;
        if (dg_gain != nullptr) {
            item.value.dg_gain = *dg_gain;
        }
        item.committed_mask = static_cast<std::uint8_t>(item.committed_mask | aec_bit);
        complete_if_ready(item);
    }

    const record* find(std::uint64_t frame_id) const noexcept {
        for (const record& item : m_records) {
            if (item.used && item.value.applies_to_frame == frame_id) {
                return &item;
            }
        }
        return nullptr;
    }

    record* find(std::uint64_t frame_id) noexcept {
        for (record& item : m_records) {
            if (item.used && item.value.applies_to_frame == frame_id) {
                return &item;
            }
        }
        return nullptr;
    }

    void consume(std::uint64_t frame_id, std::uint8_t consumer_bit) {
        if ((m_consumer_mask & consumer_bit) == 0) {
            throw std::logic_error("feedback consumer is disabled");
        }
        record* item = find(frame_id);
        if (item == nullptr || !item->ready) {
            throw std::logic_error("feedback is not ready for this frame");
        }
        item->consumed_mask = static_cast<std::uint8_t>(
            item->consumed_mask | consumer_bit);
    }

    record& record_for_source(std::uint64_t source_frame) {
        if (source_frame == std::numeric_limits<std::uint64_t>::max()) {
            throw std::logic_error("feedback frame id overflow");
        }
        const std::uint64_t target_frame = source_frame + 1;
        for (record& item : m_records) {
            if (item.used && item.value.applies_to_frame == target_frame) {
                if (item.ready) {
                    throw std::logic_error("feedback frame is already ready");
                }
                return item;
            }
        }

        record* free_record = nullptr;
        record* oldest = nullptr;
        for (record& item : m_records) {
            if (!item.used) {
                free_record = &item;
                break;
            }
            if (item.value.applies_to_frame != 0 && item.ready &&
                (item.consumed_mask & m_consumer_mask) == m_consumer_mask &&
                (oldest == nullptr || item.value.applies_to_frame < oldest->value.applies_to_frame)) {
                oldest = &item;
            }
        }
        record* selected = free_record != nullptr ? free_record : oldest;
        if (selected == nullptr) {
            throw std::logic_error("feedback latch record storage is full");
        }
        selected->used = true;
        selected->ready = false;
        selected->committed_mask = 0;
        selected->consumed_mask = 0;
        selected->value = frame_feedback{};
        selected->value.applies_to_frame = target_frame;
        return *selected;
    }

    void complete_if_ready(record& item) {
        if (item.committed_mask == m_enabled_mask) {
            item.ready = true;
            m_ready_event.notify(sc_core::SC_ZERO_TIME);
        }
    }

    std::array<record, record_capacity> m_records{};
    std::uint8_t m_enabled_mask = static_cast<std::uint8_t>(awb_bit | aec_bit);
    std::uint8_t m_consumer_mask = static_cast<std::uint8_t>(awb_bit | aec_bit);
    sc_core::sc_event m_ready_event;
};

} // namespace isp_tlm

#endif // ISP_FRAME_FEEDBACK_H
