

#include "wali/DiskUtils.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <format>
#include <iterator>
#include <ranges>
#include <string>
#include <string_view>
#include <sys/mount.h>
#include <system_error>
#include <wali/Commands.hpp>
#include <wali/Common.hpp>
#include <wali/Install.hpp>
#include <wali/widgets/WidgetData.hpp>


void Install::install(InstallHandlers handlers, WidgetDataPtr data, std::stop_token token)
{
  namespace chrono = std::chrono;
  using clock = chrono::steady_clock;

  m_stage_change = handlers.stage_change;
  m_install_state = handlers.complete;
  m_log = handlers.log;
  m_data = data;

  auto exec_stage = [this, token](std::function<bool(Install&)> f, const std::string_view stage) mutable
  {
    StageStatus state(StageStatus::Complete);

    try
    {
      log_stage_start(stage);
      state = f(std::ref(*this)) ? StageStatus::Complete : StageStatus::Fail;
      log_stage_end(stage, state);

      if (token.stop_requested())
      {
        log_info("Stopping...");
        return false;
      }
    }
    catch (const std::exception& ex)
    {
      PLOGE << "exec_stage exception: " << ex.what();
      log_stage_end(stage, StageStatus::Fail);
      m_install_state(InstallState::Fail);
    }
    return state == StageStatus::Complete;
  };

  InstallState state = InstallState::Fail;
  auto start = clock::now();

  try
  {
    // TODO swap, user shell
    on_state(InstallState::Running);

    m_tree = DiskUtils::probe();

    bool minimal =  exec_stage(&Install::filesystems,   STAGE_FS) &&
                    exec_stage(&Install::mount,         STAGE_MOUNT) &&
                    exec_stage(&Install::pacstrap,      STAGE_PACSTRAP) &&
                    exec_stage(&Install::fstab,         STAGE_FSTAB) &&
                    exec_stage(&Install::root_account,  STAGE_ROOT_ACC) &&
                    exec_stage(&Install::boot_loader,   STAGE_BOOT_LOADER);

    state = minimal ? InstallState::Partial : InstallState::Fail;

    if (minimal)
    {
      on_state(InstallState::Bootable);

      bool extra =  exec_stage(&Install::user_account,  STAGE_USER_ACC) &&
                    exec_stage(&Install::localise,      STAGE_LOCALISE) &&
                    exec_stage(&Install::video,         STAGE_VIDEO) &&
                    exec_stage(&Install::network,       STAGE_NETWORK) &&
                    exec_stage(&Install::packages,      STAGE_PACKAGES);

      state = extra ? InstallState::Complete : InstallState::Partial;
    }
  }
  catch (const std::exception& ex)
  {
    log_error(std::format("Unknown error: {}", ex.what()));
    state = InstallState::Fail;
  }

  if (state != InstallState::Fail)
  {
    const auto [size, used] = GetDevSpace{}(m_data->mounts.root_dev);
    m_data->summary.root_size = size;
    m_data->summary.root_used = used;
    m_data->summary.package_count = CountPackages{}(m_data->mounts.root_dev);
  }

  m_data->summary.duration = chrono::duration_cast<chrono::seconds>(clock::now() - start);

  // we always want to do this, and ignore any errors
  exec_stage(&Install::unmount, STAGE_UNMOUNT);

  on_state(state);
}


bool Install::filesystems()
{
  const MountData& data = m_data->mounts;

  log_info(std::format("/     -> {} with {}", data.root_dev, data.root_fs));
  log_info(std::format("/boot -> {} with {}", data.boot_dev, data.boot_fs));

  bool boot_root_valid{false}, home_valid{true};

  const auto wiped_boot_root = wipe_fs(data.boot_dev) &&
                               wipe_fs(data.root_dev);

  if (wiped_boot_root)
  {
    boot_root_valid = create_boot_filesystem(data.boot_dev) &&
                      create_ext4_filesystem(data.root_dev);

    if (boot_root_valid)
    {
      set_partition_type(data.boot_dev, PartTypeEfi);
      set_partition_type(data.root_dev, PartTypeRoot);
      home_valid = create_home_filesystem();
    }
  }

  return boot_root_valid && home_valid;
}

bool Install::wipe_fs(const std::string_view dev)
{
  log_info(std::format("Wipe filesystem on {}", dev));
  return ReadCommand::execute_read(std::format ("wipefs -a -f {}", dev)) == CmdSuccess;
}

