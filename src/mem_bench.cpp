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
#endif
