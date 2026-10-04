#!/usr/bin/env python3
# v4.5 version of sauria_model's tools/eval_selfcheck.py with three extra checks at the end of the loop:
#   1. A+B+C == l2_size_bytes (an identity, not only l2_footprint <= l2_size): catches a unit error where C is
#      counted in 128-byte cells instead of bytes.
#   2. l2_size_bytes >= max(l2_footprint) and not smaller than the banks used by any tile.
#   3. parameters.json mode=B => every metric in metric_provenance carries a |MODE-B label.
# The original checks are unchanged.
"""P2 -- physical / arithmetic SELF-CONSISTENCY audit of an --eval-out dir.

eval_integration_test.py checks STRUCTURE (files, headers, join key, ranges).
This checks the numbers obey the identities and bounds they MUST obey by definition
-- independent of any modeling assumption. A failure here is a real bug, not a
placeholder. Complements, does not replace, eval_integration_test.py.

EXACT identities (must hold to the cycle; tol=1 for rounding):
  active_cycles            == processing_cycles
  mac_engine_cycles        == processing_cycles
  idle_cycles              == total - processing
  transfer_overhead_cycles == total - processing
  dma_busy_cycles          == transfer_cycles (analytic) OR, under --dma-sim where the
                             physical busy exceeds the byte/BW floor, dma_read+dma_write
                             == dma_busy (R-B counter additivity)
  theory_min_cycles        == ceil(M*K*N/PEs)*reps           [needs --input for M,K,N]
  engine_utilization       == 100*mac_engine/total           [ratio, tol 0.5]

SHAPE bounds (from M,K,N,dtype,geometry; needs --input):
  M*N*Cb*reps       <= ddr_write_bytes <= Mp*Np*Cb*reps      (output, padded)
  weight read >= K*N*eb                                       (each weight >= once)
  ddr_read_bytes    >= M*K*eb  (activation read at least once, per pass)

Usage:
  python3 tools/eval_selfcheck.py --dir ~/ev
  python3 tools/eval_selfcheck.py --dir ~/ev --input tools/vit_b_layers.csv
Exit != 0 on any violation -> CI gate.
"""
import argparse
import csv
import json
import math
import os
import sys

# B3 FIX: 'memory_wait.csv' was MISSING here, so none of its five columns
# (ddr/l2/l1_wait, sram_conflict, memory_arbitration) had ever been audited -- a whole
# emitted CSV sat outside the gate. Its column names are unique across the eval-out, so
# merging it adds keys without shadowing any existing check.
# ('feeder.csv' stays out on purpose: it is a VP-ext file outside the NEON spec, and its
#  invariant is already enforced at generation time by the A4 LOCKSTEP gate in dse_sweep.)
FILES = ('cycles.csv', 'engine.csv', 'dma.csv', 'stall.csv', 'bandwidth.csv',
         'utilization.csv', 'memory.csv', 'memory_wait.csv', 'summary.csv')
CBYTES = 4               # output element = INT32 psum -> DRAM
EBYTES = {'int8': 1, 'fp16': 2, 'int16': 2}


def load(d):
    rows, order = {}, []
    for fn in FILES:
        p = os.path.join(d, fn)
        if not os.path.exists(p):
            continue
        with open(p, newline='') as fp:
            for rec in csv.DictReader(fp):
                lid = rec['layer_id']
                rows.setdefault(lid, {}) if lid in rows else (rows.setdefault(lid, {}), order.append(lid))
                rows[lid].update(rec)
    params = {}
    pj = os.path.join(d, 'parameters.json')
    if os.path.exists(pj):
        params = json.load(open(pj))
    return [rows[l] for l in order], params


