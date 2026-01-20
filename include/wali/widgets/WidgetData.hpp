#ifndef WALI_WIDGETDATA_H
#define WALI_WIDGETDATA_H

#include <string>
#include <wali/Common.hpp>

struct AccountsData
{
  std::string root_pass;
  std::string user_username;
  std::string user_pass;
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
};

struct PackagesData
{
  PackageSet additional;
};

struct NetworkData
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

struct WidgetData
{
  AccountsData accounts;
  MountData mounts;
  PackagesData packages;
  NetworkData network;
  LocaliseData localise;
  VideoData video;
};

using WidgetDataPtr = std::shared_ptr<WidgetData>;

#endif
