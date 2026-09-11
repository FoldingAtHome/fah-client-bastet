/******************************************************************************\

                  This file is part of the Folding@home Client.

          The fah-client runs Folding@home protein folding simulations.
                    Copyright (c) 2001-2026, foldingathome.org
                               All rights reserved.

       This program is free software; you can redistribute it and/or modify
       it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 3 of the License, or
                       (at your option) any later version.

         This program is distributed in the hope that it will be useful,
          but WITHOUT ANY WARRANTY; without even the implied warranty of
          MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
                   GNU General Public License for more details.

     You should have received a copy of the GNU General Public License along
     with this program; if not, write to the Free Software Foundation, Inc.,
           51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

                  For information regarding this software email:
                                 Joseph Coffland
                          joseph@cauldrondevelopment.com

\******************************************************************************/

#include "CoreProcess.h"
#include "Core.h"

#include <cbang/hw/CPUInfo.h>
#include <cbang/os/SystemInfo.h>
#include <cbang/os/SystemUtilities.h>
#include <cbang/log/Logger.h>
#include <cstddef>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__linux__)
#include <fstream>
#include <sched.h>
#endif

using namespace FAH::Client;
using namespace cb;
using namespace std;


namespace {
#ifdef __linux__
  bool readCPUList(const string &path, vector<unsigned> &cpus) {
    ifstream in(path);
    if (!in) return false;

    cpus.clear();
    unsigned first, last;
    for (;;) {
      if (!(in >> first)) return false;
      last = first;
      if (in.peek() == '-') {
        in.get();
        if (!(in >> last)) return false;
      }
      if (last < first || (unsigned)CPU_SETSIZE <= last) return false;
      if (!cpus.empty() && first <= cpus.back()) return false;
      for (unsigned i = first; i <= last; i++) cpus.push_back(i);
      in >> ws;
      if (in.eof()) return !cpus.empty();
      if (in.get() != ',') return false;
    }
  }
#endif


  vector<unsigned> findPerformanceCPUs() {
    vector<unsigned> cpus;

#ifdef _WIN32
#if defined(__i386__) || defined(__x86_64__) || \
    defined(_M_IX86) || defined(_M_X64)
    // Skip x86/x64 emulation on ARM.
    using IsWow64 = BOOL (WINAPI *)(HANDLE, USHORT *, USHORT *);
    auto isWow64 = reinterpret_cast<IsWow64>(GetProcAddress(
      GetModuleHandleW(L"kernel32.dll"), "IsWow64Process2"));
    USHORT processMachine, nativeMachine;
    if (isWow64 && (!isWow64(GetCurrentProcess(), &processMachine, &nativeMachine) ||
        (nativeMachine != IMAGE_FILE_MACHINE_AMD64 &&
         nativeMachine != IMAGE_FILE_MACHINE_I386))) return cpus;

    if (CPUInfo::create()->getVendor() != "GenuineIntel") return cpus;

#elif defined(__arm__) || defined(__aarch64__) || \
      defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC)
    // Windows reports heterogeneous ARM cores through EfficiencyClass.
#else
    return cpus;
#endif

    // Process affinity is limited to one processor group.
    if (GetActiveProcessorGroupCount() != 1) return cpus;
    // Affinity mask must represent every processor in the group.
    if (sizeof(DWORD_PTR) * 8 < GetActiveProcessorCount(0)) return cpus;

    DWORD size = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &size);
    if (!size) return cpus;
    vector<unsigned char> data(size);
    auto info = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(
      data.data());
    if (!GetLogicalProcessorInformationEx(RelationProcessorCore, info, &size))
      return cpus;

    DWORD_PTR mask = 0;
    BYTE low = 255, high = 0;
    const unsigned minSize = offsetof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX,
      Processor.GroupMask) + sizeof(GROUP_AFFINITY);
    for (DWORD offset = 0; offset < size; offset += info->Size) {
      if (size - offset < minSize) return {};
      info = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(
        data.data() + offset);
      if (info->Size < minSize || size - offset < info->Size ||
          info->Processor.GroupCount != 1 ||
          info->Processor.GroupMask[0].Group) return {};

      BYTE efficiency = info->Processor.EfficiencyClass;
      if (efficiency < low) low = efficiency;
      if (high < efficiency) {high = efficiency; mask = 0;}
      if (efficiency == high) mask |= info->Processor.GroupMask[0].Mask;
    }
    if (low == high) return cpus;
    for (unsigned i = 0; i < sizeof(mask) * 8; i++)
      if (mask & ((DWORD_PTR)1 << i)) cpus.push_back(i);

#elif defined(__linux__)
#if defined(__i386__) || defined(__x86_64__)
    if (CPUInfo::create()->getVendor() != "GenuineIntel") return cpus;

    // Intel core PMU lists P-core logical CPUs.
    if (!readCPUList("/sys/bus/event_source/devices/cpu_core/cpus", cpus))
      return {};

