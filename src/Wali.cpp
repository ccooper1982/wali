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
#include <wali/widgets/Common.hpp>
#include <wali/widgets/Widgets.hpp>


static plog::ColorConsoleAppender<WaliFormatter> consoleAppender;

static void init_logger ()
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

static bool check_programs_exist()
{
  static const std::vector<std::string> Programs =
  {
   "sgdisk"
    #ifdef WALI_PROD
      ,"pacstrap", "genfstab", "arch-chroot", "lshw"
    #endif
  };

  ProgramExists command;
  for (const auto& prog : Programs)
  {
    if (const auto [stat, exists] = command(prog); stat != CmdSuccess)
      return false;
  }

  return true;
}


static std::string startup_checks()
{
  if (const auto [ok, vendor] = GetCpuVendor{}(); !ok)
    return "CPU vendor not Intel/AMD, or not found";
  else if (!check_programs_exist()) // TODO show the missing command(s)
    return "Not all required commands exist";
  else if (!PlatformSizeValid{}())
    return "Platform size not found or not 64bit";
  // else if (!check(check_connection))
  //   return fail("No active internet connection");
  // else if (!check(sync_system_clock))
  //   return fail("Sync clock with timedatectl failed");
  else
    return "";
}

class HelloApplication : public Wt::WApplication
{
public:
  HelloApplication(const Wt::WEnvironment& env) : WApplication(env)
  {
    setTitle("wali");

    useStyleSheet("wali.css");

    auto hbox = root()->setLayout(make_wt<Wt::WHBoxLayout>());

    if (const auto err = startup_checks(); !err.empty())
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
      menu->setStyleClass("menu");
      Widgets::create_menu(menu);
    }
  }
};


int main(int argc, char **argv)
{
  init_logger();

  PLOGI << "Starting webtoolkit";

  return Wt::WRun(argc, argv, [](const Wt::WEnvironment& env)
  {
    // server already running by now, need to start manually so this log entry is easier to notice
    PLOGI << "Web server running on " << std::format("{}://{}", env.urlScheme(), env.hostName());

    return std::make_unique<HelloApplication>(env);
  });
}
