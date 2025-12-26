#ifndef WALI_COMMON_H
#define WALI_COMMON_H

#include <filesystem>
#include <math.h>
#include <plog/Log.h>
#include <Wt/WCheckBox.h>
#include <Wt/WComboBox.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WHBoxLayout.h>
#include <Wt/WVBoxLayout.h>
#include <Wt/WGridLayout.h>
#include <Wt/WLabel.h>
#include <Wt/WLineEdit.h>
#include <Wt/WText.h>


using namespace Wt;


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

static inline std::string format_size(const int64_t size)
{
  static const char *sizeNames[] = {"B", "KB", "MB", "GB", "TB", "PB"};

  std::string result;

  const uint64_t i = (uint64_t) std::floor(std::log(size) / std::log(1024));
  const auto display_size = size / std::pow(1024, i);

  return std::format("{:.1f} {}", display_size, sizeNames[i]);
}


template<typename T, typename ... Args>
std::unique_ptr<T> make_wt (Args...args)
{
  return std::make_unique<T>(args...);
}


template<class C, typename F>
void for_each (const C& c, F&& f) requires std::ranges::range<C>
{
  std::for_each(std::cbegin(c), std::cend(c), std::move(f));
}


#endif
