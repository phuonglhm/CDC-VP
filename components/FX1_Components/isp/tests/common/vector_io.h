// SPDX-License-Identifier: Apache-2.0
// Readers for the committed vector files (tools/gen_vectors.py): CSR write
// lists, between-frame writes and the expected-read script.
#pragma once

#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "registers/csr_desc.h"

namespace fx1_test {

struct csr_line {
   std::uint32_t frame = 0;  // frames.csrw: written before the SOF of this frame
   bool set = false;         // expected_stats.txt: a write, not an expected read
   std::string reg;
   std::uint32_t value = 0;
};

// Profile syntax (ISP-UPD-008; the same contract as tools/csrw_codec.py): every
// line is ONE full 32-bit write, applied in order --
//   REGISTER VALUE            the whole register
//   REGISTER_field VALUE      one field, every other field 0
//   REGISTER f1=V1 f2=V2 ...  several fields of one register, the others 0
//   PORT_TABLE VALUE          ISP-UPD-013: a table data port named with the
//                             table it writes (EE_LUT_WDATA_ACTIVITY 0x8000 is
//                             EE_LUT_WDATA 0x8000; tagged_ports). The name is
//                             checked against the last write of the port's
//                             selector register earlier in the same file
//                             (EE_LUT_CTRL lut_sel=ACTIVITY); not a CSR register.
// VALUE: decimal without a leading zero or 0x hex, non-negative; a field value
// is its code before the shift and must fit the field, or the name of one of
// the field's CSR enum values (EE_LUT_CTRL lut_sel=ACTIVITY; ISP-UPD-012). A read-only field, a
// tied-zero bit, a field named twice or an unknown name throws
// std::runtime_error("<file>:<line>: ..."). The result is canonical
// (register name, word) writes; the readers never merge or reorder lines.
namespace csrw {

namespace csr = cdc::components::fx1_isp::csr;

// ISP-UPD-013: {port, selector register, selector field}; the same list as
// TAGGED_PORTS in tools/csrw_codec.py.
struct tagged_port {
   const char *port, *selector, *field;
};
inline constexpr tagged_port tagged_ports[] = {{"EE_LUT_WDATA", "EE_LUT_CTRL", "lut_sel"}};

struct table_tag {
   const csr::reg_desc *port, *selector;
   const csr::field_desc *field;
   std::string name;
   std::uint32_t code;
};

// The state of one file read so far: the last word written to each selector
// register (for the PORT_TABLE check).
struct context {
   std::map<std::string, std::uint32_t> last;
};

struct field_map {
   std::map<std::string, const csr::reg_desc *> regs;
   std::map<std::string, std::pair<const csr::reg_desc *, const csr::field_desc *>> aliases;
   std::map<std::string, table_tag> tags;
   std::vector<std::string> selectors;
   field_map() {
      for (std::size_t i = 0; i < csr::num_registers; ++i) {
         regs[csr::registers[i].name] = &csr::registers[i];
      }
      for (std::size_t i = 0; i < csr::num_registers; ++i) {
         const csr::reg_desc &r = csr::registers[i];
         for (std::size_t k = 0; k < r.num_fields; ++k) {
            const std::string a = std::string(r.name) + "_" + r.fields[k].name;
            if (regs.count(a) || aliases.count(a)) {
               throw std::logic_error("csrw: alias " + a + " is ambiguous");
            }
            aliases[a] = {&r, &r.fields[k]};
         }
      }
      for (const tagged_port &tp : tagged_ports) {
         const auto port = regs.find(tp.port), sel = regs.find(tp.selector);
         const csr::field_desc *f = nullptr;
         for (std::size_t k = 0; sel != regs.end() && k < sel->second->num_fields; ++k) {
            f = std::string(sel->second->fields[k].name) == tp.field ? &sel->second->fields[k] : f;
         }
         if (port == regs.end() || !f) {
            continue;
         }
         selectors.push_back(tp.selector);
         for (std::size_t i = 0; i < csr::num_enums; ++i) {
            const csr::enum_desc &e = csr::enums[i];
            if (std::string(e.reg) == tp.selector && std::string(e.field) == tp.field) {
               const std::string t = std::string(tp.port) + "_" + e.name;
               if (regs.count(t) || aliases.count(t) || tags.count(t)) {
                  throw std::logic_error("csrw: table name " + t + " is ambiguous");
               }
               tags[t] = {port->second, sel->second, f, e.name, e.value};
            }
         }
      }
   }
};

inline const field_map &map() {
   static const field_map m;
   return m;
}

inline std::uint64_t value_of(const std::string &t, const std::string &where) {
   bool ok = !t.empty();
   if (ok && t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) {
      for (std::size_t i = 2; ok && i < t.size(); ++i) {
         ok = std::isxdigit(static_cast<unsigned char>(t[i])) != 0;
      }
   } else if (ok) {
      ok = !(t.size() > 1 && t[0] == '0');
      for (std::size_t i = 0; ok && i < t.size(); ++i) {
         ok = std::isdigit(static_cast<unsigned char>(t[i])) != 0;
      }
   }
   if (!ok || t.size() > 18) {
      throw std::runtime_error(where + ": invalid integer '" + t + "' (decimal or 0x hex, non-negative)");
   }
   return std::stoull(t, nullptr, 0);
}

// A field value: an integer literal, or one of the field's enum names
// (csr::enums, generated from the CSR); an unknown name throws.
inline std::uint64_t field_value(const csr::reg_desc &r, const std::string &field, const std::string &t,
                                 const std::string &where) {
   if (t.empty() || !(std::isalpha(static_cast<unsigned char>(t[0])) || t[0] == '_')) {
      return value_of(t, where);
   }
   bool known = false;
   for (std::size_t k = 0; k < r.num_fields; ++k) {
      known = known || field == r.fields[k].name;
   }
   if (!known) {
      throw std::runtime_error(where + ": " + r.name + " has no field '" + field + "'");
   }
   std::string names;
   for (std::size_t i = 0; i < csr::num_enums; ++i) {
      const csr::enum_desc &e = csr::enums[i];
      if (field == e.field && std::string(r.name) == e.reg) {
         if (t == e.name) {
            return e.value;
         }
         names += std::string(names.empty() ? "" : ", ") + e.name;
      }
   }
   throw std::runtime_error(where + ": " + r.name + "." + field + ": unknown value '" + t + "'" +
                            (names.empty() ? std::string(" (the field has no named values)") : " (one of " + names + ")"));
}

inline std::uint32_t encode(const csr::reg_desc &r, const std::vector<std::pair<std::string, std::uint64_t>> &fields,
                            const std::string &where) {
   std::uint32_t word = 0;
   std::vector<std::string> seen;
   for (const auto &fv : fields) {
      const csr::field_desc *f = nullptr;
      for (std::size_t k = 0; k < r.num_fields; ++k) {
         if (fv.first == r.fields[k].name) {
            f = &r.fields[k];
         }
      }
      if (!f) {
         throw std::runtime_error(where + ": " + r.name + " has no field '" + fv.first + "'");
      }
      for (const std::string &n : seen) {
         if (n == fv.first) {
            throw std::runtime_error(where + ": field " + r.name + "." + fv.first + " given twice");
         }
      }
      seen.push_back(fv.first);
      if (f->type == csr::access::ro) {
         throw std::runtime_error(where + ": " + r.name + "." + fv.first + " is RO (not writable)");
      }
      if (fv.second >> f->width) {
         throw std::runtime_error(where + ": " + r.name + "." + fv.first + " does not fit " +
                                  std::to_string(f->width) + " bit(s)");
      }
      const std::uint64_t bits = fv.second << f->lsb;
      if (bits & ~std::uint64_t{f->mask}) {
         throw std::runtime_error(where + ": " + r.name + "." + fv.first + " sets a tied-zero bit");
      }
      word |= static_cast<std::uint32_t>(bits);
   }
   return word;
}

// One line (comment stripped) -> l.reg / l.value; false for a blank line.
inline bool parse_one(const std::string &text, const std::string &where, csr_line &l, const context &ctx) {
   std::istringstream is(text);
   std::vector<std::string> tok;
   for (std::string t; is >> t;) {
      tok.push_back(t);
   }
   if (tok.empty()) {
      return false;
   }
   if (tok.size() < 2) {
      throw std::runtime_error(where + ": " + tok[0] + " has no value");
   }
   const field_map &m = map();
   const auto reg = m.regs.find(tok[0]);
   bool assigns = true;
   for (std::size_t i = 1; i < tok.size(); ++i) {
      assigns = assigns && tok[i].find('=') != std::string::npos;
   }
   if (reg != m.regs.end() && assigns) {
      std::vector<std::pair<std::string, std::uint64_t>> fv;
      for (std::size_t i = 1; i < tok.size(); ++i) {
         const std::size_t eq = tok[i].find('=');
         if (eq == 0 || eq + 1 == tok[i].size()) {
            throw std::runtime_error(where + ": malformed field assignment '" + tok[i] + "'");
         }
         fv.emplace_back(tok[i].substr(0, eq),
                         field_value(*reg->second, tok[i].substr(0, eq), tok[i].substr(eq + 1), where));
      }
      l.reg = reg->second->name;
      l.value = encode(*reg->second, fv, where);
      return true;
   }
   if (tok.size() != 2 || tok[1].find('=') != std::string::npos) {
      throw std::runtime_error(where + ": expected 'REGISTER VALUE', 'REGISTER_field VALUE' or 'REGISTER f=V ...'");
   }
   const auto a = m.aliases.find(tok[0]);
   if (reg == m.regs.end() && a != m.aliases.end()) {  // REGISTER_field VALUE (a name too)
      l.reg = a->second.first->name;
      l.value = encode(*a->second.first,
                       {{a->second.second->name, field_value(*a->second.first, a->second.second->name, tok[1], where)}},
                       where);
      return true;
   }
   const auto t = m.tags.find(tok[0]);
   if (reg == m.regs.end() && t != m.tags.end()) {  // PORT_TABLE VALUE (ISP-UPD-013)
      const table_tag &g = t->second;
      const std::uint64_t v = value_of(tok[1], where);
      if (v >> 32) {
         throw std::runtime_error(where + ": value does not fit 32 bits");
      }
      const auto last = ctx.last.find(g.selector->name);
      if (last == ctx.last.end()) {
         throw std::runtime_error(where + ": " + tok[0] + " needs an earlier " + g.selector->name + " " +
                                  g.field->name + "=" + g.name + " in the file");
      }
      const std::uint32_t cur = (last->second & g.field->mask) >> g.field->lsb;
      if (cur != g.code) {
         std::string now = std::to_string(cur);
         for (std::size_t i = 0; i < csr::num_enums; ++i) {
            const csr::enum_desc &e = csr::enums[i];
            if (std::string(e.reg) == g.selector->name && std::string(e.field) == g.field->name && e.value == cur) {
               now = e.name;
            }
         }
         throw std::runtime_error(where + ": " + tok[0] + " but the last " + g.selector->name + " write has " +
                                  g.field->name + "=" + now);
      }
      l.reg = g.port->name;
      l.value = static_cast<std::uint32_t>(v);
      return true;
   }
   const std::uint64_t v = value_of(tok[1], where);
   if (reg != m.regs.end()) {
      if (v >> 32) {
         throw std::runtime_error(where + ": value does not fit 32 bits");
      }
      l.reg = reg->second->name;
      l.value = static_cast<std::uint32_t>(v);
      return true;
   }
   throw std::runtime_error(where + ": unknown register or field " + tok[0]);
}

// parse_one, keeping ctx (one per file; none: the line is read as the first of
// its file) up to date with the selector writes.
inline bool parse(const std::string &text, const std::string &where, csr_line &l, context *ctx = nullptr) {
   const context none;
   if (!parse_one(text, where, l, ctx ? *ctx : none)) {
      return false;
   }
   if (ctx) {
      for (const std::string &s : map().selectors) {
         if (l.reg == s) {
            ctx->last[s] = l.value;
         }
      }
   }
   return true;
}

// A UTF-8 byte order mark opening the file is not part of line 1 (review
// B/C/P/D R3: tools/csrw_codec.py reads utf-8-sig); one anywhere else stays and
// is an error.
inline void strip_bom(std::string &line, unsigned n) {
   if (n == 1 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) {
      line.erase(0, 3);
   }
}

}  // namespace csrw

// profile.csrw: one write per line, the syntax above.
inline std::vector<csr_line> read_profile(const std::filesystem::path &p) {
   std::vector<csr_line> out;
   std::ifstream f(p);
   std::string line;
   csrw::context ctx;
   for (unsigned n = 1; std::getline(f, line); ++n) {
      csrw::strip_bom(line, n);
      csr_line l;
      if (csrw::parse(line.substr(0, line.find('#')), p.string() + ":" + std::to_string(n), l, &ctx)) {
         out.push_back(l);
      }
   }
   return out;
}

// frames.csrw (optional): "<frame> <profile line>".
inline std::vector<csr_line> read_frame_writes(const std::filesystem::path &p) {
   std::vector<csr_line> out;
   std::ifstream f(p);
   std::string line;
   csrw::context ctx;
   for (unsigned n = 1; std::getline(f, line); ++n) {
      csrw::strip_bom(line, n);
      std::istringstream is(line.substr(0, line.find('#')));
      csr_line l;
      std::string rest;
      if (is >> l.frame && std::getline(is, rest) &&
          csrw::parse(rest, p.string() + ":" + std::to_string(n), l, &ctx)) {
         out.push_back(l);
      }
   }
   return out;
}

// expected_stats.txt: "<REG> <value>" reads and "SET <REG> <value>" writes.
inline std::vector<csr_line> read_expected(const std::filesystem::path &p) {
   std::vector<csr_line> out;
   std::ifstream f(p);
   std::string line;
   while (std::getline(f, line)) {
      std::istringstream is(line);
      csr_line l;
      std::string first, v;
      if (!(is >> first >> v)) {
         continue;
      }
      if (first == "SET") {
         l.set = true;
         l.reg = v;
         is >> v;
      } else {
         l.reg = first;
      }
      l.value = static_cast<std::uint32_t>(std::stoull(v, nullptr, 0));
      out.push_back(l);
   }
   return out;
}

}  // namespace fx1_test
