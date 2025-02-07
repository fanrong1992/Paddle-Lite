// Copyright (c) 2019 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "lite/backends/x86/cpu_info.h"

#ifdef PADDLE_WITH_XBYAK
#include "xbyak/xbyak.h"
#include "xbyak/xbyak_util.h"
#endif

#ifdef __APPLE__
#include <sys/sysctl.h>
#include <sys/types.h>
#elif defined(_WIN32)
#define NOMINMAX  // msvc max/min macro conflict with std::min/max
#define GLOG_NO_ABBREVIATED_SEVERITIES
#include <windows.h>
#else
#include <unistd.h>
#endif  // _WIN32

#include <algorithm>
#include "lite/utils/log/cp_logging.h"

#include "lite/utils/env.h"

// DEFINE_double(fraction_of_cpu_memory_to_use,
//               1,
//               "Default use 100% of CPU memory for PaddlePaddle,"
//               "reserve the rest for page tables, etc");
double fraction_of_cpu_memory_to_use =
    paddle::lite::GetDoubleFromEnv("fraction_of_cpu_memory_to_use", 1);

// DEFINE_uint64(initial_cpu_memory_in_mb,
//               500ul,
//               "Initial CPU memory for PaddlePaddle, in MD unit.");
uint64_t initial_cpu_memory_in_mb =
    paddle::lite::GetUInt64FromEnv("initial_cpu_memory_in_mb", 500ul);

// DEFINE_double(
//     fraction_of_cuda_pinned_memory_to_use,
//     0.5,
//     "Default use 50% of CPU memory as the pinned_memory for PaddlePaddle,"
//     "reserve the rest for page tables, etc");
double fraction_of_cuda_pinned_memory_to_use = paddle::lite::GetDoubleFromEnv(
    "fraction_of_cuda_pinned_memory_to_use", 0.5);

// If use_pinned_memory is true, CPUAllocator calls mlock, which
// returns pinned and locked memory as staging areas for data exchange
// between host and device.  Allocates too much would reduce the amount
// of memory available to the system for paging.  So, by default, we
// should set false to use_pinned_memory.
// DEFINE_bool(use_pinned_memory, true, "If set, allocate cpu pinned memory.");
bool use_pinned_memory =
    paddle::lite::GetBoolFromEnv("use_pinned_memory", true);

namespace paddle {
namespace lite {
namespace x86 {

size_t CpuTotalPhysicalMemory() {
#ifdef __APPLE__
  int mib[2];
  mib[0] = CTL_HW;
  mib[1] = HW_MEMSIZE;
  int64_t size = 0;
  size_t len = sizeof(size);
  if (sysctl(mib, 2, &size, &len, NULL, 0) == 0) return (size_t)size;
  return 0L;
#elif defined(_WIN32)
  MEMORYSTATUSEX sMeminfo;
  sMeminfo.dwLength = sizeof(sMeminfo);
  GlobalMemoryStatusEx(&sMeminfo);
  return sMeminfo.ullTotalPhys;
#else
  int64_t pages = sysconf(_SC_PHYS_PAGES);
  int64_t page_size = sysconf(_SC_PAGE_SIZE);
  return pages * page_size;
#endif
}

size_t CpuMaxAllocSize() {
  // For distributed systems, it requires configuring and limiting
  // the fraction of memory to use.
  return fraction_of_cpu_memory_to_use * CpuTotalPhysicalMemory();
}

size_t CpuMinChunkSize() {
  // Allow to allocate the minimum chunk size is 4 KB.
  return 1 << 12;
}

size_t CpuMaxChunkSize() {
  // Allow to allocate the maximum chunk size is roughly 3% of CPU memory,
  // or the initial_cpu_memory_in_mb.
  return std::min(static_cast<size_t>(CpuMaxAllocSize() / 32),
                  static_cast<size_t>(initial_cpu_memory_in_mb * 1 << 20));
}

size_t CUDAPinnedMaxAllocSize() {
  // For distributed systems, it requires configuring and limiting
  // the fraction of memory to use.
  return fraction_of_cuda_pinned_memory_to_use * CpuTotalPhysicalMemory();
}

size_t CUDAPinnedMinChunkSize() {
  // Allow to allocate the minimum chunk size is 64 KB.
  return 1 << 16;
}

size_t CUDAPinnedMaxChunkSize() {
  // Allow to allocate the maximum chunk size is roughly 1/256 of CUDA_PINNED
  // memory.
  return CUDAPinnedMaxAllocSize() / 256;
}

#ifdef PADDLE_WITH_XBYAK
static Xbyak::util::Cpu cpu;
bool MayIUse(const cpu_isa_t cpu_isa) {
  using namespace Xbyak::util;  // NOLINT
  switch (cpu_isa) {
    case sse42:
      return cpu.has(Cpu::tSSE42);
    case avx:
      return cpu.has(Cpu::tAVX);
    case avx2:
      return cpu.has(Cpu::tAVX2);
    case avx512f:
      return cpu.has(Cpu::tAVX512F);
    case avx512_core:
      return true && cpu.has(Cpu::tAVX512F) && cpu.has(Cpu::tAVX512BW) &&
             cpu.has(Cpu::tAVX512VL) && cpu.has(Cpu::tAVX512DQ);
    case avx512_core_vnni:
      return true && cpu.has(Cpu::tAVX512F) && cpu.has(Cpu::tAVX512BW) &&
             cpu.has(Cpu::tAVX512VL) && cpu.has(Cpu::tAVX512DQ) &&
             cpu.has(Cpu::tAVX512_VNNI);
    case avx512_mic:
      return true && cpu.has(Cpu::tAVX512F) && cpu.has(Cpu::tAVX512CD) &&
             cpu.has(Cpu::tAVX512ER) && cpu.has(Cpu::tAVX512PF);
    case avx512_mic_4ops:
      return true && MayIUse(avx512_mic) && cpu.has(Cpu::tAVX512_4FMAPS) &&
             cpu.has(Cpu::tAVX512_4VNNIW);
    case isa_any:
      return true;
  }
  return false;
}
#else
bool MayIUse(const cpu_isa_t cpu_isa) {
  if (cpu_isa == isa_any) {
    return true;
  } else {
    return false;
  }
}
#endif

}  // namespace x86
}  // namespace lite
}  // namespace paddle
