import pandas as pd
import matplotlib.pyplot as plt

# load the benchmark results
try:
    df = pd.read_csv("data/memory_results.csv")
except FileNotFoundError:
    print("Error: memory_results.csv not found. Run the C++ benchmark first.")
    exit(1)

# filter for latency metrics and ensure they are sorted by size
latency_data = df[df["Metric"] == "Latency"].copy()
latency_data = latency_data.sort_values(by="Size_KB")

if latency_data.empty:
    print("Error: No latency data found in the CSV.")
    exit(1)

# initialize the plot
plt.figure(figsize=(10, 6))

# plot latency (X: Size in KB, Y: Latency in ns)
plt.plot(
    latency_data["Size_KB"],
    latency_data["Result"],
    marker="o",
    linestyle="-",
    color="#1f77b4",
    linewidth=2.5,
    markersize=8,
)

# use a Log2 scale for the X-axis to evenly distribute cache tiers
plt.xscale("log", base=2)

# labels and title
plt.title(
    "Memory Latency vs. Buffer Size (Cache Hierarchy)", fontsize=14, fontweight="bold"
)
plt.xlabel("Buffer Size (KB) [Log2 Scale]", fontsize=12)
plt.ylabel("Latency (nanoseconds)", fontsize=12)

# grid lines for easier reading of the exact plateau heights
plt.grid(True, which="both", linestyle="--", alpha=0.5)

# Annotate each point with its exact latency value
for index, row in latency_data.iterrows():
    plt.annotate(
        f"{row['Result']:.1f}ns",
        (row["Size_KB"], row["Result"]),
        textcoords="offset points",
        xytext=(0, 10),
        ha="center",
        fontsize=9,
    )

# Save and display
output_file = "data/memory_latency_curve.png"
plt.savefig(output_file, dpi=300, bbox_inches="tight")
print(f"Plot saved successfully as '{output_file}'.")

plt.show()