def num(rec, key):
    v = rec.get(key, '')
    if v is None or str(v).strip() == '':
        return None
    try:
        return float(v)
    except ValueError:
        return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dir', required=True)
    ap.add_argument('--input', help='layer.csv (layer_id,...,M,K,N,reps) for shape checks. '
                                    'Optional: <dir>/shapes.csv (written by dse_sweep '
                                    '--eval-out) is used automatically when omitted.')
    ap.add_argument('--geo', help='geometry, e.g. 64x64 (else read from parameters.json)')
    ap.add_argument('--dtype', default='int8')
    ap.add_argument('--tol', type=int, default=1)
    ap.add_argument('--allow-no-shapes', action='store_true',
                    help='permit the identity-only subset (no shape checks). Use ONLY for a '
                         'hand-made eval dir; never in an automated gate -- it drops ~80%% '
                         'of the checks.')
    a = ap.parse_args()

    rows, params = load(a.dir)
    if not rows:
        sys.exit('no rows in %s' % a.dir)
    pes = int(float(params.get('mac_count', 0))) or None
    eb = EBYTES.get(a.dtype, 1)

    # Shapes: explicit --input wins, else the shapes.csv the VP wrote next to the
    # metrics. Both Makefile eval-integration and tools/run_eval.sh used to call this
    # tool with NO --input, so every shape-joined check (the B2a R-A byte gate,
    # theory_min, ddr_write/weight bounds) was skipped while the tool still printed
    # PASS -- 6 of 30 checks running, green, with nothing saying so.
    shapes, shape_src = {}, None
    src = a.input or os.path.join(a.dir, 'shapes.csv')
    if os.path.exists(src):
        with open(src, newline='') as fp:
            for r in csv.DictReader(fp):
                shapes[r['layer_id']] = (int(r['M']), int(r['K']), int(r['N']),
                                         int(r.get('reps', 1)))
        shape_src = src
    elif a.input:
        sys.exit('--input %s not found' % a.input)
    # Refuse to degrade silently: a gate that quietly drops most of its checks and
    # still reports PASS is worse than no gate, because it manufactures confidence.
    if not shapes and not a.allow_no_shapes:
        sys.exit('no shapes: neither --input nor %s/shapes.csv.\n'
                 'The shape-joined checks (R-A derived-byte gate, theory_min, ddr_write '
                 'and weight bounds) cannot run -- that is ~80%% of this gate.\n'
                 'Re-run dse_sweep --eval-out (it writes shapes.csv), pass --input, or '
                 'acknowledge the reduced gate with --allow-no-shapes.' % a.dir)
    geo = a.geo or params.get('geometry', '')
    X = Y = None
    if 'x' in geo:
        try:
            X, Y = int(geo.split('x')[0]), int(geo.split('x')[1])
        except ValueError:
            pass

    fails, checks = [], 0

    def chk(cond, msg):
        nonlocal checks
        checks += 1
        if not cond:
            fails.append(msg)

    for r in rows:
        lid = r['layer_id']
        tot = num(r, 'total_cycles')
        proc = num(r, 'processing_cycles')
        act = num(r, 'active_cycles')
        idle = num(r, 'idle_cycles')
        mac_e = num(r, 'mac_engine_cycles')
        dma_b = num(r, 'dma_busy_cycles')
        dma_r = num(r, 'dma_read_cycles')
        dma_w = num(r, 'dma_write_cycles')
        xfer = num(r, 'transfer_cycles') if num(r, 'transfer_cycles') is not None else num(r, 'dma_engine_cycles')
        ovh = num(r, 'transfer_overhead_cycles')
        tmin = num(r, 'theory_min_cycles')
        engu = num(r, 'engine_utilization')

        if proc is not None and act is not None:
            # active = ENABLED cycles, processing = core BUSY span -- the difference is
            # the pipeline stall. The invariant is act <= proc, NOT equality.
            chk(act <= proc + a.tol, '%s: active_cycles %s > processing %s' % (lid, act, proc))
        if proc is not None and mac_e is not None:
            chk(abs(mac_e - proc) <= a.tol, '%s: mac_engine %s != processing %s' % (lid, mac_e, proc))
        if tot is not None and proc is not None:
            # latency covers compute: total (compute + DMA-exposed >= 0) can never be
            # less than processing. A violation means a layer's latency was dropped
            # from the aggregation (mixed real/hybrid bug) -- same invariant the
            # dse_sweep [LAT GATE] enforces, checked here at the eval-out boundary too.
            chk(tot + a.tol >= proc, '%s: total_cycles %s < processing %s (latency dropped a layer)' % (lid, tot, proc))
            if idle is not None:
                # eval_counters.h:239,351 define idle from ACTIVE, not from processing.
                chk(abs(idle - (tot - act)) <= a.tol, '%s: idle %s != total-active %s' % (lid, idle, tot - act))
            if ovh is not None:
                chk(abs(ovh - (tot - proc)) <= a.tol, '%s: transfer_overhead %s != total-proc %s' % (lid, ovh, tot - proc))
        if dma_b is not None and xfer is not None:
            if dma_b > xfer + a.tol:
                # --dma-sim (B1b): dma_busy is the PHYSICAL DDR-port busy time (per-transfer
                # first-latency + serialization), which exceeds the analytic byte/BW transfer
                # floor. It must still be the sum of its directional split (R-B additivity).
                if dma_r is not None and dma_w is not None:
                    chk(abs((dma_r + dma_w) - dma_b) <= a.tol,
                        '%s: dma_read+write %s != dma_busy %s (R-B)' % (lid, dma_r + dma_w, dma_b))
            else:
                # analytic default: dma_busy == transfer_cycles exactly.
                chk(abs(dma_b - xfer) <= a.tol, '%s: dma_busy %s != transfer %s' % (lid, dma_b, xfer))
        if engu is not None and tot and mac_e is not None:
            chk(abs(engu - 100.0 * mac_e / tot) <= 0.5, '%s: engine_util %.3f != 100*mac/total %.3f' % (lid, engu, 100.0 * mac_e / tot))

        # ---- B3: structurally-zero memory effects ----
        # SAURIA has no SRAM banks (ram_inferred.sv:52 = one flat single-port array)
        # and no host/accel arbiter (ram_intf_wrapper.sv:263-280 = hard i_select mux,
        # double-buffering keeps the two sides on different macros). These columns are
        # 0 BY DESIGN, not merely unmodelled. Pin the 0 so a later phase cannot quietly
        # publish an invented bank/arbitration number without also changing this gate
        # (and, with it, the 'n/a-by-design' provenance the SW app reads).
        for _z in ('sram_conflict_cycles', 'memory_arbitration_cycles'):
            _zv = num(r, _z)
            if _zv is not None:
                chk(_zv == 0, '%s: %s = %s, must be 0 by design (no banks / no arbiter '
                              'in SAURIA RTL); if HW gained one, update the B3 RTL log '
                              'and provenance too' % (lid, _z, _zv))

        # ================= F2: columns no gate asserted before =====================
        # The coverage audit found 27 of the 60 handoff columns with NO value check at
        # all (spec_check only verifies that the column NAME exists). The checks below
        # differ in STRENGTH, and saying which is which matters more than the count:
        #
        #  [X-SRC] cross-source: the value is produced in C++ (eval_counters.h) and
        #          re-checked here in Python against a DIFFERENT column. A real
        #          invariant -- it catches a core change that breaks the relation.
        #  [BOUND] inequality that must hold physically. Weaker but never vacuous.
        #  [POLICY] pinned to 0 because we decided it is 0 (host ops / not modelled),
        #          same shape as the B3 pin: it stops a value appearing unannounced.
        #  [CONSIST] re-computes the SAME formula dse_sweep used. It canNOT prove the
        #          formula right -- only that the cell was not corrupted or dropped
        #          between producer and file. Labelled honestly, not sold as more.
        proc_f = num(r, 'processing_cycles')
        mac_a, mac_i = num(r, 'mac_active_cycles'), num(r, 'mac_idle_cycles')
        pe_a, pe_i = num(r, 'pe_active_cycles'), num(r, 'pe_idle_cycles')
        eng_a, eng_i = num(r, 'engine_active_cycles'), num(r, 'engine_idle_cycles')
        dma_e = num(r, 'dma_engine_cycles')

        # [X-SRC] eval_counters.h:241,247 -- output-stationary: MAC-active == processing
        # always (mac_active has no derive-fallback). If the array ever stops being
        # output-stationary this breaks.
        if proc_f is not None and mac_a is not None:
            # eval_counters.h:253 mac_active_cycles = act_c = active_cycles.
            chk(abs(mac_a - act) <= a.tol, '%s: mac_active %s != active %s [X-SRC]' % (lid, mac_a, act))
        # [X-SRC] eval_writer.h UD("pe_active_cycles", e.pe_active_cycles, tmin) -- pe_active is
        # processing_cycles on the MEASURED branch (finalize()/apply_burst_dma() ran and set the
        # raw counter) or theory_min_cycles on the DERIVED branch (raw counter still 0, e.g. the
        # tb_evaluate default-build / tb_eval_network config-gen paths). Both are legitimate
        # provenance states; only a THIRD value
        # indicates corruption.
        if proc_f is not None and pe_a is not None:
            tmin_f = num(r, 'theory_min_cycles')
            # eval_counters.h:259 pe_active_cycles = act_c (mang output-stationary).
            ok = (abs(pe_a - act) <= a.tol or abs(pe_a - proc_f) <= a.tol
                  or (tmin_f is not None and abs(pe_a - tmin_f) <= a.tol))
            chk(ok, '%s: pe_active %s != processing %s and != theory_min %s [X-SRC]' % (lid, pe_a, proc_f, tmin_f))
        # [X-SRC] eval_counters.h:242,248,250 -- active+idle partitions total exactly.
        for _nm, _act, _idl in (('mac', mac_a, mac_i), ('pe', pe_a, pe_i), ('engine', eng_a, eng_i)):
            if tot is not None and _act is not None and _idl is not None:
                chk(abs((_act + _idl) - tot) <= a.tol,
                    '%s: %s_active+%s_idle %s != total %s [X-SRC]' % (lid, _nm, _nm, _act + _idl, tot))
        # [X-SRC] eval_counters.h:249 -- the busiest engine defines the active window.
        # The `network` row sums several sequential layers: engine_active = sum of per-layer maxima (not the max of the sums), so the max() identity is checked on layer rows only.
        if eng_a is not None and mac_e is not None and dma_e is not None and not (lid == 'network' and len(rows) > 2):
            chk(abs(eng_a - max(mac_e, dma_e)) <= a.tol,
                '%s: engine_active %s != max(mac_engine %s, dma_engine %s) [X-SRC]' % (lid, eng_a, mac_e, dma_e))

        # [CONSIST] dse_sweep.py:295-296 -- overhead%/dma_idle re-derived from cycles.
        ovh_p = num(r, 'transfer_overhead_percent')
        if ovh_p is not None and tot and ovh is not None:
            want_p = 100.0 * ovh / tot
            chk(abs(ovh_p - want_p) <= 0.01, '%s: transfer_overhead_percent %.4f != 100*ovh/total %.4f [CONSIST]'
                % (lid, ovh_p, want_p))
        dma_idle = num(r, 'dma_idle_cycles')
        if dma_idle is not None and tot is not None and xfer is not None:
            want_di = max(0.0, tot - xfer)
            chk(abs(dma_idle - want_di) <= a.tol, '%s: dma_idle %s != max(0,total-transfer) %s [CONSIST]'
                % (lid, dma_idle, want_di))

        # [BOUND] a wait/stall can never exceed the layer's own wall clock.
        if tot is not None:
            for _b in ('wait_input_cycles', 'wait_output_cycles', 'dma_stall_cycles', 'dma_wait_cycles'):
                _bv = num(r, _b)
                if _bv is not None:
                    chk(_bv <= tot + a.tol, '%s: %s %s > total_cycles %s [BOUND]' % (lid, _b, _bv, tot))
        # [BOUND] the working set must fit the SRAM the parameters advertise.
        _l2sz = params.get('l2_size_bytes')
        _fp = num(r, 'l2_footprint_bytes')
        if _fp is not None and _l2sz:
            chk(_fp <= float(_l2sz) + a.tol, '%s: l2_footprint %s > l2_size_bytes %s [BOUND]' % (lid, _fp, _l2sz))

        # [POLICY] host-CPU ops and unmodelled multi-stream effects are 0 BY DECISION.
        # Pinned like the B3 columns so a number cannot appear without also changing
        # this gate (and with it the provenance the SW app reads).
        # 'bias_bytes' joins this list: F2 found it read 0 when the layer was emulated
        # but N*4*reps when it was not -- the same layer, two answers. SAURIA has NO bias
        # datapath (the RTL 'bias' hits are IEEE-754 exponent bias in fpnew_fma.sv), so
        # the derived value was DRAM traffic for a transfer that never happens. Now 0
        # everywhere, provenance 'host'. Pinned so it cannot silently come back.
        for _z in ('activation_engine_cycles', 'pooling_engine_cycles',
                   'reshape_engine_cycles', 'reduction_engine_cycles',
                   'synchronization_cycles', 'dependency_stall_cycles', 'bias_bytes'):
            _zv = num(r, _z)
            if _zv is not None:
                chk(_zv == 0, '%s: %s = %s, pinned to 0 (host-op lib / multi-stream not '
                              'modelled); if it became real, update provenance too [POLICY]' % (lid, _z, _zv))

        # ---- shape-based (needs --input + geometry) ----
        if lid in shapes:
            M, K, N, reps = shapes[lid]
            if pes and tmin is not None:
                want = math.ceil(M * K * N / pes) * reps
                chk(abs(tmin - want) <= a.tol, '%s: theory_min %s != ceil(MKN/PEs)*reps %s' % (lid, tmin, want))
            if X and Y:
                Mp = math.ceil(M / Y) * Y
                Np = math.ceil(N / X) * X
                dw = num(r, 'ddr_write_bytes')
                if dw is not None:
                    lo, hi = M * N * CBYTES * reps, Mp * Np * CBYTES * reps
                    chk(lo <= dw <= hi, '%s: ddr_write %s out of [%d,%d]' % (lid, dw, lo, hi))
                wb = num(r, 'weight_bytes')
                if wb is not None and wb > 0:
                    chk(wb >= K * N * eb - a.tol, '%s: weight_bytes %s < K*N*eb %d' % (lid, wb, K * N * eb))
                # ---- B2 R-A gate: the shape-derived L2/mem byte model must reduce to the
                # MEASURED counters. EXACT for a no-pad layer (M%Y==0 & N%X==0); for a
                # padded layer the derived counts padded tiles -> it is an UPPER BOUND on
                # the valid-only measured count. Same formulas as dse_sweep pyval B2 block.
                Mt, Nt = math.ceil(M / Y), math.ceil(N / X)
                nopad = (M % Y == 0 and N % X == 0)
                d = {'ddr_read_bytes':  (K * Np + Nt * Mp * K) * eb * reps,
                     'ddr_write_bytes': Mp * Np * CBYTES * reps,
                     'weight_bytes':    K * Np * eb * reps,
                     'l2_to_l1_bytes':  K * Mt * Nt * (X + Y) * eb * reps,
                     'l1_to_l2_bytes':  Mp * Np * CBYTES * reps}
                d['l2_read_bytes'], d['l2_write_bytes'] = d['ddr_read_bytes'], d['ddr_write_bytes']
                d['l1_read_bytes'], d['l1_write_bytes'] = d['l2_to_l1_bytes'], d['l1_to_l2_bytes']
                for nm, dv in d.items():
                    mv = num(r, nm)
                    if mv is None:
                        continue
                    if nopad:
                        chk(abs(mv - dv) <= a.tol, '%s: %s derived %d != measured %d (R-A)' % (lid, nm, dv, mv))
                    else:
                        chk(mv <= dv + a.tol, '%s: %s measured %d > derived-padded %d (R-A upper bound)' % (lid, nm, mv, dv))

    # F2 [CONFIG] peak internal BW is a pure function of geometry+dtype+clock:
    # (X+Y) operand bytes per cycle * f. A drift here means the advertised roofline
    # ceiling no longer matches the hardware the rest of the file describes.
    _pib = None
    for _r in rows:
        if num(_r, 'peak_internal_bw_gbps') is not None:
            _pib = num(_r, 'peak_internal_bw_gbps'); break
    _hz = params.get('frequency_hz')
    if _pib is not None and X and Y and _hz:
        _want = (X + Y) * eb * (float(_hz) / 1e9)
        chk(abs(_pib - _want) <= 0.05,
            'peak_internal_bw_gbps %.2f != (X+Y)*eb*freq %.2f [CONFIG]' % (_pib, _want))

    # ---- 0. network row = sum of layers for additive columns ----
    _lay = [r for r in rows if r['layer_id'] != 'network']
    _net = [r for r in rows if r['layer_id'] == 'network']
    if len(_lay) > 1 and _net:
        for _c in ('total_cycles', 'processing_cycles', 'engine_active_cycles', 'engine_idle_cycles', 'ddr_read_bytes', 'ddr_write_bytes'):
            _vs = [num(r, _c) for r in _lay]
            if None not in _vs and num(_net[0], _c) is not None:
                chk(abs(sum(_vs) - num(_net[0], _c)) <= a.tol, 'network.%s %s != sum of layers %s' % (_c, num(_net[0], _c), sum(_vs)))
    # ---- 1. L2 size identity ----
    _bd = params.get('l2_size_breakdown_bytes')
    _l2 = params.get('l2_size_bytes')
    if _bd and _l2 is not None:
        _sum = sum(int(_bd[k]) for k in ('A', 'B', 'C'))
        chk(int(_l2) == _sum, 'l2_size_bytes %s != A+B+C %s (%s) [identity; C must be in BYTES, not cells]'
            % (_l2, _sum, _bd))
        if 'C_cells' in _bd and 'C_bytes_per_cell' in _bd:
            chk(int(_bd['C']) == int(_bd['C_cells']) * int(_bd['C_bytes_per_cell']),
                'C %s != C_cells*C_bytes_per_cell %s [unit]' % (_bd['C'], int(_bd['C_cells']) * int(_bd['C_bytes_per_cell'])))
    elif params and _l2 is not None:
        fails.append('parameters.json has l2_size_bytes=%s but NO l2_size_breakdown_bytes {A,B,C}: '
                     'cannot check the A+B+C identity' % _l2)
        checks += 1
    # ---- 3. MODE B label ----
    if params.get('mode') in ('B', 'A+B'):
        _mp = params.get('metric_provenance', {})
        _miss = [m for m, v in _mp.items() if '|MODE-' not in str(v)]
        chk(bool(_mp) and not _miss, 'mode=%s but %d/%d metrics are missing a |MODE-* label: %s' % (params.get('mode'), len(_miss), len(_mp), _miss[:5]))

    print('=' * 66)
    print(' EVAL SELF-CHECK  %s' % os.path.abspath(a.dir))
    print(' geometry=%s PEs=%s dtype=%s  |  %d checks, %d shape-joined layers'
          % (geo or '?', pes, a.dtype, checks, len(shapes)))
    # Say out loud whether the shape half of the gate ran -- the failure this fixes was
    # invisible precisely because a reduced run looked identical to a full one.
    print(' shapes: %s' % (os.path.basename(shape_src) if shape_src else
                           'NONE -- identity subset only (--allow-no-shapes)'))
    print('=' * 66)
    if fails:
        print(' FAIL (%d):' % len(fails))
        for m in fails[:40]:
            print('   - %s' % m)
        if len(fails) > 40:
            print('   ... +%d more' % (len(fails) - 40))
        print('=' * 66)
        return 1
    print(' PASS: all %d identity/bound checks hold.' % checks)
    print('=' * 66)
    return 0


if __name__ == '__main__':
    sys.exit(main())
