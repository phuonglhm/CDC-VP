#include "test_support.h"
#include "interpolation_oracle.h"
using namespace h264::inter;

int main() {
    try {
        for (unsigned seed : {20261010u, 0x12345678u, 0x87654321u}) {
            for (unsigned kind = 0; kind < 6; ++kind) {
                auto pixels = pattern(16,16,seed);
                for (unsigned y = 0; y < 16; ++y) for (unsigned x = 0; x < 16; ++x) {
                    if (kind == 0) pixels[y*16+x] = 77;
                    if (kind == 1) pixels[y*16+x] = uint8_t(x*9 + y*7);
                    if (kind == 2) pixels[y*16+x] = (x == 7 && y == 7) ? 255 : 0;
                    if (kind == 3) pixels[y*16+x] = (x+y)%2 ? 255 : 0;
                    if (kind == 4) pixels[y*16+x] = x < 8 ? 0 : 255;
                }
                SampleReader read = [&](int x, int y) { return pixels[std::clamp(y,0,15)*16 + std::clamp(x,0,15)]; };
                for (int y = -2; y <= 17; ++y) for (int x = -2; x <= 17; ++x)
                    for (int fy = 0; fy < 4; ++fy) for (int fx = 0; fx < 4; ++fx)
                        require(luma_qpel(read,x*4+fx,y*4+fy) == oracle_luma(read,x*4+fx,y*4+fy), "luma phase oracle");
                for (int y : {-2,0,7,15,17}) for (int x : {-2,0,7,15,17})
                    for (int fy = 0; fy < 8; ++fy) for (int fx = 0; fx < 8; ++fx)
                        require(chroma_eighth(read,x*8+fx,y*8+fy) == oracle_chroma(read,x*8+fx,y*8+fy), "chroma phase oracle");
            }
        }
        // A six-tap half-sample distinguishes AVC from a bilinear shortcut.
        SampleReader impulse = [](int x, int y) { return uint8_t(x == 0 && y == 0 ? 255 : 0); };
        require(luma_qpel(impulse,2,0) == 159, "half-sample six tap anchor");
        require(luma_qpel(impulse,2,2) == 100, "diagonal unrounded anchor");
        std::cout << "Interpolation: " << checks << " checks; 3 seeds; all 16 luma/64 chroma phases PASS\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
