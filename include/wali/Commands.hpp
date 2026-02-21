#ifndef WALI_COMMANDS_H
#define WALI_COMMANDS_H

#include <algorithm>
#include <cctype>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwctype>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <ios>
#include <optional>
#include <ranges>
#include <sstream>
#include <stop_token>
#include <string>
#include <string_view>

#include <plog/Log.h>

#include <thread>
#include <wali/Common.hpp>

using OutputHandler = std::function<void(const std::string_view)>;

inline const int CmdSuccess = 0;
inline const int CmdFail = -1;

class Command
{
protected:

  Command() = default;

  virtual ~Command()
  {
    close();
  }

protected:
  int close()
  {
    int r = CmdSuccess;

    if (m_fd)
    {
      r = ::pclose(m_fd);
      m_fd = nullptr;
    }

    return r;
  }

protected:
  FILE * m_fd{nullptr};
};

struct ReadCommand : public Command
{
  // static int execute_line (const std::string_view cmd, OutputHandler&& handler)
  // {
  //   ReadCommand rc;
  //   return execute(cmd, std::move(handler), 1);
  // }

  static int execute(const std::string_view cmd)
  {
    return execute(cmd, nullptr, -1);
  }

  static int execute (const std::string_view cmd, OutputHandler&& handler, const int max_lines = -1)
  {
    ReadCommand rc;
    return rc.do_execute(cmd, std::move(handler), max_lines);
  }

  static int execute (const std::string_view cmd, std::string_view exec, std::stop_token tkn, OutputHandler&& handler)
  {
    auto get_exec_name = [&]()
    {
      const auto delim = cmd.find(' ');
      return delim == std::string_view::npos ? cmd : cmd.substr(0, delim);
    };

    ReadCommand rc;
    rc.set_executable(exec.empty() ? get_exec_name() : exec);
    return rc.do_execute(cmd, std::move(handler), -1, tkn);
  }

  void interrupt()
  {
    // if (m_pid > 1)
    // {
    //   PLOGW << "Killing " << m_pid;
    //   if (const auto st = kill(m_pid, SIGKILL); st != CmdSuccess)
    //   {
    //     PLOGE << "Kill failed: " << ::strerror(st);
    //   }
    // }
    if (!m_executable.empty())
    {
      if (auto fd = ::popen(std::format("pkill {}", m_executable).c_str(), "r") ; fd)
      {
        if (::pclose(fd) != 0)
        {
          PLOGE << "Failed to kill: " << m_executable;
        }
      }
    }
  }

private:

  void set_executable(const std::string_view exec)
  {
    m_executable = exec;
  }

  // void get_pid()
  // {
  //   PLOGE << "get_pid() looking for process: " << m_executable;

  //   auto pidof = [this] -> bool
  //   {
  //     PLOGW << "Opening pidof";

  //     if (auto fd = ::popen(std::format("pidof {}", m_executable).c_str(), "r") ; fd)
  //     {
  //       char buff[13];
  //       char * pid = std::fgets(buff, sizeof(buff), fd);

  //       if (pid == nullptr)
  //         PLOGW << "pid nullptr";
  //       else if (strlen(pid))
  //         PLOGW << "pid = " << pid;
  //       else
  //         PLOGW << "pid empty";

  //       // if (char * pid = std::fgets(buff, sizeof(buff), fd); pid != nullptr && strlen(pid))
  //       if (pid != nullptr && strlen(pid))
  //       {
  //         PLOGW << "Actually found the lad";
  //         m_pid = std::strtol(buff, nullptr, 10);
  //       }
  //       ::pclose(fd);

  //       return m_pid != -1;
  //     }

  //     return false;
  //   };

  //   static const int MaxAttempts = 10;

  //   for (int i = 0 ; !pidof() && i < MaxAttempts ; ++i)
  //   {
  //     std::this_thread::sleep_for(std::chrono::milliseconds{100});
  //   }

  //   if (m_pid == -1)
  //   {
  //     PLOGE << "Failed to get PID for " << m_executable;
  //   }
  // }

