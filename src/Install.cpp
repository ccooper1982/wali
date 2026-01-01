#include "wali/Common.hpp"
#include <cstring>
#include <filesystem>
#include <format>
#include <iterator>
#include <string>
#include <sys/mount.h>
#include <system_error>
#include <wali/Commands.hpp>
#include <wali/Install.hpp>
#include <wali/widgets/Widgets.hpp>

// Hex codes: https://gist.github.com/gotbletu/a05afe8a76d0d0e8ec6659e9194110d2
static const constexpr char PartTypeEfi[] = "ef00";
static const constexpr char PartTypeRoot[] = "8304";
static const constexpr char PartTypeHome[] = "8302";


void Install::install(Handlers&& handlers)
{
  m_stage_change = handlers.stage_change;
  m_install_state = handlers.complete;
  m_log = handlers.log;

  auto exec_stage = [this](std::function<bool()> f, const std::string_view stage) mutable
  {
    bool ok{false};
    try
    {
      log_stage_start(stage);
      ok = f();
      log_stage_end(stage, ok ? StageStatus::Complete : StageStatus::Fail);
    }
    catch (const std::exception& ex)
    {
      m_install_state(InstallState::Fail);
      log_stage_end(stage, StageStatus::Fail);
    }
    return ok;
  };

  bool minimal{}, extra{};

  try
  {
    // TODO swap, network, time (need timezones)

    minimal = exec_stage(std::bind(&Install::filesystems, std::ref(*this)), "Create Filesystems") &&
              exec_stage(std::bind(&Install::mount, std::ref(*this)), "Mounting") &&
              exec_stage(std::bind(&Install::pacstrap, std::ref(*this)), "Pacstrap") &&
              exec_stage(std::bind(&Install::fstab, std::ref(*this)), "Generate fstab") &&
              exec_stage(std::bind(&Install::root_account, std::ref(*this)), "Root Account") &&
              exec_stage(std::bind(&Install::boot_loader, std::ref(*this)), "Boot loader");

    if (minimal)
    {
      m_install_state(InstallState::Bootable);

      // TODO user shell, additional packages
      extra = exec_stage(std::bind(&Install::user_account, std::ref(*this)), "User Account") &&
              exec_stage(std::bind(&Install::localise, std::ref(*this)), "Localise") &&
              exec_stage(std::bind(&Install::network, std::ref(*this)), "Network");
    }
  }
  catch (const std::exception& ex)
  {
    log_error(std::format("Unknown error: {}", ex.what()));
  }

  if (extra)
    m_install_state(InstallState::Complete);
  else
    m_install_state(minimal ? InstallState::Partial : InstallState::Fail);
}


bool Install::filesystems()
{
  const auto boot = Widgets::get_partitions()->get_boot();
  const auto root = Widgets::get_partitions()->get_root();

  log_info(std::format("/     -> {} with {}", root->get_device(), root->get_fs()));
  log_info(std::format("/boot -> {} with {}", boot->get_device(), boot->get_fs()));

  bool boot_root_valid{false}, home_valid{true};

  const auto wiped_boot_root = wipe_fs(root->get_device()) &&
                               wipe_fs(boot->get_device());

  if (wiped_boot_root)
  {
    boot_root_valid = create_boot_filesystem(boot->get_device()) &&
                      create_ext4_filesystem(root->get_device());

    if (boot_root_valid)
    {
      set_partition_type(boot->get_device(), PartTypeEfi);
      set_partition_type(root->get_device(), PartTypeRoot);
      home_valid = create_home_filesystem();
    }
  }

  return boot_root_valid && home_valid;
}

bool Install::wipe_fs(const std::string_view dev)
{
  log_info(std::format("Wiping filesystem on {}", dev));
  ReadCommand wipe;
  return wipe.execute(std::format ("wipefs -a -f {}", dev)) == CmdSuccess;
}

bool Install::create_home_filesystem()
{
  bool home_valid{true};

  const auto root = Widgets::get_partitions()->get_root();
  const auto home = Widgets::get_partitions()->get_home();

  if (home->get_mount_target() == HomeMountTarget::Existing)
  {
    log_info(std::format("/home -> {}", home->get_device()));
  }
  else if(home->is_home_on_root())
  {
    log_info(std::format("/home -> {} with {}", root->get_device(), root->get_fs()));
  }
  else
  {
    // new partition
    const auto home_dev = home->get_device();
    const auto home_fs = home->get_fs();

    log_info(std::format("/home -> {} with {}", home_dev, home_fs));

    home_valid = wipe_fs(home_dev) && create_ext4_filesystem(home_dev);

    if (home_valid)
      set_partition_type(home_dev, PartTypeHome);
  }

  return home_valid;
}

