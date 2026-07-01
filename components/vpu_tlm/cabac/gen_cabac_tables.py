#!/usr/bin/env python3
"""
gen_cabac_tables.py

Extract `inittable[...] = VALUE;` assignments from cabac_ucontext.v and
emit simple binary LUT files under `tables/` plus a `tables.json` manifest.

Usage:
  python3 scripts/gen_cabac_tables.py --src cabac --out tables

The script looks for the following logical tables and writes these files:
  - cabac_ctx_islice.bin   -> base 0x10000000
  - cabac_ctx_init2.bin    -> base 0x10000100
  - cabac_ctx_init1.bin    -> base 0x10000200

The output `tables/tables.json` contains a small manifest array.
"""

import argparse
import os
import re
import json
import sys


def read_lines(path):
    with open(path, 'r', encoding='utf-8', errors='ignore') as f:
        return f.readlines()


def parse_parameters(lines):
    # Very small parameter extractor: finds name = NUMBER occurrences
    text = '\n'.join(lines)
    params = {}
    for m in re.finditer(r"([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([0-9]+)", text):
        try:
            params[m.group(1)] = int(m.group(2))
        except Exception:
            pass
    return params


def parse_number(tok, params):
    t = tok.strip().replace('_', '')
    if not t:
        return 0
    if t in params:
        return int(params[t])
    # Verilog style width'base literals: e.g. 8'hFF, 16'd42
    m = re.match(r"(?:\d+)?'([hHdDbBoO])([0-9A-Fa-f_]+)$", t)
    if m:
        base_char = m.group(1).lower()
        num = m.group(2).replace('_', '')
        try:
            if base_char == 'h':
                return int(num, 16)
            if base_char == 'd':
                return int(num, 10)
            if base_char == 'b':
                return int(num, 2)
            if base_char == 'o':
                return int(num, 8)
        except Exception:
            return 0
    # 0x prefixed
    if t.startswith('0x') or t.startswith('0X'):
        try:
            return int(t, 16)
        except Exception:
            return 0
    # decimal
    if re.match(r'^\d+$', t):
        return int(t)
    return 0


def eval_expr(expr, params):
    # supports simple additions/subtractions and parameter names
    if expr is None:
        return 0
    s = expr.split('//')[0]
    s = re.sub(r'\s+', '', s)
    if s == '':
        return 0
    parts = re.split(r'([+-])', s)
    total = 0
    sign = 1
    for p in parts:
        if p == '':
            continue
        if p == '+':
            sign = 1
            continue
        if p == '-':
            sign = -1
            continue
        val = parse_number(p, params)
        total += sign * val
        sign = 1
    return total


def find_block(lines, start_idx):
    # find first 'begin' then capture until matching 'end'
    i = start_idx
    while i < len(lines) and 'begin' not in lines[i]:
        i += 1
    if i >= len(lines):
        return []
    depth = 0
    out = []
    for j in range(i, len(lines)):
        l = lines[j]
        low = l.lower()
        begins = len(re.findall(r'\bbegin\b', low))
        ends = len(re.findall(r'\bend\b', low))
        depth += begins - ends
        out.append(l)
        if depth <= 0:
            break
    return out


def parse_inittable_block(block_lines, params):
    PROCESS_NUM = int(params.get('PROCESS_NUM', 186))
    CNU = int(params.get('CNU', 154))
    tbl = [CNU & 0xFF] * PROCESS_NUM
    assign_re = re.compile(r"inittable\s*\[\s*([^\]]+)\s*\]\s*=\s*([^;]+);")
    for l in block_lines:
        for m in assign_re.finditer(l):
            idx_expr = m.group(1).strip()
            val_expr = m.group(2).strip()
            idx = eval_expr(idx_expr, params)
            val = eval_expr(val_expr, params)
            if 0 <= idx < PROCESS_NUM:
                tbl[idx] = val & 0xFF
    return tbl


def write_bin(path, tbl):
    with open(path, 'wb') as f:
        f.write(bytes(tbl))


def main():
    ap = argparse.ArgumentParser(description='Generate CABAC table binaries from RTL')
    ap.add_argument('--src', default='cabac', help='source folder containing cabac_ucontext.v')
    ap.add_argument('--rtl', default='cabac_ucontext.v', help='rtl filename inside src (default cabac_ucontext.v)')
    ap.add_argument('--out', default='tables', help='output folder for .bin and tables.json')
    args = ap.parse_args()

    src = args.src
    rtl_path = os.path.join(src, args.rtl) if not os.path.isabs(args.rtl) else args.rtl
    if not os.path.exists(rtl_path):
        # try common alternative
        alt = os.path.join('cabac', args.rtl)
        if os.path.exists(alt):
            rtl_path = alt
        else:
            print('ERROR: could not find', rtl_path, file=sys.stderr)
            sys.exit(2)

    lines = read_lines(rtl_path)
    params = parse_parameters(lines)

    # mapping name -> (search string, base address)
    mapping = [
        ('cabac_ctx_islice', 'gp_slice_type == 2', 0x10000000),
        ('cabac_ctx_init2', 'initType == 2', 0x10000100),
        ('cabac_ctx_init1', 'initType = 1', 0x10000200),
    ]

    os.makedirs(args.out, exist_ok=True)
    manifest = []
    found_any = False

    for name, pattern, base in mapping:
        idx = 0
        for i, l in enumerate(lines):
            if pattern in l:
                idx = i
                break
        if idx == 0:
            continue
        blk = find_block(lines, idx)
        if not blk:
            continue
        tbl = parse_inittable_block(blk, params)
        outfn = f"{name}.bin"
        outpath = os.path.join(args.out, outfn)
        write_bin(outpath, tbl)
        manifest.append({
            'name': name,
            'file': outfn,
            'base': hex(base),
            'size': len(tbl)
        })
        print(f'Wrote {outpath} ({len(tbl)} bytes) base={hex(base)}')
        found_any = True

    if manifest:
        with open(os.path.join(args.out, 'tables.json'), 'w') as jf:
            json.dump(manifest, jf, indent=2)
        print('Wrote manifest:', os.path.join(args.out, 'tables.json'))

    if not found_any:
        print('No inittable blocks found; nothing written', file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
