#ifndef MEM_BENCH_HPP
#define MEM_BENCH_HPP

/*
 * mem_bench.hpp contains the Node struct and all methods' definition
 *
 * struct Node is padded to 64 bytes to match standard cache line size and
 * prevent false sharing
 *
 * generate_topology_aware_sizes generates an array of test sizes (in KB) based
 * on hardware cache boundaries
 *
 * run_latency_benchmark runs the pointer-chasing latency test
 *
 * run_bandwidth_benchmark runs the sequqntial read bandwidth test
 * it is one thread per logical processor by default
 */

#include <cstddef>
#include <fstream>
#include <vector>

struct Node {
  Node *next;
  char pad[64 - sizeof(Node *)];
};

std::vector<size_t> generate_topology_aware_sizes();

void run_latency_benchmark(std::ofstream &csv_file,
                           const std::vector<size_t> &sizes_kb);

void run_bandwidth_benchmark(std::ofstream &csv_file, unsigned num_threads);

#endif
