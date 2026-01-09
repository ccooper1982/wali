#ifndef WALI_COMMON_H
#define WALI_COMMON_H

#include <concepts>
#include <filesystem>
#include <functional>
#include <math.h>
#include <ranges>
#include <set>
#include <string_view>
#include <plog/Log.h>


namespace fs = std::filesystem;

inline static const fs::path InstallLogPath {"/var/log/ali/install.log"};
inline static const fs::path RootMnt{"/mnt"};
inline static const fs::path EfiMnt{"/mnt/efi"};
inline static const fs::path HomeMnt{"/mnt/home"};
inline static const fs::path FsTabPath{"/mnt/etc/fstab"};

using StringViewVec = std::vector<std::string_view>;
using PackageSet = std::set<std::string>;

inline static const constexpr auto STAGE_FS           = "Create Filesystems";
inline static const constexpr auto STAGE_MOUNT        = "Mount";
inline static const constexpr auto STAGE_PACSTRAP     = "Pacstrap";
inline static const constexpr auto STAGE_FSTAB        = "Generate Filesystem Table";
inline static const constexpr auto STAGE_ROOT_ACC     = "Root Account";
inline static const constexpr auto STAGE_BOOT_LOADER  = "Bootloader";
inline static const constexpr auto STAGE_USER_ACC     = "User Account";
inline static const constexpr auto STAGE_LOCALISE     = "Localise";
inline static const constexpr auto STAGE_VIDEO        = "Video";
inline static const constexpr auto STAGE_NETWORK      = "Network";
inline static const constexpr auto STAGE_PACKAGES     = "Packages";


enum class GpuVendor
{
  Unknown,
  Amd,
  Nvidia,
  Vm,
  Intel
};

enum class CpuVendor
{
  None,
  Amd,
  Intel
};

enum class HomeMountTarget
{
  Root,     // /home mounted to /
  New,      // /home mounted to new partition (wipe, create fs)
  Existing  // /home mounted to existing partition (don't wipe or create fs)
};

static inline std::string format_size(const int64_t size)
{
  static const char *sizeNames[] = {"B", "KB", "MB", "GB", "TB", "PB"};

  const uint64_t i = (uint64_t) std::floor(std::log(size) / std::log(1024));
  const auto display_size = size / std::pow(1024, i);

  return std::format("{:.1f} {}", display_size, sizeNames[i]);
}

template<class C, typename F>
void for_each (C& c, F f) requires std::ranges::constant_range<C> || std::ranges::range<C>
{
  if constexpr (std::ranges::constant_range<C>)
    std::for_each(std::cbegin(c), std::cend(c), std::move(f));
  else
    std::for_each(std::begin(c), std::end(c), std::move(f));
}

template<class C>
std::string flatten(const C& c, const char sep = ' ')
  requires  std::ranges::range<C> &&
            std::convertible_to<typename C::value_type, std::string_view>
{
  std::stringstream ss;
  for (const auto& package : c)
    ss << package << sep;

  return ss.str();
}

#endif
