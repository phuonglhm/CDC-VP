// SPDX-License-Identifier: Apache-2.0
//
// FIPS 180-4 test vectors for the self-contained digest used to identify a
// firmware ELF in every result row (§8.1).
//
// The vectors matter more than they look: the padding path has two shapes —
// one tail block when the remainder leaves room for the 64-bit length, two
// when it does not — and a 55/56/64-byte trio is what separates them. An
// implementation that only ever saw short inputs would hash every firmware
// image incorrectly and consistently, which is the worst way to be wrong.

#include "tpu_v3/bench/sha256.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace bench = cdc::components::tpu_v3::bench;

namespace {

int failures = 0;

void expect(const std::string& input, const std::string& expected,
            const char* what)
{
    const std::vector<unsigned char> bytes(input.begin(), input.end());
    const std::string actual = bench::sha256_hex(bytes);
    if (actual != expected) {
        std::cerr << "CHECK failed: " << what << "\n  expected " << expected
                  << "\n  actual   " << actual << '\n';
        ++failures;
    }
}

} // namespace

int main(int argc, char* argv[])
{
    expect("",
           "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
           "the empty string");
    expect("abc",
           "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
           "\"abc\"");
    // 56 bytes: the remainder plus the 0x80 terminator leaves no room for the
    // length, so this is the two-tail-block path.
    expect("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
           "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
           "the 56-byte two-block-tail vector");
    // 55 bytes: the largest input that still fits its length in one tail block.
    expect(std::string(55, 'a'),
           "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318",
           "55 bytes, the one-block-tail boundary");
    // Exactly one full block and nothing left over.
    expect(std::string(64, 'a'),
           "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb",
           "64 bytes, a whole block with an empty remainder");
    expect(std::string(1000, 'x'),
           "44f8354494a5ba03ba1792a8d3e9c534c47a9181980fde7a3f44b06ef2ae7c7f",
           "1000 bytes, several blocks");

    // A missing file reports "no digest" rather than the digest of nothing:
    // an empty-input hash is a valid-looking value for the wrong thing.
    if (!bench::sha256_file("/nonexistent/path/for/this/test").empty()) {
        std::cerr << "CHECK failed: a missing file produced a digest\n";
        ++failures;
    }

    if (argc > 1) {
        // Round-trip through the filesystem, so the file path is exercised and
        // not only the in-memory one.
        const std::string path = std::string(argv[1]) + "/sha256_probe.bin";
        {
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            file << "abc";
        }
        if (bench::sha256_file(path)
            != "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f2001"
               "5ad") {
            std::cerr << "CHECK failed: sha256_file disagrees with "
                         "sha256_hex on the same bytes\n";
            ++failures;
        }
        std::remove(path.c_str());
    }

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "test_sha256: all checks passed\n";
    return 0;
}
