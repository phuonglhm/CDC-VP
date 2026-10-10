import gzip
import json
from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve()
records = {}
for path in sorted(root.glob('*.gcov.json.gz')):
    with gzip.open(path, 'rt') as stream:
        data = json.load(stream)
    for file in data['files']:
        name = file['file']
        if '/Inter-Prediction/src/' not in name:
            continue
        records[name] = file
if len(records) != 7:
    raise SystemExit(f'Expected 7 production source files, found {len(records)}')
report = ['GCC 15.2.0 gcov JSON coverage; source .cpp files only',
          'Line covered = execution count > 0; branch outcomes taken = branch count > 0',
          'Standard-library/SystemC/shared/DMA headers and test code excluded', '']
totals = [0, 0, 0, 0]
for name, record in sorted(records.items()):
    lines = record['lines']
    branches = [branch for line in lines for branch in line.get('branches', [])]
    counts = [sum(line['count'] > 0 for line in lines), len(lines),
              sum(branch['count'] > 0 for branch in branches), len(branches)]
    totals = [a + b for a, b in zip(totals, counts)]
    relative = name.split('/Inter-Prediction/')[1]
    report.append(f'{relative}: lines {counts[0]}/{counts[1]} ({counts[0]/counts[1]*100:.2f}%); '
                  f'branch outcomes {counts[2]}/{counts[3]} ({counts[2]/counts[3]*100:.2f}%)')
    report.append('  Uncovered lines: ' + ','.join(str(line['line_number']) for line in lines if not line['count']))
report += ['', f'TOTAL: lines {totals[0]}/{totals[1]} ({totals[0]/totals[1]*100:.2f}%); '
           f'branch outcomes {totals[2]}/{totals[3]} ({totals[2]/totals[3]*100:.2f}%)']
result = '\n'.join(report) + '\n'
(root / 'coverage_summary.txt').write_text(result, encoding='utf-8')
print(result)