  // open pipe to cmd and call handler function for each line or when buffer is full.
  // - if handler can be nullptr, no point is reported
  // - if stop_token is set, the command is interruptable
  //
  // stderr is redirected to stdout (pacstrap, and perhaps others, output errors to stderr)
  int do_execute (const std::string_view cmd, OutputHandler&& handler, const int max_lines, std::stop_token tkn = std::stop_token{})
  {
    if (cmd.empty())
      return CmdSuccess;
    else if (m_fd = ::popen(std::format("{} 2>&1", cmd).data(), "r"); !m_fd)
    {
      PLOGE << "Failed to open pipe to: " << cmd;
      return CmdFail;
    }

    int stat{CmdFail};

    char buff[4096];
    int n_lines{0};
    size_t n_chars{0};
    bool interrupted{};

    // don't get PID unless command can be interrupted
    // if (tkn.stop_possible())
    //   get_pid();

    for (char c ; ((c = fgetc(m_fd)) != EOF) ; )
    {
      if (interrupted = tkn.stop_possible() && tkn.stop_requested(); interrupted)
      {
        PLOGW << "Command read interrupted";
        break;
      }

      if (!handler)
        continue;

      if (c == '\n' || n_chars == sizeof(buff))
      {
        handler(std::string_view {buff, n_chars});

        if (++n_lines == max_lines)
          break;

        n_chars = 0;
      }
      else
        buff[n_chars++] = c;
    }

    if (interrupted)
      interrupt();
    else if (handler && n_chars)
      handler(std::string_view {buff, n_chars});

    stat = close();

    if (stat != CmdSuccess)
    {
      PLOGE << "Command '" << cmd << "' exited with: " << ::strerror(stat);
    }

    return stat;
  }

private:
  int m_pid{-1}; // 0 is the idle process, so -1 seems sensible
  std::string m_executable;
};

// Opens a pipe to cmd then writes input to the process.
class WriteCommand : public Command
{
public:
  int execute (const std::string_view cmd, const std::string_view input)
  {
    if (input.empty())
      return CmdSuccess;

    if (m_fd = ::popen(cmd.data(), "w"); m_fd)
    {
      fputs(std::format("{};", input).c_str(), m_fd);
      // fputs("exit;", m_fd);
      return close();
    }
    return CmdFail;
  }
};


// chroot
struct Chroot : public ReadCommand
{
  virtual bool operator()(const std::string_view cmd)
  {
    return operator()(cmd, [](const std::string_view m) { PLOGI << m; });
  }

  virtual bool operator()(const std::string_view cmd, OutputHandler handler)
  {
    const auto chroot_cmd = std::format("arch-chroot {} {}", RootMnt.string(), cmd);
    return execute(chroot_cmd, std::move(handler)) == CmdSuccess;
  }
};

struct ChrootWrite : public ReadCommand
{
  bool operator()(const std::string_view input)
  {
    // use process substitution because:
    //  - with: `(command1) | (command2)`
    //  - the webserver would not shutdown with ctrc+c, because command2 did not exit, even when command1 exited
    //  - So instead, call /bin/bash within chroot so it treats the command1 as a schell script
    const auto cmd = std::format("arch-chroot {} /bin/bash < <(echo \"{}\")", RootMnt.string(), input);
    return execute(cmd, [](const std::string_view m) { PLOGI << m; }) == CmdSuccess;
  }
};

// localisation
struct GetTimeZones : public ReadCommand
{
  std::vector<std::string> operator()()
  {
    std::vector<std::string> zones;
    zones.reserve(600); // "timedatectl list-timezones | wc -l" returns 598

    const auto stat = execute("timedatectl list-timezones", [&](const std::string_view line)
    {
      if (!line.empty())
        zones.emplace_back(line);
    });

    return stat == CmdSuccess ? zones : std::vector<std::string>{};
  }
};


