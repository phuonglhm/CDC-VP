#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""fx1_unit_npu_abi_doc_sync: the SDK's register ABI page against the component's (review N5).

  test_npu_abi_doc_sync.py <components/FX1_Components/npu/README.md> <package/docs/NPU_REGISTER_ABI.md>

The SDK page (vp/doc/VP_FX1_V3.0/npu/register_abi.md) is adapted for the SDK:
paths and CDC-VP history differ, the ABI must not. Every table row and every
section heading of the component page (up to its Tests section) must be in the
SDK page verbatim, and the SDK page has no row or heading of its own beyond
those; it must not point at CDC-VP sources or plan history.
"""
import re
import sys

comp = open(sys.argv[1]).read()
comp = comp[:comp.index("\n## Tests")]
sdk = open(sys.argv[2]).read()
failures = 0


def check(ok, what):
    global failures
    print("  [%s] %s" % (" ok " if ok else "FAIL", what))
    failures += 0 if ok else 1


rows = lambda t: [l for l in t.splitlines() if l.startswith("| ")]
heads = lambda t: [l for l in t.splitlines() if re.match(r"#{2,4} ", l)]
check(rows(comp) == rows(sdk), "table rows: %d, identical and in order" % len(rows(comp)))
missing = [r for r in rows(comp) if r not in rows(sdk)]
for r in missing[:5]:
    print("        missing: %s" % r[:110])
check(heads(comp) == heads(sdk), "section headings: %d, identical and in order" % len(heads(comp)))
bad = [w for w in ("fw/fx1_soc", "components/", "platforms/", "review N", "plan N", "clone", "npu_tlm`", "tests/")
       if w in sdk]
check(not bad, "no CDC-VP source path or plan history in the SDK page (%s)" % (", ".join(bad) or "none"))
print("RESULT %s (%d failure%s)" % ("FAIL" if failures else "PASS", failures, "" if failures == 1 else "s"))
sys.exit(1 if failures else 0)
