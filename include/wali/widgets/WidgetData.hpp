#ifndef WALI_WIDGETDATA_H
#define WALI_WIDGETDATA_H

#include <chrono>
#include <string>
#include <wali/Common.hpp>

enum class HomeMountTarget
{
  Root,     // /home mounted to root partition
  New,      // /home mounted to new partition (wipe, create fs)
  Existing  // /home mounted to existing partition (don't wipe or create fs)
};

enum class Bootloader
{
  SystemdBoot,
  Grub
};

struct AccountsData
{
  std::string root_pass;
  std::string user_username;
  std::string user_pass;
  std::string user_shell;
  bool user_sudo{true};
};

struct MountData
{
  std::string boot_dev;
  std::string boot_fs;
  std::string root_dev;
  std::string root_fs;
  std::string home_dev;
  std::string home_fs;
  HomeMountTarget home_target;
  Bootloader boot_loader;
  bool zram{true};
};

struct PackagesData
{
  PackageSet additional;
};

struct DesktopData
{
  PackageSet desktop;
  PackageSet dm;
  ServiceSet services;
  bool iwd{};
  bool netmanager{};
};

struct netmanagerata
{
  std::string hostname;
  bool ntp;
  bool copy_config;
};

struct LocaliseData
{
  std::string timezone;
  std::string locale;
  std::string keymap;
};

struct VideoData
{
  PackageSet drivers;
};

struct Summary
{
  std::size_t package_count{};
  std::chrono::seconds duration{};
  std::string root_size;
  std::string root_used;
};

struct WidgetData
{
  MountData mounts;
  AccountsData accounts;
  LocaliseData localise;
  PackagesData packages;
  DesktopData desktop;
  netmanagerata network;
  VideoData video;
  Summary summary;
};

using WidgetDataPtr = std::shared_ptr<WidgetData>;

#endif
