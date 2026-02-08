

#include "wali/DiskUtils.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
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
      if (token.stop_requested())
      {
        log_error("Stop requested");
        return false;
      }
      else
      {
        log_stage_start(stage);
        state = f(std::ref(*this)) ? StageStatus::Complete : StageStatus::Fail;
        log_stage_end(stage, state);
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

    const bool minimal =  exec_stage(&Install::filesystems,   STAGE_FS) &&
                          exec_stage(&Install::mount,         STAGE_MOUNT) &&
                          exec_stage(&Install::pacstrap,      STAGE_PACSTRAP) &&
                          exec_stage(&Install::localise,      STAGE_LOCALISE) &&
                          exec_stage(&Install::fstab,         STAGE_FSTAB) &&
                          exec_stage(&Install::root_account,  STAGE_ROOT_ACC) &&
                          exec_stage(&Install::boot_loader,   STAGE_BOOT_LOADER);

    state = minimal ? InstallState::Partial : InstallState::Fail;

    if (minimal)
    {
      // TODO these stages can fail and we don't quit so they all return false,
      //      so change them to return void
      on_state(InstallState::Bootable);

      const bool extra =  exec_stage(&Install::user_account,  STAGE_USER_ACC) &&
                          exec_stage(&Install::video,         STAGE_VIDEO) &&
                          exec_stage(&Install::desktop,       STAGE_DESKTOP) &&
                          exec_stage(&Install::network,       STAGE_NETWORK) &&
                          exec_stage(&Install::swap,          STAGE_SWAP) &&
                          exec_stage(&Install::packages,      STAGE_PACKAGES);

      state = extra ? InstallState::Complete : InstallState::Partial;
    }
  }
  catch (const std::exception& ex)
  {
    log_error(std::format("Unknown error: {}", ex.what()));
    state = InstallState::Fail;
  }

  // if cancelled, we can't be sure of the state
  state = token.stop_requested() ? InstallState::Fail : state;

  if (state != InstallState::Fail)
  {
    const auto [size, used] = GetDevSpace{}(m_data->mounts.root_dev);
    m_data->summary.root_size = size;
    m_data->summary.root_used = used;
    m_data->summary.package_count = CountPackages{}(m_data->mounts.root_dev);
    m_data->summary.duration = chrono::duration_cast<chrono::seconds>(clock::now() - start);
  }

  // we always want to do this, ignoring any errors
  exec_stage(&Install::unmount, STAGE_UNMOUNT);

  on_state(state);
}


bool Install::filesystems()
{
  const MountData& data = m_data->mounts;

  log_info(std::format("/     -> {} with {}", data.root_dev, data.root_fs));
  log_info(std::format("/boot -> {} with {}", data.boot_dev, data.boot_fs));

  bool volumes_created{true};

  const bool fs_created = create_boot_filesystem() && create_root_filesystem() && create_home_filesystem();

  if (fs_created)
  {
    // need to mount devices to create subvolumes
    bool mounted_home{true}, mounted_root{false};

    if (data.root_fs == "btrfs")
    {
      mounted_root = Mount{}(data.root_dev, RootMnt.string());
      volumes_created = mounted_root && create_btrfs_subvolume(RootMnt, "@");
    }

    if (data.home_target != HomeMountTarget::Root)
      mounted_home = Mount{}(data.home_dev, HomeMnt.string());

    // don't create subvolume, we assume it already exists (and is name @home)
    // TODO this shouldn't assume existing subvol is @home
    if (data.home_fs == "btrfs" && mounted_home && data.home_target != HomeMountTarget::Existing)
      volumes_created &= create_btrfs_subvolume(HomeMnt, "@home");

    if (mounted_root)
      unmount(); // recursive
  }

  return fs_created && volumes_created;
}

bool Install::wipe_fs(const std::string_view dev)
{
  log_info(std::format("Wipe filesystem on {}", dev));
  return ReadCommand::execute_read(std::format ("wipefs -a -f {}", dev)) == CmdSuccess;
}

