/* Software License Agreement (BSD License)
 *
 *  Copyright (c) 2019, iRobot ROS
 *  All rights reserved.
 *
 *  This file is part of ros2-performance, which is released under BSD-3-Clause.
 *  You may use, distribute and modify this code under the BSD-3-Clause license.
 */

#include <sys/resource.h>
#include <sys/types.h>

#include <cmath>
#include <ctime>
#include <iomanip>
#include <string>
#include <thread>

#include "performance_metrics/resource_usage_logger.hpp"

namespace performance_metrics
{

ResourceUsageLogger::ResourceUsageLogger(const std::string & filename)
: m_filename(filename)
{
  _pid = getpid();
  _pagesize = getpagesize();

  _log = false;
  _done = true;
}

void ResourceUsageLogger::start(std::chrono::milliseconds period)
{
  m_file.open(m_filename, std::fstream::out);
  if (!m_file.is_open()) {
    std::cout << "[ResourceUsageLogger]: Error. Could not open file " << m_filename << std::endl;
    std::cout << "[ResourceUsageLogger]: Not logging." << std::endl;
    return;
  }

  std::cout << "[ResourceUsageLogger]: Logging to " << m_filename << std::endl;

  _t1_real_start = std::chrono::steady_clock::now();
  _t1_user = std::clock();
  _t1_real = std::chrono::steady_clock::now();
  _log = true;
  _done = false;

  // create a detached thread that monitors resource usage periodically
  std::thread logger_thread([ = ]() {
      int64_t i = 1;
      while (this->_log) {
        std::this_thread::sleep_until(_t1_real_start + period * i);
        if (i == 1) {
          _print_header(m_file);
          // print a line of zeros for better visualization
          _print(m_file);
        }
        _get();
        _print(m_file);
        i++;
        _done = true;
      }
    });
  logger_thread.detach();
}

void ResourceUsageLogger::stop()
{
  _log = false;
  while (_done == false) {
    // Wait until we are done logging.
    continue;
  }
  m_file.close();
}

void ResourceUsageLogger::print_resource_usage()
{
  _print_header(std::cout);
  _print(std::cout);
}

void ResourceUsageLogger::set_system_info(int pubs, int subs, float frequency)
{
  if (_log == true) {
    std::cout <<
      "[ResourceUsageLogger]: You have to set system info before starting the logger!" <<
      std::endl;
    return;
  }

  m_pubs = pubs;
  m_subs = subs;
  m_frequency = frequency;

  m_got_system_info = true;
}

// Get shared resources data
void ResourceUsageLogger::_get()
{
  // Get elapsed time since we started logging
  auto t2_real_start = std::chrono::steady_clock::now();
  _resources.elasped_ms =
    std::chrono::duration_cast<std::chrono::milliseconds>(t2_real_start - _t1_real_start).count();

  // Get CPU usage percentage
  auto t2_user = std::clock();
  auto t2_real = std::chrono::steady_clock::now();
  double time_elapsed_user_ms = 1000.0 * (t2_user - _t1_user) / CLOCKS_PER_SEC;
  int n_threads = std::thread::hardware_concurrency();
  double time_elapsed_real_ms =
    std::chrono::duration_cast<std::chrono::milliseconds>(t2_real - _t1_real).count();
  _resources.cpu_usage = time_elapsed_user_ms / (time_elapsed_real_ms * n_threads) * 100;

  // Get mallinfo
#if (defined(__UCLIBC__) || defined(__GLIBC__))
  auto mem_info = mallinfo();
  _resources.mem_arena_KB = mem_info.arena >> 10;
  _resources.mem_in_use_KB = mem_info.uordblks >> 10;
  _resources.mem_mmap_KB = mem_info.hblkhd >> 10;
#endif

  // Get rss
  struct rusage usage;
  getrusage(RUSAGE_SELF, &usage);
  _resources.mem_max_rss_KB = usage.ru_maxrss;

  // Get vsz from /proc/[pid]/statm
  std::string virtual_mem_pages_string;
  std::string statm_path = "/proc/" + std::to_string(_pid) + "/statm";
  std::ifstream in;
  in.open(statm_path);

  if (in.is_open()) {
    in >> virtual_mem_pages_string;
    _resources.mem_virtual_KB = ((uint64_t)std::stoi(virtual_mem_pages_string) * _pagesize) >> 10;
    in.close();
  } else {
    _resources.mem_virtual_KB = -1;
  }
}

void ResourceUsageLogger::_print_header(std::ostream & stream)
{
  const char separator = ' ';
  const int wide_space = 15;
  const int narrow_space = 10;

  stream << std::left << std::setw(wide_space) << std::setfill(separator) << "time[ms]";
  stream << std::left << std::setw(narrow_space) << std::setfill(separator) << "cpu[%]";
  stream << std::left << std::setw(wide_space) << std::setfill(separator) << "arena[KB]";
  stream << std::left << std::setw(wide_space) << std::setfill(separator) << "in_use[KB]";
  stream << std::left << std::setw(wide_space) << std::setfill(separator) << "mmap[KB]";
  stream << std::left << std::setw(wide_space) << std::setfill(separator) << "rss[KB]";
  stream << std::left << std::setw(wide_space) << std::setfill(separator) << "vsz[KB]";

  if (m_got_system_info) {
    stream << std::left << std::setw(wide_space) << std::setfill(separator) << "pubs";
    stream << std::left << std::setw(wide_space) << std::setfill(separator) << "subs";
    stream << std::left << std::setw(wide_space) << std::setfill(separator) << "frequency";
  }

  stream << std::endl;
}

void ResourceUsageLogger::_print(std::ostream & stream)
{
  const char separator = ' ';
  const int wide_space = 15;
  const int narrow_space = 10;

  stream << std::left << std::setw(wide_space) << std::setfill(separator) << std::setprecision(
    wide_space - 1) << std::round(_resources.elasped_ms);
  stream << std::left << std::setw(narrow_space) << std::setfill(separator) <<
    std::setprecision(2) << _resources.cpu_usage;
  stream << std::left << std::setw(wide_space) << std::setfill(separator) <<
    _resources.mem_arena_KB;
  stream << std::left << std::setw(wide_space) << std::setfill(separator) <<
    _resources.mem_in_use_KB;
  stream << std::left << std::setw(wide_space) << std::setfill(separator) <<
    _resources.mem_mmap_KB;
  stream << std::left << std::setw(wide_space) << std::setfill(separator) <<
    _resources.mem_max_rss_KB;
  stream << std::left << std::setw(wide_space) << std::setfill(separator) <<
    _resources.mem_virtual_KB;

  if (m_got_system_info) {
    stream << std::left << std::setw(wide_space) << std::setfill(separator) << m_pubs;
    stream << std::left << std::setw(wide_space) << std::setfill(separator) << m_subs;
    stream << std::left << std::setw(wide_space) << std::setfill(separator) << std::fixed <<
      m_frequency << std::defaultfloat;
  }

  stream << std::endl;
}

}  // namespace performance_metrics
