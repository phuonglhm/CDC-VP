// tb_has_host_neo.cpp -- round trip LayerInstr -> neo_lite register file -> LayerInstr over every instruction of compiled programs.
//
//   tb_has_host_neo [--csv <file>] [--quiet] <dir> [<dir> ...]     dir = program directory with mmio.txt ("W addr value" lines, v4.5 protocol)
//
// For each instruction popped from has::MmioCompat: plan() -> 82 words (+ ext lines as text) -> the CPU write sequence (libneo_cfg write_sequence) ->
// words read back from those writes -> decode() -> compared field by field with the normalised original, and tile by tile (TileIter).
// Also: the packed pooling / residual words against has/gvu_regs_dfc.h namespace neo (an independent packer of the same hardware registers), and the
// reasons (A-* / B-*) of the layers the hardware cannot run as given (COMPILER_GUIDE.md, support matrix).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include "has/gvu_regs_dfc.h"
#include "has/has_host_neo.h"

using namespace has;
namespace HN = has::host_neo;

namespace
{
    const char *kind_of(const LayerInstr &l)
    {
        switch (l.opcode)
        {
        case 0x12: return "GEMM_FUSED";
        case 0x13: return "FUSED_ATTN";
        case 0x14: return "LAYERNORM";
        case 0x15: return l.mode == 0 ? "ADD" : l.mode == 1 ? "MAX_POOL" : l.mode == 5 ? "AVG_POOL" : l.mode == 6 ? "SLICE" : l.mode == 7 ? "CONCAT" : "ELEM_OTHER";
        case 0x16: return "DMA_TENSOR";
        }
        return "OTHER";
    }
    struct Stat { long n{0}, as_given{0}, only_a{0}, error{0}, field_bad{0}, tile_bad{0}, dec_err{0}, word_bad{0}, text_bad{0}; std::map<std::string, long> codes; };

    bool same_tiles(const LayerGeom &a, const LayerGeom &b)
    {
        TileIter x(a), y(b);
        if (x.count() != y.count()) return false;
        for (long i = 0; i < x.count(); i++)
        {
            const TileGeom p = x.at(i), q = y.at(i);
            if (std::memcmp(&p, &q, sizeof p) != 0) return false;
        }
        return true;
    }
}

