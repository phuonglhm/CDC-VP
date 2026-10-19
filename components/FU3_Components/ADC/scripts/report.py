"""Validate requirement/test references and summarize evidence, not HDS sign-off."""
import argparse
import json
import pathlib
import xml.etree.ElementTree as ET

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("junit", nargs="+", type=pathlib.Path)
parser.add_argument("--output", required=True, type=pathlib.Path)
args = parser.parse_args()
requirements = json.loads((pathlib.Path(__file__).resolve().parents[1] /
                           "tests/hds-requirements.json").read_text())
profiles = []
valid = True
for path in args.junit:
    cases = list(ET.parse(path).getroot().iter("testcase"))
    passed = {c.attrib["name"] for c in cases if c.find("failure") is None and
              c.find("error") is None and c.find("skipped") is None and
              c.attrib.get("status", "run") not in ("notrun", "disabled")}
    names = {c.attrib["name"] for c in cases}
    missing = sorted({t for r in requirements["requirements"] for t in r["tests"]} - names)
    ok = bool(cases) and len(passed) == len(cases) and not missing
    valid &= ok
    profiles.append({"junit": str(path), "total": len(cases), "passed": len(passed),
                     "failed_or_skipped": sorted(names - passed), "missing_tests": missing})
report = {"source": requirements["source"], "regression_pass": valid,
          "hds_signoff": False, "profiles": profiles,
          "requirements": requirements["requirements"],
          "limitations": "See docs/spec-decisions.md. Policy checks are not evidence resolving HDS ambiguities."}
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
print(f"ADC regression {'PASS' if valid else 'FAIL'}; report: {args.output}; HDS sign-off: not claimed")
raise SystemExit(0 if valid else 1)
