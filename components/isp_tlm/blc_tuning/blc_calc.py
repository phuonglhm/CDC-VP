import glob
import os
import struct

width = 2688
height = 1520
pattern = 'BGGR'

files = glob.glob('/Users/hvu/internship/fpt/CDC-VP/components/isp_tlm/blc_tuning/*.raw')
files.sort()

print("BLC Tuning Analysis (Width: 2688, Height: 1520, Pattern: BGGR)")
print("-" * 60)
print(f"{'Gain':<10} | {'B':<10} | {'Gb':<10} | {'Gr':<10} | {'R':<10}")
print("-" * 60)

for f in files:
    filename = os.path.basename(f)
    gain_str = filename.split('_')[1]
    
    with open(f, 'rb') as file:
        data = file.read()
        
    expected_size = width * height * 2
    if len(data) != expected_size:
        continue
        
    # Unpack as little-endian unsigned shorts
    pixels = struct.unpack(f"<{width * height}H", data)
    
    # Accumulators
    b_sum, gb_sum, gr_sum, r_sum = 0, 0, 0, 0
    count = (width * height) // 4
    
    for y in range(0, height, 2):
        row1_idx = y * width
        row2_idx = (y + 1) * width
        for x in range(0, width, 2):
            b_sum += pixels[row1_idx + x]
            gb_sum += pixels[row1_idx + x + 1]
            gr_sum += pixels[row2_idx + x]
            r_sum += pixels[row2_idx + x + 1]
            
    print(f"{gain_str:<10} | {b_sum/count:<10.2f} | {gb_sum/count:<10.2f} | {gr_sum/count:<10.2f} | {r_sum/count:<10.2f}")

print("-" * 60)
