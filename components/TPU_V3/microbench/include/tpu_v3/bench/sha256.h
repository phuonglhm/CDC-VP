// SPDX-License-Identifier: Apache-2.0
//
// SHA-256 of a file, for the firmware ELF identity §8.1 requires in every
// result row.
//
// Self-contained rather than a dependency: the component tree links SystemC and
// the pinned accelerator sources and nothing else, and adding OpenSSL to every
// consumer of a benchmark row so one hash can be computed would be a poor
// trade. FIPS 180-4, checked against the published test vectors in
// `tests/test_sha256.cpp`.

#ifndef CDC_COMPONENTS_TPU_V3_BENCH_SHA256_H
#define CDC_COMPONENTS_TPU_V3_BENCH_SHA256_H

#include <cstdint>
#include <string>
#include <vector>

namespace cdc::components::tpu_v3::bench {

/// Lower-case hex digest of `bytes`.
std::string sha256_hex(const std::vector<unsigned char>& bytes);

/// Lower-case hex digest of a file's contents, or an empty string when the
/// file cannot be read.
///
/// Empty rather than a thrown exception or a zero digest: a row whose ELF hash
/// is missing must say `unavailable`, and a digest of nothing would be a
/// perfectly valid-looking hash of the wrong thing.
std::string sha256_file(const std::string& path);

} // namespace cdc::components::tpu_v3::bench

#endif // CDC_COMPONENTS_TPU_V3_BENCH_SHA256_H
