// SPDX-License-Identifier: Apache-2.0
// VP-only post-Demosaic capture writer (fx1_isp/fx1_isp_debug.h): RGB16 data,
// RGB8 PNG preview and JSON metadata per frame, streamed row by row into
// temporary files and published by rename once the frame has retired with
// every output DONE.
#include "fx1_isp/fx1_isp_debug.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <dirent.h>
#include <sys/stat.h>

#include "debug/sha256.h"

namespace cdc::components::fx1_isp {

const char *to_string(demosaic_frame_end how) {
   switch (how) {
   case demosaic_frame_end::completed:
      return "frame_completed";
   case demosaic_frame_end::failed:
      return "frame_failed";
   case demosaic_frame_end::reset:
      return "frame_reset";
   }
   return "unknown";
}

namespace {

constexpr std::uint16_t max12 = 0x0FFF;
const char *const bayer_names[4] = {"RGGB", "GRBG", "GBRG", "BGGR"};

std::uint32_t crc32_update(std::uint32_t crc, const std::uint8_t *p, std::size_t n) {
   static const std::array<std::uint32_t, 256> table = [] {
      std::array<std::uint32_t, 256> t{};
      for (std::uint32_t i = 0; i < 256; ++i) {
         std::uint32_t c = i;
         for (int k = 0; k < 8; ++k) {
            c = (c & 1u) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
         }
         t[i] = c;
      }
      return t;
   }();
   for (std::size_t i = 0; i < n; ++i) {
      crc = table[(crc ^ p[i]) & 0xFFu] ^ (crc >> 8);
   }
   return crc;
}

void put_be32(std::vector<std::uint8_t> &b, std::uint32_t v) {
   for (int s = 24; s >= 0; s -= 8) {
      b.push_back(static_cast<std::uint8_t>(v >> s));
   }
}

// One PNG chunk: length, type, data, CRC-32 over type and data.
void write_chunk(std::ofstream &f, const char *type, const std::vector<std::uint8_t> &data) {
   std::vector<std::uint8_t> b;
   put_be32(b, static_cast<std::uint32_t>(data.size()));
   b.insert(b.end(), type, type + 4);
   b.insert(b.end(), data.begin(), data.end());
   put_be32(b, crc32_update(0xFFFFFFFFu, b.data() + 4, b.size() - 4) ^ 0xFFFFFFFFu);
   f.write(reinterpret_cast<const char *>(b.data()), static_cast<std::streamsize>(b.size()));
}

bool exists_dir(const std::string &p) {
   struct stat st {};
   return ::stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

void make_dir(const std::string &p) {
   if (::mkdir(p.c_str(), 0777) != 0 && !(errno == EEXIST && exists_dir(p))) {
      throw std::runtime_error("demosaic dump: cannot create directory " + p + ": " + std::strerror(errno));
   }
}

// A run owns its directory: files of an earlier run could otherwise sit under
// the same frame keys (ordinal, epoch and sequence restart with the model).
void require_empty(const std::string &p) {
   DIR *d = ::opendir(p.c_str());
   if (!d) {
      throw std::runtime_error("demosaic dump: cannot read directory " + p + ": " + std::strerror(errno));
   }
   bool empty = true;
   while (const dirent *e = ::readdir(d)) {
      if (std::strcmp(e->d_name, ".") != 0 && std::strcmp(e->d_name, "..") != 0) {
         empty = false;
         break;
      }
   }
   ::closedir(d);
   if (!empty) {
      throw std::runtime_error("demosaic dump: " + p + " is not empty; each run needs a new or empty directory");
   }
}

std::string json_escape(const std::string &s) {
   std::string o;
   for (const char c : s) {
      if (c == '"' || c == '\\') {
         o += '\\';
         o += c;
      } else if (static_cast<unsigned char>(c) < 0x20) {
         char buf[8];
         std::snprintf(buf, sizeof buf, "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(c)));
         o += buf;
      } else {
         o += c;
      }
   }
   return o;
}

std::string metadata(const demosaic_dump::capture &c) {
   const demosaic_frame_info &i = c.info;
   const std::uint64_t row_bytes = 6ull * i.width;
   std::ostringstream o;
   const char *b = i.bayer_pattern < 4 ? bayer_names[i.bayer_pattern] : "?";
   o << "{\n"
     << "  \"schema\": \"fx1-isp/demosaic-capture/1\",\n"
     << "  \"vp_only\": true,\n"
     << "  \"note\": \"VP debug tap of the FX1 ISP model; not an ISP hardware output (no CSR, DMA, IRQ or DDR "
        "buffer)\",\n"
     << "  \"stage\": \"post_demosaic_pre_ccm\",\n"
     << "  \"applied\": [\"input_formatter\", \"blc\", \"lsc\", \"bpc\", \"wb\", \"dg\", \"demosaic\"],\n"
     << "  \"not_applied\": [\"ccm\", \"gamma\", \"csc\", \"gtm\", \"nr2d\", \"ee\", \"cnf\", \"resizer\", "
        "\"output_formatter\"],\n"
     << "  \"status\": \"" << c.status << "\",\n"
     << "  \"frame\": {\"ordinal\": " << c.ordinal << ", \"epoch\": " << i.epoch << ", \"sequence\": " << i.sequence
     << ", \"frame_id\": " << i.frame_id << ", \"profile_id\": " << i.profile_id << ", \"input_tag\": " << i.input_tag
     << ", \"input_ts\": " << i.input_ts << ", \"sof_time_ns\": " << std::fixed << std::setprecision(3)
     << i.sof_time_ns << ", \"outputs\": " << i.outputs << "},\n"
     << "  \"geometry\": {\"input_width\": " << i.input_width << ", \"input_height\": " << i.input_height
     << ", \"width\": " << i.width << ", \"height\": " << i.height << ", \"trim_x\": " << i.trim_x
     << ", \"trim_y\": " << i.trim_y << ", \"bayer\": \"" << b << "\", \"bayer_code\": " << i.bayer_pattern << "},\n"
     << "  \"layout\": {\"data_suffix\": \".rgb16\", \"order\": \"RGB\", \"interleaved\": true, \"component\": "
        "\"uint16_le\", \"significant_bits\": 12, \"bits\": \"[11:0]\", \"max\": 4095, \"header_bytes\": 0, "
        "\"row_padding_bytes\": 0, \"row_bytes\": "
     << row_bytes << ", \"bytes\": " << row_bytes * i.height << "},\n"
     << "  \"sha256\": \"" << c.sha256 << "\",\n"
     << "  \"preview\": {\"suffix\": \".png\", \"format\": \"PNG RGB8\", \"rule\": \"v8 = v12 >> 4 per component; "
        "no white balance, gamma, tone mapping or auto-level\"},\n"
     << "  \"config\": {\"input_depth\": \"" << (i.input_depth == 1 ? "RAW10" : "RAW12")
     << "\", \"blc_en\": " << (i.blc_en ? "true" : "false") << ", \"lsc_en\": " << (i.lsc_en ? "true" : "false")
     << ", \"bpc_en\": " << (i.bpc_en ? "true" : "false") << ", \"wb_en\": " << (i.wb_en ? "true" : "false")
     << ", \"wb_gain\": {\"r\": " << i.wb_gain_r << ", \"gr\": " << i.wb_gain_gr << ", \"gb\": " << i.wb_gain_gb
     << ", \"b\": " << i.wb_gain_b << ", \"format\": \"UQ4.8\"}, \"dg_en\": " << (i.dg_en ? "true" : "false")
     << ", \"dg_gain\": " << i.dg_gain << ", \"dms_margin_green\": " << i.dms_margin_green
     << ", \"dms_margin_rb\": " << i.dms_margin_rb << "},\n"
     << "  \"files\": \"<base>.rgb16, <base>.png and <base>.json share one base name\"\n"
     << "}\n";
   return o.str();
}

// Copies `from` to `to` (a temporary name of the caller); false on any error.
bool stage_file(const std::string &from, const std::string &to) {
   std::ifstream in(from, std::ios::binary);
   if (!in) {
      return false;
   }
   std::ofstream out(to, std::ios::binary | std::ios::trunc);
   out << in.rdbuf();
   out.flush();
   return out && !in.bad();
}

}  // namespace

struct demosaic_dump::impl {
   struct open_frame {
      std::size_t index = 0;  // in captures
      std::ofstream rgb, png;
      std::string rgb_tmp, png_tmp;
      debug::sha256 sha;
      std::uint32_t adler_a = 1, adler_b = 0;
      std::uint32_t rows = 0;
      bool done = false;    // pipeline_done seen
      std::string failure;  // first write or contract error
   };
   std::string dir;
   namer name;
   std::vector<capture> caps;
   std::map<std::uint64_t, open_frame> open;
   std::vector<std::string> errors;
   std::ofstream index;
   std::uint64_t ordinal = 0;
   bool finished = false;
   std::vector<std::uint8_t> data, png_row;

   void fail(open_frame &f, const std::string &why) {
      if (f.failure.empty()) {
         f.failure = why;
      }
   }
   void discard(open_frame &f) {
      f.rgb.close();
      f.png.close();
      std::remove(f.rgb_tmp.c_str());
      std::remove(f.png_tmp.c_str());
   }
   void log(const capture &c, const std::string &detail) {
      index << "{\"ordinal\": " << c.ordinal << ", \"epoch\": " << c.info.epoch << ", \"sequence\": "
            << c.info.sequence << ", \"frame_id\": " << c.info.frame_id << ", \"profile_id\": " << c.info.profile_id
            << ", \"status\": \"" << c.status << "\", \"base\": \"" << json_escape(c.base) << "\", \"sha256\": \""
            << c.sha256 << "\"" << (detail.empty() ? "" : ", \"detail\": \"" + json_escape(detail) + "\"") << "}\n";
      index.flush();
      if (!index) {
         errors.push_back("demosaic dump: cannot write " + dir + "/index.jsonl");
      }
   }
   void publish(open_frame &f, capture &c) {
      write_chunk(f.png, "IEND", {});
      f.rgb.flush();
      f.png.flush();
      if (!f.rgb || !f.png) {
         throw std::runtime_error("write failed");
      }
      f.rgb.close();
      f.png.close();
      c.sha256 = f.sha.hex();
      const std::string json_tmp = c.base + ".json.part";
      {
         std::ofstream j(json_tmp, std::ios::trunc);
         j << metadata(c);
         j.flush();
         if (!j) {
            std::remove(json_tmp.c_str());
            throw std::runtime_error("cannot write " + json_tmp);
         }
      }
      // Data first: a JSON is never left next to missing data.
      const std::pair<std::string, std::string> moves[] = {
          {f.rgb_tmp, c.base + ".rgb16"}, {f.png_tmp, c.base + ".png"}, {json_tmp, c.base + ".json"}};
      for (const auto &m : moves) {
         if (std::rename(m.first.c_str(), m.second.c_str()) != 0) {
            const std::string why = "cannot rename " + m.first + ": " + std::strerror(errno);
            for (const auto &u : moves) {
               std::remove(u.first.c_str());
               std::remove(u.second.c_str());
            }
            throw std::runtime_error(why);
         }
      }
   }
};

demosaic_dump::demosaic_dump(const std::string &dir, namer name) : impl_(std::make_unique<impl>()) {
   impl &m = *impl_;
   m.dir = dir.empty() ? std::string(".") : dir;
   make_dir(m.dir);
   require_empty(m.dir);
   make_dir(m.dir + "/frames");
   m.name = name ? std::move(name) : namer([d = m.dir](const capture &c) {
      char b[96];
      std::snprintf(b, sizeof b, "/frames/f%04llu_e%llu_s%llu", static_cast<unsigned long long>(c.ordinal),
                    static_cast<unsigned long long>(c.info.epoch), static_cast<unsigned long long>(c.info.sequence));
      return d + b;
   });
   m.index.open(m.dir + "/index.jsonl", std::ios::trunc);
   if (!m.index) {
      throw std::runtime_error("demosaic dump: cannot create " + m.dir + "/index.jsonl");
   }
}

demosaic_dump::~demosaic_dump() {
   try {
      finish();
   } catch (...) {
   }
}

void demosaic_dump::frame_begin(const demosaic_frame_info &f) {
   impl &m = *impl_;
   capture c;
   c.ordinal = ++m.ordinal;
   c.info = f;
   c.status = "open";
   c.base = m.name(c);
   m.caps.push_back(c);
   auto ins = m.open.emplace(f.sequence, impl::open_frame{});
   if (!ins.second) {
      m.errors.push_back("demosaic dump: frame sequence " + std::to_string(f.sequence) + " begun twice");
      return;
   }
   impl::open_frame &o = ins.first->second;
   o.index = m.caps.size() - 1;
   o.rgb_tmp = c.base + ".rgb16.part";
   o.png_tmp = c.base + ".png.part";
   o.rgb.open(o.rgb_tmp, std::ios::binary | std::ios::trunc);
   o.png.open(o.png_tmp, std::ios::binary | std::ios::trunc);
   if (!o.rgb || !o.png) {
      m.fail(o, "cannot create " + (o.rgb ? o.png_tmp : o.rgb_tmp) + ": " + std::strerror(errno));
      return;
   }
   static const std::uint8_t signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
   o.png.write(reinterpret_cast<const char *>(signature), sizeof signature);
   std::vector<std::uint8_t> ihdr;
   put_be32(ihdr, f.width);
   put_be32(ihdr, f.height);
   ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});  // 8-bit truecolour, deflate, no filter, no interlace
   write_chunk(o.png, "IHDR", ihdr);
}

void demosaic_dump::row(std::uint64_t sequence, std::uint32_t y, const std::uint16_t *rgb) {
   impl &m = *impl_;
   const auto it = m.open.find(sequence);
   if (it == m.open.end()) {
      return;  // a frame that has already ended
   }
   impl::open_frame &o = it->second;
   const demosaic_frame_info &info = m.caps[o.index].info;
   if (!o.failure.empty()) {
      return;
   }
   if (y != o.rows || y >= info.height || o.done) {
      m.fail(o, "row " + std::to_string(y) + " out of order (expected " + std::to_string(o.rows) + ")");
      return;
   }
   const std::size_t n = 3u * std::size_t{info.width};
   // RGB16: explicit little-endian encoding of each 12-bit component.
   m.data.resize(2u * n);
   // PNG scanline: filter type 0, then v12 >> 4.
   m.png_row.resize(1u + n);
   m.png_row[0] = 0;
   for (std::size_t i = 0; i < n; ++i) {
      const std::uint16_t v = rgb[i];
      if (v > max12) {
         m.fail(o, "component above 12 bits at row " + std::to_string(y));
         return;
      }
      m.data[2u * i] = static_cast<std::uint8_t>(v & 0xFFu);
      m.data[2u * i + 1u] = static_cast<std::uint8_t>(v >> 8);
      m.png_row[1u + i] = static_cast<std::uint8_t>(v >> 4);
   }
   o.rgb.write(reinterpret_cast<const char *>(m.data.data()), static_cast<std::streamsize>(m.data.size()));
   o.sha.update(m.data.data(), m.data.size());
   // zlib stream of stored deflate blocks, one IDAT chunk per row.
   std::vector<std::uint8_t> idat;
   if (y == 0) {
      idat.push_back(0x78);  // CM 8, CINFO 7
      idat.push_back(0x01);  // FCHECK, no dictionary, fastest
   }
   const bool last = y + 1u == info.height;
   for (std::size_t at = 0; at < m.png_row.size();) {
      const std::size_t len = std::min<std::size_t>(0xFFFFu, m.png_row.size() - at);
      const bool final_block = last && at + len == m.png_row.size();
      idat.push_back(final_block ? 1u : 0u);
      idat.push_back(static_cast<std::uint8_t>(len & 0xFFu));
      idat.push_back(static_cast<std::uint8_t>(len >> 8));
      idat.push_back(static_cast<std::uint8_t>(~len & 0xFFu));
      idat.push_back(static_cast<std::uint8_t>((~len >> 8) & 0xFFu));
      idat.insert(idat.end(), m.png_row.begin() + static_cast<std::ptrdiff_t>(at),
                  m.png_row.begin() + static_cast<std::ptrdiff_t>(at + len));
      at += len;
   }
   for (const std::uint8_t b : m.png_row) {
      o.adler_a = (o.adler_a + b) % 65521u;
      o.adler_b = (o.adler_b + o.adler_a) % 65521u;
   }
   if (last) {
      put_be32(idat, (o.adler_b << 16) | o.adler_a);
   }
   write_chunk(o.png, "IDAT", idat);
   if (!o.rgb || !o.png) {
      m.fail(o, "write failed at row " + std::to_string(y) + ": " + std::strerror(errno));
      return;
   }
   ++o.rows;
}

void demosaic_dump::pipeline_done(std::uint64_t sequence) {
   impl &m = *impl_;
   const auto it = m.open.find(sequence);
   if (it == m.open.end()) {
      return;
   }
   impl::open_frame &o = it->second;
   o.done = true;
   if (o.rows != m.caps[o.index].info.height) {
      m.fail(o, "pipeline completed after " + std::to_string(o.rows) + " of " +
                    std::to_string(m.caps[o.index].info.height) + " rows");
   }
}

void demosaic_dump::frame_end(std::uint64_t sequence, demosaic_frame_end how) {
   impl &m = *impl_;
   const auto it = m.open.find(sequence);
   if (it == m.open.end()) {
      return;  // not begun here (observer set late) or already ended
   }
   impl::open_frame &o = it->second;
   capture &c = m.caps[o.index];
   std::string detail;
   if (how == demosaic_frame_end::completed && !o.done) {
      m.fail(o, "frame retired as completed before the pipeline completed it");
   }
   if (how == demosaic_frame_end::completed && o.failure.empty()) {
      c.status = to_string(how);  // the JSON's status
      try {
         m.publish(o, c);
      } catch (const std::exception &e) {
         c.status = "open";
         m.fail(o, e.what());
      }
   }
   if (c.status == "open") {
      m.discard(o);
      c.sha256.clear();
      if (!o.failure.empty()) {
         c.status = "capture_error";
         detail = o.failure;
         m.errors.push_back("demosaic dump: frame " + std::to_string(c.ordinal) + " (" + to_string(how) +
                            "): " + o.failure);
      } else {
         c.status = to_string(how);
      }
      c.base.clear();
   }
   m.log(c, detail);
   m.open.erase(it);
}

void demosaic_dump::finish() {
   impl &m = *impl_;
   if (m.finished) {
      return;
   }
   m.finished = true;
   for (auto &e : m.open) {
      capture &c = m.caps[e.second.index];
      m.discard(e.second);
      c.status = "incomplete";
      c.base.clear();
      m.log(c, "no frame end before the end of the run");
   }
   m.open.clear();
   m.index.close();
}

const std::vector<demosaic_dump::capture> &demosaic_dump::captures() const { return impl_->caps; }

bool demosaic_dump::ok() const { return impl_->errors.empty(); }

const std::vector<std::string> &demosaic_dump::errors() const { return impl_->errors; }

void demosaic_dump::copy_capture(const std::string &from_base, const std::string &to_base) {
   // The JSON is the commit marker of a set. Stage all three files first: a
   // failure there leaves the destination as it was. Then withdraw the old
   // JSON, move the data in and the JSON last: a failure in between leaves no
   // JSON, so no set claims data it does not describe.
   const char *const ext[3] = {".rgb16", ".png", ".json"};
   const auto cleanup = [&](bool targets) {
      for (const char *e : ext) {
         std::remove((to_base + e + ".part").c_str());
         if (targets) {
            std::remove((to_base + e).c_str());
         }
      }
   };
   for (const char *e : ext) {
      if (!stage_file(from_base + e, to_base + e + ".part")) {
         const std::string why = "demosaic dump: cannot copy " + from_base + e + " to " + to_base + e + ".part";
         cleanup(false);
         throw std::runtime_error(why);
      }
   }
   if (std::remove((to_base + ".json").c_str()) != 0 && errno != ENOENT) {
      const std::string why = "demosaic dump: cannot replace " + to_base + ".json: " + std::strerror(errno);
      cleanup(false);
      throw std::runtime_error(why);
   }
   for (const char *e : ext) {
      if (std::rename((to_base + e + ".part").c_str(), (to_base + e).c_str()) != 0) {
         const std::string why = "demosaic dump: cannot rename " + to_base + e + ".part: " + std::strerror(errno);
         cleanup(true);
         throw std::runtime_error(why);
      }
   }
}

}  // namespace cdc::components::fx1_isp
