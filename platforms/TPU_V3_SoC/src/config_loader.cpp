// SPDX-License-Identifier: Apache-2.0

#include "config_loader.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace cdc::platforms::tpu_v3_soc {

namespace tpu = cdc::components::tpu_v3;

namespace {

std::string trim(const std::string& text)
{
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

[[noreturn]] void syntax_error(const std::string& path, unsigned line,
                               const std::string& reason)
{
    std::ostringstream message;
    message << path << ':' << line << ": " << reason;
    throw std::runtime_error(message.str());
}

/// Decimal, `0x` hex, or a binary suffix (`K`/`KiB`, `M`/`MiB`, `G`/`GiB`).
///
/// Rejects trailing garbage rather than stopping at it: `strtoull` happily
/// reads `4Mib` as 4, and a silently 4-byte core SRAM would be a very
/// confusing failure three phases later.
std::uint64_t parse_unsigned(const std::string& where, const std::string& text)
{
    const std::string value = trim(text);
    if (value.empty()) {
        throw std::runtime_error(where + ": expected a number, got an empty value");
    }

    std::size_t consumed = 0;
    std::uint64_t number = 0;
    try {
        number = std::stoull(value, &consumed, 0);
    } catch (const std::exception&) {
        throw std::runtime_error(where + ": '" + value + "' is not a number");
    }

    std::string suffix = trim(value.substr(consumed));
    std::transform(suffix.begin(), suffix.end(), suffix.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    std::uint64_t multiplier = 1;
    if (suffix.empty() || suffix == "b") {
        multiplier = 1;
    } else if (suffix == "k" || suffix == "kib") {
        multiplier = 1024ull;
    } else if (suffix == "m" || suffix == "mib") {
        multiplier = 1024ull * 1024;
    } else if (suffix == "g" || suffix == "gib") {
        multiplier = 1024ull * 1024 * 1024;
    } else {
        throw std::runtime_error(where + ": '" + value
                                 + "' has an unrecognised suffix '" + suffix
                                 + "'; accepted suffixes are K/KiB, M/MiB, G/GiB");
    }

    if (multiplier != 1 && number > (0xFFFFFFFFFFFFFFFFull / multiplier)) {
        throw std::runtime_error(where + ": '" + value + "' overflows 64 bits");
    }
    return number * multiplier;
}

unsigned parse_unsigned_small(const std::string& where, const std::string& text)
{
    const std::uint64_t value = parse_unsigned(where, text);
    if (value > 0xFFFFFFFFull) {
        throw std::runtime_error(where + ": " + std::to_string(value)
                                 + " does not fit in 32 bits");
    }
    return static_cast<unsigned>(value);
}

/// Apply one `key: value` pair. Shared by the file reader and by `--set`
/// style overrides so the two cannot diverge in what they accept.
void apply_key(const std::string& where, const std::string& key,
               const std::string& value, tpu::tpu_soc_config& config)
{
    if (key == "platform") {
        if (value != "tpu_v3_soc") {
            throw std::runtime_error(
                where + ": platform is '" + value
                + "' but this executable is tpu_v3_soc. Refusing to run a "
                  "configuration written for another platform");
        }
    } else if (key == "name") {
        config.name = value;
    } else if (key == "mesh_x") {
        config.mesh_x = parse_unsigned_small(where, value);
    } else if (key == "mesh_y") {
        config.mesh_y = parse_unsigned_small(where, value);
    } else if (key == "chips") {
        config.chips = parse_unsigned_small(where, value);
    } else if (key == "core_sram_size_bytes") {
        const std::uint64_t size = parse_unsigned(where, value);
        for (auto& core : config.chip.core) {
            core.sram_size_bytes = size;
        }
    } else if (key == "global_ram_size_bytes") {
        config.global_ram_size_bytes = parse_unsigned(where, value);
    } else if (key == "sa_geometry") {
        // One key rather than two, so a half-set geometry — 64 rows and 128
        // columns — cannot be expressed at all.
        const auto cross = value.find('x');
        if (cross == std::string::npos) {
            throw std::runtime_error(
                where + ": sa_geometry must be written as ROWSxCOLUMNS, for "
                        "example '64x64'; got '"
                + value + "'");
        }
        const unsigned rows =
            parse_unsigned_small(where, value.substr(0, cross));
        const unsigned columns =
            parse_unsigned_small(where, value.substr(cross + 1));
        for (auto& core : config.chip.core) {
            core.sa.rows = rows;
            core.sa.columns = columns;
        }
    } else if (key == "sa_datatype") {
        const auto datatype = tpu::matrix_datatype_from_string(value);
        for (auto& core : config.chip.core) {
            core.sa.datatype = datatype;
        }
    } else if (key == "sa_source_revision") {
        for (auto& core : config.chip.core) {
            core.sa.source_revision = value;
        }
    } else if (key == "dma_max_burst_bytes") {
        const std::uint64_t size = parse_unsigned(where, value);
        for (auto& core : config.chip.core) {
            core.dma.max_burst_bytes = size;
        }
    } else if (key == "local_sram_data_width_bits") {
        const unsigned width = parse_unsigned_small(where, value);
        for (auto& core : config.chip.core) {
            core.local_sram_fabric.data_width_bits = width;
        }
    } else if (key == "local_sram_banks") {
        const unsigned banks = parse_unsigned_small(where, value);
        for (auto& core : config.chip.core) {
            core.local_sram_fabric.bank_count = banks;
        }
    } else if (key == "local_sram_bank_mapping") {
        const auto mapping = tpu::bank_mapping_from_string(value);
        for (auto& core : config.chip.core) {
            core.local_sram_fabric.mapping = mapping;
        }
    } else if (key == "local_sram_pipeline_stages") {
        const unsigned stages = parse_unsigned_small(where, value);
        for (auto& core : config.chip.core) {
            core.local_sram_fabric.pipeline_stages = stages;
        }
    } else if (key == "local_sram_arbitration") {
        const auto policy = tpu::arbitration_policy_from_string(value);
        for (auto& core : config.chip.core) {
            core.local_sram_fabric.arbitration = policy;
        }
    } else if (key == "noc_timing") {
        config.timing = tpu::noc_timing_from_string(value);
    } else {
        throw std::runtime_error(
            where + ": unknown configuration key '" + key
            + "'. Accepted keys are: platform, name, mesh_x, mesh_y, chips, "
              "core_sram_size_bytes, global_ram_size_bytes, sa_geometry, "
              "sa_datatype, sa_source_revision, dma_max_burst_bytes, "
              "local_sram_data_width_bits, local_sram_banks, "
              "local_sram_bank_mapping, local_sram_pipeline_stages, "
              "local_sram_arbitration, noc_timing");
    }
}

/// `argv[i]`'s value, with a clear error when it is missing. Options take a
/// separate argument; `--chips=4` is not accepted, deliberately, because
/// supporting both spellings doubles the parsing surface for no benefit.
std::string take_value(int argc, char** argv, int& index,
                       const std::string& option)
{
    if (index + 1 >= argc) {
        throw std::runtime_error(option + " requires a value");
    }
    return argv[++index];
}

} // namespace

tpu::tpu_soc_config default_config()
{
    // Decision record D15 leaves the local-SRAM datapath width, bank count and
    // pipeline depth open, so the component schema has no default for them and
    // refuses zero — a default-constructed `tpu_soc_config` deliberately does
    // not validate. The platform has to supply something to be runnable
    // without `--config`, and this is the one place it does: named, in the
    // open, and printed with the word "provisional" in every report that uses
    // it. Hiding these numbers in a member initialiser is exactly what D15
    // forbids.
    tpu::tpu_soc_config config;
    for (auto& core : config.chip.core) {
        core.local_sram_fabric = tpu::provisional_local_sram_fabric();
    }
    return config;
}

tpu::tpu_soc_config load_config_file(const std::string& path)
{
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("cannot open configuration file '" + path
                                 + '\'');
    }

    tpu::tpu_soc_config config = default_config();
    // A file that never sets `name` is still traceable: default to its stem.
    const auto slash = path.find_last_of('/');
    const auto stem_begin = slash == std::string::npos ? 0 : slash + 1;
    const auto dot = path.find_last_of('.');
    const auto stem_end =
        (dot == std::string::npos || dot < stem_begin) ? path.size() : dot;
    config.name = path.substr(stem_begin, stem_end - stem_begin);

    std::string line;
    unsigned number = 0;
    while (std::getline(file, line)) {
        ++number;

        const auto comment = line.find('#');
        if (comment != std::string::npos) {
            line = line.substr(0, comment);
        }
        const std::string content = trim(line);
        if (content.empty()) {
            continue;
        }

        if (content != line && !line.empty()
            && (line.front() == ' ' || line.front() == '\t')) {
            syntax_error(path, number,
                         "indented line. This format has no nesting; write "
                         "'key: value' at column zero");
        }

        const auto colon = content.find(':');
        if (colon == std::string::npos) {
            syntax_error(path, number, "expected 'key: value'");
        }

        const std::string key = trim(content.substr(0, colon));
        const std::string value = trim(content.substr(colon + 1));
        if (key.empty()) {
            syntax_error(path, number, "empty key");
        }
        if (value.empty()) {
            // Almost always the first line of an attempted nested block —
            // `chip:` followed by indented children. Saying only "no value"
            // sends the reader looking for a missing number instead of at the
            // structure they wrote.
            syntax_error(path, number,
                         "key '" + key
                             + "' has no value. This format has no nesting; "
                               "write 'key: value' at column zero");
        }

        apply_key(path + ':' + std::to_string(number), key, value, config);
    }

    return config;
}

cli_options parse_command_line(int argc, char** argv,
                               tpu::tpu_soc_config& config)
{
    cli_options options;

    // Two passes: --config must take effect before any override, whatever
    // order they appear in. Otherwise `--chips 4 --config x.yaml` would
    // silently discard the override.
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--config") {
            options.config_path = take_value(argc, argv, i, arg);
        } else if (arg == "--help" || arg == "-h") {
            options.show_help = true;
        } else if (arg == "--version") {
            options.show_version = true;
        }
    }

    if (options.show_help || options.show_version) {
        return options;
    }

    if (!options.config_path.empty()) {
        config = load_config_file(options.config_path);
    }

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--config") {
            ++i; // consumed above
        } else if (arg == "--chips") {
            apply_key(arg, "chips", take_value(argc, argv, i, arg), config);
        } else if (arg == "--mesh-x") {
            apply_key(arg, "mesh_x", take_value(argc, argv, i, arg), config);
        } else if (arg == "--mesh-y") {
            apply_key(arg, "mesh_y", take_value(argc, argv, i, arg), config);
        } else if (arg == "--core-sram-size") {
            apply_key(arg, "core_sram_size_bytes",
                      take_value(argc, argv, i, arg), config);
        } else if (arg == "--global-ram-size") {
            apply_key(arg, "global_ram_size_bytes",
                      take_value(argc, argv, i, arg), config);
        } else if (arg == "--sa-geometry") {
            apply_key(arg, "sa_geometry", take_value(argc, argv, i, arg),
                      config);
        } else if (arg == "--sa-datatype") {
            apply_key(arg, "sa_datatype", take_value(argc, argv, i, arg),
                      config);
        } else if (arg == "--local-sram-banks") {
            apply_key(arg, "local_sram_banks", take_value(argc, argv, i, arg),
                      config);
        } else if (arg == "--local-sram-data-width-bits") {
            apply_key(arg, "local_sram_data_width_bits",
                      take_value(argc, argv, i, arg), config);
        } else if (arg == "--noc-timing") {
            apply_key(arg, "noc_timing", take_value(argc, argv, i, arg),
                      config);
        } else if (arg == "--name") {
            apply_key(arg, "name", take_value(argc, argv, i, arg), config);
        } else if (arg == "--print-address-map") {
            options.print_address_map = true;
        } else if (arg == "--time-limit-ns") {
            options.time_limit_ns = static_cast<double>(
                parse_unsigned(arg, take_value(argc, argv, i, arg)));
        } else {
            throw std::runtime_error("unknown option '" + arg
                                     + "'. Try --help");
        }
    }

    return options;
}