bool Install::create_home_filesystem()
{
  bool home_valid{true};

  const MountData& data = m_data->mounts;

  if (data.home_target == HomeMountTarget::Existing)
    log_info(std::format("/home -> {}", data.home_dev));
  else if(data.home_target == HomeMountTarget::Root)
    log_info(std::format("/home -> {} with {}", data.root_dev, data.root_fs));
  else
  {
    // new partition
    const auto home_dev = data.home_dev;
    const auto home_fs = data.home_fs;

    log_info(std::format("/home -> {} with {}", home_dev, home_fs));

    home_valid = wipe_fs(home_dev) && create_ext4_filesystem(home_dev);

    if (home_valid)
      set_partition_type(home_dev, PartTypeHome);
  }

  return home_valid;
}

bool Install::create_boot_filesystem(const std::string_view part_dev)
{
  log_info(std::format("Create vfat32 on {}", part_dev));
  return CreateVfat32Filesystem{}(part_dev);
}

bool Install::create_ext4_filesystem(const std::string_view part_dev)
{
  log_info(std::format("Create ext4 on {}", part_dev));
  return CreateExt4Filesystem{}(part_dev);
}

void Install::set_partition_type(const std::string_view part_dev, const std::string_view type)
{
  const int part_num = DiskUtils::get_partition_part_number(m_tree, part_dev);
  const auto parent_dev = DiskUtils::get_partition_disk(m_tree, part_dev);

  log_info(std::format("Set partition type {} for {}", type, part_dev));

  if (!SetPartitionType{}(parent_dev, part_num, type))
    log_warning(std::format("Set partition type failed on {}", part_dev));
}

// mount
bool Install::mount()
{
  const MountData data = m_data->mounts;

  bool mounted_root{}, mounted_boot{}, mounted_home{true};

  mounted_root = do_mount(data.root_dev, RootMnt.string());

  if (mounted_root)
  {
    mounted_boot = do_mount(data.boot_dev, EfiMnt.string());

    if (data.home_target != HomeMountTarget::Root)
      mounted_home = do_mount(data.home_dev, HomeMnt.string());
  }

  return mounted_root && mounted_boot && mounted_home;
}

bool Install::unmount()
{
  log_info("Recursive unmount");
  return Unmount{}(RootMnt.string(), true);
}


bool Install::do_mount(const std::string_view dev, const std::string_view path)
{
  log_info(std::format("Mount {} onto {}", path, dev));

  std::error_code ec;
  if (fs::create_directory(path, ec); ec)
  {
    log_error(std::format("do_mount(): failed creating directory: {} - {}", path, ec.message()));
    return false;
  }

  return Mount{}(dev, path);
}


// pacman
bool Install::pacstrap()
{
  static PackageSet packages =
  {
    "base",
    "archlinux-keyring",
    "linux",
    "linux-firmware",
    "sudo",
    "reflector",      // pacman mirrors list
    "gpm"             // laptop touchpad support
  };

  if (const auto vendor = GetCpuVendor{}(); vendor != CpuVendor::None)
    packages.insert(vendor == CpuVendor::Amd ? "amd-ucode" : "intel-ucode");

  std::stringstream cmd_string;
  cmd_string << "pacstrap -K " <<  RootMnt.string() << ' ';
  cmd_string << flatten(packages);

  const int stat = ReadCommand::execute_read(cmd_string.str(), [this](const std::string_view m)
  {
    log_info(m);
  });

  if (stat != CmdSuccess)
    log_error("Pacstrap encountered an error");

  return stat == CmdSuccess;
}

bool Install::packages()
{
  log_info("Install additional packages");
  install_packages(m_data->packages.additional);
  return true;
}

bool Install::install_packages(const PackageSet& packages)
{
  if (packages.empty())
    return true;

  std::stringstream ss;
  ss << "pacman -S --noconfirm " << flatten(packages);

  const auto ok = Chroot{}(ss.str(), [this](const std::string_view m)
  {
    log_info(m);
  });

  if (!ok)
    log_error("pacman failed to install package(s)");

  return ok;
}


// fstab
bool Install::fstab ()
{
  fs::create_directory((RootMnt / FsTabPath).parent_path());

  const auto cmd_string = std::format("genfstab -U {} > {}", RootMnt.string(), FsTabPath.string());

  log_info(cmd_string);

  const auto stat = ReadCommand::execute_read(cmd_string);;
  if (stat != CmdSuccess)
    log_error(std::format("genfstab failed: {}", ::strerror(stat)));

  return stat == CmdSuccess;
}

