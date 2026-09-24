/*
 * the implementation uses pointer chasing.
 * pointer chasing is creating a linked list scattered randomly across a buffer
 * so the CPU cannot predict the next memory address.
 *
 * struct Node is padded to 64 bytes to match standard cache line size and
 * prevent false sharing
 */

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>

struct Node {
  Node *next;
  char pad[64 - sizeof(Node *)];
};

// run_latency_benchmark runs the benchmark and puts data in a csv file.
// test sizes from 4KB (L1 Cache) up to 64MB (main memory)
void run_latency_benchmark(std::ofstream &csv_file) {
  std::vector<size_t> sizes_kb = {4, 16, 64, 256, 1024, 4096, 16384, 65536};

  for (size_t size_kb : sizes_kb) {
    size_t num_nodes = (size_kb * 1024) / sizeof(Node);
    std::vector<Node> buffer(num_nodes);

    // generate random access indeces to defeat the prefetcher
    std::vector<size_t> indices(num_nodes);
    std::iota(indices.begin(), indices.end(), 0);
    std::mt19937 gen(42);
    std::shuffle(indices.begin(), indices.end(), gen);

    // link nodes randomly based on the shuffled indices
    for (size_t i = 0; i < num_nodes - 1; i++) {
      buffer[indices[i]].next = &buffer[indices[i + 1]];
    }
    buffer[indices.back()].next = nullptr;

    Node *head = &buffer[indices[0]];
    Node *current = head;

    // warmup pass to ensure the data is populated in the hierarchy
    for (size_t i = 0; i < num_nodes && current; i++) {
      current = current->next;
    }

    current = head;

    size_t count = 0;

    // timing block
    auto start = std::chrono::high_resolution_clock::now();
    while (current) {
      current = current->next;
      count++;
    }
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::nano> time_ns = end - start;
    double latency_per_access = time_ns.count() / count;

    csv_file << "Latency," << size_kb << "," << latency_per_access << "\n";
  }
}

int main() {
  std::cout << "Hello, world!" << std::endl;
  return 0;
}
