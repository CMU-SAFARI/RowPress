import sys
import os

out_file_path = sys.argv[1]

bit_flip_counts = []
row_counts = []
headers = []
bit_count = 0
row_count = 0

with open(out_file_path) as f:
    lines = f.read().splitlines()
    for line in lines:
        if "Experiment" in line:
            bit_flip_counts.append(bit_count)
            row_counts.append(row_count)
            headers.append(line)
            bit_count = 0
            row_count = 0

        elif "REPORT" in line:    
            test = int(line.strip().split(" ")[-1].strip())
            bit_count = bit_count + test
            if test > 0:
                row_count = row_count + 1

bit_flip_counts.append(bit_count)
row_counts.append(row_count)

for idx, element in enumerate(headers):
    print(element)
    print("# of rows with bit flip: " + str(row_counts[idx + 1]))
    print("# of bit flips: " + str(bit_flip_counts[idx + 1]))
