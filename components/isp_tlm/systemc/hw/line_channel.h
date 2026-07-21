#ifndef ISP_LINE_CHANNEL_H
#define ISP_LINE_CHANNEL_H

#include <systemc>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace isp_tlm {

enum class line_plane : std::uint8_t { raw, rgb, yuv444, y, uv };

struct line_meta {
    std::uint64_t frame_id = 0;
    std::uint32_t row = 0;
    std::uint32_t width_pixels = 0;
    std::uint32_t valid_samples = 0;
    std::uint32_t dst_offset_bytes = 0;
    line_plane plane = line_plane::raw;
    bool start_of_frame = false;
    bool end_of_frame = false;
};

template <class T>
class basic_line_view {
public:
    using value_type = T;

    basic_line_view() = default;
    basic_line_view(T* data, std::size_t size) : m_data(data), m_size(size) {}

    T* data() const { return m_data; }
    std::size_t size() const { return m_size; }
    T& operator[](std::size_t index) const {
        if (index >= m_size) {
            throw std::out_of_range("line view index");
        }
        return m_data[index];
    }

private:
    T* m_data = nullptr;
    std::size_t m_size = 0;
};

using slot_id = std::uint32_t;
constexpr slot_id invalid_slot = std::numeric_limits<slot_id>::max();

template <class T> class line_channel;
template <class T> class write_handle;
template <class T> class read_handle;

template <class T>
class write_handle {
public:
    write_handle() = default;
    write_handle(const write_handle&) = delete;
    write_handle& operator=(const write_handle&) = delete;

    write_handle(write_handle&& other) noexcept { move_from(std::move(other)); }
    write_handle& operator=(write_handle&& other) noexcept {
        if (this != &other) {
            cancel_noexcept();
            move_from(std::move(other));
        }
        return *this;
    }

    ~write_handle() { cancel_noexcept(); }

    basic_line_view<T> view(std::size_t length = 0);
    slot_id slot() const;
    bool valid() const noexcept;

private:
    friend class line_channel<T>;
    write_handle(line_channel<T>* channel, slot_id slot, std::uint64_t generation)
        : m_channel(channel), m_slot(slot), m_generation(generation) {}

    void cancel_noexcept() noexcept;
    void invalidate() noexcept {
        m_channel = nullptr;
        m_slot = invalid_slot;
        m_generation = 0;
    }
    void move_from(write_handle&& other) noexcept {
        m_channel = other.m_channel;
        m_slot = other.m_slot;
        m_generation = other.m_generation;
        other.invalidate();
    }

    line_channel<T>* m_channel = nullptr;
    slot_id m_slot = invalid_slot;
    std::uint64_t m_generation = 0;
};

template <class T>
class read_handle {
public:
    using element_type = std::remove_const_t<T>;

    read_handle() = default;
    read_handle(const read_handle&) = delete;
    read_handle& operator=(const read_handle&) = delete;

    read_handle(read_handle&& other) noexcept { move_from(std::move(other)); }
    read_handle& operator=(read_handle&& other) noexcept {
        if (this != &other) {
            release_noexcept();
            move_from(std::move(other));
        }
        return *this;
    }

    ~read_handle() { release_noexcept(); }

    basic_line_view<const element_type> view(std::size_t length = 0) const;
    slot_id slot() const;
    const line_meta& meta() const;
    bool valid() const noexcept;

private:
    friend class line_channel<element_type>;
    read_handle(line_channel<element_type>* channel, slot_id slot, std::uint64_t generation)
        : m_channel(channel), m_slot(slot), m_generation(generation) {}

    void release_noexcept() noexcept;
    void invalidate() noexcept {
        m_channel = nullptr;
        m_slot = invalid_slot;
        m_generation = 0;
    }
    void move_from(read_handle&& other) noexcept {
        m_channel = other.m_channel;
        m_slot = other.m_slot;
        m_generation = other.m_generation;
        other.invalidate();
    }

