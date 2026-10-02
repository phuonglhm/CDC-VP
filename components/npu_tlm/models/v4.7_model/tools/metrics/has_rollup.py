#!/usr/bin/env python3
"""Roll up a run of tools/has/tb_has_npu_top into per-instruction, per-operation and network metrics.

Usage:
  python3 tools/metrics/has_rollup.py <run dir> [--cmp <run dir> ...] [--out <dir>] [--freq GHZ]

A run directory holds the testbench output:
  run.log            stdout of tb_has_npu_top ([DFC], [STEP], [HOST] lines and the final summary)
  metrics_tiles.csv  per-tile core counters, written when FE_METRICS_CSV=<file> is set (optional)
Run the testbench with --trace for exact per-instruction cycles ([DFC] lines); without it they are derived from the
[STEP] lines and include the register writes between instructions (the totals are the same).
Example:
  FE_METRICS_CSV=run_c/metrics_tiles.csv tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3 --trace > run_c/run.log
  python3 tools/metrics/has_rollup.py run_c --out run_c/rollup
The reference runs of the data package ($FE_WORK/reference_runs/*) are run directories.

Outputs (in --out, default <run dir>/rollup):
  instructions.csv  one row per finished instruction: operation, name, tiles, cycles, core passes and counters,
                    real and executed MACs, PE utilization, SRAM bytes, result; plus cycles of each --cmp run
  operations.csv    cycles and share of the total per operation (GEMM_FUSED, ELEM_ADD, FUSED_ATTN, ...)
  network.json      network summary (the numbers of report.md)
  report.md         readable summary

Definitions:
  real MACs     MACs of the network layer, from the instruction description in the [STEP] line: GEMM_FUSED
                cout x H x W x cin x kh x kw (kh = kw = 1 for im2col or token inputs), FUSED_ATTN 2 x NQ x L x D
                (Q.K^T and A.V). Padded channels and positions are not counted.
  executed MACs MACs issued by the core including padding (sum of macs_theory in metrics_tiles.csv)
  core busy     sum of busy over the GEMM tiles of metrics_tiles.csv; the attention products that FUSED_ATTN runs on
                the core are reported separately ("RCE on core" line of the log) and are not part of it
  PE utilization  real MACs / (core busy x 1024), GEMM only; a second figure adds the attention products to
                both terms when the program has FUSED_ATTN instructions
  GOPS          2 x real MACs x f / cycles; FPS = f / total cycles
Works on a partial run: only finished instructions are counted and the status is reported as RUNNING.
"""
import argparse
import csv
import json
import os
import re
import sys
from collections import OrderedDict, defaultdict

PE = 1024  # 32 x 32 array

RE_DFC = re.compile(r'\[DFC\] instr (\d+) op (\S+) (\S+) tiles (\d+) cycles (\d+)')
RE_STEP = re.compile(r'\[STEP\] (\d+)/(\d+) kind=\d+ tensor=\d+ \[(\d+),(\d+),(\d+)\] elements=(\d+) bad=(\d+) (\w+)'
                     r'.*?#\s*\[\d+\]\s*(?:step \d+ )?(\S+)\s*(.*)$')
RE_HOST = re.compile(r'\[HOST\] H (\S+) .*bad=(\d+) (\w+)')
RE_RESULT = re.compile(r'RESULT: (\w+)(?: \(steps \d+, tensors (\d+), elements (\d+), bad tensors (\d+), bad elements (\d+))?')
RE_TOTAL = re.compile(r'sim cycles (\d+) \(DMA wait (\d+)')
RE_ATTN_CORE = re.compile(r'RCE (?:products )?on (?:the )?core.*?passes (\d+), core cycles (\d+)')
RE_VU_EST = re.compile(r'FUSED_ATTN (\d+), LAYERNORM (\d+), rows (\d+), cycles (\d+)')

CSV_SUMS = ('busy', 'exec', 'pe_cycles', 'mac_nz', 'macs_theory', 'a_rd_bytes', 'b_rd_bytes', 'c_rd_bytes', 'c_wr_bytes')