// accounts
bool Install::root_account()
{
  const auto& root_password = m_data->accounts.root_pass;

  bool set{};
  if (root_password.empty())
    log_error("Root password empty");
  else
    set = set_password("root", root_password);

  return set;
}

bool Install::user_account()
{
  const auto& user = m_data->accounts.user_username;
  const auto& password = m_data->accounts.user_pass;
  const auto user_sudo = m_data->accounts.user_sudo;

  if (user.empty())
  {
    log_info("No user to create");
    return true;
  }
  else if (password.empty())
  {
    log_error("No user password set");
    return false;
  }

  log_info(std::format("Create account for {}", user));

  const std::string_view wheel_group = user_sudo ? "-G wheel" : "";

  if (!Chroot{}(std::format("useradd -s /usr/bin/bash -m {} {}", wheel_group, user)))
    log_warning("Failed to create user account");

  if (!set_password(user, password))
    log_warning("Failed to set user password"); // TODO should fail?

  if (user_sudo && !add_to_sudoers(user))
    log_warning("Failed to allow user to sudo");

  return true;
}

bool Install::set_password(const std::string_view user, const std::string_view pass)
{
  log_info(std::format("Set password for {}", user));

  return ChrootWrite{}(std::format("echo \"{}:{}\" | chpasswd", user, pass));
}

bool Install::add_to_sudoers (const std::string_view user)
{
  // we can edit /etc/sudoers, but creating an entry in /etc/sudoers.d is less prone to error:
  //   https://wiki.archlinux.org/title/Sudo#Configure_sudo_using_drop-in_files_in_/etc/sudoers.d
  // note: user added to wheel group by user_account()

  static const fs::perms SudoersFilePerms = fs::perms::owner_read | fs::perms::group_read;
  static const fs::path SudoersDir{RootMnt / "etc/sudoers.d"};

  // the directory should be empty, but sanity check
  const auto count = std::distance(fs::directory_iterator{SudoersDir}, fs::directory_iterator{});
  const auto sudoers_file = fs::path(SudoersDir / std::format("{:02}_{}", count, user));

  log_info(std::format("Adding {} to sudoers", user));

  {
    std::ofstream sudoers_stream{sudoers_file};
    sudoers_stream << std::format("{} ALL=(ALL) ALL", user);
  }

  std::error_code ec;
  if (fs::permissions(sudoers_file, SudoersFilePerms, ec); ec)
    log_warning(std::format("Failed to add user to sudoers: {}", ::strerror(ec.value())));

  return true;
}



// boot loader
bool Install::boot_loader()
{
  static const fs::path EfiConfigTargetDir{"/efi/grub"};
  static const fs::path EfiConfigTarget{EfiConfigTargetDir  / "grub.cfg"};

  log_info("Install packages for grub");

  // TODO systemd-boot
  if (!install_packages({"grub", "efibootmgr", "os-prober"}))
  {
    log_error("Failed to install packages required for grub");
    return false;
  }

  // install
  log_info("Run grub-install");
  const auto install_cmd = std::format("grub-install --target=x86_64-efi --efi-directory=/efi --boot-directory=/efi --bootloader-id=GRUB");

  if (!Chroot{}(install_cmd))
  {
    log_error("grub-install failed");
    return false;
  }

  // configure
  log_info("Configure grub");

  fs::create_directory(EfiMnt / "grub"); // outside of arch-chroot, so full path required: /mnt/efi/grub
  return Chroot{}(std::format("grub-mkconfig -o {}", EfiConfigTarget.string()));
}