    line_channel<element_type>* m_channel = nullptr;
    slot_id m_slot = invalid_slot;
    std::uint64_t m_generation = 0;
};

template <class T>
class line_channel : public sc_core::sc_prim_channel {
public:
    using value_type = T;
    using write_type = write_handle<T>;
    using read_type = read_handle<const T>;
    using write_result = write_type;

    struct snapshot_type {
        std::uint64_t published = 0;
        std::uint64_t read = 0;
        std::uint64_t released = 0;
        std::uint32_t occupancy_high_water = 0;
        std::uint64_t blocked_producers = 0;
        std::uint64_t blocked_consumers = 0;
        std::uint64_t logical_bytes = 0;
        std::uint64_t published_logical_bytes = 0;
        std::uint64_t read_logical_bytes = 0;
        std::uint64_t producer_wait_events = 0;
        std::uint64_t consumer_wait_events = 0;
        sc_core::sc_time producer_wait = sc_core::SC_ZERO_TIME;
        sc_core::sc_time consumer_wait = sc_core::SC_ZERO_TIME;
    };

    line_channel(sc_core::sc_module_name name, std::size_t slot_count,
                 std::size_t samples_per_slot)
        : sc_core::sc_prim_channel(name), m_slot_count(slot_count),
          m_samples_per_slot(samples_per_slot),
          m_payload(slot_count == 0 || samples_per_slot == 0
                        ? nullptr
                        : std::make_unique<T[]>(slot_count * samples_per_slot)),
          m_state(slot_count == 0 ? nullptr : std::make_unique<slot_state[]>(slot_count)),
          m_generation(slot_count == 0 ? nullptr
                                      : std::make_unique<std::uint64_t[]>(slot_count)),
          m_meta(slot_count == 0 ? nullptr : std::make_unique<line_meta[]>(slot_count)),
          m_free_queue(slot_count == 0 ? nullptr : std::make_unique<slot_id[]>(slot_count)),
          m_ready_queue(slot_count == 0 ? nullptr : std::make_unique<slot_id[]>(slot_count)) {
        if (slot_count == 0 || samples_per_slot == 0 ||
            slot_count > static_cast<std::size_t>(invalid_slot)) {
            throw std::invalid_argument("line channel dimensions must be non-zero");
        }
        for (slot_id i = 0; i < slot_count; ++i) {
            m_state[i] = slot_state::free;
            m_generation[i] = 0;
            m_free_queue[i] = i;
        }
        m_free_count = slot_count;
    }

    line_channel(const line_channel&) = delete;
    line_channel& operator=(const line_channel&) = delete;

    write_type reserve_result() {
        if (m_free_count == 0) {
            const sc_core::sc_time start = sc_core::sc_time_stamp();
            do {
                sc_core::wait(m_credit_event);
            } while (m_free_count == 0);
            record_producer_blocked_episode();
            record_producer_wait_duration(sc_core::sc_time_stamp() - start);
        }
        return reserve_now();
    }

    write_type try_reserve_result() {
        if (m_free_count == 0) {
            return {};
        }
        return reserve_now();
    }
    void record_producer_blocked_episode() noexcept {
        ++m_blocked_producers;
        ++m_producer_wait_events;
    }

    void record_producer_wait_duration(const sc_core::sc_time& duration) noexcept {
        m_producer_wait += duration;
    }

    void publish(write_type&& handle, const line_meta& meta) {
        validate_write(handle);
        if (meta.valid_samples > m_samples_per_slot) {
            throw std::logic_error("line metadata exceeds slot capacity");
        }
        const slot_id id = handle.m_slot;
        m_meta[id] = meta;
        m_state[id] = slot_state::published;
        enqueue_ready(id);
        ++m_published;
        const std::uint64_t bytes = logical_bytes(meta.valid_samples);
        m_logical_bytes = saturating_add(m_logical_bytes, bytes);
        m_published_logical_bytes =
            saturating_add(m_published_logical_bytes, bytes);
        handle.invalidate();
        m_data_pending = true;
        request_update();
    }