def real_macs(op, shape, detail):
    """Real MACs of one instruction from its [STEP] description, or None when it cannot be derived."""
    c, h, w = shape
    if op == 'GEMM_FUSED':
        m = re.search(r'\bcin (\d+)', detail)
        if not m:
            return None
        k = re.search(r'\b(\d+)x(\d+) s\d+', detail)
        kk = 1 if (not k or 'im2col' in detail) else int(k.group(1)) * int(k.group(2))
        return c * h * w * int(m.group(1)) * kk
    if op == 'FUSED_ATTN':
        m = re.search(r'\bNQ (\d+) L (\d+) D (\d+)', detail)
        return 2 * int(m.group(1)) * int(m.group(2)) * int(m.group(3)) if m else None
    return 0


def load_run(d):
    log = os.path.join(d, 'run.log')
    if not os.path.exists(log):
        sys.exit('no run.log in %s' % d)
    dfc, steps, hosts, net = {}, OrderedDict(), [], {}
    for line in open(log, errors='replace'):
        m = RE_DFC.search(line)
        if m:
            dfc[int(m.group(1))] = {'op': m.group(3), 'tiles': int(m.group(4)), 'cycles': int(m.group(5))}
            continue
        m = RE_STEP.search(line)
        if m:
            i = int(m.group(1))
            net['instructions_in_program'] = int(m.group(2))
            detail = m.group(10).strip()
            steps[i] = {'shape': (int(m.group(3)), int(m.group(4)), int(m.group(5))), 'elements': int(m.group(6)),
                        'bad': int(m.group(7)), 'result': m.group(8), 'op_desc': m.group(9), 'detail': detail}
            t = re.search(r' tiles=(\d+) .*?sim_cycles=(\d+)', line)
            if t:
                steps[i]['cum_tiles'], steps[i]['cum_cycles'] = int(t.group(1)), int(t.group(2))
            continue
        m = RE_HOST.search(line)
        if m:
            hosts.append({'op': m.group(1), 'bad': int(m.group(2)), 'result': m.group(3)})
            continue
        m = RE_ATTN_CORE.search(line)
        if m:
            net['attn_core_passes'], net['attn_core_cycles'] = int(m.group(1)), int(m.group(2))
        m = RE_VU_EST.search(line)
        if m:
            net['vector_unit_rows'], net['vector_unit_cycles_est'] = int(m.group(3)), int(m.group(4))
        m = RE_RESULT.search(line)
        if m:
            net['result'] = m.group(1)
            if m.group(2):
                net['tensors'], net['elements'] = int(m.group(2)), int(m.group(3))
                net['bad_tensors'], net['bad_elements'] = int(m.group(4)), int(m.group(5))
        m = RE_TOTAL.search(line)
        if m:
            net['total_cycles'], net['dma_wait'] = int(m.group(1)), int(m.group(2))

    # Without --trace the log has no [DFC] line: per-instruction cycles and tiles are then the differences of the
    # cumulative [STEP] counters (they include the register writes between two instructions).
    net['cycle_source'] = 'DFC' if dfc else 'STEP'
    if not dfc:
        prev_c, prev_t = 0, 0
        for i, s in steps.items():
            if 'cum_cycles' not in s:
                continue
            op = s['op_desc']
            if op == 'ELEM_WISE':
                w = s['detail'].split()
                op = 'ELEM_' + (w[0] if w else 'WISE')
            dfc[i] = {'op': op, 'tiles': s['cum_tiles'] - prev_t, 'cycles': s['cum_cycles'] - prev_c}
            prev_c, prev_t = s['cum_cycles'], s['cum_tiles']

    tiles = defaultdict(lambda: dict.fromkeys(CSV_SUMS + ('passes',), 0))
    path = os.path.join(d, 'metrics_tiles.csv')
    net['has_tile_csv'] = os.path.exists(path)
    if net['has_tile_csv']:
        seen = set()
        with open(path, errors='replace') as f:
            for r in csv.DictReader(x for x in f if not x.startswith('#')):
                try:
                    key = (int(r['step']), r['tile'], r['cin'], r['n_ctx'], r['busy'], r['exec'])
                    if key in seen:          # duplicate line of a resumed run
                        continue
                    seen.add(key)
                    t = tiles[int(r['step']) + 1]   # step = instruction index - 1
                    for k in CSV_SUMS:
                        t[k] += int(r[k])
                    t['passes'] += 1
                except (KeyError, ValueError, TypeError):
                    continue                          # truncated last line of a running job

    rows = []
    for i, s in steps.items():
        d_ = dfc.get(i, {})
        op = d_.get('op', s['op_desc'])
        base_op = 'GEMM_FUSED' if s['op_desc'] == 'GEMM_FUSED' else ('FUSED_ATTN' if s['op_desc'] == 'FUSED_ATTN' else op)
        name = s['detail'].split(',')[0]
        t = tiles.get(i)
        rm = real_macs(base_op, s['shape'], s['detail'])
        row = OrderedDict(instr=i, op=op, name=name, tiles=d_.get('tiles', ''), cycles=d_.get('cycles', ''),
                          result=s['result'], bad=s['bad'], elements=s['elements'], real_macs='' if rm is None else rm)
        if t:
            row.update(core_passes=t['passes'], busy=t['busy'], exec=t['exec'], executed_macs=t['macs_theory'],
                       pe_util_pct=round(100.0 * rm / (t['busy'] * PE), 2) if (rm and t['busy']) else '',
                       a_rd_bytes=t['a_rd_bytes'], b_rd_bytes=t['b_rd_bytes'], c_rd_bytes=t['c_rd_bytes'],
                       c_wr_bytes=t['c_wr_bytes'])
        else:
            row.update(core_passes='', busy='', exec='', executed_macs='', pe_util_pct='', a_rd_bytes='',
                       b_rd_bytes='', c_rd_bytes='', c_wr_bytes='')
        rows.append(row)
    return rows, hosts, net


