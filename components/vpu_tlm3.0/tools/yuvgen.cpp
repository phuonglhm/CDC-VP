#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    if (argc != 5) {
        std::cerr << "Usage: yuvgen OUTPUT.yuv WIDTH HEIGHT FRAMES\n";
        return 1;
    }
    try {
        const std::string path = argv[1];
        const unsigned width = static_cast<unsigned>(std::stoul(argv[2]));
        const unsigned height = static_cast<unsigned>(std::stoul(argv[3]));
        const unsigned frames = static_cast<unsigned>(std::stoul(argv[4]));
        if (!width || !height || (width & 1U) || (height & 1U) || !frames) {
            throw std::invalid_argument("width/height must be positive even values; frames > 0");
        }
        std::ofstream out(path, std::ios::binary);
        if (!out) throw std::runtime_error("cannot open output");
        for (unsigned f = 0; f < frames; ++f) {
            for (unsigned y = 0; y < height; ++y) {
                for (unsigned x = 0; x < width; ++x) {
                    const auto sample = static_cast<std::uint8_t>(16 +
                        ((x * 180 / width + y * 40 / height + f * 13) % 220));
                    out.put(static_cast<char>(sample));
                }
            }
            for (unsigned y = 0; y < height / 2; ++y)
                for (unsigned x = 0; x < width / 2; ++x)
                    out.put(static_cast<char>(96 + ((x + f * 3) % 64)));
            for (unsigned y = 0; y < height / 2; ++y)
                for (unsigned x = 0; x < width / 2; ++x)
                    out.put(static_cast<char>(160 - ((y + f * 5) % 64)));
        }
        return out ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
