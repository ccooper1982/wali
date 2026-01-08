#ifndef WALI_COMMANDS_H
#define WALI_COMMANDS_H

#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include <plog/Log.h>

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

class ReadCommand : public Command
{
public:

  static int execute_read_line (const std::string_view cmd, OutputHandler&& handler)
  {
    ReadCommand rc;
    return execute_read(cmd, std::move(handler), 1);
  }

  static int execute_read(const std::string_view cmd)
  {
    return execute_read(cmd, nullptr);
  }

  static int execute_read (const std::string_view cmd, OutputHandler&& handler, const int max_lines = -1)
  {
    ReadCommand rc;
    return rc.do_execute_read(cmd, std::move(handler), max_lines);
  }

private:
  int do_execute_read (const std::string_view cmd, OutputHandler&& handler, const int max_lines = -1)
  {
    if (cmd.empty())
      return CmdSuccess;

    int stat{CmdFail};

    // redirect stderr to stdout (pacstrap, and perhaps others, output errors to stderr)
    if (m_fd = ::popen(std::format("{} 2>&1", cmd).data(), "r"); m_fd)
    {
      char buff[4096];
      int n_lines{0};
      size_t n_chars{0};

      for (char c ; ((c = fgetc(m_fd)) != EOF) && n_chars < sizeof(buff); )
      {
        if (!handler)
          continue;

        if (c == '\n')
        {
          handler(std::string_view {buff, n_chars});

          if (++n_lines == max_lines)
            break;

          n_chars = 0;
        }
        else
        {
          buff[n_chars++] = c;
        }
      }

      if (handler && n_chars)
        handler(std::string_view {buff, n_chars});

      stat = close();

      if (stat != CmdSuccess)
      {
        PLOGE << "Command '" << cmd << "' failed: " << ::strerror(stat);
      }
    }

    return stat;
  }
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
    const auto chroot_cmd = std::format("arch-chroot {} {}", RootMnt.string(), cmd);

    // caller doesn't want output, so write to stdout
    const auto stat = execute_read(chroot_cmd, [](const std::string_view m)
    {
      PLOGI << m;
    });

    return stat == CmdSuccess;
  }

  virtual bool operator()(const std::string_view cmd, OutputHandler&& handler)
  {
    const auto chroot_cmd = std::format("arch-chroot {} {}", RootMnt.string(), cmd);
    return execute_read(chroot_cmd, std::move(handler)) == CmdSuccess;
  }
};

struct ChrootWrite : public WriteCommand
{
  // bool operator()(const std::string_view cmd, const std::string_view input)
  bool operator()(const std::string_view input)
  {
    // const std::string script = std::format("(cat<<EOF | arch-chroot {}\n{};\nexit $?;\nEOF)", RootMnt.string(), cmd);
    // PLOGE << script;
    // return execute(script) == CmdSuccess;

    return execute(std::format("arch-chroot {}", RootMnt.string()), input) == CmdSuccess;
  }
};

// localisation
struct GetTimeZones : public ReadCommand
{
  std::vector<std::string> operator()()
  {
    std::vector<std::string> zones;
    zones.reserve(600); // "timedatectl list-timezones | wc -l" returns 598

    const auto stat = execute_read("timedatectl list-timezones", [&](const std::string_view line)
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

    const auto stat = execute_read("localectl list-keymaps",[&](const std::string_view line)
    {
      if (!line.empty())
        keys.emplace_back(line);
    });

    return stat == CmdSuccess ? keys : std::vector<std::string>{};
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
  std::tuple<bool, CpuVendor> operator()()
  {
    CpuVendor vendor = CpuVendor::None;

    const auto stat = execute_read_line("cat /proc/cpuinfo | grep \"^model name\"", [&vendor](const std::string_view line)
    {
      if (line.find("AMD") != std::string::npos)
        vendor = CpuVendor::Amd;
      else if (line.find("Intel") != std::string::npos)
        vendor = CpuVendor::Intel;
    });

    return {stat == CmdSuccess, vendor};
  }
};


struct ProgramExists : public ReadCommand
{
  std::tuple<int, bool> operator()(const std::string_view program)
  {
    bool exists{false};
    const auto stat = execute_read(std::format("command -v {}", program),[&exists](const std::string_view line)
    {
      exists = !line.empty() ;
    });

    return {stat, exists};
  }
};


struct Reboot : public ReadCommand
{
  bool operator()()
  {
    return execute_read("reboot -f") == CmdSuccess;
  }
};


struct GetGpuVendor : public ReadCommand
{
  GpuVendor vendor{GpuVendor::Unknown};
  unsigned short n_amd{0}, n_nvidia{0}, n_vm{0}, n_intel{0};

  GpuVendor operator()()
  {
    const auto stat = execute_read("lshw -C display | grep 'vendor: '", [&](const std::string_view m)
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
inline const constexpr char ext4[] = "ext4";
inline const constexpr char vfat32[] = "vfat -F 32";

template<const char * cmd>
struct CreateFilesystem : public ReadCommand
{
  bool operator()(const std::string_view dev)
  {
    // const auto stat = execute_read(std::format("mkfs.{} {}", cmd, dev), [](const std::string_view m)
    // {
    //   PLOGI << m;
    // });
    // PLOGE << "stat=" << stat;
    // return stat == CmdSuccess;
    return execute_read(std::format("mkfs.{} {}", cmd, dev)) == CmdSuccess;
  }
};

using CreateExt4Filesystem = CreateFilesystem<ext4>;
using CreateVfat32Filesystem = CreateFilesystem<vfat32>;

// mount
struct Mount : public ReadCommand
{
  bool operator()(const std::string_view dev, const std::string_view mount_point)
  {
    return execute_read(std::format("mount {} {}", dev, mount_point)) == CmdSuccess;
  }
};

struct Unmount : public ReadCommand
{
  bool operator()(const std::string_view mount_point, const bool recursive)
  {
    return execute_read(std::format("umount {} {}", recursive ? "-R" : "", mount_point)) == CmdSuccess;
  }
};



#endif
