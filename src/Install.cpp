#include <sys/mount.h>
#include <system_error>
#include <wali/Commands.hpp>
#include <wali/Install.hpp>
#include <wali/widgets/Widgets.hpp>

// Hex codes: https://gist.github.com/gotbletu/a05afe8a76d0d0e8ec6659e9194110d2
static const constexpr char PartTypeEfi[] = "ef00";
static const constexpr char PartTypeRoot[] = "8304";
static const constexpr char PartTypeHome[] = "8302";

static const fs::path RootMnt{"/mnt"};
static const fs::path EfiMnt{"/mnt/efi"};
static const fs::path HomeMnt{"/mnt/home"};
static const fs::path FsTabPath{"/mnt/etc/fstab"};


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
    const bool minimal =  exec_stage(std::bind(&Install::filesystems, std::ref(*this)), "filesystems") &&
                          exec_stage(std::bind(&Install::mount, std::ref(*this)), "mount") ;

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

  log_info(std::format("\n/boot -> {} with {}"
                       "\n/ -> {} with {}"
                       "\n/home -> {} with {}",
                       boot->get_device(), boot->get_fs(),
                       root->get_device(), root->get_fs(),
                       home->get_device(), home->get_fs()));

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
