#include "fu3/aes/aes_core.h"
#include "vector_file.h"
#include <systemc>
#include <iostream>
#include <tuple>

struct Record { unsigned round; std::string phase; fu3::aes::Block value; };
int sc_main(int argc, char** argv) {
    try {
        if (argc != 3) throw std::runtime_error("expected key width and FIPS fixture");
        const std::string bits = argv[1];
        if (bits != "128" && bits != "256") throw std::runtime_error("invalid width");
        const bool wide = bits == "256";
        const unsigned rounds = wide ? 14 : 10;
        std::ifstream file(argv[2]);
        if (!file) throw std::runtime_error("FIPS literal fixture unavailable");
        std::vector<Record> expected[2];
        std::string line;
        unsigned total = 0;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream row(line);
            std::string width, direction, hex, extra;
            Record record{};
            if (!(row >> width >> direction >> record.round >> record.phase >> hex) || (row >> extra) ||
                (width != "128" && width != "256") || (direction != "enc" && direction != "dec"))
                throw std::runtime_error("invalid FIPS literal record");
            decode_hex(hex, record.value, 16); ++total;
            if (width == bits) expected[direction == "dec"].push_back(record);
        }
        if (total != 248) throw std::runtime_error("incomplete FIPS fixture");
        unsigned checks = 0;
        fu3::aes::Key key{};
        for (unsigned i = 0; i < 32; ++i) key[i] = static_cast<std::uint8_t>(i);
        fu3::aes::AesCore core; core.initialize(key, wide);
        for (unsigned inverse = 0; inverse < 2; ++inverse) {
            const auto& records = expected[inverse];
            if (records.size() != 5 * rounds + 2) throw std::runtime_error("incomplete selected trace");
            std::vector<Record> actual;
            const auto result = core.transform(records.front().value, !inverse,
                [&](unsigned round, const char* phase, const fu3::aes::Block& value) {
                    actual.push_back({round, phase, value});
                });
            if (actual.size() != records.size()) {
                std::cerr << "SPEC_FAIL trace" << bits << " incorrect phase/round count\n"; return 1;
            }
            for (std::size_t i = 0; i < records.size(); ++i) {
                const auto& want = records[i]; const auto& got = actual[i];
                if (got.round != want.round || got.phase != want.phase || got.value != want.value) {
                    std::cerr << "SPEC_FAIL trace" << bits << " direction=" << inverse
                              << " record=" << i << " round=" << want.round << " phase=" << want.phase << '\n';
                    return 1;
                }
                ++checks;
            }
            // Observer must leave the regular production transform result unchanged.
            if (result != records.back().value || core.transform(records.front().value, !inverse) != result) {
                std::cerr << "SPEC_FAIL trace" << bits << " output mismatch\n"; return 1;
            }
            ++checks;
        }
        std::cout << "SPEC_PASS trace" << bits << " checks=" << checks
                  << " source=HDS:p10-11+FIPS197:C.1/C.3 scope=all-literal-stages-and-round-keys\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "TEST_ERROR " << e.what() << '\n'; return 2;
    }
}