    read_type read() {
        if (m_ready_count == 0) {
            const sc_core::sc_time start = sc_core::sc_time_stamp();
            do {
                sc_core::wait(m_data_event);
            } while (m_ready_count == 0);
            ++m_blocked_consumers;
            ++m_consumer_wait_events;
            m_consumer_wait += sc_core::sc_time_stamp() - start;
        }
        return read_now();
    }

    read_type try_read() {
        if (m_ready_count == 0) {
            return {};
        }
        return read_now();
    }

    void release(read_type&& handle) {
        validate_read(handle);
        const slot_id id = handle.m_slot;
        m_state[id] = slot_state::free;
        enqueue_free(id);
        ++m_released;
        handle.invalidate();
        m_credit_pending = true;
        request_update();
    }

    std::size_t available() const noexcept { return m_ready_count; }
    std::size_t credits() const noexcept { return m_free_count; }
    std::size_t capacity() const noexcept { return m_slot_count; }
    std::size_t line_capacity() const noexcept { return m_samples_per_slot; }

    sc_core::sc_event& data_event() noexcept { return m_data_event; }
    const sc_core::sc_event& data_event() const noexcept { return m_data_event; }
    sc_core::sc_event& credit_event() noexcept { return m_credit_event; }
    const sc_core::sc_event& credit_event() const noexcept { return m_credit_event; }

    snapshot_type snapshot() const noexcept {
        snapshot_type result;
        result.published = m_published;
        result.read = m_read;
        result.released = m_released;
        result.occupancy_high_water = m_occupancy_high_water;
        result.blocked_producers = m_blocked_producers;
        result.blocked_consumers = m_blocked_consumers;
        result.logical_bytes = m_logical_bytes;
        result.published_logical_bytes = m_published_logical_bytes;
        result.read_logical_bytes = m_read_logical_bytes;
        result.producer_wait_events = m_producer_wait_events;
        result.consumer_wait_events = m_consumer_wait_events;
        result.producer_wait = m_producer_wait;
        result.consumer_wait = m_consumer_wait;
        return result;
    }
    void reset_metrics() {
        if (m_free_count != m_slot_count || m_ready_count != 0) {
            throw std::logic_error("cannot reset channel metrics with live slots");
        }
        m_published = 0;
        m_read = 0;
        m_released = 0;
        m_occupancy_high_water = 0;
        m_blocked_producers = 0;
        m_blocked_consumers = 0;
        m_logical_bytes = 0;
        m_published_logical_bytes = 0;
        m_read_logical_bytes = 0;
        m_producer_wait_events = 0;
        m_consumer_wait_events = 0;
        m_producer_wait = sc_core::SC_ZERO_TIME;
        m_consumer_wait = sc_core::SC_ZERO_TIME;
        m_data_pending = false;
        m_credit_pending = false;
    }

protected:
    void update() override {
        if (m_data_pending) {
            m_data_pending = false;
            m_data_event.notify(sc_core::SC_ZERO_TIME);
        }
        if (m_credit_pending) {
            m_credit_pending = false;
            m_credit_event.notify(sc_core::SC_ZERO_TIME);
        }
    }

private:
    enum class slot_state : std::uint8_t { free, reserved, published, reading };

    friend class write_handle<T>;
    friend class read_handle<const T>;

    write_type reserve_now() {
        const slot_id id = dequeue_free();
        m_state[id] = slot_state::reserved;
        ++m_generation[id];
        const std::uint32_t occupancy = static_cast<std::uint32_t>(m_slot_count - m_free_count);
        m_occupancy_high_water = std::max(m_occupancy_high_water, occupancy);
        return write_type(this, id, m_generation[id]);
    }

