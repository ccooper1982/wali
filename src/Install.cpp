#include "wali/Common.hpp"
#include <cstring>
#include <filesystem>
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
  m_complete = handlers.complete;
  m_log = handlers.log;

  auto exec_stage = [this](std::function<bool()> f, const std::string_view stage) mutable
  {
    log_stage_start(stage);

    bool ok{false};
    try
    {
      ok = f();
    }
    catch (const std::exception& ex)
    {
      m_complete(InstallState::Fail);
      log_stage_end(stage, StageStatus::Fail);
    }
    return ok;
  };

  try
  {
    // TODO swap, network, time (need timezones)

    // commands after fstab are done with arch-chroot
    const bool minimal =  exec_stage(std::bind(&Install::filesystems, std::ref(*this)), "Create Filesystems") &&
                          exec_stage(std::bind(&Install::mount, std::ref(*this)), "Mounting") &&
                          exec_stage(std::bind(&Install::pacstrap, std::ref(*this)), "Pacstrap") &&
                          exec_stage(std::bind(&Install::fstab, std::ref(*this)), "Generate fstab") &&
                          exec_stage(std::bind(&Install::root_account, std::ref(*this)), "Root Account") &&
                          exec_stage(std::bind(&Install::boot_loader, std::ref(*this)), "Boot loader");

    if (minimal)
    {
      m_complete(InstallState::Complete);
    }
  }
  catch (const std::exception& ex)
  {
    m_complete(InstallState::Fail);
  }
}


bool Install::filesystems()
{
  const auto boot = Widgets::get_partitions()->get_boot();
  const auto root = Widgets::get_partitions()->get_root();
  const auto home = Widgets::get_partitions()->get_home();

  log_info(std::format("/     -> {} with {}", root->get_device(), root->get_fs()));
  log_info(std::format("/boot -> {} with {}", boot->get_device(), boot->get_fs()));
  log_info(std::format("/home -> {} with {}",home->get_device(), home->get_fs()));

  bool boot_root_valid{false}, home_valid{true};

  const auto wiped_boot_root = wipe_fs(boot->get_device()) &&
                               wipe_fs(root->get_device());

  if (wiped_boot_root)
  {
    boot_root_valid = create_boot_filesystem(boot->get_device()) &&
                      create_ext4_filesystem(root->get_device());

    if (boot_root_valid)
    {
      set_partition_type(boot->get_device(), PartTypeEfi);
      set_partition_type(root->get_device(), PartTypeRoot);

      if (!home->is_home_on_root())
      {
        home_valid = wipe_fs(home->get_device()) &&
                     create_ext4_filesystem(home->get_device());

        if (home_valid)
          set_partition_type(home->get_device(), PartTypeHome);
      }
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

    if (home->get_device() != root->get_device())
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

  const int stat = ::mount(dev.data(), path.data(), fs.data(), 0, nullptr) ;

  if (stat != CmdSuccess)
    log_error(std::format("do_mount(): {} -> {} {}", path, dev, ::strerror(errno)));

  return stat == CmdSuccess;
}


// pacstrap
bool Install::pacstrap()
{
  static const std::set<std::string> Packages =
  {
    "base",
    "usb_modeswitch", // for usb devices that can switch modes (recharging/something else)
    "usbmuxd",
    "usbutils",
    "reflector",      // pacman mirrors list
    "dmidecode",      // BIOS / hardware info, may not actually be useful for most users
    "e2fsprogs",      // useful
    "gpm",            // laptop touchpad support
    "less",           // useful
    "linux"
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

bool Install::set_password(const std::string_view user, const std::string_view pass)
{
  log_info(std::format("Setting password for {}", user));

  ChrootWrite cmd;
  return cmd("chpasswd", std::format("{}:{}", user, pass));
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
