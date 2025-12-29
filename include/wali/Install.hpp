#ifndef WALI_INSTALL_H
#define WALI_INSTALL_H

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
  Complete,
  Fail
};

enum class InstallLogLevel
{
  Info,
  Warning,
  Error
};

using OnStageChange = std::function<void(const std::string_view, const StageStatus)>;
using OnInstallComplete = std::function<void(const InstallState)>;
using OnLog = std::function<void(const std::string_view, const InstallLogLevel)>;

struct Handlers
{
  OnStageChange stage_change;
  OnInstallComplete complete;
  OnLog log;
};

class Install
{
public:

  void install(Handlers&& handlers);

private:
  // stages

  // filesystems
  bool filesystems();
  bool create_boot_filesystem(const std::string_view part_dev);
  bool create_ext4_filesystem(const std::string_view part_dev);
  void set_partition_type(const std::string_view part_dev, const std::string_view type);
  bool wipe_fs(const std::string_view dev);
  bool fstab ();

  // mounting
  bool mount();
  bool do_mount(const std::string_view dev, const std::string_view path, const std::string_view fs);

  // pacman
  bool pacstrap();
  bool install_packages(const std::vector<std::string>& packages);

  // acounts
  bool root_account();
  bool set_password(const std::string_view user, const std::string_view pass);

  // boot loader
  bool boot_loader();


private:
  void log_stage_start(const std::string_view stage)
  {
    PLOGI << "Stage start: " << stage;
    //std::format("{} - Start", stage)
    m_stage_change(stage, StageStatus::Start);
  }

  void log_stage_end(const std::string_view stage, const StageStatus state)
  {
    PLOGI << "Stage end: " << stage;
    //std::format("{} - {}", stage, ok ? "Success" : "Fail")
    m_stage_change(stage, state);
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

  void log_error(const std::string_view msg)
  {
    PLOGE << msg;
    m_log(msg, InstallLogLevel::Error);
  }

private:
  OnStageChange m_stage_change;
  OnInstallComplete m_complete;
  OnLog m_log;
};

#endif