def summarize(rows, hosts, net, freq):
    done = [r for r in rows if r['cycles'] != '']
    s = OrderedDict()
    s['status'] = net.get('result', 'RUNNING (partial run)')
    s['instructions_finished'] = len(done)
    s['instructions_in_program'] = net.get('instructions_in_program', '')
    s['instructions_failed'] = sum(1 for r in rows if r['result'] != 'PASS')
    s['host_steps'] = len(hosts)
    s['host_steps_failed'] = sum(1 for h in hosts if h['result'] != 'PASS')
    # whole-run counts of the testbench (instruction outputs and host-step outputs); partial run: instructions only
    s['elements_compared'] = net.get('elements', sum(r['elements'] for r in rows))
    s['elements_mismatching'] = net.get('bad_elements', sum(r['bad'] for r in rows) + sum(h['bad'] for h in hosts))
    s['sum_instruction_cycles'] = sum(r['cycles'] for r in done)
    s['instruction_cycle_source'] = ('[DFC] lines (run with --trace)' if net.get('cycle_source') == 'DFC'
                                     else '[STEP] differences (no --trace; includes register writes)')
    s['total_cycles'] = net.get('total_cycles', '')
    s['dma_wait'] = net.get('dma_wait', '')
    gemm = [r for r in done if r['busy'] != '']
    busy = sum(r['busy'] for r in gemm)
    # utilization only over instructions that have core counters (a running job may lag in metrics_tiles.csv)
    real_util = sum(r['real_macs'] for r in gemm if r['op'] == 'GEMM_FUSED' and r['real_macs'] != '')
    real_gemm = sum(r['real_macs'] for r in done if r['op'] == 'GEMM_FUSED' and r['real_macs'] != '')
    real_attn = sum(r['real_macs'] for r in done if r['op'] == 'FUSED_ATTN' and r['real_macs'] != '')
    unknown = sum(1 for r in done if r['real_macs'] == '')
    s['real_macs'] = real_gemm + real_attn
    s['real_macs_gemm'] = real_gemm
    s['real_macs_attention'] = real_attn
    s['instructions_without_real_macs'] = unknown
    s['tiles'] = sum(r['tiles'] for r in done if r['tiles'] != '')
    if net.get('has_tile_csv'):
        s['core_passes'] = sum(r['core_passes'] for r in gemm)
        s['core_busy_gemm'] = busy
        s['executed_macs_gemm'] = sum(r['executed_macs'] for r in gemm)
        s['pe_util_gemm_pct'] = round(100.0 * real_util / (busy * PE), 2) if busy else ''
        s['gops_on_core_busy_gemm'] = round(2 * real_util * freq / busy, 0) if busy else ''
    if 'attn_core_cycles' in net:
        s['attention_core_passes'] = net['attn_core_passes']
        s['attention_core_cycles'] = net['attn_core_cycles']
        if net.get('has_tile_csv') and busy:
            b2 = busy + net['attn_core_cycles']
            s['core_busy_with_attention'] = b2
            s['pe_util_with_attention_pct'] = round(100.0 * (real_util + real_attn) / (b2 * PE), 2)
    if 'vector_unit_cycles_est' in net:
        s['vector_unit_cycles_estimated'] = net['vector_unit_cycles_est']
    tot = net.get('total_cycles')
    if tot:
        s['frequency_ghz'] = freq
        s['gops_on_total'] = round(2 * s['real_macs'] * freq / tot, 0)
        s['fps'] = round(freq * 1e9 / tot, 2)
    return s


