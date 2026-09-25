import pandas as pd
import matplotlib.pyplot as plt
import os

# load the benchmark results from the data folder
file_path = "data/memory_results.csv"
if not os.path.exists(file_path):
    print(f"Error: {file_path} not found. Run the C++ benchmark first.")
    exit(1)

df = pd.read_csv(file_path)

# separate latency and bandwidth data
latency_data = df[df["Metric"] == "Latency"].copy()
# handle Size_KB safely (convert to numeric, ignoring non-numeric like '256MB' if they snuck in)
latency_data["Size_KB"] = pd.to_numeric(latency_data["Size_KB"], errors="coerce")
latency_data = latency_data.dropna(subset=["Size_KB"]).sort_values(by="Size_KB")

bandwidth_data = df[df["Metric"] == "Bandwidth"].copy()

# initialize a figure with two subplots side-by-side (Ratio 3:1)
fig, (ax1, ax2) = plt.subplots(
    1, 2, figsize=(14, 6), gridspec_kw={"width_ratios": [3, 1]}
)

# Subplot 1: Latency Curve
ax1.plot(
    latency_data["Size_KB"],
    latency_data["Result"],
    marker="o",
    linestyle="-",
    color="#1f77b4",
    linewidth=2.5,
    markersize=8,
)
ax1.set_xscale("log", base=2)
ax1.set_title("Memory Latency vs. Buffer Size", fontsize=14, fontweight="bold")
ax1.set_xlabel("Buffer Size (KB) [Log2 Scale]", fontsize=12)
ax1.set_ylabel("Latency (nanoseconds)", fontsize=12)
ax1.grid(True, which="both", linestyle="--", alpha=0.5)

# annotate each latency point
for index, row in latency_data.iterrows():
    ax1.annotate(
        f"{row['Result']:.1f}ns",
        (row["Size_KB"], row["Result"]),
        textcoords="offset points",
        xytext=(0, 10),
        ha="center",
        fontsize=9,
    )

# Subplot 2: Bandwidth Bar Chart
if not bandwidth_data.empty:
    bw_val = bandwidth_data["Result"].values[0]
    ax2.bar(["Max DRAM\nRead"], [bw_val], color="#ff7f0e", width=0.5)
    ax2.set_title("Multi-threaded\nBandwidth", fontsize=14, fontweight="bold")
    ax2.set_ylabel("Throughput (GB/s)", fontsize=12)
    ax2.grid(axis="y", linestyle="--", alpha=0.7)

    # Annotate the bandwidth bar
    ax2.text(
        0,
        bw_val + (bw_val * 0.02),
        f"{bw_val:.2f} GB/s",
        ha="center",
        va="bottom",
        fontsize=11,
        fontweight="bold",
    )

plt.tight_layout()

# save the plot into the data folder
output_file = "data/memory_benchmark_dashboard.png"
plt.savefig(output_file, dpi=300, bbox_inches="tight")
print(f"Plot saved successfully as '{output_file}'.")

plt.show()
