/**
 * @file raw_loader.h
 * @brief RAW Bayer image loader utility
 *
 * Loads RAW Bayer images from disk and provides them to SystemC testbenches.
 * Auto-detects the file format based on file size:
 *
 *   1. **RAW16 little-endian** (preferred):
 *      2 bytes per pixel, low 12 bits valid for 12-bit sensors.
 *      File size = W * H * 2.
 *
 *   2. **Packed 12-bit (3 bytes per 2 pixels)**:
 *      File size = W * H * 12 / 8.
 *
 *   3. **Packed 10-bit (5 bytes per 4 pixels)**:
 *      File size = W * H * 10 / 8.
 *
 * The pixel values are returned as `std::uint16_t` (12-bit aligned, 0..4095)
 * regardless of the on-disk format, so the SystemC DUT always sees the same
 * input format as if it came from a 12-bit sensor.
 */
#ifndef ISP_RAW_LOADER_H
#define ISP_RAW_LOADER_H

#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace raw_loader {

/**
 * @brief RAW file format detected from file size.
 */
enum class RawFormat {
    RAW16_LE,        // 16-bit per pixel, little-endian
    PACKED12,        // 12-bit packed (3 bytes per 2 pixels)
    PACKED10,        // 10-bit packed (5 bytes per 4 pixels)
    UNKNOWN          // size does not match any known pattern
};

inline const char* format_name(RawFormat fmt) {
    switch (fmt) {
    case RawFormat::RAW16_LE:  return "RAW16_LE (2 bytes/pixel)";
    case RawFormat::PACKED12:  return "PACKED12 (3 bytes/2 pixels)";
    case RawFormat::PACKED10:  return "PACKED10 (5 bytes/4 pixels)";
    default:                   return "UNKNOWN";
    }
}

/**
 * @brief Detect RAW format from file size.
 *
 * @return Detected format, or UNKNOWN if no known pattern matches.
 */
inline RawFormat detect_format(std::uint32_t width, std::uint32_t height,
                               std::size_t file_size_bytes) {
    const std::size_t s_raw16  = static_cast<std::size_t>(width) * height * 2u;
    const std::size_t s_pack12 = static_cast<std::size_t>(width) * height * 12u / 8u;
    const std::size_t s_pack10 = static_cast<std::size_t>(width) * height * 10u / 8u;

    if (file_size_bytes == s_raw16)  return RawFormat::RAW16_LE;
    if (file_size_bytes == s_pack12) return RawFormat::PACKED12;
    if (file_size_bytes == s_pack10) return RawFormat::PACKED10;
    return RawFormat::UNKNOWN;
}

/**
 * @brief Load a RAW Bayer file into a flat `std::vector<std::uint16_t>`.
 *
 * The vector is sized `width * height` and contains 12-bit pixel values
 * (0..4095) regardless of the source format.
 *
 * @param path         Path to the .raw file
 * @param width        Image width in pixels
 * @param height       Image height in pixels
 * @param[out] pixels  Filled with `width * height` 12-bit pixel values
 * @param format_out   Optional, returns the detected format
 * @throws std::runtime_error if file cannot be read or format is unsupported.
 */
inline void load(const std::string& path,
                 std::uint32_t width, std::uint32_t height,
                 std::vector<std::uint16_t>& pixels,
                 RawFormat* format_out = nullptr)
{
    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    if (!ifs) {
        throw std::runtime_error("raw_loader: cannot open '" + path + "'");
    }
    const std::size_t file_size = static_cast<std::size_t>(ifs.tellg());
    ifs.seekg(0, std::ios::beg);

    const RawFormat fmt = detect_format(width, height, file_size);
    if (fmt == RawFormat::UNKNOWN) {
        throw std::runtime_error(
            "raw_loader: file size " + std::to_string(file_size) +
            " bytes does not match any known RAW format for " +
            std::to_string(width) + "x" + std::to_string(height));
    }

    const std::size_t total = static_cast<std::size_t>(width) * height;
    pixels.assign(total, 0);

    std::vector<std::uint8_t> buf(file_size);
    ifs.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(file_size));
    if (!ifs) {
        throw std::runtime_error("raw_loader: short read from '" + path + "'");
    }

    switch (fmt) {
    case RawFormat::RAW16_LE: {
        for (std::size_t i = 0; i < total; ++i) {
            const std::uint16_t lo = buf[2 * i];
            const std::uint16_t hi = buf[2 * i + 1];
            pixels[i] = static_cast<std::uint16_t>(lo | (hi << 8));
        }
        break;
    }
    case RawFormat::PACKED12: {
        // 2 pixels per 3 bytes; little-endian: p0 = byte0 | (byte1<<8),
        // p1 = (byte1 & 0x0F)<<8 | byte2 ... actually common convention:
        //   byte0 = p0[7:0], byte1 = p0[11:8] | p1[3:0], byte2 = p1[11:4]
        for (std::size_t i = 0; i < total; i += 2) {
            const std::size_t bi = (i / 2) * 3;
            const std::uint16_t p0 = static_cast<std::uint16_t>(
                buf[bi] | ((buf[bi + 1] & 0x0F) << 8));
            const std::uint16_t p1 = static_cast<std::uint16_t>(
                ((buf[bi + 1] & 0xF0) >> 4) | (buf[bi + 2] << 4));
            pixels[i]     = p0;
            pixels[i + 1] = p1;
        }
        break;
    }
    case RawFormat::PACKED10: {
        // 4 pixels per 5 bytes: 10|10|10|10 packed MSB-first.
        for (std::size_t i = 0; i < total; i += 4) {
            const std::size_t bi = (i / 4) * 5;
            pixels[i + 0] = static_cast<std::uint16_t>((buf[bi]     << 2) | (buf[bi + 4] & 0x03));
            pixels[i + 1] = static_cast<std::uint16_t>((buf[bi + 1] << 2) | ((buf[bi + 4] >> 2) & 0x03));
            pixels[i + 2] = static_cast<std::uint16_t>((buf[bi + 2] << 2) | ((buf[bi + 4] >> 4) & 0x03));
            pixels[i + 3] = static_cast<std::uint16_t>((buf[bi + 3] << 2) | ((buf[bi + 4] >> 6) & 0x03));
        }
        break;
    }
    default:
        // unreachable: caught above
        break;
    }

    if (format_out) *format_out = fmt;
}