// localise
bool Install::localise()
{
  static const fs::path TimezonePath{"/usr/share/zoneinfo/"};
  static const fs::path LocaleGen{"/etc/locale.gen"};
  static const fs::path LocaleConf{"/etc/locale.conf"};
  static const fs::path TerminalConf{"/etc/vconsole.conf"};

  const auto& [zone, locale, keymap] = m_data->localise;

  if (!locale.empty())
  {
    log_info("Update locale.gen");

    // NOTE: appending is a bit lazy, and perhaps not obvious to future readers
    if (!ChrootWrite{}(std::format("echo \"{} UTF-8\" >> {}", locale, LocaleGen.string())))
      log_warning(std::format("Failed to update {}", LocaleGen.string()));
    else
    {
      if (log_info("Create locales"); !Chroot{}("locale-gen"))
        log_warning("locale-gen failed");
      else
      {
        log_info("Setting locales");

        if (!ChrootWrite{}(std::format("echo \"LANG={}\" >> {}", locale, LocaleConf.string())))
          log_warning(std::format("Failed to update {}", LocaleConf.string()));
      }
    }
  }

  if (!zone.empty())
  {
    log_info("Set timezone");
    if (!Chroot{}(std::format("ln -sf {} /etc/localtime", (TimezonePath / zone).string())))
      log_warning("Failed to set locale timezone");
    else if (log_info("Syncing clock"); ReadCommand::execute_read("hwclock --systohc") != CmdSuccess)
      log_warning("Failed to sync hardware clock");
  }

  if (!keymap.empty())
  {
    log_info("Set vconsole keymap");
    if (!ChrootWrite{}(std::format("echo \"KEYMAP={}\" >> {}", keymap, TerminalConf.string())))
      log_warning(std::format("Failed to update {}", TerminalConf.string()));
  }

  return true;
}



// network
bool Install::network()
{
  static const fs::path HostnamePath{"/etc/hostname"};

  const auto [hostname, ntp, copy_conf] = m_data->network;

  log_info("Set hostname");
  if (!ChrootWrite{}(std::format("echo \"{}\" > {}", hostname, HostnamePath.string() )))
    log_warning("Failed to create /etc/hostname");

  if (copy_conf)
  {
    setup_iwd();
    setup_networkd();
  }

  if (ntp)
    enable_service({"systemd-timesyncd.service"});

  return true;
}

bool Install::setup_iwd()
{
  // iwd config: https://wiki.archlinux.org/title/Iwd#Network_configuration
  static const fs::path LiveIwdConfigPath{"/var/lib/iwd"};
  static const fs::path TargetIwdConfigPath{RootMnt / "var/lib/iwd"};

  log_info("Install and enable iwd");

  bool ok{};

  if (install_packages({"iwd"}) && enable_service({"iwd.service"}))
  {
    static const std::vector<std::string_view> Extensions = {".psk", ".open", ".8021x"};

    log_info("Copy iwd config");

    const auto src = LiveIwdConfigPath.string();
    const auto dest = TargetIwdConfigPath.string();
    const auto cmd = std::format("mkdir -p {} && cp -rf {}/* {}", dest, src, dest);

    if (ReadCommand::execute_read(cmd) != CmdSuccess)
      log_warning("Failed to copy iwd configs");
    else if (count_files(LiveIwdConfigPath, Extensions) != 1)
      log_warning("More than one wifi config, cannot set auto connect");
    else
      ok = true;
  }
  else
    log_warning("iwd package or service enable failed");

  return ok;
}

bool Install::setup_networkd()
{
  static const fs::path LiveConfigPath {"/etc/systemd/network"};
  static const fs::path TargetConfigPath {RootMnt / "etc/systemd/network"};

  const auto src = LiveConfigPath.string();
  const auto dest = TargetConfigPath.string();
  const auto cmd = std::format("mkdir -p {} && cp -rf {}/* {}", dest, src, dest);

  log_info("Copy networkd config");

  bool ok{};
  if (ReadCommand::execute_read(cmd) != CmdSuccess)
    PLOGW << "Failed to copy systemd-network configs";
  else
    ok = enable_service({"systemd-networkd.service", "systemd-resolved.service"});

  return ok;
}

// video
bool Install::video()
{
  log_info("Installing video drivers");

  if (!install_packages(m_data->video.drivers))
    log_warning("Failed to install GPU drivers");

  return true;
}


// general
bool Install::enable_service(const std::vector<std::string_view> services)
{
  bool enabled_all{true};

  for (const auto& svc : services)
  {
    log_info(std::format("Enable service {}", svc));

    const auto enabled = Chroot{}(std::format("systemctl enable {}", svc));
    if (!enabled)
      log_error(std::format("Failed to enable service: {}", svc));

    enabled_all &= enabled;
  }

  return enabled_all;
}

std::size_t Install::count_files (const fs::path& dir, const std::vector<std::string_view> ext)
{
  std::size_t count{};

  auto has_ext = [&](const fs::directory_entry& entry)
  {
    return rng::contains(ext, entry.path().extension());
  };

  for ([[maybe_unused]] const auto& entry : fs::directory_iterator{dir} | view::filter(has_ext))
    ++count;

  return count;
}
