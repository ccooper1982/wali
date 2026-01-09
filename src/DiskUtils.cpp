#include <exception>
#include <functional>
#include <ranges>
#include <string.h>
#include <blkid/blkid.h>
#include <libmount/libmount.h>
#include <sys/mount.h>
#include <plog/Log.h>
#include <wali/DiskUtils.hpp>


Tree PartitionUtils::m_tree;

static const std::string EfiPartitionType {"c12a7328-f81f-11d2-ba4b-00a0c93ec93b"};


struct Probe
{
  Probe(const std::string_view dev,
        const int part_flags = BLKID_PARTS_ENTRY_DETAILS,
        const int super_block_flags = BLKID_SUBLKS_VERSION | BLKID_SUBLKS_FSINFO | BLKID_SUBLKS_TYPE)
  {
    if (pr = blkid_new_probe_from_filename(dev.data()); pr)
    {
      blkid_probe_enable_partitions(pr, 1);
      blkid_probe_set_partitions_flags(pr, part_flags);

      blkid_probe_enable_superblocks(pr, 1);
      blkid_probe_set_superblocks_flags(pr, super_block_flags);
    }
    else
      PLOGE << "Probe on " << dev << " failed: " << strerror(errno) << '\n';
  }

  ~Probe()
  {
    if (pr)
      blkid_free_probe(pr);
  }

  bool valid() const
  {
    return pr != nullptr;
  }

  blkid_probe pr{nullptr};
};


bool PartitionUtils::do_probe(const ProbeOpts opts, const bool gpt_only)
{
  blkid_cache cache;
  if (blkid_get_cache(&cache, nullptr) != 0)
  {
    PLOGE << "Could not create blkid cache";
    return false;
  }

  if (blkid_probe_all(cache) != 0)
  {
    PLOGE << "blkid cache probe failed";
    return false;
  }

  m_tree.clear();

  try
  {
    const auto dev_it = blkid_dev_iterate_begin (cache);

    for (blkid_dev dev ; blkid_dev_next(dev_it, &dev) == 0 ;)
    {
      const auto dev_name = blkid_dev_devname(dev);
      const auto parent = add_to_tree(dev_name);

      if (!parent) // device is whole disk (a parent)
        continue;

      probe_partitions(*parent, dev_name, opts, gpt_only);

      std::sort(std::begin(m_tree.at(*parent)), std::end(m_tree.at(*parent)), [](const Partition& a, const Partition& b)
      {
        return a.dev < b.dev;
      });
    }

    blkid_dev_iterate_end(dev_it);
  }
  catch (const std::exception& ex)
  {
    PLOGE << "do_probe(): " << ex.what();
    return false;
  }

  return true;
}

void PartitionUtils::probe_partitions(const std::string& parent_dev, const std::string_view dev, const ProbeOpts opts, const bool gpt_only)
{
  Probe probe{parent_dev};

  if (!probe.valid())
  {
    PLOGE << "Probe invalid for: " << dev;
    return;
  }

  blkid_partlist ls;
  if (ls = blkid_probe_get_partitions(probe.pr); ls == nullptr)
  {
    PLOGE << "Get partitions failed for: " << dev;
    return;
  }

  const auto part_table = blkid_partlist_get_table(ls);

  if (!part_table)
  {
    PLOGE << "Failed to get partition table: " << dev;
    return;
  }

  const char * part_table_type =  blkid_parttable_get_type(part_table);
  const bool is_gpt = part_table_type ? std::string_view{part_table_type} == "gpt" : false;

  if ((gpt_only && !is_gpt) || (opts == ProbeOpts::UnMounted && is_dev_mounted(dev)))
    return;

  if (auto partition = probe_partition(dev); partition)
  {
    partition->parent_dev = parent_dev;
    partition->is_gpt = is_gpt;
    m_tree.at(parent_dev).push_back(std::move(*partition));
  }
  else
  {
    PLOGE << "Probe partition failed: " << dev;
  }
}