    read_type read_now() {
        const slot_id id = dequeue_ready();
        m_state[id] = slot_state::reading;
        ++m_read;
        const std::uint64_t bytes = logical_bytes(m_meta[id].valid_samples);
        m_read_logical_bytes = saturating_add(m_read_logical_bytes, bytes);
        return read_type(this, id, m_generation[id]);
    }

    static std::uint64_t logical_bytes(std::uint32_t samples) noexcept {
        const std::uint64_t width = static_cast<std::uint64_t>(sizeof(T));
        if (samples != 0 &&
            width > (std::numeric_limits<std::uint64_t>::max)() / samples) {
            return (std::numeric_limits<std::uint64_t>::max)();
        }
        return static_cast<std::uint64_t>(samples) * width;
    }

    static std::uint64_t saturating_add(std::uint64_t lhs,
                                        std::uint64_t rhs) noexcept {
        if (rhs > (std::numeric_limits<std::uint64_t>::max)() - lhs) {
            return (std::numeric_limits<std::uint64_t>::max)();
        }
        return lhs + rhs;
    }
    void validate_write(const write_type& handle) const {
        if (handle.m_channel != this || handle.m_slot >= m_slot_count ||
            handle.m_generation != m_generation[handle.m_slot] ||
            m_state[handle.m_slot] != slot_state::reserved) {
            throw std::logic_error("invalid line write handle");
        }
    }

    void validate_read(const read_type& handle) const {
        if (handle.m_channel != this || handle.m_slot >= m_slot_count ||
            handle.m_generation != m_generation[handle.m_slot] ||
            m_state[handle.m_slot] != slot_state::reading) {
            throw std::logic_error("invalid line read handle");
        }
    }

    bool is_write_valid(const write_type& handle) const noexcept {
        return handle.m_channel == this && handle.m_slot < m_slot_count &&
               handle.m_generation == m_generation[handle.m_slot] &&
               m_state[handle.m_slot] == slot_state::reserved;
    }

    bool is_read_valid(const read_type& handle) const noexcept {
        return handle.m_channel == this && handle.m_slot < m_slot_count &&
               handle.m_generation == m_generation[handle.m_slot] &&
               m_state[handle.m_slot] == slot_state::reading;
    }

    basic_line_view<T> write_view(const write_type& handle, std::size_t length) {
        validate_write(handle);
        const std::size_t count = length == 0 ? m_samples_per_slot : length;
        if (count > m_samples_per_slot) {
            throw std::logic_error("line view exceeds slot capacity");
        }
        return {m_payload.get() + static_cast<std::size_t>(handle.m_slot) * m_samples_per_slot,
                count};
    }

    basic_line_view<const T> read_view(const read_type& handle, std::size_t length) const {
        validate_read(handle);
        const std::size_t count = length == 0 ? m_meta[handle.m_slot].valid_samples : length;
        if (count > m_meta[handle.m_slot].valid_samples) {
            throw std::logic_error("line view exceeds valid samples");
        }
        return {m_payload.get() + static_cast<std::size_t>(handle.m_slot) * m_samples_per_slot,
                count};
    }

    const line_meta& read_meta(const read_type& handle) const {
        validate_read(handle);
        return m_meta[handle.m_slot];
    }

    void cancel_write(write_type& handle) noexcept {
        if (!is_write_valid(handle)) {
            handle.invalidate();
            return;
        }
        const slot_id id = handle.m_slot;
        m_state[id] = slot_state::free;
        enqueue_free(id);
        handle.invalidate();
        m_credit_pending = true;
        request_update();
    }

    void cancel_read(read_type& handle) noexcept {
        if (!is_read_valid(handle)) {
            handle.invalidate();
            return;
        }
        const slot_id id = handle.m_slot;
        m_state[id] = slot_state::free;
        enqueue_free(id);
        ++m_released;
        handle.invalidate();
        m_credit_pending = true;
        request_update();
    }

