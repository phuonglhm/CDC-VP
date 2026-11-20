#!/usr/bin/env python3
"""Compares the LATENCY lines of the port (tb_rtl_ref_gvu_latency <revision>) with the LATENCY lines of an RTL oracle summary.
usage: python3 check_latency.py <port lines file> <oracle summary file>   (exit 0 = every port line is found in the summary)"""
import sys
port = [l.strip() for l in open(sys.argv[1]) if l.startswith("LATENCY")]
rtl = {l.strip() for l in open(sys.argv[2]) if l.startswith("LATENCY")}
bad = [l for l in port if l not in rtl]
for l in port: print(("OK   " if l in rtl else "FAIL ") + l)
print("CHECK_LATENCY", len(port) - len(bad), "of", len(port), "lines match")
sys.exit(1 if bad else 0)