struct GetLocales
{
  std::vector<std::string> operator()()
  {
    static const fs::path SupportedLocalesPath {"/usr/share/i18n/SUPPORTED"};

    std::vector<std::string> locales;
    locales.reserve(500); // "cat /usr/share/i18n/SUPPORTED | wc -l" returns 500

    char buff[512] = {'\0'};
    std::ifstream stream{SupportedLocalesPath};

    while (stream.getline(buff, sizeof(buff)).good())
    {
      auto is_utf8 = [buff]() -> std::optional<std::string_view>
      {
        const std::string_view line{buff};
        const auto space = line.find(' ');

        if (space == std::string_view::npos || space+1 == line.size())
          return {};

        if (const auto charset = line.substr(space+1); charset == "UTF-8")
          return line.substr(0, space);
        else
          return {};
      };

      if (const auto locale = is_utf8(); locale)
        locales.emplace_back(*locale);
    }

    return locales;
  }
};


struct GetKeyMaps : public ReadCommand
{
  std::vector<std::string> operator()()
  {
    std::vector<std::string> keys;

    const auto stat = execute("localectl list-keymaps",[&](const std::string_view line)
    {
      if (!line.empty())
        keys.emplace_back(line);
    });

    return stat == CmdSuccess ? keys : std::vector<std::string>{};
  }
};


struct Reboot : public ReadCommand
{
  bool operator()()
  {
    return execute("reboot -i --no-wall") == CmdSuccess;
  }
};


struct GetGpuVendor : public ReadCommand
{
  GpuVendor vendor{GpuVendor::Unknown};
  unsigned short n_amd{0}, n_nvidia{0}, n_vm{0}, n_intel{0};

  GpuVendor operator()()
  {
    const auto stat = execute("lspci | grep -E 'VGA|Display'", [&](const std::string_view m)
    {
      if (!m.empty())
      {
        n_amd += ::strcasestr(m.data(), "amd") ? 1 : 0;
        n_nvidia += ::strcasestr(m.data(), "nvidia") ? 1 : 0;
        n_vm += ::strcasestr(m.data(), "vmware") ? 1 : 0;
        n_intel += ::strcasestr(m.data(), "intel") ? 1 : 0;
      }
    });

    if (stat == CmdSuccess && n_amd + n_nvidia + n_vm + n_intel == 1)
    {
      if (n_amd)
        vendor = GpuVendor::Amd;
      else if (n_nvidia)
        vendor = GpuVendor::Nvidia;
      else if (n_intel)
        vendor = GpuVendor::Intel;
      else
        vendor = GpuVendor::Vm;
    }

    return vendor;
  }
};

// filesystems

struct CreateFilesystem : public ReadCommand
{
  static bool vfat32 (const std::string_view dev)
  {
    return CreateFilesystem{}("vfat -F 32", dev);
  }

  static bool ext4 (const std::string_view dev)
  {
    return CreateFilesystem{}("ext4", dev);
  }

  static bool btrfs (const std::string_view dev)
  {
    // actually: n=max(16k, page_size)
    return CreateFilesystem{}("btrfs -n 16k", dev);
  }

private:
  bool operator()(const std::string_view mkfs, const std::string_view dev)
  {
    return execute(std::format("mkfs.{} {}", mkfs, dev)) == CmdSuccess;
  }
};


struct CreateBtrfsSubVolume : public ReadCommand
{
  bool operator()(const std::string_view path)
  {
    const auto stat = execute(std::format("btrfs subvolume create {}", path), [](const std::string_view m)
    {
      PLOGI << m;
    });

    if (stat != CmdSuccess)
    {
      PLOGE << "Failed to create btrfs subvolume";
    }
    return stat == CmdSuccess;
  }
};

// partitions
struct ClearPartitions : public ReadCommand
{
  bool operator()(const std::string_view dev)
  {
    const auto ok = execute(std::format("sgdisk --clear {}", dev)) == CmdSuccess;
    if (!ok)
    {
      PLOGE << "Failed to delete partitions: " << dev;
    }
    return ok;
  }
};

struct CreatePartitionTable : public ReadCommand
{
  bool operator()(const std::string_view disk, OutputHandler handler)
  {
    ClearPartitions{}(disk);

    // this forces the label creation, whilst sgdisk --label does not
    return execute(std::format("echo 'label: gpt' | sfdisk {}", disk), std::move(handler)) == CmdSuccess;
  }
};