/**
 * @brief Save a flat `std::uint16_t` buffer as RAW16 little-endian.
 */
inline void save_raw16(const std::string& path,
                       const std::vector<std::uint16_t>& pixels)
{
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) {
        throw std::runtime_error("raw_loader: cannot open output '" + path + "'");
    }
    for (std::uint16_t v : pixels) {
        const std::uint8_t lo = static_cast<std::uint8_t>(v & 0xFF);
        const std::uint8_t hi = static_cast<std::uint8_t>((v >> 8) & 0xFF);
        ofs.put(static_cast<char>(lo));
        ofs.put(static_cast<char>(hi));
    }
}

/**
 * @brief Save a YUV420 byte buffer to disk in NV12 (semi-planar) layout.
 *
 * NV12 layout (matches the output of `yuv420_block::process`):
 *   - Y plane: W * H bytes
 *   - UV plane: 2 * ((W+1)/2) * ((H+1)/2) bytes, interleaved as
 *               U,V,U,V,...
 *
 * Total file size = W*H + 2 * ((W+1)/2) * ((H+1)/2).
 *
 * Most raw YUV viewers (ffplay, mplayer, etc.) and the original
 * `visualize.py` expect NV12. Writing planar Y/U/V here would break
 * compatibility with those tools.
 */
inline void save_yuv420(const std::string& path,
                        const std::vector<std::uint8_t>& yuv,
                        std::uint32_t out_w, std::uint32_t out_h)
{
    const std::uint32_t half_w = (out_w + 1) / 2;
    const std::uint32_t half_h = (out_h + 1) / 2;
    const std::size_t expected =
        static_cast<std::size_t>(out_w) * out_h +
        2u * half_w * half_h;

    if (yuv.size() != expected) {
        std::cerr << "[raw_loader] WARNING: YUV buffer size " << yuv.size()
                  << " != expected " << expected
                  << " (still writing the buffer as-is)" << std::endl;
    }

    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) {
        throw std::runtime_error("raw_loader: cannot open output '" + path + "'");
    }
    ofs.write(reinterpret_cast<const char*>(yuv.data()),
              static_cast<std::streamsize>(yuv.size()));
}

/**
 * @brief Save a YUV420 byte buffer to disk in planar Y/U/V layout.
 *
 * Most viewers expect NV12; this planar variant is mainly useful for
 * debugging or for pipelines that emit planar YUV420.
 *
 * Layout:
 *   - Y plane: W * H bytes
 *   - U plane: ((W+1)/2) * ((H+1)/2) bytes
 *   - V plane: same size as U
 */
inline void save_yuv420_planar(const std::string& path,
                               const std::vector<std::uint8_t>& yuv,
                               std::uint32_t out_w, std::uint32_t out_h)
{
    const std::uint32_t half_w = (out_w + 1) / 2;
    const std::uint32_t half_h = (out_h + 1) / 2;
    const std::size_t expected =
        static_cast<std::size_t>(out_w) * out_h +
        2u * half_w * half_h;

    if (yuv.size() != expected) {
        std::cerr << "[raw_loader] WARNING: planar YUV buffer size " << yuv.size()
                  << " != expected " << expected
                  << " (still writing the buffer as-is)" << std::endl;
    }
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) {
        throw std::runtime_error("raw_loader: cannot open output '" + path + "'");
    }
    ofs.write(reinterpret_cast<const char*>(yuv.data()),
              static_cast<std::streamsize>(yuv.size()));
}

/**
 * @brief Save a 16-bit planar image (e.g. processed Bayer) as PGM.
 *
 * Convenient for quick visual inspection with `eog`, `feh`, or `gimp`.
 */
inline void save_pgm(const std::string& path,
                     const std::vector<std::uint16_t>& pixels,
                     std::uint32_t w, std::uint32_t h,
                     std::uint32_t max_val = 4095)
{
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) {
        throw std::runtime_error("raw_loader: cannot open output '" + path + "'");
    }
    ofs << "P5\n" << w << " " << h << "\n" << max_val << "\n";
    for (std::uint16_t v : pixels) {
        const std::uint8_t lo = static_cast<std::uint8_t>(v & 0xFF);
        const std::uint8_t hi = static_cast<std::uint8_t>((v >> 8) & 0xFF);
        ofs.put(static_cast<char>(lo));
        ofs.put(static_cast<char>(hi));
    }
}

} // namespace raw_loader

#endif // ISP_RAW_LOADER_H