// If dev is a disk, adds to tree.
// If dev is a partition, finds the parent disk, and adds to tree
std::optional<std::string> PartitionUtils::add_to_tree(const std::string& dev)
{
  std::optional<std::string> parent;

  if (Probe probe {dev}; probe.valid())
  {
    if (blkid_probe_is_wholedisk(probe.pr) && !m_tree.contains(dev))
      m_tree.emplace(dev, Partitions{});
    else
    {
      // dev is a partition: add parent to tree if not present
      const dev_t disk_no = blkid_probe_get_wholedisk_devno(probe.pr);
      const char * parent_dev = blkid_devno_to_devname(disk_no);

      if (parent_dev)
      {
        parent = parent_dev;

        if (!m_tree.contains(parent_dev))
          m_tree.emplace(parent_dev, Partitions{});
      }
    }
  }
  return parent;
}


std::optional<Partition> PartitionUtils::probe_partition(const std::string_view part_dev)
{
  static const unsigned SectorsPerPartSize = 512;

  Probe probe (part_dev);

  if (!probe.valid())
  {
    PLOGE << "probe_partition(): probe failed for: " << part_dev;
    return {};
  }


  auto pr = probe.pr;

  // full probe required for PART_ENTRY values
  if (const int r = blkid_do_fullprobe(pr) ; r == BLKID_PROBE_ERROR)
  {
    PLOGE << "Fullprobe failed: " << strerror(r);
    return {};
  }

  auto get_value = [pr, part_dev](const char * name, std::function<void(const char *)> func)
  {
    if (blkid_probe_has_value(pr, name))
    {
      const char * value{};
      if (blkid_probe_lookup_value(pr, name, &value, nullptr); value)
        func(value);
      else
        PLOGE << "probe_partition(): value lookup failed for " << name;
    }
    else
      PLOGE << "probe_partition(): " << part_dev << " does not have " << name;
  };


  Partition partition {.dev = std::string{part_dev}};

  get_value("TYPE", [&partition](const char * value)
  {
    partition.fs_type = value;
  });
  get_value("PART_ENTRY_SIZE", [&partition](const char * value)
  {
    partition.size = SectorsPerPartSize * std::strtoull (value, nullptr, 10);
  });
  get_value("PART_ENTRY_TYPE", [&partition](const char * value)
  {
    partition.type_uuid = value;
    partition.is_efi = partition.type_uuid == EfiPartitionType;
  });
  get_value("PART_ENTRY_NUMBER", [&partition](const char * value)
  {
    partition.part_number = static_cast<int>(std::strtol(value, nullptr, 10) & 0xFFFFFFFF);
  });

  if (partition.fs_type == "vfat")
  {
    get_value("VERSION", [&partition](const char * value)
    {
      partition.is_fat32 = (strcmp(value, "FAT32") == 0) ;
    });
  }

  return partition;
}


// TODO do we have std::optional<T&> ?
std::optional<std::reference_wrapper<const Partition>> PartitionUtils::get_partition(const std::string_view dev)
{
  for (const auto& parts : m_tree | std::views::values)
  {
    const auto it = std::ranges::find_if(parts, [&](const Partition& part){ return part.dev == dev; });
    if (it != std::ranges::cend(parts))
      return *it;
  }

  return std::nullopt;
}


std::string PartitionUtils::get_partition_fs (const std::string_view dev)
{
  const auto opt = get_partition(dev);
  return opt ? opt->get().fs_type : std::string{};
}


std::string PartitionUtils::get_partition_parent (const std::string_view dev)
{
  const auto opt = get_partition(dev);
  return opt ? opt->get().parent_dev : std::string{};
}


int PartitionUtils::get_partition_part_number (const std::string_view dev)
{
  const auto opt = get_partition(dev);
  return opt ? opt->get().part_number : 0;
}


bool PartitionUtils::is_mounted(const std::string_view path_or_dev, const bool is_dev)
{
  bool mounted = false;

  if (auto table = mnt_new_table(); table)
  {
    if (mnt_table_parse_mtab(table, nullptr) == 0)
    {
      mounted = is_dev ? mnt_table_find_source(table, path_or_dev.data(), MNT_ITER_FORWARD) != nullptr :
                         mnt_table_find_target(table, path_or_dev.data(), MNT_ITER_FORWARD) != nullptr ;
    }

    mnt_free_table(table);
  }

  return mounted;
}


bool PartitionUtils::is_path_mounted(const std::string_view path)
{
  return is_mounted(path, false);
}


bool PartitionUtils::is_dev_mounted(const std::string_view path)
{
  return is_mounted(path, true);
}