struct CreatePartition : public ReadCommand
{
  bool operator()(const std::string_view disk, const std::uint16_t part_num, const std::int64_t size_mb, OutputHandler handler)
  {
    // <part_num>:start:<part_size>  (where start is default=first available sector)
    return execute(std::format("sgdisk -n {}:-:+{}MiB {}", part_num, size_mb, disk), std::move(handler)) == CmdSuccess;
  }

  bool operator()(const std::string_view disk, const std::uint16_t part_num, OutputHandler handler)
  {
    // note: sgdisk on Arch returns 0 for success (https://man.archlinux.org/man/sgdisk.8.en),
    //       whilst other sources say 1 is success (https://linux.die.net/man/8/sgdisk)
    // <part_num>:start:<part_size>
    //  start is default (first available sector)
    //  part_size is default (remaining space)
    return execute(std::format("sgdisk -n {}:-:- {}", part_num, disk), std::move(handler)) == CmdSuccess;
  }
};


struct SetPartitionType : public ReadCommand
{
  bool operator()(const std::string_view dev, const std::uint16_t part_num, const std::string_view type)
  {
    return execute(std::format("sgdisk -t {}:{} {}", part_num, type, dev)) == CmdSuccess;
  }
};

// mount
struct Mount : public ReadCommand
{
  bool operator()(const std::string_view dev, const std::string_view path, const std::string_view opts = "")
  {
    const auto opts_str = opts.empty() ? "" : std::format("-o {}", opts);
    return execute(std::format("mount {} --mkdir {} {}", opts_str, dev, path)) == CmdSuccess;
  }
};

struct Unmount : public ReadCommand
{
  bool operator()(const std::string_view mount_point, const bool recursive)
  {
    return execute(std::format("umount {} {}", recursive ? "-R" : "", mount_point)) == CmdSuccess;
  }
};

// general
struct PlatformSizeValid : public ReadCommand
{
  bool operator()()
  {
    static const std::filesystem::path file {"/sys/firmware/efi/fw_platform_size"};

    int size{0};

    if (std::filesystem::exists(file) && std::filesystem::file_size(file))
    {
      std::string str;
      // should only ever be one line in this file
      std::ifstream stream{file};
      if (std::getline(stream, str); !str.empty())
        size = std::stoi(str);
    }

    return size == 64;
  }
};


struct GetCpuVendor : public ReadCommand
{
  CpuVendor operator()()
  {
    CpuVendor vendor = CpuVendor::None;

    const auto stat = execute("cat /proc/cpuinfo | grep \"^model name\"", [&vendor](const std::string_view line)
    {
      if (line.find("AMD") != std::string::npos)
        vendor = CpuVendor::Amd;
      else if (line.find("Intel") != std::string::npos)
        vendor = CpuVendor::Intel;
    }, 1);

    return stat == CmdSuccess ? vendor : CpuVendor::None;
  }
};


struct ProgramExists : public ReadCommand
{
  bool operator()(const std::string_view program)
  {
    bool exists{false};
    const auto stat = execute(std::format("command -v {}", program),[&exists](const std::string_view line)
    {
      exists = !line.empty() ;
    });

    return stat == CmdSuccess && exists;
  }
};


struct CountPackages
{
  std::size_t operator()(const fs::path root_dev)
  {
    std::size_t n{};

    Chroot{}("pacman -Q | wc -l", [&n](const std::string_view m)
    {
      if (!m.empty())
        n = std::stoul(std::string{m.data(), m.size()});
    });

    return n;
  }
};


struct GetDevSpace
{
  std::pair<std::string, std::string> operator()(const std::string_view dev)
  {
    std::string size, used;

    Chroot{}(std::format("df -h --output=size {} | tail -n 1", dev), [&](const auto m){ size = m; });
    Chroot{}(std::format("df -h --output=used {} | tail -n 1", dev), [&](const auto m){ used = m ;});

    return {size, used};
  }
};


#endif
