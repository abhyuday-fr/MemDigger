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
