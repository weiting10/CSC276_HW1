# plot_latency.py -- combine L1 / L2 / L3 / DRAM latency samples into one figure
# Usage:  python3 plot_latency.py
# Expects l1.csv, l2.csv, l3.csv, dram.csv in the same folder
# (each file = one header line, then one number per line).

import numpy as np
import matplotlib
matplotlib.use("Agg")            # draw to a file; no display needed (works over ssh)
import matplotlib.pyplot as plt

files = {
    "L1":   "l1.csv",
    "L2":   "l2.csv",
    "L3":   "l3.csv",
    "DRAM": "dram.csv",
}

# ---- 1. Load the data ---------------------------------------------------
data = {}
for label, path in files.items():
    data[label] = np.loadtxt(path, skiprows=1)   # skip the header line
    d = data[label]
    print(f"{label:5s} n={len(d):5d}  median={np.median(d):6.0f}  "
          f"min={d.min():6.0f}  max={d.max():8.0f}")

# ---- 2. Choose the x-axis range -----------------------------------------
# A few samples are huge (interrupts, context switches). Cut the axis at the
# 99th percentile of everything so those outliers don't squash the plot.
all_samples = np.concatenate(list(data.values()))
x_max = np.percentile(all_samples, 99)
bins = np.linspace(0, x_max, 150)

# ---- 3. Plot one histogram per level, overlaid --------------------------
plt.figure(figsize=(10, 5))
for label, d in data.items():
    plt.hist(d[d <= x_max], bins=bins, alpha=0.6, label=label)

plt.xlabel("Access latency (CPU cycles)")
plt.ylabel("Number of samples")
plt.title("Cache / memory access latency distribution")
plt.legend()
plt.tight_layout()
plt.savefig("latency.png", dpi=150)
print("Saved figure to latency.png")
