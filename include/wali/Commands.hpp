#ifndef WALI_COMMANDS_H
#define WALI_COMMANDS_H

#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
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
  ReadCommand() = default;

protected:

  int execute_read_line (const std::string_view cmd, OutputHandler&& handler)
  {
    return execute_read(cmd, std::move(handler), 1);
  }

  int execute_read (const std::string_view cmd, OutputHandler&& handler, const int max_lines = -1)
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


struct PlatformSizeValid : public ReadCommand
{
  std::tuple<int, bool> operator()()
  {
    int size{0}, stat{CmdFail};

    static const std::filesystem::path file {"/sys/firmware/efi/fw_platform_size"};

    if (std::filesystem::exists(file) && std::filesystem::file_size(file))
    {
      std::string str;
      // should only ever be one line in this file
      std::ifstream stream{file};
      std::getline(stream, str);
      if (!str.empty())
      {
        size = std::stoi(str);
        stat = CmdSuccess;
      }
    }

    return {stat, size == 64};
  }
};


struct GetCpuVendor : public ReadCommand
{
  std::tuple<int, CpuVendor> operator()()
  {
    CpuVendor vendor = CpuVendor::None;

    const auto stat = execute_read_line("cat /proc/cpuinfo | grep \"^model name\"", [&vendor](const std::string_view line)
    {
      if (line.find("AMD") != std::string::npos)
        vendor = CpuVendor::Amd;
      else if (line.find("Intel") != std::string::npos)
        vendor = CpuVendor::Intel;
    });

    return {stat, vendor};
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


struct GetTimeZones : public ReadCommand
{
  std::tuple<int, std::vector<std::string>> operator()()
  {
    std::vector<std::string> zones;
    zones.reserve(600); // "timedatectl list-timezones | wc -l" returns 598

    const auto stat = execute_read(std::format("timedatectl list-timezones"),[&zones](const std::string_view line)
    {
      if (!line.empty())
        zones.emplace_back(line);
    });

    return {stat, zones};
  }
};

#endif