bool Install::create_boot_filesystem()
{
  const MountData& data = m_data->mounts;

  wipe_fs(data.boot_dev);

  set_partition_type(data.boot_dev, PartTypeEfi);

  log_info(std::format("Create vfat32 on {}", data.boot_dev));
  return CreateFilesystem::vfat32(data.boot_dev);
}

bool Install::create_root_filesystem()
{
  const MountData& data = m_data->mounts;

  wipe_fs(data.root_dev);

  set_partition_type(data.root_dev, PartTypeRoot);

  if (data.root_fs == "ext4")
    return create_ext4_filesystem(data.root_dev);
  else
    return create_btrfs_filesystem(data.root_dev) ;
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
    const auto home_dev = data.home_dev;
    const auto home_fs = data.home_fs;

    wipe_fs(home_dev);

    // new partition
    log_info(std::format("/home -> {} with {}", home_dev, home_fs));

    if (!wipe_fs(home_dev))
      return false;

    home_valid = home_fs == "ext4" ?  create_ext4_filesystem(home_dev) :
                                      create_btrfs_filesystem(home_dev);

    set_partition_type(home_dev, PartTypeHome);
  }

  return home_valid;
}

bool Install::create_ext4_filesystem(const std::string_view dev)
{
  log_info(std::format("Create ext4 on {}", dev));
  return CreateFilesystem::ext4(dev);
}

bool Install::create_btrfs_filesystem(const std::string_view dev)
{
  log_info(std::format("Create btrfs on {}", dev));
  return CreateFilesystem::btrfs(dev);
}

bool Install::create_btrfs_subvolume(const fs::path mount_point, const std::string_view name)
{
  const auto path = (mount_point / name).string();

  log_info(std::format("Create btrfs subvolume {}", path));
  return CreateBtrfsSubVolume{}(path);
}

void Install::set_partition_type(const std::string_view dev, const std::string_view type)
{
  const int part_num = DiskUtils::get_partition_part_number(m_tree, dev);
  const auto parent_dev = DiskUtils::get_partition_disk(m_tree, dev);

  log_info(std::format("Set partition type {} for {}", type, dev));

  log_warning_if(SetPartitionType{}(parent_dev, part_num, type), std::format("Set partition type failed on {}", dev));
}

// mount
bool Install::mount()
{
  const MountData data = m_data->mounts;

  bool mounted_root{}, mounted_boot{}, mounted_home{true};

  const std::string_view root_opts = data.root_fs == "btrfs" ? "subvol=@" : "";
  mounted_root = do_mount(data.root_dev, RootMnt.string(), root_opts);

  if (mounted_root)
  {
    mounted_boot = do_mount(data.boot_dev, BootMnt.string());

    // for btrfs, we need an entry in fstab for @home subvolume, so we still mount
    // even if home is on the root partition
    if (data.home_target != HomeMountTarget::Root || data.home_fs == "btrfs")
    {
      // TODO this is incomplete: if existing partition is btrfs, we're assuming there's @home
      //      subvolume
      const auto home_opts = data.home_fs == "btrfs" ? "subvol=@home" : "" ;
      mounted_home = do_mount(data.home_dev, HomeMnt.string(), home_opts);
    }
  }

  return mounted_root && mounted_boot && mounted_home;
}

bool Install::unmount()
{
  log_info("Recursive unmount");
  return Unmount{}(RootMnt.string(), true);
}

bool Install::do_mount(const std::string_view dev, const std::string_view path, const std::string_view opts)
{
  log_info(std::format("Mount {} onto {} {}", path, dev, opts.empty() ? "" : std::format("with options {}", opts)));
  return Mount{}(dev, path, opts);
}