def write_outputs(out, rows, s, cmps, label):
    os.makedirs(out, exist_ok=True)
    hdr = list(rows[0].keys()) + ['cycles_' + lab for lab, _ in cmps] if rows else []
    cmp_map = [(lab, {r['instr']: r for r in cr}) for lab, cr in cmps]
    with open(os.path.join(out, 'instructions.csv'), 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(hdr)
        for r in rows:
            extra = []
            for _, m in cmp_map:
                o = m.get(r['instr'])
                extra.append(o['cycles'] if o and o['name'] == r['name'] else '')
            w.writerow(list(r.values()) + extra)

    ops = OrderedDict()
    for r in rows:
        if r['cycles'] == '':
            continue
        o = ops.setdefault(r['op'], {'count': 0, 'cycles': 0, 'tiles': 0})
        o['count'] += 1
        o['cycles'] += r['cycles']
        o['tiles'] += r['tiles'] or 0
    tot_i = s['sum_instruction_cycles'] or 1
    with open(os.path.join(out, 'operations.csv'), 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['op', 'instructions', 'tiles', 'cycles', 'share_pct'])
        for k, o in ops.items():
            w.writerow([k, o['count'], o['tiles'], o['cycles'], round(100.0 * o['cycles'] / tot_i, 2)])

    comparisons = []
    for lab, cr in cmps:
        m = {r['instr']: r for r in cr}
        common = [r for r in rows if r['cycles'] != '' and r['instr'] in m and m[r['instr']]['cycles'] != ''
                  and m[r['instr']]['name'] == r['name']]
        a = sum(r['cycles'] for r in common)
        b = sum(m[r['instr']]['cycles'] for r in common)
        comparisons.append(OrderedDict(run=lab, common_instructions=len(common), cycles_this=a, cycles_other=b,
                                       change_pct=round(100.0 * (a - b) / b, 2) if b else ''))
    doc = OrderedDict(run=label, summary=s, operations=ops, comparisons=comparisons)
    with open(os.path.join(out, 'network.json'), 'w') as f:
        json.dump(doc, f, indent=1)

    def n(v):
        return '{:,}'.format(v) if isinstance(v, int) else str(v)
    L = ['# Run roll-up: %s' % label, '',
         '| Quantity | Value |', '|---|---:|',
         '| Status | %s |' % s['status'],
         '| Instructions finished / in program | %s / %s |' % (s['instructions_finished'], s['instructions_in_program']),
         '| Failed instructions / host steps | %s / %s (host steps %s) |' % (s['instructions_failed'], s['host_steps_failed'], s['host_steps']),
         '| Elements compared / mismatching | %s / %s |' % (n(s['elements_compared']), n(s['elements_mismatching'])),
         '| Total cycles | %s |' % n(s['total_cycles']),
         '| Sum of instruction cycles | %s (%s) |' % (n(s['sum_instruction_cycles']), s['instruction_cycle_source']),
         '| DMA wait (not hidden) | %s |' % n(s['dma_wait']),
         '| Tiles | %s |' % n(s['tiles']),
         '| Real MACs (GEMM + attention) | %s (%s + %s) |' % (n(s['real_macs']), n(s['real_macs_gemm']), n(s['real_macs_attention']))]
    if 'core_busy_gemm' in s:
        L += ['| Core passes (GEMM) | %s |' % n(s['core_passes']),
              '| Core busy cycles (GEMM) | %s |' % n(s['core_busy_gemm']),
              '| Executed MACs incl. padding (GEMM) | %s |' % n(s['executed_macs_gemm']),
              '| PE utilization, GEMM (real MACs / (busy x 1024)) | %s %% |' % s['pe_util_gemm_pct'],
              '| Throughput on GEMM core busy | %s GOPS |' % n(int(s['gops_on_core_busy_gemm'])) if s['gops_on_core_busy_gemm'] != '' else '| Throughput on GEMM core busy | |']
    else:
        L += ['| Core counters | no metrics_tiles.csv in the run directory |']
    if 'attention_core_cycles' in s:
        L += ['| Attention products on the core (passes / cycles) | %s / %s |' % (n(s['attention_core_passes']), n(s['attention_core_cycles']))]
        if 'pe_util_with_attention_pct' in s:
            L += ['| Core busy incl. attention products | %s |' % n(s['core_busy_with_attention']),
                  '| PE utilization incl. attention products | %s %% |' % s['pe_util_with_attention_pct']]
    if 'vector_unit_cycles_estimated' in s:
        L += ['| Softmax / LayerNorm in the vector unit (estimated) | %s |' % n(s['vector_unit_cycles_estimated'])]
    if 'fps' in s:
        L += ['| Throughput on total cycles | %s GOPS |' % n(int(s['gops_on_total'])),
              '| Frame rate at %s GHz | %s FPS |' % (s['frequency_ghz'], s['fps'])]
    if s['instructions_without_real_macs']:
        L += ['', 'Real MACs could not be derived for %d instructions (not counted).' % s['instructions_without_real_macs']]
    L += ['', '| Operation | Instructions | Tiles | Cycles | Share |', '|---|---:|---:|---:|---:|']
    for k, o in ops.items():
        L.append('| %s | %d | %s | %s | %.2f %% |' % (k, o['count'], n(o['tiles']), n(o['cycles']), 100.0 * o['cycles'] / tot_i))
    for c in comparisons:
        L += ['', 'Against %s on %d common instructions: %s vs %s cycles (%+.2f %%).'
              % (c['run'], c['common_instructions'], n(c['cycles_this']), n(c['cycles_other']), c['change_pct'] or 0)]
    L += ['', 'Per instruction: instructions.csv. Definitions: header of tools/metrics/has_rollup.py.']
    text = '\n'.join(L) + '\n'
    with open(os.path.join(out, 'report.md'), 'w', encoding='utf-8') as f:
        f.write(text)
    return text


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('run', help='run directory (run.log, optional metrics_tiles.csv)')
    ap.add_argument('--cmp', action='append', default=[], help='another run directory to compare with (repeatable)')
    ap.add_argument('--out', help='output directory (default <run>/rollup)')
    ap.add_argument('--freq', type=float, default=0.8, help='clock in GHz for GOPS and FPS (default 0.8)')
    a = ap.parse_args()
    rows, hosts, net = load_run(a.run)
    if not rows:
        sys.exit('no finished instruction ([STEP] line) in %s/run.log' % a.run)
    s = summarize(rows, hosts, net, a.freq)
    cmps = [(os.path.basename(os.path.normpath(c)), load_run(c)[0]) for c in a.cmp]
    out = a.out or os.path.join(a.run, 'rollup')
    print(write_outputs(out, rows, s, cmps, os.path.basename(os.path.normpath(a.run))), end='')
    print('written: %s/{instructions.csv,operations.csv,network.json,report.md}' % out)


if __name__ == '__main__':
    main()
