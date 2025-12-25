#ifndef WALILIB_COMMON_H
#define WALILIB_COMMON_H

#include <filesystem>

namespace fs = std::filesystem;

static inline const fs::path InstallLogPath {"/var/log/ali/install.log"};

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

#endif
