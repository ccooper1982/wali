#ifndef WALI_COMMON_H
#define WALI_COMMON_H

#include <filesystem>
#include <math.h>
#include <plog/Log.h>

namespace fs = std::filesystem;

inline static const fs::path InstallLogPath {"/var/log/ali/install.log"};
inline static const fs::path RootMnt{"/mnt"};
inline static const fs::path EfiMnt{"/mnt/efi"};
inline static const fs::path HomeMnt{"/mnt/home"};
inline static const fs::path FsTabPath{"/mnt/etc/fstab"};

enum class GpuVendor
{
  Unknown,
  Amd,
  Nvidia,
  VM,
  Intel
};

enum class CpuVendor
{
  None,
  Amd,
  Intel
};

static inline std::string format_size(const int64_t size)
{
  static const char *sizeNames[] = {"B", "KB", "MB", "GB", "TB", "PB"};

  std::string result;

  const uint64_t i = (uint64_t) std::floor(std::log(size) / std::log(1024));
  const auto display_size = size / std::pow(1024, i);

  return std::format("{:.1f} {}", display_size, sizeNames[i]);
}

template<class C, typename F>
void for_each (const C& c, F&& f) requires std::ranges::range<C>
{
  std::for_each(std::cbegin(c), std::cend(c), std::move(f));
}


#endif
