#ifndef WALI_INSTALL_H
#define WALI_INSTALL_H

#include <Wt/WSignal.h>
#include <concepts>
#include <functional>
#include <stop_token>
#include <string_view>
#include <type_traits>
#include <utility>
#include <wali/Common.hpp>
#include <wali/DiskUtils.hpp>
#include <wali/widgets/WidgetData.hpp>


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


using OnStageChange = std::function<void(const std::string, const StageStatus)>;
using OnInstallComplete = std::function<void(const InstallState)>;
using OnLog = std::function<void(const std::string, const InstallLogLevel)>;

struct InstallHandlers
{
  OnStageChange stage_change;
  OnLog log;
  OnInstallComplete complete;
};


class Install final
{
public:

  void install(InstallHandlers handlers, WidgetDataPtr data, std::stop_token token);

private:

  // filesystems
  bool filesystems();
  bool fstab ();
  bool create_root_filesystem();
  bool create_home_filesystem();
  bool create_boot_filesystem();
  bool create_ext4_filesystem(const std::string_view dev);
  bool create_btrfs_filesystem(const std::string_view dev);
  bool create_btrfs_subvolume(const fs::path mount_point, const std::string_view name);
  void set_partition_type(const std::string_view dev, const std::string_view type);
  bool wipe_fs(const std::string_view dev);

  // mounting
  bool mount();
  bool unmount();
  bool do_mount(const std::string_view dev, const std::string_view path, const std::string_view opts = "");

  // pacman
  bool pacstrap();
  bool packages();
  bool install_packages(const PackageSet& packages);

  // acounts
  bool root_account();
  bool user_account();
  bool set_password(const std::string_view user, const std::string_view pass);
  bool add_to_sudoers (const std::string_view username);

  // bootloader
  bool boot_loader();
  bool boot_loader_grub();
  bool boot_loader_sysdboot();

  // locisation
  bool localise();

  // network
  bool network();
  bool setup_iwd();
  bool setup_network_manager();

  // swap
  bool swap();

  // desktop
  bool desktop();

  // video
  bool video();

  // services
  bool enable_service(const ServiceSet& services);

  // general
  static std::size_t count_files (const fs::path& dir, const std::vector<std::string_view> ext);

  // functions to call plog, then call a handler for the UI
  void log_stage_start(const std::string_view stage)
  {
    PLOGI << "Stage start: " << stage;
    m_stage_change(std::string{stage}, StageStatus::Start);
  }

  void log_stage_end(const std::string_view stage, const StageStatus state)
  {
    PLOGI << "Stage end: " << stage;
    m_stage_change(std::string{stage}, state);
  }

  void log_info(const std::string_view msg)
  {
    PLOGI << msg;
    m_log(std::string{msg}, InstallLogLevel::Info);
  }

  void log_warning(const std::string_view msg)
  {
    PLOGW << msg;
    m_log(std::string{msg}, InstallLogLevel::Warning);
  }

  // log warning if `ok` is false
  void log_warning_if(const bool ok, const std::string_view msg)
  {
    if (!ok)
      log_warning(msg);
  }

  void log_error(const std::string_view msg)
  {
    PLOGE << msg;
    m_log(std::string{msg}, InstallLogLevel::Error);
  }

  bool log_error_if(const bool ok, const std::string_view msg)
  {
    if (!ok)
      log_error (msg);

    return ok;
  }

  void on_state(const InstallState state)
  {
    PLOGI << "Install state: " << std::to_underlying(state);
    m_install_state(state);
  }

private:
  OnStageChange m_stage_change;
  OnInstallComplete m_install_state;
  OnLog m_log;
  WidgetDataPtr m_data;
  Tree m_tree;
};

#endif
