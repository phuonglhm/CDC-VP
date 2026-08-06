// SPDX-License-Identifier: Apache-2.0

#include "floo_noc_model/noc_metrics.hpp"

#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <systemc>

namespace {

int failures = 0;

void check(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

} // namespace

int sc_main(int, char**)
{
    using floo::model::latency_histogram;
    using floo::model::metric_source;
    using floo::model::metric_source_tag;
    using floo::model::transaction_metrics;

    latency_histogram histogram;
    for (const std::uint64_t sample : {1u, 2u, 2u, 4u, 10u}) {
        histogram.add(sample);
    }
    check(histogram.valid() && histogram.count() == 5
              && histogram.sum() == 19,
          "histogram count and sum must cover every sample");
    check(histogram.min() == 1 && histogram.max() == 10,
          "histogram min/max must use the observed endpoints");
    check(histogram.percentile(50, 100) == 2,
          "P50 must use the exact nearest-rank sample");
    check(histogram.percentile(95, 100) == 10
              && histogram.percentile(99, 100) == 10,
          "tail percentiles must select the observed tail");
    check(histogram.percentile(1, 100) == 1
              && histogram.percentile(100, 100) == 10,
          "nearest-rank endpoints must remain in range");

    bool bad_percentile_refused = false;
    try {
        (void)histogram.percentile(0, 100);
    } catch (const std::invalid_argument&) {
        bad_percentile_refused = true;
    }
    check(bad_percentile_refused,
          "a zero percentile must be rejected rather than underflow rank");

    transaction_metrics bucket;
    bucket.add(8, 11);
    bucket.add(32, 19);
    check(bucket.valid() && bucket.transactions() == 2
              && bucket.payload_bytes() == 40
              && bucket.latency().sum() == 30,
          "transaction bucket must reconcile count, bytes and latency");

    bucket.reset();
    check(bucket.valid() && bucket.transactions() == 0
              && bucket.payload_bytes() == 0
              && bucket.latency().empty(),
          "metric reset must clear every transaction field");

    latency_histogram overflow;
    overflow.add(std::numeric_limits<std::uint64_t>::max());
    overflow.add(1);
    check(!overflow.valid(),
          "latency sum overflow must invalidate instead of wrapping");

    check(std::string(metric_source_tag(metric_source::measured)) == "M"
              && std::string(metric_source_tag(metric_source::derived)) == "D"
              && std::string(metric_source_tag(metric_source::analytic)) == "A"
              && std::string(metric_source_tag(metric_source::static_spec))
                  == "S",
          "metric source tags must match the dashboard contract");

    if (failures == 0) {
        std::cout << "PASS: NoC metric primitives\n";
        return 0;
    }
    std::cerr << "FAIL: " << failures << " NoC metric checks failed\n";
    return 1;
}
