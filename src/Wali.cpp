#include <Wt/WText.h>
#include <math.h>

#include <Wt/WApplication.h>
#include <Wt/WMenu.h>
#include <Wt/WMenuItem.h>
#include <Wt/WDialog.h>
#include <Wt/WStackedWidget.h>
#include <plog/Init.h>
#include <plog/Appenders/ColorConsoleAppender.h>

#include <wali/Commands.hpp>
#include <wali/LogFormat.hpp>
#include <wali/widgets/AccountsWidget.hpp>
#include <wali/widgets/Common.hpp>
#include <wali/widgets/IntroductionWidget.hpp>
#include <wali/widgets/InstallWidget.hpp>
#include <wali/widgets/NetworkWidget.hpp>
#include <wali/widgets/PartitionWidget.hpp>
#include <wali/widgets/Widgets.hpp>


static plog::ColorConsoleAppender<WaliFormatter> consoleAppender;

inline void init_logger ()
{
  static bool init = false;

  if (!init)
  {
    #ifdef WALI_DEBUG
      plog::init(plog::verbose, &consoleAppender);
    #else
      plog::init(plog::info, &consoleAppender);
    #endif
  }

  init = true;
}

static std::pair<int, bool> check_programs_exist()
{
  static const std::vector<std::string> Programs =
  {
    "pacman", "localectl", "locale-gen", "loadkeys", "setfont", "timedatectl", "ip", "lsblk",
    "mount", "swapon", "ln", "hwclock", "chpasswd", "passwd", "sgdisk", "useradd"

    #ifdef WALI_PROD
      ,"pacstrap", "genfstab", "arch-chroot", "lshw"
    #endif
  };

  for (const auto& cmd : Programs)
  {
    // TODO
  }

  return {CmdSuccess, true};
}

static std::tuple<bool, std::string> startup_checks()
{
  auto fail = [](const std::string_view err = "")
  {
    return std::make_tuple(false, std::string{err});
  };

  auto check = []<typename F>(F f)
  {
    // using R = std::invoke_result_t<F>;
    const auto [stat, result] = f();
    return stat == CmdSuccess;
  };

  if (!check(GetCpuVendor{}))
    return fail("CPU vendor not Intel/AMD, or not found");
  else if (!check(check_programs_exist))
    return fail("Not all required commands exist");
  else if (!check(PlatformSizeValid{}))
    return fail("Platform size not found or not 64bit");
  //
  // else if (!check(check_connection))
  //   return fail("No active internet connection");
  // else if (!check(sync_system_clock))
  //   return fail("Sync clock with timedatectl failed");
  else
    return {true, ""};
}



class HelloApplication : public Wt::WApplication
{
public:
  HelloApplication(const Wt::WEnvironment& env) : WApplication(env)
  {
    setTitle("wali");

    useStyleSheet("wali.css");

    auto hbox = root()->setLayout(make_wt<Wt::WHBoxLayout>());

    if (const auto [ok, err] = startup_checks(); !ok)
    {
      hbox->addWidget(make_wt<WText>(std::format("<h1>Startup checks failed</h1> <br/> <h2>{}</h2>", err)));
      hbox->addStretch(1);
    }
    else
    {
      // menu on left, selected menu displays on right
      auto menu_container = hbox->addWidget(make_wt<Wt::WContainerWidget>());
      auto menu_contents = hbox->addWidget(make_wt<Wt::WStackedWidget>());
      hbox->addStretch(1);

      menu_container->setStyleClass("menu");

      auto menu = menu_container->addNew<WMenu>(menu_contents);
      Widgets::create_intro(menu);
      Widgets::create_partitions(menu);
      Widgets::create_network(menu);
      Widgets::create_accounts(menu);
      Widgets::create_install(menu);
    }
  }
};


int main(int argc, char **argv)
{
  init_logger();

  PLOGI << "Starting webtoolkit";

  return Wt::WRun(argc, argv, [](const Wt::WEnvironment& env)
  {
    // server already running by now, need to start manually
    PLOGI << "Web server running on " << std::format("{}://{}", env.urlScheme() ,env.hostName());

    return std::make_unique<HelloApplication>(env);
  });
}
