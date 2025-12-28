#ifndef WALI_INSTALL_H
#define WALI_INSTALL_H

#include <exception>
#include <format>
#include <functional>
#include <string_view>
#include <wali/Common.hpp>


enum class InstallState
{
  None,
  Fail,
  Running,
  Bootable,   // bootable system
  Partial,    // system is bootable, but optional stage failed
  Complete    // complete success
};

enum class StageStatus
{
  Start,
  Ok,
  Fail
};

enum class InstallLogLevel
{
  Info,
  Warning,
  Error
};

using OnStateChange = std::function<void(const std::string_view, const StageStatus)>;
using OnInstallComplete = std::function<void(const InstallState)>;
using OnLog = std::function<void(const std::string_view, const InstallLogLevel)>;

struct Handlers
{
  OnStateChange state_change;
  OnInstallComplete complete;
  OnLog log;
};

class Install
{
public:

  void install(Handlers&& handlers);

private:
  // stages
  StageStatus filesystems();
  bool create_boot_filesystem(const std::string_view part_dev);
  bool create_ext4_filesystem(const std::string_view part_dev);
  void set_partition_type(const std::string_view part_dev, const std::string_view type);

  StageStatus mount();

  // other
  bool wipe_fs(const std::string_view dev);

private:
  void log_stage_start(const std::string_view stage)
  {
    PLOGI << "Stage start: " << stage;
    //std::format("{} - Start", stage)
  }

  void log_stage_end(const std::string_view stage, const StageStatus state)
  {
    PLOGI << "Stage end: " << stage;
    //std::format("{} - {}", stage, ok ? "Success" : "Fail")
  }

  void log_info(const std::string_view msg)
  {
    PLOGI << msg;
    m_log(msg, InstallLogLevel::Info);
  }

  void log_warning(const std::string_view msg)
  {
    PLOGW << msg;
    m_log(msg, InstallLogLevel::Warning);
  }

private:
  OnStateChange m_state_change;
  OnInstallComplete m_complete;
  OnLog m_log;
};

#endif
