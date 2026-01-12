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


// Tree DiskUtils::m_tree;

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


Tree DiskUtils::do_probe(const ProbeOpts opts, const bool gpt_only)
{
  static const fs::path DevSeq {"/dev/disk/by-diskseq"};

  std::map<int, fs::path> seq_path_map; // i.e. {1, /dev/sda}, {2, /dev/sdb}

  // returns: {<is_whole_disk>, <sequence_number>}
  auto get_info = [](const std::string_view entry) -> std::optional<std::pair<bool,int>>
  {
    const auto delim = entry.find('-');
    const auto is_disk = delim == std::string_view::npos;
    int seq = is_disk ? std::stol(entry.substr(0,delim).data()) : std::stol(entry.data());

    if (is_disk)
      return {{true, seq}};
    else if (entry.contains("part"))
      return {{false,seq}};
    else
      return {};
  };

  Tree tree;

  for(const auto& sequence : fs::directory_iterator{DevSeq})
  {
    const auto target = fs::weakly_canonical(sequence.path());
    const auto seq_opt = get_info(sequence.path().stem().string());

    if (!fs::exists(target) || !seq_opt || !sequence.is_block_file())
      continue;

    if (const auto [is_disk, seq] = *seq_opt; is_disk)
    {
      seq_path_map[seq] = target;
      tree.emplace(target.string(), std::vector<Partition>{});
    }
  }

  for(const auto& sequence : fs::directory_iterator{DevSeq})
  {
    const auto target = fs::weakly_canonical(sequence.path());
    const auto seq_opt = get_info(sequence.path().stem().string());

    if (!fs::exists(target) || !seq_opt || !sequence.is_block_file())
      continue;

    const auto [is_disk, seq] = *seq_opt;
    const auto disk_dev = seq_path_map.at(seq);

    if (!is_disk)
      tree[disk_dev].emplace_back(Partition{.dev = target});
  }

  for (auto& [disk, parts] : tree)
  {
    for(auto& part : parts)
      probe_partitions(disk, part, opts, gpt_only);

    std::ranges::sort(parts, [](const Partition&a, const Partition& b){ return a.dev < b.dev; });
  }

  return tree;
}


void DiskUtils::probe_partitions(const std::string& parent_dev, Partition& part, const ProbeOpts opts, const bool gpt_only)
{
  const auto& dev = part.dev;

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

  if (probe_partition(part))
  {
    part.parent_dev = parent_dev;
    part.is_gpt = is_gpt;
  }
  else
  {
    PLOGE << "Probe partition failed: " << dev;
  }
}


bool DiskUtils::probe_partition(Partition& partition)
{
  static const unsigned SectorsPerPartSize = 512;

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


// std::string DiskUtils::get_partition_fs (const std::string_view dev)
// {
//   const auto opt = get_partition(dev);
//   return opt ? opt->get().fs_type : std::string{};
// }


std::string DiskUtils::get_partition_parent (const Tree& tree, const std::string_view dev)
{
  const auto opt = get_partition(tree, dev);
  return opt ? opt->get().parent_dev : std::string{};
}


int DiskUtils::get_partition_part_number (const Tree& tree, const std::string_view dev)
{
  const auto opt = get_partition(tree, dev);
  return opt ? opt->get().part_number : 0;
}


// std::optional<int64_t> DiskUtils::get_partition_size (const std::string_view dev)
// {
//   if (const auto opt = get_partition(dev); opt)
//     return opt->get().size;

//   return {};
// }


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
