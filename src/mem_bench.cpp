/*
 * mem_bench.cpp implements methods in mem_bench.hpp
 *
 * compatible for both linux/unix and windows
 */

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <sched.h>
#include <string>
#include <thread>

#include "mem_bench.hpp"

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <pthread.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#endif

#ifdef __linux__

static bool read_cache_index(int index, int &level_out, size_t &size_kb_out,
                             std::string &type_out) {
  std::string base =
      "/sys/devices/system/cpu/cpu0/cache/index" + std::to_string(index);

  std::ifstream level_file(base + "/level");
  std::ifstream type_file(base + "/type");
  std::ifstream size_file(base + "/size");

  if (!level_file || !type_file || !size_file)
    return false;

  level_file >> level_out;
  std::getline(type_file, type_out);

  std::string size_str;
  size_file >> size_str;
  if (size_str.empty())
    return false;

  char unit = size_str.back();
  size_t value = std::stoull(size_str.substr(0, size_str.size() - 1));
  if (unit == 'K')
    size_kb_out = value;
  else if (unit == 'M')
    size_kb_out = value * 1024;
  else
    size_kb_out = value / 1024;

  return true;
}

static void detect_linux_caches(size_t &l1d, size_t &l2, size_t l3) {
  l1d = l2 = l3 = 0;
  for (int i = 0; i < 8; i++) {
    int level = 0;
    size_t size_kb = 0;
    std::string type;
    if (!read_cache_index(i, level, size_kb, type)) {
      continue;
    }
    if (type == "Instruction") {
      continue;
    }
    if (level == 1 && l1d == 0)
      l1d = size_kb;
    else if (level == 2 && l2 == 0)
      l2 = size_kb;
    else if (level == 3 && l3 == 0)
      l3 = size_kb;
  }
}
#endif

// cross-platform pin-current-thread-to-logical-processor helper
static void pin_thread_to_core(unsigned core_index) {
#ifdef _WIN32
  SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)1 << core_index);
#elif defined(__linux__)
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_index, &cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#else
  (void)core_index; // no-op on unspoorted platforms
#endif
}

// simple spin-wait barrier so all threads begin their timed region together
class SpinBarrier {
public:
  explicit SpinBarrier(unsigned count)
      : total_(count), waiting_(0), generation_(0) {}

  void arrive_and_wait() {
    unsigned gen = generation_.load(std::memory_order_acquire);
    if (waiting_.fetch_add(1, std::memory_order_acq_rel) + 1 == total_) {
      waiting_.store(0, std::memory_order_release);
      generation_.fetch_add(1, std::memory_order_release);
    } else {
      while (generation_.load(std::memory_order_acquire) == gen) {
        std::this_thread::yield();
      }
    }
  }

private:
  unsigned total_;
  std::atomic<unsigned> waiting_;
  std::atomic<unsigned> generation_;
};

