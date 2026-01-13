#ifndef WALILIB_DISKUTILS_H
#define WALILIB_DISKUTILS_H

#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <wali/Common.hpp>


struct Disk
{
  Disk(const std::string& dev) : dev(dev)
  {

  }

  std::string dev;
  int64_t size{};
  bool is_gpt{};
};

inline bool operator<(const Disk& a, const Disk& b)
{
    return a.dev < b.dev;
}

struct Partition
{
  std::string dev;        // /dev/sda1, /dev/nvme0n1p3, etc
  std::string fs_type;    // ext4, vfat, etc
  std::string type_uuid;  // partition type UUID (useful to identify EFI)
  int64_t size{};
  int part_number{};      // partition number
  bool is_efi{};          // if part type UUID is for EFI
  bool is_fat32{};        // if fs_type is VFAT and version is FAT32
  bool is_mounted{};
};


inline std::ostream& operator<<(std::ostream& q, const Partition& p)
{
  q << "\nPartition: " << p.dev << '\n';
  // q << '\t' << "Parent: " << p.parent_dev << '\n';
  q << '\t' << "Filesystem: " << p.fs_type << '\n';
  q << '\t' << "Size: " << p.size << '\n';
  q << '\t' << "Part Type UUID: " << p.type_uuid << '\n';
  q << '\t' << "Part Num: " << p.part_number << '\n';
  q << '\t' << "EFI: " << p.is_efi << '\n';
  q << '\t' << "FAT32: " << p.is_fat32 << '\n';
  return q;
}


using Partitions = std::vector<Partition>;
using Tree = std::map<Disk, Partitions>;
using TreePair = Tree::value_type;


// Hex codes: https://gist.github.com/gotbletu/a05afe8a76d0d0e8ec6659e9194110d2
inline static const constexpr char PartTypeEfi[] = "ef00";
inline static const constexpr char PartTypeRoot[] = "8304";
inline static const constexpr char PartTypeHome[] = "8302";


class DiskUtils
{
public:

  static Tree probe() { return do_probe(); }

  static std::optional<int64_t> get_disk_size (const std::string_view dev);
  static int get_partition_part_number (const Tree& tree, const std::string_view dev);
  static std::string get_partition_disk (const Tree& tree, const std::string_view dev);
  // static std::string get_partition_fs (const std::string_view dev);
  // static std::optional<int64_t> get_partition_size (const std::string_view dev);

  static bool is_path_mounted(const std::string_view path);
  static bool is_dev_mounted(const std::string_view path);

private:
  static Tree do_probe();
  static void probe_disk(Disk& disk);
  static bool probe_partition(Partition& partition);

  static std::optional<std::reference_wrapper<const Partition>> get_partition(const Tree& tree, const std::string_view dev);

  static bool is_mounted(const std::string_view path_or_dev, const bool is_dev);
};

#endif