// pacman
bool Install::pacstrap()
{
  static PackageSet packages =
  {
    "base",
    "linux",
    "linux-firmware",
    "sudo",
    "which",          // used in user_shell()
    "nano",
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

  log_error_if(ok, "pacman failed to install package(s)");

  return ok;
}


// fstab
bool Install::fstab ()
{
  fs::create_directory((RootMnt / FsTabPath).parent_path());

  const auto cmd_string = std::format("genfstab -U {} > {}", RootMnt.string(), FsTabPath.string());

  log_info(cmd_string);

  const auto stat = ReadCommand::execute_read(cmd_string);
  log_error_if(stat == CmdSuccess, std::format("genfstab failed: {}", ::strerror(stat)));

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

  user_shell();

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
    sudoers_stream << std::format("{} ALL=(ALL) ALL\n", user);
  }

  std::error_code ec;
  if (fs::permissions(sudoers_file, SudoersFilePerms, ec); ec)
    log_warning(std::format("Failed to add user to sudoers: {}", ::strerror(ec.value())));

  return true;
}

void Install::user_shell()
{
  const auto& shell = m_data->accounts.user_shell;
  const auto& username = m_data->accounts.user_username;

  log_info(std::format("Set user shell: {}", shell));

  if (shell == "sh" || install_packages({shell}))
  {
    std::string path;
    const bool have_path = Chroot{}(std::format("which {}", shell), [&](const std::string_view m){ path = m; });

    if (have_path && !path.empty())
      log_warning_if(Chroot{}(std::format("chsh -s {} {}", path, username)) == CmdSuccess, "Failed to set user shell");
    else
      log_warning("Failed to get path of installed shell");
  }
}


// boot loader
bool Install::boot_loader()
{
  if (m_data->mounts.boot_loader == Bootloader::Grub)
    return boot_loader_grub();
  else
    return boot_loader_sysdboot();
}


bool Install::boot_loader_grub()
{
  static const fs::path EfiConfigTargetDir{"/boot/grub"};
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
  const auto install_cmd = std::format("grub-install --target=x86_64-efi --efi-directory=/boot --boot-directory=/boot --bootloader-id=GRUB");

  if (!Chroot{}(install_cmd))
  {
    log_error("grub-install failed");
    return false;
  }

  // configure
  log_info("Configure grub");

  fs::create_directory(BootMnt / "grub"); // outside of arch-chroot, so full path required: /mnt/boot/grub
  return Chroot{}(std::format("grub-mkconfig -o {}", EfiConfigTarget.string()));
}


bool Install::boot_loader_sysdboot()
{
  static const fs::path LoaderConfig {BootMnt / "loader/loader.conf"};
  static const fs::path EntriesFile {BootMnt / "loader/entries/arch.conf"};
  static const auto LoaderConfigContent = "default  arch.conf\n"
                                          "timeout  4\n"
                                          "console-mode max\n"
                                          "editor   no\n";

  static const auto EntryContent = "title   Arch Linux\n"
                                   "linux   /vmlinuz-linux\n"
                                   "initrd  /initramfs-linux.img\n";

  log_info("Run bootctl");

  const auto root_uuid = DiskUtils::get_partition_uuid(m_tree, m_data->mounts.root_dev);
  if (root_uuid.empty())
  {
    log_error("Failed to get boot partition UUID");
    return false;
  }

  // this installs the boot manager to /mnt/boot/EFI/[systemd,BOOT]
  if (!Chroot{}("bootctl install"))
  {
    log_error("bootctl failed to install the boot manager");
    return false;
  }

  bool ok{};

  log_info("Create loader config");

  fs::create_directory (LoaderConfig.parent_path());
  fs::create_directory (EntriesFile.parent_path());

  {
    if (std::ofstream loader_stream{LoaderConfig}; !loader_stream)
    {
      log_error("Failed to open loader config");
      return false;
    }
    else if (loader_stream << LoaderConfigContent; !loader_stream)
    {
      log_error("Failed to write to loader config");
      return false;
    }
  }

  log_info("Create entry file");

  if (std::ofstream entry_stream{EntriesFile}; !entry_stream)
    log_error("Failed to open loader config");
  else
  {
    std::string_view root_flags;

    if (m_data->mounts.root_fs == "btrfs")
      root_flags = "rootflags=subvol=@";

    entry_stream << EntryContent <<  "options root=UUID=" << root_uuid << " " << root_flags << " rw\n";
    entry_stream.close();

    ok = log_error_if(Chroot{}("bootctl install"), "bootctl failed to parse config");
  }

  return ok;
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
  log_warning_if(ChrootWrite{}(std::format("echo \"{}\" > {}", hostname, HostnamePath.string())), "Failed to create /etc/hostname");

  enable_service({"systemd-resolved"});

  if (m_data->desktop.iwd)
    setup_iwd();

  if (m_data->desktop.netmanager)
    setup_network_manager();

  if (ntp)
    enable_service({"systemd-timesyncd"});

  return true;
}