std::vector<size_t> generate_topology_aware_sizes() {
  size_t l1d = 0, l2 = 0, l3 = 0, total_ram_kb = 0;

#ifdef _WIN32
  DWORD bufferSize = 0;
  GetLogicalProcessorInformation(nullptr, &bufferSize);
  std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> buffer(
      bufferSize / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
  GetLogicalProcessorInformation(buffer.data(), &bufferSize);

  for (const auto &info : buffer) {
    if (info.Relationship == RelationCache) {
      if (info.Cache.Level == 1 &&
          (info.Cache.Type == CacheData || info.Cache.Type == CacheUnified) &&
          l1d == 0) {
        l1d = info.Cache.Size / 1024;
      } else if (info.Cache.Level == 2 && l2 == 0) {
        l2 = info.Cache.Size / 1024;
      } else if (info.Cache.Level == 3 && l3 == 0) {
        l3 = info.Cache.Size / 1024;
      }
    }
  }

  MEMORYSTATUSEX memInfo;
  memInfo.dwLength = sizeof(MEMORYSTATUSEX);
  GlobalMemoryStatusEx(&memInfo);
  total_ram_kb = memInfo.ullTotalPhys / 1024;

#elif defined(__linux__)
  detect_linux_caches(l1d, l2, l3);

  struct sysinfo info;
  sysinfo(&info);
  total_ram_kb = (info.totalram * info.mem_unit) / 1024;
#endif

  std::cout << "Detected Topology:\n"
            << "L1d Cache: " << l1d << " KB\n"
            << "L2 Cache:  " << l2 << " KB\n"
            << "L3 Cache:  " << l3 << " KB\n"
            << "Total RAM: " << total_ram_kb / (1024 * 1024) << " GB\n\n";

  std::vector<size_t> sizes_kb;

  if (l1d > 0) {
    sizes_kb.push_back(l1d / 2);
    sizes_kb.push_back(l1d);
    sizes_kb.push_back(l1d * 2);
  }
  if (l2 > 0) {
    sizes_kb.push_back(l2 / 2);
    sizes_kb.push_back(l2);
    sizes_kb.push_back(l2 * 2);
  }
  if (l3 > 0) {
    sizes_kb.push_back(l3 / 2);
    sizes_kb.push_back(l3);
    sizes_kb.push_back(l3 * 2);
  }

  if (l1d == 0 && l2 == 0 && l3 == 0) {
    sizes_kb = {16, 32, 128, 256, 1024, 4096, 8192, 32768};
  }

  size_t safe_ram_test = std::min<size_t>(256 * 1024, total_ram_kb / 10);
  sizes_kb.push_back(safe_ram_test);

  std::sort(sizes_kb.begin(), sizes_kb.end());
  sizes_kb.erase(std::unique(sizes_kb.begin(), sizes_kb.end()), sizes_kb.end());

  return sizes_kb;
}

void run_latency_benchmark(std::ofstream &csv_file,
                           const std::vector<size_t> &sizes_kb) {
  for (size_t size_kb : sizes_kb) {
    size_t num_nodes = (size_kb * 1024) / sizeof(Node);
    if (num_nodes < 2)
      continue;

    std::vector<Node> buffer(num_nodes);
    std::vector<size_t> indices(num_nodes);
    std::iota(indices.begin(), indices.end(), 0);
    std::mt19937 gen(42);
    std::shuffle(indices.begin(), indices.end(), gen);

    for (size_t i = 0; i < num_nodes - 1; ++i) {
      buffer[indices[i]].next = &buffer[indices[i + 1]];
    }
    buffer[indices.back()].next = nullptr;

    Node *head = &buffer[indices[0]];

    Node *current = head;
    for (size_t i = 0; i < num_nodes && current; ++i) {
      current = current->next;
    }

    constexpr int kTrials = 5;
    double best_latency_per_access = std::numeric_limits<double>::max();

    for (int t = 0; t < kTrials; ++t) {
      current = head;
      size_t count = 0;

      auto start = std::chrono::steady_clock::now();
      while (current) {
        current = current->next;
        count++;
      }
      auto end = std::chrono::steady_clock::now();

      std::chrono::duration<double, std::nano> time_ns = end - start;
      double latency_per_access = time_ns.count() / count;
      best_latency_per_access =
          std::min(best_latency_per_access, latency_per_access);
    }

    csv_file << "Latency," << size_kb << "," << best_latency_per_access << "\n";
  }
}

void run_bandwidth_benchmark(std::ofstream &csv_file, unsigned num_threads) {
  if (num_threads == 0)
    num_threads = 1;

  const size_t per_thread_bytes = 256 * 1024 * 1024; // 256 MB per thread
  const size_t per_thread_elems = per_thread_bytes / sizeof(uint64_t);

  SpinBarrier start_barrier(num_threads);
  std::vector<std::thread> threads;
  std::vector<double> thread_seconds(num_threads, 0.0);
  std::vector<uint64_t> thread_sums(num_threads, 0);

  std::atomic<bool> begin_flag{false};

  for (unsigned t = 0; t < num_threads; ++t) {
    threads.emplace_back([&, t]() {
      pin_thread_to_core(t);

      // allocate and touch this thread's private buffer before the timed region
      std::vector<uint64_t> buffer(per_thread_elems, 1);

      start_barrier
          .arrive_and_wait(); // ensure all threads finished allocating/touching

      while (!begin_flag.load(std::memory_order_acquire)) {
        std::this_thread::yield(); // wait for the launcher's go-signal
      }

      auto start = std::chrono::steady_clock::now();
      uint64_t sum = 0;
      for (size_t i = 0; i < buffer.size(); ++i) {
        sum += buffer[i];
      }
      auto end = std::chrono::steady_clock::now();

      thread_sums[t] = sum;
      thread_seconds[t] = std::chrono::duration<double>(end - start).count();
    });
  }

  // give threads a moment to reach the barrier then release them together
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  begin_flag.store(true, std::memory_order_release);

  for (auto &th : threads)
    th.join();

  // wall clock time is the max across threads (the slowest one bounds the
  // parallel region)
  double wall_seconds =
      *std::max_element(thread_seconds.begin(), thread_seconds.end());
  size_t total_bytes = per_thread_bytes * num_threads;

  double bandwidth_gbps =
      (total_bytes / (1024.0 * 1024.0 * 1024.0)) / wall_seconds;
  csv_file << "Bandwidth," << (per_thread_bytes / 1024) * num_threads << ","
           << bandwidth_gbps << "\n";

  // prevent dead code elimination of each thread's sum
  volatile uint64_t sink = 0;
  for (auto s : thread_sums)
    sink += s;
}
