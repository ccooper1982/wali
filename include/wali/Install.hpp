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

  // filesystems
  bool filesystems();
  bool create_home_filesystem();
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
  bool user_account();
  bool set_password(const std::string_view user, const std::string_view pass);
  bool add_to_sudoers (const std::string_view username);

  // boot loader
  bool boot_loader();

  // localise
  bool localise();

  // network
  bool network();

  // services
  bool enable_service(const std::string_view name);

private:
  // functions to call plog, then call a handler for the UI
  void log_stage_start(const std::string_view stage)
  {
    PLOGI << "Stage start: " << stage;
    m_stage_change(stage, StageStatus::Start);
  }

  void log_stage_end(const std::string_view stage, const StageStatus state)
  {
    PLOGI << "Stage end: " << stage;
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
  OnInstallComplete m_install_state;
  OnLog m_log;
};

#endif