bool Install::create_boot_filesystem(const std::string_view part_dev)
{
  log_info(std::format("Creating vfat32 on {}", part_dev));
  return CreateVfat32Filesystem{}(part_dev);
}

bool Install::create_ext4_filesystem(const std::string_view part_dev)
{
  log_info(std::format("Creating ext4 on {}", part_dev));
  return CreateExt4Filesystem{}(part_dev);
}

void Install::set_partition_type(const std::string_view part_dev, const std::string_view type)
{
  const int part_num = PartitionUtils::get_partition_part_number(part_dev);
  const auto parent_dev = PartitionUtils::get_partition_parent(part_dev);

  ReadCommand cmd;
  if (cmd.execute(std::format("sgdisk -t{}:{} {}", part_num, type, parent_dev)) != CmdSuccess)
    log_warning(std::format("Set partition type failed on {}", part_dev));
}

// mount
bool Install::mount()
{
  const auto root = Widgets::get_partitions()->get_root();
  const auto boot = Widgets::get_partitions()->get_boot();
  const auto home = Widgets::get_partitions()->get_home();

  bool mounted_root{}, mounted_boot{}, mounted_home{true};

  mounted_root = do_mount(root->get_device(), RootMnt.string(), root->get_fs());

  if (mounted_root)
  {
    mounted_boot = do_mount(boot->get_device(), EfiMnt.string(), boot->get_fs());

    if (!home->is_home_on_root())
      mounted_home = do_mount(home->get_device(), HomeMnt.string(), home->get_fs());
  }

  return mounted_root && mounted_boot && mounted_home;
}


bool Install::do_mount(const std::string_view dev, const std::string_view path, const std::string_view fs)
{
  log_info(std::format("Mounting {} onto {}", path, dev));

  std::error_code ec;
  if (fs::create_directory(path, ec); ec)
  {
    log_error(std::format("do_mount(): failed creating directory: {} - {}", path, ec.message()));
    return false;
  }

  return Mount{}(dev, path);
}


// pacstrap
bool Install::pacstrap()
{
  static const std::vector<std::string> Packages =
  {
    "base",
    "archlinux-keyring",
    "linux",
    "sudo",
    "usb_modeswitch", // for usb devices that can switch modes (recharging/something else)
    "usbmuxd",
    "usbutils",
    "reflector",      // pacman mirrors list
    "dmidecode",      // BIOS / hardware info, may not actually be useful for most users
    "gpm",            // laptop touchpad support
    "e2fsprogs",      // useful
    "less",           // useful
  };

  log_info("This may take a while, depending on your connection speed");

  std::stringstream cmd_string;
  cmd_string << "pacstrap -K " <<  RootMnt.string();
  for (const auto& package : Packages)
    cmd_string << ' ' << package;

  ReadCommand cmd;
  const int stat = cmd.execute_read_line(cmd_string.str(), [this](const std::string_view m)
  {
    log_info(m);
  });

  if (stat != CmdSuccess)
    log_error("Pacstrap encountered an error");

  return stat == CmdSuccess;
}


// fstab
bool Install::fstab ()
{
  fs::create_directory((RootMnt / FsTabPath).parent_path());

  int stat{CmdFail};

  ReadCommand cmd;
  stat = cmd.execute(std::format("genfstab -U {} > {}", RootMnt.string(), FsTabPath.string()));
  if (stat != CmdSuccess)
    log_error(std::format("genfstab failed: {}", ::strerror(stat)));

  return stat == CmdSuccess;
}

// accounts
bool Install::root_account()
{
  const auto root_password = Widgets::get_account()->get_root_password();

  bool set{};
  if (root_password.empty())
    log_error("Root password empty");
  else
    set = set_password("root", root_password);

  return set;
}

