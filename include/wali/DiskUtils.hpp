#ifndef WALILIB_DISKUTILS_H
#define WALILIB_DISKUTILS_H

#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <wali/Common.hpp>

// partitions, mounting, filesystems
enum class PartitionStatus
{
  None,
  Ok,
  SizeSmall,
  Type,
  NotPartition,
  Error
};


struct Partition
{
  std::string dev;        // /dev/sda1, /dev/nvmen1p3, etc
  std::string parent_dev; // /dev/sda, /dev/nvmen1, etc
  std::string fs_type;    // ext4, vfat, etc
  std::string type_uuid;  // partition type UUID (useful to identify EFI)
  int64_t size{0};        // partition/filesystem size
  int part_number{0};     // partition number
  bool is_efi{false};     // if part type UUID is for EFI
  bool is_fat32{false};   // if fs_type is VFAT and version is FAT32
  bool is_gpt{false};     // if partition table is GPT
};


inline std::ostream& operator<<(std::ostream& q, const Partition& p)
{
  q << "\nPartition: " << p.dev << '\n';
  q << '\t' << "Parent: " << p.parent_dev << '\n';
  q << '\t' << "Filesystem: " << p.fs_type << '\n';
  q << '\t' << "Size: " << p.size << '\n';
  q << '\t' << "Part Type UUID: " << p.type_uuid << '\n';
  q << '\t' << "Part Num: " << p.part_number << '\n';
  q << '\t' << "EFI: " << p.is_efi << '\n';
  q << '\t' << "FAT32: " << p.is_fat32 << '\n';
  q << '\t' << "GPT: " << p.is_gpt << '\n';
  return q;
}


using Partitions = std::vector<Partition>;
using Tree = std::map<std::string, Partitions>; // parent device, partitions
using TreePair = Tree::value_type;

enum class ProbeOpts
{
  All,
  UnMounted
};

// Hex codes: https://gist.github.com/gotbletu/a05afe8a76d0d0e8ec6659e9194110d2
inline static const constexpr char PartTypeEfi[] = "ef00";
inline static const constexpr char PartTypeRoot[] = "8304";
inline static const constexpr char PartTypeHome[] = "8302";


class DiskUtils
{
public:

  // Prove all block devices that are unmounted,
  static Tree probe_for_partitioning()
  {
    return do_probe(ProbeOpts::UnMounted, false);
  }

  // Probe all block devices that are not mounted and are GPT.
  static Tree probe_for_install()
  {
    return do_probe(ProbeOpts::UnMounted, true);
  }

  // Probe all block devices, storing information for all partitions,
  // irrespective of partition type and mount state.
  // This call clears previous probe results.
  static Tree probe_for_os_discover()
  {
    return do_probe(ProbeOpts::All, false);
  }

  static std::optional<int64_t> get_disk_size (const std::string_view dev);
  static int get_partition_part_number (const Tree& tree, const std::string_view dev);
  static std::string get_partition_parent (const Tree& tree, const std::string_view dev);
  // static std::string get_partition_fs (const std::string_view dev);
  // static std::optional<int64_t> get_partition_size (const std::string_view dev);

  static bool is_path_mounted(const std::string_view path);
  static bool is_dev_mounted(const std::string_view path);

private:
  static Tree do_probe(const ProbeOpts opts, const bool gpt_only);
  static void probe_partitions(const std::string& parent_dev, Partition& part, const ProbeOpts opts, const bool gpt_only);
  static bool probe_partition(Partition& partition);

  static std::optional<std::reference_wrapper<const Partition>> get_partition(const Tree& tree, const std::string_view dev);

  static bool is_mounted(const std::string_view path_or_dev, const bool is_dev);
};

#endif
