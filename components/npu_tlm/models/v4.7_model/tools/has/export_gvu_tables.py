"""Export the GVU golden tables ($FE_WORK/has/vectors/gvu/gvu_tables.json) as one value per line for the C++ testbenches.
Usage (repository root): python3 tools/has/export_gvu_tables.py [json] [out dir]   (default out: tools/has/vec_gvu)
"""
import json
import os
import sys

src = sys.argv[1] if len(sys.argv) > 1 else "fe_work/has/vectors/gvu/gvu_tables.json"
out = sys.argv[2] if len(sys.argv) > 2 else "tools/has/vec_gvu"
os.makedirs(out, exist_ok=True)
tables = json.load(open(src))
for name, values in tables.items():
    with open(os.path.join(out, name + ".txt"), "w") as f:
        f.write("\n".join(str(int(v)) for v in values) + "\n")
print("exported %d tables to %s" % (len(tables), out))
