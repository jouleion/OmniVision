import numpy as np
import csv
import re
from collections import defaultdict

GROUND_TRUTH = 1000

def compute_mae(grid):
    if len(grid) != 4 or any(len(row) != 4 for row in grid):
        return None
    arr = np.array(grid)
    return np.mean(np.abs(arr - GROUND_TRUTH))

results = defaultdict(lambda: {"S1": [], "S2": []})

with open("cleaned_log.csv", "r") as f:
    lines = f.readlines()

current_params = None
i = 0

while i < len(lines):
    line = lines[i].strip()
    if line.startswith("FREQ="):
        current_params = line
        i += 1
        continue

    if line.startswith("FRAME"):
        try:
            while "S1:" not in lines[i]:
                i += 1
            i += 1

            s1_grid = []
            for _ in range(4):
                row = list(map(int, lines[i].split()))
                s1_grid.append(row)
                i += 1

            while "S2:" not in lines[i]:
                i += 1
            i += 1

            s2_grid = []
            for _ in range(4):
                row = list(map(int, lines[i].split()))
                s2_grid.append(row)
                i += 1

            s1_error = compute_mae(s1_grid)
            s2_error = compute_mae(s2_grid)

            if s1_error is not None:
                results[current_params]["S1"].append(s1_error)

            if s2_error is not None:
                results[current_params]["S2"].append(s2_error)

        except Exception:
            pass

        continue

    i += 1

with open("results.csv", "w", newline="") as f:
    writer = csv.writer(f)

    writer.writerow([
        "FREQ", "INTER", "SHARP",
        "S1_avg_error", "S2_avg_error",
        "S1_frames", "S2_frames"
    ])

    for params, sensors in results.items():
        match = re.match(r"FREQ=(\d+)INTER=(\d+)SHARP=(\d+)", params)

        if match:
            freq, inter, sharp = match.groups()
        else:
            freq, inter, sharp = ("?", "?", "?")

        s1_vals = sensors["S1"]
        s2_vals = sensors["S2"]

        s1_avg = np.mean(s1_vals) if s1_vals else ""
        s2_avg = np.mean(s2_vals) if s2_vals else ""

        writer.writerow([
            freq,
            inter,
            sharp,
            round(s1_avg, 2) if s1_avg != "" else "",
            round(s2_avg, 2) if s2_avg != "" else "",
            len(s1_vals),
            len(s2_vals)
        ])

print("results.csv generated")
