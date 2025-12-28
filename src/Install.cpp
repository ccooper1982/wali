#include <wali/Commands.hpp>
#include <wali/Install.hpp>
#include <wali/widgets/Widgets.hpp>

// Hex codes: https://gist.github.com/gotbletu/a05afe8a76d0d0e8ec6659e9194110d2
static const constexpr char PartTypeEfi[] = "ef00";
static const constexpr char PartTypeRoot[] = "8304";
static const constexpr char PartTypeHome[] = "8302";

void Install::install(Handlers&& handlers)
{
  m_state_change = handlers.state_change;
  m_complete = handlers.complete;
  m_log = handlers.log;

  auto exec_stage = [this](std::function<StageStatus()> f, const std::string_view stage) mutable
  {
    log_stage_start(stage);

    const StageStatus status = f();

    log_stage_end(stage, status);
    return status == StageStatus::Ok;
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


StageStatus Install::filesystems()
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
      }

      if (home_valid)
        set_partition_type(home->get_device(), PartTypeHome);
    }
  }

  return boot_root_valid && home_valid ? StageStatus::Ok : StageStatus::Fail;
}

bool Install::wipe_fs(const std::string_view dev)
{
  log_info(std::format("Wiping filesystem on {}", dev));
  ReadCommand wipe;
  return wipe.execute(std::format ("wipefs -a -f {}", dev)) == CmdSuccess;
}

bool Install::create_boot_filesystem(const std::string_view part_dev)
{
  log_info(std::format("Creating boot on {}", part_dev));
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

// utils

StageStatus Install::mount()
{
  return StageStatus::Ok;
}