int main(int argc, char **argv)
{
#ifdef _WIN32
    _putenv_s("HAS_NEO_REV", "1");
#else
    setenv("HAS_NEO_REV", "1", 1);
#endif
    std::vector<std::string> dirs;
    std::string csv_path;
    bool quiet = false;
    std::string show_code;
    for (int i = 1; i < argc; i++)
    {
        if (!std::strcmp(argv[i], "--csv") && i + 1 < argc) csv_path = argv[++i];
        else if (!std::strcmp(argv[i], "--quiet")) quiet = true;
        else if (!std::strcmp(argv[i], "--show") && i + 1 < argc) show_code = argv[++i];   // print the ext lines of the layers that carry this code
        else dirs.push_back(argv[i]);
    }
    if (dirs.empty()) { std::fprintf(stderr, "usage: %s [--csv f] [--quiet] <dir> ...\n", argv[0]); return 2; }
    std::ofstream csv;
    if (!csv_path.empty()) { csv.open(csv_path); csv << "program,index,host_seq,kind,as_given,only_class_a,issues\n"; }

    bool all_pass = true;
    std::map<std::string, Stat> total;
    for (const std::string &dir : dirs)
    {
        std::ifstream f(dir + "/mmio.txt");
        if (!f) { std::printf("cannot open %s/mmio.txt\n", dir.c_str()); return 2; }
        MmioCompat m(1 << 20);
        std::map<std::string, Stat> st;
        long idx = 0, mmio_err = 0, sanity_bad = 0, shown = 0;
        std::string line;
        while (std::getline(f, line))
        {
            std::istringstream is(line);
            std::string tag; uint32_t a, v;
            if (!(is >> tag) || tag != "W" || !(is >> std::hex >> a >> v)) continue;
            m.write(a, v);
            LayerInstr li;
            while (m.pop(li))
            {
                m.retire(li);
                const LayerInstr want = HN::normalize(li);
                const char *kind = kind_of(li);
                Stat &S = st[kind];
                S.n++;
                const HN::Plan p = HN::plan(li);
                if (!p.error.empty()) { S.error++; all_pass = false; if (shown++ < 10) std::printf("%s #%ld %s: plan error: %s\n", dir.c_str(), idx, kind, p.error.c_str()); idx++; continue; }
                // the trace as text: CPU writes + ext lines
                const auto writes = neo::write_sequence(p.img, 0);
                neo::Image back;
                for (const auto &w : writes) neo::apply_write(back, 0, w.first, w.second);
                std::vector<HN::Ext> ext2;
                for (const HN::Ext &e : p.ext)
                {
                    HN::Ext q;
                    if (!HN::parse_ext_line(HN::ext_line(e), q) || q.field != e.field || q.value != e.value || q.code != e.code) S.text_bad++;
                    ext2.push_back(q);
                }
                std::vector<std::string> derr;
                const LayerInstr got = HN::decode(back, ext2, &derr);
                S.dec_err += static_cast<long>(derr.size());
                if (shown < 10) for (const std::string &s : derr) std::printf("%s #%ld %s: decode: %s\n", dir.c_str(), idx, kind, s.c_str());
                long bad = 0;
                for (const HN::FieldDef &fd : HN::fields())
                    if (fd.get(want) != fd.get(got))
                    {
                        bad++;
                        if (shown++ < 10) std::printf("%s #%ld %s: field %s: %lld -> %lld\n", dir.c_str(), idx, kind, fd.name, (long long)fd.get(want), (long long)fd.get(got));
                    }
                S.field_bad += bad;
                if (li.opcode == 0x12 && !same_tiles(want.geom, got.geom)) { S.tile_bad++; if (shown++ < 10) std::printf("%s #%ld %s: tiles differ\n", dir.c_str(), idx, kind); }
                // independent packer of the pooling / residual words (has/gvu_regs_dfc.h namespace neo)
                if (want.opcode == 0x15 && (want.mode == 1 || want.mode == 5) && p.error.empty())
                {
                    const uint32_t p0 = p.img.gvu(neo::G_POOL0);
                    const unsigned ct = p0 >> 24;
                    const int hp = want.h + 2 * want.pool_p, wp = want.w + 2 * want.pool_p, nw = (wp + 31) / 32, oh = (hp - want.pool_k) / want.pool_s + 1;
                    const auto ref = gvu_regs_dfc::neo::pool_regs(unsigned(want.pool_k - 2), unsigned(want.pool_s), unsigned(nw), ct, unsigned(oh), unsigned(nw * hp), unsigned(nw * want.pool_s));
                    if (ref[0].data != p0 || ref[1].data != p.img.gvu(neo::G_POOL1)) { S.word_bad++; if (shown++ < 10) std::printf("%s #%ld pool words differ from gvu_regs_dfc::neo\n", dir.c_str(), idx); }
                }
                if (want.opcode == 0x15 && want.mode == 0)
                {
                    rtl_ref_gvu::ResidualHwParams hp;
                    if (HN::detail::add_map(want, hp))
                    {
                        gvu_regs_dfc::AddRegs c;
                        c.scale_a = hp.sa; c.scale_b = hp.sb; c.zp_out = hp.z; c.shift_out = unsigned(hp.sh);
                        const unsigned rows = p.img.gvu(neo::G_RES0) & 0xFFFFu;
                        const auto ref = gvu_regs_dfc::neo::add_run(c, rows, 0xFFFFFFFFu, hp.bias);
                        const neo::GvuReg rr[5] = {neo::G_RES0, neo::G_RES1, neo::G_RES2, neo::G_RES3, neo::G_RES4};
                        bool ok = true;
                        for (int i = 0; i < 5; i++) ok = ok && ref[size_t(i)].data == p.img.gvu(rr[i]);
                        if (!ok) { S.word_bad++; if (shown++ < 10) std::printf("%s #%ld residual words differ from gvu_regs_dfc::neo\n", dir.c_str(), idx); }
                    }
                }
                // sanity expectations written before the run
                if ((want.opcode == 0x12 && want.geom.pad_t > 0 && !std::count(p.issues.begin(), p.issues.end(), std::string("B-PAD"))) ||
                    ((want.opcode == 0x13 || want.opcode == 0x14) && !std::count(p.issues.begin(), p.issues.end(), std::string("A-H3"))) ||
                    (want.opcode == 0x12 && !std::count(p.issues.begin(), p.issues.end(), std::string("A-H1"))))
                    sanity_bad++;
                if (!show_code.empty() && std::count(p.issues.begin(), p.issues.end(), show_code))
                    for (const HN::Ext &e : p.ext) std::printf("%s #%ld %s: %s\n", dir.c_str(), idx, kind, HN::ext_line(e).c_str());
                if (p.as_given()) S.as_given++;
                if (p.only_class_a()) S.only_a++;
                std::string iss;
                for (const std::string &s : p.issues) { S.codes[s]++; iss += (iss.empty() ? "" : ";") + s; }
                if (csv.is_open()) csv << dir << ',' << idx << ',' << li.host_seq << ',' << kind << ',' << (p.as_given() ? 1 : 0) << ',' << (p.only_class_a() ? 1 : 0) << ',' << iss << '\n';
                idx++;
            }
            if (m.last_error() != Err::NONE) { mmio_err++; m.write(mmio::STATUS, 0); }
        }
        std::printf("== %s: %ld instructions, %ld MMIO errors\n", dir.c_str(), idx, mmio_err);
        for (auto &kv : st)
        {
            const Stat &S = kv.second;
            std::printf("  %-11s n %4ld | words+ext round trip: field diffs %ld, tile diffs %ld, decode errors %ld, ext text diffs %ld, plan errors %ld, packer diffs %ld | "
                        "as given on HW %ld, only class A %ld\n",
                        kv.first.c_str(), S.n, S.field_bad, S.tile_bad, S.dec_err, S.text_bad, S.error, S.word_bad, S.as_given, S.only_a);
            if (!quiet)
            {
                std::printf("      codes:");
                for (auto &c : S.codes) std::printf(" %s=%ld", c.first.c_str(), c.second);
                std::printf("\n");
            }
            Stat &T = total[kv.first];
            T.n += S.n; T.as_given += S.as_given; T.only_a += S.only_a;
            for (auto &c : S.codes) T.codes[c.first] += c.second;
            if (S.field_bad || S.tile_bad || S.dec_err || S.text_bad || S.error || S.word_bad) all_pass = false;
        }
        if (sanity_bad) { std::printf("  sanity expectations violated: %ld\n", sanity_bad); all_pass = false; }
        if (mmio_err) all_pass = false;
    }
    std::printf("[tb_has_host_neo] RESULT: %s\n", all_pass ? "PASS" : "FAIL");
    return all_pass ? 0 : 1;
}