    void enqueue_free(slot_id id) noexcept {
        m_free_queue[m_free_tail] = id;
        m_free_tail = (m_free_tail + 1) % m_slot_count;
        ++m_free_count;
    }
    slot_id dequeue_free() noexcept {
        const slot_id id = m_free_queue[m_free_head];
        m_free_head = (m_free_head + 1) % m_slot_count;
        --m_free_count;
        return id;
    }
    void enqueue_ready(slot_id id) noexcept {
        m_ready_queue[m_ready_tail] = id;
        m_ready_tail = (m_ready_tail + 1) % m_slot_count;
        ++m_ready_count;
    }
    slot_id dequeue_ready() noexcept {
        const slot_id id = m_ready_queue[m_ready_head];
        m_ready_head = (m_ready_head + 1) % m_slot_count;
        --m_ready_count;
        return id;
    }

    const std::size_t m_slot_count;
    const std::size_t m_samples_per_slot;
    std::unique_ptr<T[]> m_payload;
    std::unique_ptr<slot_state[]> m_state;
    std::unique_ptr<std::uint64_t[]> m_generation;
    std::unique_ptr<line_meta[]> m_meta;
    std::unique_ptr<slot_id[]> m_free_queue;
    std::unique_ptr<slot_id[]> m_ready_queue;

    std::size_t m_free_head = 0;
    std::size_t m_free_tail = 0;
    std::size_t m_free_count = 0;
    std::size_t m_ready_head = 0;
    std::size_t m_ready_tail = 0;
    std::size_t m_ready_count = 0;
    std::uint64_t m_published = 0;
    std::uint64_t m_read = 0;
    std::uint64_t m_released = 0;
    std::uint32_t m_occupancy_high_water = 0;
    std::uint64_t m_blocked_producers = 0;
    std::uint64_t m_blocked_consumers = 0;
    std::uint64_t m_logical_bytes = 0;
    std::uint64_t m_published_logical_bytes = 0;
    std::uint64_t m_read_logical_bytes = 0;
    std::uint64_t m_producer_wait_events = 0;
    std::uint64_t m_consumer_wait_events = 0;
    sc_core::sc_time m_producer_wait = sc_core::SC_ZERO_TIME;
    sc_core::sc_time m_consumer_wait = sc_core::SC_ZERO_TIME;
    bool m_data_pending = false;
    bool m_credit_pending = false;
    sc_core::sc_event m_data_event;
    sc_core::sc_event m_credit_event;
};

template <class T>
basic_line_view<T> write_handle<T>::view(std::size_t length) {
    if (!m_channel) {
        throw std::logic_error("invalid line write handle");
    }
    return m_channel->write_view(*this, length);
}

template <class T>
slot_id write_handle<T>::slot() const {
    if (!valid()) {
        throw std::logic_error("invalid line write handle");
    }
    return m_slot;
}

template <class T>
bool write_handle<T>::valid() const noexcept {
    return m_channel != nullptr && m_channel->is_write_valid(*this);
}

template <class T>
void write_handle<T>::cancel_noexcept() noexcept {
    if (m_channel) {
        m_channel->cancel_write(*this);
    }
}

template <class T>
basic_line_view<const typename read_handle<T>::element_type>
read_handle<T>::view(std::size_t length) const {
    if (!m_channel) {
        throw std::logic_error("invalid line read handle");
    }
    return m_channel->read_view(*this, length);
}

template <class T>
slot_id read_handle<T>::slot() const {
    if (!valid()) {
        throw std::logic_error("invalid line read handle");
    }
    return m_slot;
}

template <class T>
const line_meta& read_handle<T>::meta() const {
    if (!m_channel) {
        throw std::logic_error("invalid line read handle");
    }
    return m_channel->read_meta(*this);
}

template <class T>
bool read_handle<T>::valid() const noexcept {
    return m_channel != nullptr && m_channel->is_read_valid(*this);
}

template <class T>
void read_handle<T>::release_noexcept() noexcept {
    if (m_channel) {
        m_channel->cancel_read(*this);
    }
}

} // namespace isp_tlm

#endif // ISP_LINE_CHANNEL_H