std::string usage_text(const std::string& program)
{
    std::ostringstream out;
    out << "Usage: " << program << " [options]\n"
        << '\n'
        << "TPU_V3 SoC virtual platform.\n"
        << '\n'
        << "Options:\n"
        << "  --config <file>            configuration file (key: value "
           "subset, not YAML)\n"
        << "  --name <text>              configuration name used in reports\n"
        << "  --chips <n>                number of TPU chips (1..8)\n"
        << "  --mesh-x <n>               mesh width\n"
        << "  --mesh-y <n>               mesh height\n"
        << "  --core-sram-size <bytes>   core SRAM capacity per NEO-CORE "
           "(K/M/G suffixes accepted)\n"
        << "  --global-ram-size <bytes>  global RAM/HBM capacity\n"
        << "  --sa-geometry <RxC>        matrix-engine geometry; 64x64 "
           "(128x128 awaits its gate)\n"
        << "  --sa-datatype <bf16_fp32|fp16_fp32|int8_int32>\n"
        << "  --local-sram-banks <n>     physical banks behind core SRAM "
           "(provisional, D15)\n"
        << "  --local-sram-data-width-bits <n>   bank access width "
           "(provisional, D15)\n"
        << "  --noc-timing <fast|detailed>\n"
        << "  --time-limit-ns <n>        stop after this much simulated time\n"
        << "  --print-address-map        print every mapped region and exit\n"
        << "  --version                  print the build manifest summary\n"
        << "  --help                     this text\n"
        << '\n'
        << "Configuration keys (one 'key: value' per line, '#' comments):\n"
        << "  platform, name, mesh_x, mesh_y, chips, core_sram_size_bytes,\n"
        << "  global_ram_size_bytes, sa_geometry, sa_datatype, "
           "sa_source_revision,\n"
        << "  dma_max_burst_bytes, local_sram_data_width_bits, "
           "local_sram_banks,\n"
        << "  local_sram_bank_mapping, local_sram_pipeline_stages, "
           "local_sram_arbitration,\n"
        << "  noc_timing\n"
        << '\n'
        << "Command-line options override the configuration file regardless "
           "of order.\n";
    return out.str();
}

} // namespace cdc::platforms::tpu_v3_soc