#elif defined(__arm__) || defined(__aarch64__)
    vector<unsigned> online;
    if (!readCPUList("/sys/devices/system/cpu/online", online)) return cpus;

    vector<unsigned> capacities;
    unsigned low = ~0u, high = 0;
    for (auto cpu: online) {
      ifstream in("/sys/devices/system/cpu/cpu" + to_string(cpu) +
        "/cpu_capacity");
      unsigned capacity;
      if (!(in >> capacity)) return {};
      capacities.push_back(capacity);
      if (capacity < low) low = capacity;
      if (high < capacity) high = capacity;
    }
    if (low == high) return cpus;
    for (size_t i = 0; i < online.size(); i++)
      if (capacities[i] == high) cpus.push_back(online[i]);
#else
    return cpus;
#endif

#else
    return cpus;
#endif

    unsigned expected = SystemInfo::instance().getPerformanceCPUCount();
    if ((expected && expected != cpus.size()) ||
        SystemInfo::instance().getCPUCount() <= cpus.size()) cpus.clear();
    return cpus;
  }


  const vector<unsigned> &getPerformanceCPUs() {
    static const auto cpus = findPerformanceCPUs();
    return cpus;
  }

#ifdef __linux__
  struct RestoreAffinity {
    cpu_set_t mask;
    bool active = false;
    ~RestoreAffinity() {
      if (active && sched_setaffinity(0, sizeof(mask), &mask))
        LOG_WARNING("Failed to restore client thread affinity");
    }
  };
#endif
}


uint32_t CoreProcess::getPerformanceCPUCount() {
  auto count = SystemInfo::instance().getPerformanceCPUCount();
  return count ? count : (uint32_t)getPerformanceCPUs().size();
}


CoreProcess::CoreProcess(const std::string &path) :
  path(SystemUtilities::absolute(path)) {

  // Set environment library paths
  vector<string> paths;
  paths.push_back(SystemUtilities::dirname(this->path));
  const string &ldPath = SystemUtilities::library_path;
  if (SystemUtilities::getenv(ldPath))
    SystemUtilities::splitPaths(SystemUtilities::getenv(ldPath), paths);
  set(ldPath, SystemUtilities::joinPaths(paths));

  // Set working directory
  setWorkingDirectory("work");
}


void CoreProcess::exec(const vector<string> &_args, unsigned cpuBudget) {
  vector<string> args;
  args.push_back(path);
  args.insert(args.end(), _args.begin(), _args.end());

#if defined(_WIN32) || defined(__linux__)
  const auto &cpus = getPerformanceCPUs();
  bool restrict = !cpus.empty() && cpuBudget <= cpus.size();
#endif
#ifdef __linux__
  // Child inherits the launching thread's affinity.
  RestoreAffinity restore;
  if (restrict && !sched_getaffinity(0, sizeof(restore.mask), &restore.mask)) {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    for (auto cpu: cpus)
      if (CPU_ISSET(cpu, &restore.mask)) CPU_SET(cpu, &mask);
    if (CPU_COUNT(&mask) && cpuBudget <= (unsigned)CPU_COUNT(&mask))
      restore.active = !sched_setaffinity(0, sizeof(mask), &mask);
  }
#endif

  LOG_INFO(3, "Running FahCore: " << Subprocess::assemble(args));
  Subprocess::exec(args, Subprocess::NULL_STDOUT | Subprocess::NULL_STDERR |
    Subprocess::CREATE_PROCESS_GROUP | Subprocess::W32_HIDE_WINDOW,
    Subprocess::PRIORITY_IDLE);

#ifdef _WIN32
  if (restrict) {
    HANDLE process = OpenProcess(
      PROCESS_QUERY_INFORMATION | PROCESS_SET_INFORMATION, FALSE, (DWORD)getPID());
    if (!process) return;
    DWORD_PTR allowed = 0, system = 0, mask = 0;
    unsigned count = 0;
    if (GetProcessAffinityMask(process, &allowed, &system))
      for (auto cpu: cpus)
        if (allowed & ((DWORD_PTR)1 << cpu)) {
          mask |= (DWORD_PTR)1 << cpu;
          count++;
        }
    bool restricted = mask && cpuBudget <= count &&
      SetProcessAffinityMask(process, mask);
    CloseHandle(process);
    if (restricted) LOG_INFO(3, "Restricted FahCore to performance CPUs");
  }
#elif defined(__linux__)
  if (restore.active) LOG_INFO(3, "Restricted FahCore to performance CPUs");
#endif
}


void CoreProcess::stop() {
  if (killedByClient) return; // Already killed, just waiting for it to exit

  uint64_t now = Time::now();

  // ``stop()`` is called about once a second while stopping.  A long gap
  // means the client was not running, e.g. system suspend, so restart the
  // grace period rather than spend it while asleep.
  if (interruptTime && 60 < now - lastStop) interruptTime = now;
  lastStop = now;

  if (!interruptTime) {
    interruptTime = now;
    interrupt();

  } else if (interruptTime + 60 < now) {
    LOG_WARNING("Core did not shutdown gracefully, killing process");
    kill();
    killedByClient = true;
  }
}
