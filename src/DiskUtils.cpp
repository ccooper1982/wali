
// #include <ali/commands.hpp>
#include <string.h>
#include <blkid/blkid.h>
#include <libmount/libmount.h>
#include <sys/mount.h>
#include <wali/DiskUtils.hpp>

const fs::path HomeMnt{"/mnt/home"};
const fs::path RootMnt{"/mnt"};
const fs::path EfiMnt{"/mnt/efi"};
const fs::path FsTabPath{"/mnt/etc/fstab"};


Partitions PartitionUtils::m_parts;

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
      ;// TODO logging // qCritical() << "probe on " << dev << " failed: " << strerror(errno) << '\n';
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
  // TODO logging // qDebug() << "Enter";

  m_parts.clear();

  const auto tree = create_tree();

  std::cout << "do_probe(): tree " << tree.size() << '\n';

  // for each disk, if partition table is GPT, read partition
  for(const auto& [disk, parts] : tree)
  {
    if (Probe probe{disk}; probe.valid())
    {
      if (auto ls = blkid_probe_get_partitions(probe.pr); ls)
      {
        const auto part_table = blkid_partlist_get_table(ls);

        if (!part_table)
          continue;

        const char * part_table_type =  blkid_parttable_get_type(part_table);
        const bool is_gpt = part_table_type ? std::string_view{part_table_type} == "gpt" : false;

        if (gpt_only && !is_gpt)
          continue;

        for (const auto& part_dev : parts)
        {
          if (opts == ProbeOpts::UnMounted && is_dev_mounted(part_dev))
            continue;

          if (auto [status, partition] = probe_partition(part_dev); status == PartitionStatus::Ok)
          {
            partition.parent_dev = disk;
            partition.is_gpt = is_gpt;

            std::cout << partition; // TODO logging

            m_parts.push_back(std::move(partition));
          }
        }
      }
    }
  }

  std::sort(m_parts.begin(), m_parts.end(), [](const Partition& a, const Partition& b)
  {
    return a.dev < b.dev;
  });

  // TODO logging // qDebug() << "Leave";
  return true;
}


PartitionUtils::Tree PartitionUtils::create_tree()
{
  Tree tree;

  auto add_tree = [&tree](const std::string& dev)
  {
    if (Probe probe {dev}; probe.valid())
    {
      if (blkid_probe_is_wholedisk(probe.pr))
      {
        if (!tree.contains(dev))
          tree.emplace(dev, std::vector<std::string>{});
      }
      else
      {
        // dev is a partition, so gets its parent device (disk) and associate in tree
        const dev_t disk_no = blkid_probe_get_wholedisk_devno(probe.pr);
        if (const char * disk_name = blkid_devno_to_devname(disk_no); disk_name)
        {
          if (!tree.contains(disk_name))
            tree.emplace(disk_name, std::vector<std::string>{});

          tree[disk_name].emplace_back(dev);
        }
      }
    }
  };


  if (blkid_cache cache; blkid_get_cache(&cache, nullptr) != 0)
  {
    std::cout << "could not create blkid cache\n";
    // TODO logging // qCritical() << "could not create blkid cache";
  }
  else
  {
    if (blkid_probe_all(cache) != 0)
    {
      std::cout << "blkid cache probe failed\n";
      // TODO logging // qCritical() << "blkid cache probe failed";
    }
    else
    {
      // iterate block devices: this can return partitions or disks
      const auto dev_it = blkid_dev_iterate_begin (cache);

      for (blkid_dev dev ; blkid_dev_next(dev_it, &dev) == 0 ;)
      {
        const auto dev_name = blkid_dev_devname(dev);
        add_tree(dev_name);
      }

      blkid_dev_iterate_end(dev_it);
    }
  }

  return tree;
}


std::tuple<PartitionStatus, Partition> PartitionUtils::probe_partition(const std::string_view part_dev)
{
  static const unsigned SectorsPerPartSize = 512;

  // TODO logging // qDebug() << "Enter: " << part_dev;

  auto make_error = []{ return std::make_pair(PartitionStatus::Error, Partition{}); };

  PartitionStatus status{PartitionStatus::None};

  Probe probe (part_dev);

  if (!probe.valid())
    return make_error();

  auto pr = probe.pr;

  // full probe required for PART_ENTRY values
  if (const int r = blkid_do_fullprobe(pr) ; r == BLKID_PROBE_ERROR)
  {
    // TODO logging // qCritical() << "fullprobe failed: " << strerror(r);
    return make_error();
  }
  else
  {
    Partition partition {.dev = std::string{part_dev}};

    if (blkid_probe_has_value(pr, "TYPE"))
    {
      const char * type {nullptr};
      blkid_probe_lookup_value(pr, "TYPE", &type, nullptr);
      partition.fs_type = type;
    }

    if (blkid_probe_has_value(pr, "PART_ENTRY_SIZE"))
    {
      const char * part_size{nullptr};
      if (blkid_probe_lookup_value(pr, "PART_ENTRY_SIZE", &part_size, nullptr); part_size)
        partition.size = SectorsPerPartSize * std::strtoull (part_size, nullptr, 10);
    }

    if (blkid_probe_has_value(pr, "PART_ENTRY_TYPE"))
    {
      const char * type_uuid{nullptr};
      blkid_probe_lookup_value(pr, "PART_ENTRY_TYPE", &type_uuid, nullptr);
      partition.type_uuid = type_uuid;
      partition.is_efi = partition.type_uuid == EfiPartitionType;
    }

    if (blkid_probe_has_value(pr, "PART_ENTRY_NUMBER"))
    {
      const char * number{nullptr};
      blkid_probe_lookup_value(pr, "PART_ENTRY_NUMBER", &number, nullptr);
      partition.part_number = static_cast<int>(std::strtol(number, nullptr, 10) & 0xFFFFFFFF);
    }

    if (partition.fs_type == "vfat")
    {
      if (blkid_probe_has_value(pr, "VERSION"))
      {
        const char * vfat_version{nullptr};
        blkid_probe_lookup_value(pr, "VERSION", &vfat_version, nullptr);
        partition.is_fat32 = (strcmp(vfat_version, "FAT32") == 0) ;
      }
      else
      {
        // TODO logging // qCritical() << "could not get vfat version";
        status = PartitionStatus::Error;
      }
    }

    if (status == PartitionStatus::None)
      return {PartitionStatus::Ok, partition};
    else
      return make_error();
  }
}


std::optional<std::reference_wrapper<const Partition>> PartitionUtils::get_partition(const std::string_view dev)
{
  const auto it = std::find_if(std::cbegin(m_parts), std::cend(m_parts), [&dev](const Partition& part)
  {
    return part.dev == dev;
  });

  if (it == std::cend(m_parts))
    return std::nullopt;
  else
    return std::ref(*it);
}


std::string PartitionUtils::get_partition_fs (const std::string_view dev)
{
  if (const auto opt = get_partition(dev) ; opt)
    return opt->get().fs_type;
  else
    return std::string{};
}


std::string PartitionUtils::get_partition_parent (const std::string_view dev)
{
  if (const auto opt = get_partition(dev) ; opt)
    return opt->get().parent_dev;
  else
    return std::string{};
}


int PartitionUtils::get_partition_part_number (const std::string_view dev)
{
  if (const auto opt = get_partition(dev) ; opt)
    return opt->get().part_number;
  else
    return 0;
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