bool Install::user_account()
{
  const auto user = Widgets::get_account()->get_user_username();
  const auto password = Widgets::get_account()->get_user_password();

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

  const auto user_sudo = Widgets::get_account()->get_user_can_sudo();
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
  log_info(std::format("Setting password for {}", user));

  return ChrootWrite{}("chpasswd", std::format("{}:{}", user, pass));
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
  const fs::path EfiConfigTargetDir{"/efi/grub"};
  const fs::path EfiConfigTarget{EfiConfigTargetDir  / "grub.cfg"};

  // TODO systemd-boot
  if (!install_packages({"grub", "efibootmgr", "os-prober"}))
  {
    log_error("Failed to install packages required for grub");
    return false;
  }

  // install
  const auto install_cmd = std::format("grub-install --target=x86_64-efi --efi-directory=/efi --boot-directory=/efi --bootloader-id=GRUB");

  if (!Chroot{}(install_cmd))
  {
    log_error("grub-install failed");
    return false;
  }

  // configure
  fs::create_directory(EfiMnt / "grub"); // outside of arch-chroot, so full path required: /mnt/efi/grub

  const auto config_cmd = std::format("grub-mkconfig -o {}", EfiConfigTarget.string());
  return Chroot{}(config_cmd);
}


// localise
bool Install::localise()
{
  static const fs::path TimezonePath{"/usr/share/zoneinfo/"};
  static const fs::path LocaleGen{"/etc/locale.gen"};
  static const fs::path LocaleConf{"/etc/locale.conf"};

  const auto& keymap = Widgets::get_localise()->get_keymap();
  const auto& zone = Widgets::get_localise()->get_timezone();
  auto locale = Widgets::get_localise()->get_locale();

  // empty command so arch-choot starts a shell
  // NOTE: appending is a bit lazy, and perhaps not obvious to future readers

  // UI returns "en_GB.UTF8" , correct entry is "en_GB.UTF8 UTF-8"
  if (!ChrootWrite{}("", std::format("echo \"{} UTF-8\" >> {}", locale, LocaleGen.string())))
    log_warning(std::format("Failed to update {}", LocaleGen.string()));
  else
  {
    if (!Chroot{}("locale-gen"))
      log_warning("locale-gen failed");
    else
    {
      // localectl wants <country>.UTF-8 , but UI returns <country>.UTF8
      locale.insert(locale.find_last_of('8'), 1, '-');

      if (!Chroot{}(std::format("localectl set-locale {}", locale)))
        log_warning("localectl failed to set locale");
    }
  }

  if (!Chroot{}(std::format("ln -sf {} /etc/localtime", (TimezonePath / zone).string())))
    log_warning("Failed to set locale timezone");

  if (!Chroot{}(std::format("localectl set-keymap {}", keymap)))
    log_warning("Failed to set virtual terminal keymap");

  return true;
}


// network
bool Install::network()
{
  static const fs::path HostnamePath{"/etc/hostname"};
  static const fs::path LiveIwdConfigPath{"/var/lib/iwd"};
  static const fs::path TargetIwdConfigPath{RootMnt / "var/lib/iwd"};

  const auto& hostname = Widgets::get_network()->hostname();
  const bool copy_conf = Widgets::get_network()->copy_config();
  const bool ntp = Widgets::get_network()->ntp();

  if (!ChrootWrite{}("", std::format("echo \"{}\" > {}", hostname, HostnamePath.string() )))
    log_warning("Failed to create /etc/hostname");

  if (copy_conf)
  {
    const auto cmd_string = std::format("mkdir -p {} && cp -r {}/* {}", TargetIwdConfigPath.string(),
                                                                        LiveIwdConfigPath.string(),
                                                                        TargetIwdConfigPath.string());

    if (ReadCommand cmd; cmd.execute(cmd_string) != CmdSuccess)
      log_warning("Failed to copy iwd config files");
  }

  if (ntp && !enable_service("systemd-timesyncd.service"))
    log_warning("Failed to enable timesync for ntp");

  return true;
}

// general
bool Install::install_packages(const std::vector<std::string>& packages)
{
  if (packages.empty())
    return true;

  std::stringstream ss;
  ss << "pacman -S --noconfirm ";
  for (const auto& package : packages)
    ss << package << ' ';

  const auto ok = Chroot{}(ss.str());
  if (!ok)
    log_error("pacman failed to install to package(s)");

  return ok;
}

bool Install::enable_service(const std::string_view name)
{
  log_info(std::format("Enabling service {}", name));

  const bool enabled = Chroot{}(std::format("systemctl enable {}", name));

  if (!enabled)
    log_error(std::format("Failed to enable service: {}", name));

  return enabled;
}
