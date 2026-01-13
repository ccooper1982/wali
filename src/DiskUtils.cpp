#include <algorithm>
#include <exception>
#include <filesystem>
#include <functional>
#include <optional>
#include <ranges>
#include <string.h>
#include <string>
#include <unistd.h>
#include <blkid/blkid.h>
#include <libmount/libmount.h>
#include <sys/mount.h>
#include <plog/Log.h>
#include <wali/DiskUtils.hpp>
#include <wali/Common.hpp>


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


Tree DiskUtils::do_probe()
{
  static const fs::path DevSeq {"/dev/disk/by-diskseq"};

  std::map<int, std::string> seq_path_map; // i.e. {1, /dev/sda}, {2, /dev/sdb}

  auto is_block_disk = [](const fs::directory_entry& dir_entry)
  {
    const auto& entry = dir_entry.path().stem().string();
    const auto delim = entry.find('-');
    return dir_entry.is_block_file() && delim == std::string_view::npos;
  };

  auto get_seq = [](const fs::directory_entry& dir_entry)
  {
    const auto& entry = dir_entry.path().stem().string();
    const auto delim = entry.find('-');
    return delim == std::string_view::npos ? std::stol(entry.data()) : std::stol(entry.substr(0,delim).data());
  };

  auto is_partition = [](const fs::directory_entry& dir_entry)
  {
    const auto& entry = dir_entry.path().stem().string();
    return entry.find('-') != std::string_view::npos;
  };

  Tree tree;

  for (const auto& entry : fs::directory_iterator{DevSeq} | std::views::filter(is_block_disk))
  {
    const auto target = fs::weakly_canonical(entry.path());
    const auto seq = get_seq(entry);

    if (!fs::exists(target))
      continue;

    Disk disk{target};
    probe_disk(disk);

    tree[disk] = std::vector<Partition>{};
    seq_path_map[seq] = disk.dev;
  }

  for (const auto& entry : fs::directory_iterator{DevSeq} | std::views::filter(is_partition))
  {
    const auto target = fs::weakly_canonical(entry.path());
    const auto seq = get_seq(entry);
    const auto disk_dev = seq_path_map.at(seq);

    Partition part { .dev = target, .is_mounted = is_dev_mounted(target.string()) };

    probe_partition(part);

    tree[disk_dev].push_back(std::move(part));
  }

  for (auto& [disk, parts] : tree)
    std::ranges::sort(parts, [](const Partition&a, const Partition& b){ return a.dev < b.dev; });

  return tree;
}


void DiskUtils::probe_disk(Disk& disk)
{
  Probe probe{disk.dev};

  if (!probe.valid())
  {
    PLOGE << "Probe invalid for: " << disk.dev;
    return;
  }

  blkid_partlist ls;
  if (ls = blkid_probe_get_partitions(probe.pr); ls == nullptr)
  {
    // warning because /dev/loop0 has no partition table
    PLOGW << "probe_disk failed for: " << disk.dev;
    return;
  }

  const auto part_table = blkid_partlist_get_table(ls);

  if (!part_table)
  {
    // this a warning because /dev/loop0 doesn't have partitions
    PLOGW << "Failed to get partition table: " << disk.dev;
    return;
  }

  const char * part_table_type =  blkid_parttable_get_type(part_table);
  disk.is_gpt = part_table_type ? std::string_view{part_table_type} == "gpt" : false;

  if (const auto size_opt = get_disk_size(disk.dev); size_opt)
    disk.size = *size_opt;
}


bool DiskUtils::probe_partition(Partition& partition)
{
  static const unsigned SectorsPerPartSize = 512;

  PLOGW << "probe_partition() " << partition.dev;

  Probe probe (partition.dev);

  if (!probe.valid())
  {
    PLOGE << "probe_partition(): probe failed for: " << partition.dev;
    return false;
  }

  auto pr = probe.pr;

  // full probe required for PART_ENTRY values
  if (const int r = blkid_do_fullprobe(pr) ; r == BLKID_PROBE_ERROR)
  {
    PLOGE << "Fullprobe failed: " << strerror(r);
    return false;
  }

  auto get_value = [pr, dev=partition.dev](const char * name, std::function<void(const char *)> func, const bool required = true)
  {
    const char * value{};
    if (blkid_probe_has_value(pr, name))
    {
      if (blkid_probe_lookup_value(pr, name, &value, nullptr); value)
        func(value);
    }

    if (!value && required)
    {
      PLOGE << "probe_partition(): " << dev << " does not have " << name;
    }
  };

  get_value("TYPE", [&partition, get_value](const char * value)
  {
    partition.fs_type = value;

    if (partition.fs_type == "vfat")
    {
      get_value("VERSION", [&partition](const char * value)
      {
        partition.is_fat32 = (strcmp(value, "FAT32") == 0) ;
      });
    }
  }, false);

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

  return true;
}


std::optional<int64_t> DiskUtils::get_disk_size (const std::string_view dev)
{
  if (int fd = open(dev.data(), O_RDONLY); fd != -1)
  {
    int64_t size = blkid_get_dev_size(fd);
    close(fd);
    return size;
  }
  return {};
}

// TODO do we have std::optional<T&> ?
std::optional<std::reference_wrapper<const Partition>> DiskUtils::get_partition(const Tree& tree, const std::string_view dev)
{
  for (const auto& parts : tree | std::views::values)
  {
    const auto it = std::ranges::find_if(parts, [&](const Partition& part){ return part.dev == dev; });
    if (it != std::ranges::cend(parts))
      return *it;
  }

  return std::nullopt;
}


std::string DiskUtils::get_partition_disk (const Tree& tree, const std::string_view dev)
{
  for (const auto& [disk, parts] : tree)
  {
    auto part_view = parts | std::views::filter([dev](const Partition& part){ return part.dev == dev; });
    if (part_view)
      return disk.dev;
  }

  return {};
}


int DiskUtils::get_partition_part_number (const Tree& tree, const std::string_view dev)
{
  const auto opt = get_partition(tree, dev);
  return opt ? opt->get().part_number : 0;
}


bool DiskUtils::is_mounted(const std::string_view path_or_dev, const bool is_dev)
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


bool DiskUtils::is_path_mounted(const std::string_view path)
{
  return is_mounted(path, false);
}


bool DiskUtils::is_dev_mounted(const std::string_view path)
{
  return is_mounted(path, true);
}
