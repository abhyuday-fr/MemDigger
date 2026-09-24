/*
 * MemDigger implements the minimal version of MLC.
 * Supports memory read latency and bandwidth benchmarking.
 * Uses pointer chasing and sequential loop techniques for latency and bandwidth
 * measurements respectively.
 * The data is written to a csv file, which can be
 * used by a python script to plot the graphs.
 */

#include <filesystem> // for data dir creation
#include <fstream>
#include <iostream>
#include <thread> // for hardware_concurrency

#include "mem_bench.hpp"

int main() {
  std::cout << "Initializing Hardware-Aware Memory Benchmark...\n";

  std::vector<size_t> custom_test_sizes = generate_topology_aware_sizes();

  std::filesystem::create_directory("data");

  std::ofstream csv("data/memory_results.csv");
  if (!csv.is_open()) {
    std::cerr << "Failed to open memory_results.csv for writing.\n";
    return 1;
  }

  csv << "Metric,Size_KB,Result\n";

  // latency
  std::cout << "Running Latency Benchmark (Pointer Chasing)...\n";
  run_latency_benchmark(csv, custom_test_sizes);

  // bandwidth
  unsigned num_threads = std::thread::hardware_concurrency();

  if (num_threads == 0) {
    num_threads = 1; // fallback if detection fails
  }

  std::cout << "Running Bandwidth Benchmark (Sequential Read, " << num_threads
            << " threads)...\n";
  run_bandwidth_benchmark(csv, num_threads);

  csv.close();
  std::cout << "Done. Results saved to memory_results.csv\n";
  return 0;
}
