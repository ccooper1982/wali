#include <Wt/WEnvironment.h>
#include <Wt/WText.h>
#include <math.h>

#include <Wt/WApplication.h>
#include <Wt/WMenu.h>
#include <Wt/WMenuItem.h>
#include <Wt/WDialog.h>
#include <Wt/WServer.h>
#include <Wt/WStackedWidget.h>
#include <plog/Init.h>
#include <plog/Appenders/ColorConsoleAppender.h>

#include <string_view>
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

static bool sync_system_clock()
{
  ReadCommand cmd;
  return cmd.execute("timedatectl") == CmdSuccess;
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
  else if (!sync_system_clock())
    return "Sync clock with timedatectl failed";
  else
    return "";
}

class WaliApplication : public Wt::WApplication
{
public:
  WaliApplication(const Wt::WEnvironment& env) : WApplication(env)
  {
    setTitle("wali");

    useStyleSheet("wali.css");

    enableUpdates(true);

    auto hbox = root()->setLayout(make_wt<Wt::WHBoxLayout>());
    hbox->setSpacing(0);

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
      menu_contents->setStyleClass("menu_content");
      menu_contents->setPadding(0);

      auto menu = menu_container->addNew<WMenu>(menu_contents);
      menu->setStyleClass("menu");
      Widgets::create_menu(menu, env.server());
    }
  }
};


static std::unique_ptr<WApplication> create_app(const WEnvironment& env)
{
  return std::make_unique<WaliApplication>(env);
}


// TODO quite sure there should be a way to get this from WServer / WEnvironment _prior_
//      to create_app() being called
static std::tuple<std::string_view, std::string_view> get_server_host(const int argc, char ** argv)
{
  std::vector<std::string_view> args(argv, argv+argc);

  const auto it_address = std::find_if(std::cbegin(args), std::cend(args), [](const std::string_view arg)
                          {
                            return arg == "--http-address";
                          });

  const auto it_port = std::find_if(std::cbegin(args), std::cend(args), [](const std::string_view arg)
                       {
                         return arg == "--http-port";
                       });

  // shouldn't happen because it suggests server won't reach this point
  if (it_address == std::cend(args) || it_port == std::cend(args) ||
      it_address+1 == std::cend(args) || it_port+1 == std::cend(args))
    return {};
  else
    return {*(it_address+1), *(it_port+1)};
}


int main(int argc, char **argv)
{
  init_logger();

  PLOGI << "Starting webtoolkit";

  try
  {
    WServer server(argc, argv);

    server.addEntryPoint(Wt::EntryPointType::Application, &create_app);

    if (server.start())
    {
      const auto [host, port] = get_server_host(argc, argv);

      PLOGI << "Web server running on " << host << ':' << port;

      const int signal = WServer::waitForShutdown();

      PLOGW << "Shutdown: " << signal;

      server.stop();
    }
  }
  catch (Wt::WServer::Exception& wex)
  {
    PLOGE << "Wt exception: " << wex.what();
    return 1;
  }
  catch (std::exception &e)
  {
    PLOGE << "exception: " << e.what();
    return 1;
  }

  return 0;

  // TODO Reinstate this when shutdown issue solved
  // return Wt::WRun(argc, argv, [](const Wt::WEnvironment& env)
  // {
  //   PLOGI << "Web server running on " << std::format("{}://{}", env.urlScheme(), env.hostName());
  //   return std::make_unique<WaliApplication>(env);
  // });
}
