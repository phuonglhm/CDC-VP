// SPDX-License-Identifier: Apache-2.0
// Row-streaming building blocks for the FX1 ISP pipeline (M3).
//
// Every processing block is a `row_stage`: it receives the rows of a frame in
// raster order and emits its output rows as soon as they are computable, so a
// neighbourhood block of vertical radius r emits row y once row y+r has
// arrived (or at the end of the frame). This mirrors the hardware line-buffer
// latency and keeps the model streaming. Kernels are pure integer functions
// with no SystemC dependency.
#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <stdexcept>
#include <vector>

namespace cdc::components::fx1_isp::pipe {

struct rgb {
   std::uint16_t r, g, b;
};

struct yuv {
   std::uint16_t y, u, v;
};

template <class T>
using row = std::vector<T>;

struct frame_info {
   std::uint32_t width;
   std::uint32_t height;
};

// A block consuming rows of In and producing rows of Out.
template <class In, class Out>
class row_stage {
public:
   using sink = std::function<void(row<Out> &&)>;
   virtual ~row_stage() = default;

   // Called at the accepted SOF with the frame geometry at this stage's input
   // and the configuration snapshot already applied. Returns the output geometry.
   virtual frame_info begin(const frame_info &in) = 0;
   virtual void push(const row<In> &r, const sink &out) = 0;
   // End of frame: emit every remaining row.
   virtual void finish(const sink &out) = 0;
};

// How a neighbourhood reads outside the frame.
enum class border { replicate, mirror /* reflect-101: -1 -> 1 */, mirror_edge /* reflect: -1 -> 0 */ };

inline std::int64_t border_index(std::int64_t i, std::int64_t n, border b) {
   if (n <= 0) {
      throw std::logic_error("border_index on empty extent");
   }
   while (i < 0 || i >= n) {
      switch (b) {
      case border::replicate:
         return i < 0 ? 0 : n - 1;
      case border::mirror:
         if (n == 1) {
            return 0;
         }
         i = i < 0 ? -i : 2 * (n - 1) - i;
         break;
      case border::mirror_edge:
         i = i < 0 ? -i - 1 : 2 * n - 1 - i;
         break;
      }
   }
   return i;
}

// Vertical window over a row stream. `Row` is one whole row (e.g.
// row<uint16_t>, or a struct carrying several planes of the same row). Holds
// the rows a centre row needs and resolves out-of-frame rows with a border
// policy (the same policy should be applied horizontally by the kernel).
template <class Row>
class row_window {
public:
   row_window(unsigned radius, border b) : radius_(radius), border_(b) {}

   void begin(std::uint32_t height) {
      height_ = height;
      rows_.clear();
      first_ = 0;
      received_ = 0;
      next_ = 0;
   }
   void add(const Row &r) {
      rows_.push_back(r);
      ++received_;
   }
   // True when the next centre row can be produced (its lower neighbours have
   // arrived, or the frame has ended).
   bool ready(bool frame_ended) const {
      return next_ < height_ && (frame_ended || next_ + radius_ < received_);
   }
   std::uint32_t centre() const { return next_; }
   // Row at vertical offset dy (-radius..radius) from the current centre.
   const Row &at(int dy) const {
      const std::int64_t y = border_index(static_cast<std::int64_t>(next_) + dy, height_, border_);
      if (y < first_ || y >= static_cast<std::int64_t>(first_ + rows_.size())) {
         throw std::logic_error("row_window: row not resident");
      }
      return rows_[static_cast<std::size_t>(y - first_)];
   }
   // Advance the centre and drop rows no longer needed. For every border policy
   // above, the rows a centre y reads lie in [max(0, y - r), y + r], so rows
   // below y - r can go.
   void advance() {
      ++next_;
      while (!rows_.empty() && first_ + radius_ < next_) {
         rows_.pop_front();
         ++first_;
      }
   }

private:
   unsigned radius_;
   border border_;
   std::uint32_t height_ = 0;
   std::deque<Row> rows_;
   std::uint32_t first_ = 0;     // frame row index of rows_.front()
   std::uint32_t received_ = 0;
   std::uint32_t next_ = 0;      // next centre row to emit
};

// Pointwise stage helper.
template <class In, class Out>
class point_stage : public row_stage<In, Out> {
public:
   using typename row_stage<In, Out>::sink;
   frame_info begin(const frame_info &in) override {
      info_ = in;
      y_ = 0;
      configure();
      return in;
   }
   void push(const row<In> &r, const sink &out) override {
      row<Out> o(r.size());
      for (std::uint32_t x = 0; x < r.size(); ++x) {
         o[x] = pixel(r[x], x, y_);
      }
      ++y_;
      out(std::move(o));
   }
   void finish(const sink &) override {}

protected:
   virtual void configure() {}
   virtual Out pixel(const In &p, std::uint32_t x, std::uint32_t y) const = 0;
   frame_info info_{};
   std::uint32_t y_ = 0;
};

}  // namespace cdc::components::fx1_isp::pipe