bool Install::setup_iwd()
{
  // iwd config: https://wiki.archlinux.org/title/Iwd#Network_configuration
  static const fs::path LiveIwdConfigPath{RootMnt / "etc/iwd/main.conf"};
  static const auto IwdConfig = "[General]\nEnableNetworkConfiguration=true";
  static const auto create_config = std::format("mkdir -p {} && echo -e \"{}\" > {}", LiveIwdConfigPath.parent_path().string(),
                                                                                      IwdConfig,
                                                                                      LiveIwdConfigPath.string());

  log_info("Install and enable iwd");

  log_warning_if(install_packages({"iwd"}) && enable_service({"iwd", "systemd-networkd"}), "iwd package or service enable failed");
  log_warning_if(ReadCommand::execute_read(create_config) == CmdSuccess, "Failed to create IWD config");

  if (m_data->network.copy_config)
  {
    static const fs::path LiveIwdConnectionsPath{"/var/lib/iwd"};
    static const fs::path TargetIwdConnectionsPath{RootMnt / "var/lib/iwd"};

    log_info("Copy iwd config");

    const auto src = LiveIwdConnectionsPath.string();
    const auto dest = TargetIwdConnectionsPath.string();
    const auto cmd = std::format("mkdir -p {} && cp -rf {}/* {}", dest, src, dest);

    log_warning_if(ReadCommand::execute_read(cmd) == CmdSuccess, "Failed to copy iwd configs");
  }

  return true;
}

bool Install::setup_network_manager()
{
  static const fs::path LiveConfigPath {"/etc/systemd/network"};
  static const fs::path TargetConfigPath {RootMnt / "etc/systemd/network"};

  if (m_data->network.copy_config)
  {
    const auto src = LiveConfigPath.string();
    const auto dest = TargetConfigPath.string();
    const auto cmd = std::format("mkdir -p {} && cp -rf {}/* {}", dest, src, dest);

    log_info("Copy NetworkManager configs");
    log_warning_if(ReadCommand::execute_read(cmd) == CmdSuccess, "Failed to copy systemd-network configs");
  }

  return install_packages({"networkmanager"}) && enable_service({"NetworkManager"});
}


// sawp
bool Install::swap()
{
  static const fs::path ZramConfig {RootMnt / "etc/systemd/zram-generator.conf"};

  if (!m_data->mounts.zram)
    return true;

  log_info("Install zram generator");

  if (!install_packages({"zram-generator"}))
    return false;

  log_info("Create zram config");

  {
    std::ofstream cfg_file{ZramConfig, std::ios_base::out | std::ios_base::trunc};
    cfg_file << "[zram0]\n";
    cfg_file << "zram-size = min(ram / 4, 4096)\n"; // 25% of ram or 4GB
    cfg_file << "compression-algorithm = zstd\n";   // sensible
  }

  log_error_if(fs::exists(ZramConfig) && fs::file_size(ZramConfig), "Failed to create zram config");

  enable_service({"systemd-zram-setup@zram0.service"});

  return true;
}


// desktop
bool Install::desktop()
{
  log_info(std::format("Install {} packages", m_data->desktop.desktop.size() + m_data->desktop.dm.size()));
  return install_packages(m_data->desktop.dm) && install_packages(m_data->desktop.desktop) && enable_service(m_data->desktop.services);
}


// video
bool Install::video()
{
  log_info("Install video drivers");

  if (!install_packages(m_data->video.drivers))
    log_warning("Failed to install GPU drivers");

  return true;
}


// general
bool Install::enable_service(const ServiceSet& services)
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
