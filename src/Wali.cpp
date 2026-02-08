#include "Wt/WPushButton.h"
#include "Wt/WVBoxLayout.h"
#include "wali/Install.hpp"
#include <algorithm>
#include <concepts>
#include <ranges>
#include <string>
#include <string_view>

#include <Wt/WAnchor.h>
#include <Wt/WEnvironment.h>
#include <Wt/WGlobal.h>
#include <Wt/WGridLayout.h>
#include <Wt/WBorderLayout.h>
#include <Wt/WLength.h>
#include <Wt/WLink.h>
#include <Wt/WSignal.h>
#include <Wt/WApplication.h>
#include <Wt/WMenu.h>
#include <Wt/WMenuItem.h>
#include <Wt/WDialog.h>
#include <Wt/WEvent.h>
#include <Wt/WServer.h>
#include <Wt/WStackedWidget.h>
#include <plog/Init.h>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <wali/Commands.hpp>
#include <wali/LogFormat.hpp>
#include <wali/widgets/Common.hpp>
#include <wali/widgets/AccountsWidget.hpp>
#include <wali/widgets/DesktopWidget.hpp>
#include <wali/widgets/InstallWidget.hpp>
#include <wali/widgets/LocaliselWidget.hpp>
#include <wali/widgets/MountsWidget.hpp>
#include <wali/widgets/MountsWidget.hpp>
#include <wali/widgets/NetworkWidget.hpp>
#include <wali/widgets/PackagesWidget.hpp>
#include <wali/widgets/WaliWidget.hpp>
#include <wali/widgets/WidgetData.hpp>


static plog::ColorConsoleAppender<WaliFormatter> consoleAppender;
static WidgetDataPtr data;


static void init_logger ()
{
  static bool init = false;

  if (!init)
  {
    plog::init(plog::info, &consoleAppender);
    init = true;
  }
}

static bool sync_system_clock()
{
  ReadCommand cmd;
  return cmd.execute_read("timedatectl") == CmdSuccess;
}

static std::string startup_checks()
{
  if (!PlatformSizeValid{}())
    return "Platform size not found or not 64bit";
  else if (!sync_system_clock())
    return "Sync clock with timedatectl failed";
  else
    return "";
}


class NavBar : public WContainerWidget
{
public:
  NavBar(WStackedWidget * stack)
  {
    m_link = addWidget(make_wt<WAnchor>(WLink{}, "&#8592; Back"));
    m_link->setStyleClass("nav_back");
    m_link->clicked().connect([=, this]()
    {
      stack->setCurrentIndex(0);
      m_link->setHidden(true);
    });

    m_link->setHidden(true);
  }

  void show_link(const bool show)
  {
    m_link->setHidden(!show);
  }

private:
  WAnchor * m_link;
};


inline static const constexpr std::string_view Names[] =
{
  "Filesystems", "Network", "Accounts",
  "Locale", "Desktop", "Packages"
};


class WaliApplication : public Wt::WApplication
{
  // TODO move to HomeWidget
  std::unique_ptr<WContainerWidget> create_home_widget (WStackedWidget * stack)
  {
    auto cont = make_wt<WContainerWidget>();
    cont->setObjectName("Home");
    cont->setMargin(0);
    cont->setPadding(0);
    cont->setContentAlignment(AlignmentFlag::Center);

    auto btns_layout = cont->setLayout(make_wt<WGridLayout>());

    btns_layout->setVerticalSpacing(18);
    btns_layout->setHorizontalSpacing(18);

    for (int r = 0 ; r < 2 ; ++r)
    {
      for (int c = 0 ; c < 3 ; ++c)
      {
        const auto index = c+(r*3);
        const auto& name = Names[index];

        auto btn = btns_layout->addWidget(make_wt<WPushButton>(name.data()), r, c);
        btn->setObjectName(name.data());
        btn->setMinimumSize(100, 40);
        btn->setMaximumSize(100, 40);
        //+1 because the first widget in the stack is home/menu page
        btn->clicked().connect([stack, widget_index = index+1]()
        {
          if (widget_index < stack->count())
            stack->setCurrentIndex(widget_index);
        });
      }
    }


    m_btn_install = btns_layout->addWidget(make_wt<WPushButton>("Install"), 2, 1);
    m_btn_install->resize(100, 40);
    #ifndef WALI_SKIP_VALIDATION
      m_btn_install->disable();
    #endif
    m_btn_install->clicked().connect([=]()
    {
      // InstallWidget is last in stack
      stack->setCurrentIndex(stack->count()-1);
    });

    return cont;
  }

  std::unique_ptr<WStackedWidget> create_stack ()
  {
    auto stack = make_wt<WStackedWidget>();
    m_stack = stack.get();

    auto add_page = [this]<typename W>() requires (std::derived_from<W, WaliWidget>)
    {
      auto widget = m_stack->addWidget(make_wt<W>(data));

      auto wali_widget = dynamic_cast<WaliWidget *>(widget);
      wali_widget->data_valid().connect([this](const bool v)
      {
        on_validity(v);
      });

      return widget;
    };

    stack->addWidget(create_home_widget(stack.get()));
    // ugly syntax to call the generic lambda because operator() is templated
    add_page.operator()<MountsWidget>();
    add_page.operator()<NetworkWidget>();
    add_page.operator()<AccountWidget>();
    add_page.operator()<LocaliseWidget>();
    add_page.operator()<DesktopWidget>();
    add_page.operator()<PackagesWidget>();
    m_install_widget = add_page.operator()<InstallWidget>();
    m_install_widget->install_state().connect([this](InstallState st)
    {
      m_nav_bar->setHidden(st == InstallState::Running || st == InstallState::Complete);
    });

    return stack;
  }

  // A WaliWidget has a data_valid() event, which when triggered, will call on_validity().
  void on_validity(const bool valid)
  {
    #ifndef WALI_SKIP_VALIDATION
      const auto have_invalid = rng::any_of(m_stack->children(), [](const WWidget * w)
      {
        const auto wali_widget = dynamic_cast<const WaliWidget *>(w);
        return wali_widget ? !wali_widget->is_data_valid() : false;
      });
      // only valid if the caller is valid and no other widgets are invalid
      m_btn_install->setEnabled(valid && !have_invalid);
    #endif
  }

public:
  WaliApplication(const Wt::WEnvironment& env) : WApplication(env)
  {
    setTitle("wali");
    useStyleSheet("wali.css");
    enableUpdates(true);

    auto layout = root()->setLayout(make_wt<WVBoxLayout>());

    if (const auto err = startup_checks() ; !err.empty())
    {
      PLOGE << err;
      layout->addWidget(make_wt<WText>(std::format("<h1>Startup checks failed</h1> <br/> <h2>{}</h2>", err)));
      layout->addStretch(1);
    }
    else
    {
      data = std::make_shared<WidgetData>();

      root()->setMargin(0);
      root()->setPadding(0);

      layout->setContentsMargins(0,0,0,0);
      layout->setSpacing(0);

      auto header = layout->addWidget(make_wt<WContainerWidget>());
      auto nav = layout->addWidget(make_wt<WContainerWidget>(), 0, AlignmentFlag::Center);
      // auto waffle = layout->addWidget(make_wt<WText>(intro_waffle), 0, AlignmentFlag::Center);
      // waffle->setWidth(600);

      layout->addSpacing(50);
      auto stack = layout->addWidget(create_stack(), 1, AlignmentFlag::Center);
      layout->addStretch(2);

      header->setStyleClass("home_north");
      auto header_layout = header->setLayout(make_wt<WVBoxLayout>());
      header_layout->setSpacing(0);
      header_layout->setContentsMargins(0,0,0,0);

      auto txt_title = header_layout->addWidget(make_wt<WText>("Web Arch Linux Installer"));
      auto txt_version = header_layout->addWidget(make_wt<WText>(std::format("v{}", WALI_VERSION)), 1);

      txt_title->setStyleClass("bar_title");
      txt_version->setStyleClass("bar_version");

      nav->setStyleClass("nav_bar");
      nav->setHeight(30);

      m_nav_bar = nav->addWidget(make_wt<NavBar>(stack));

      stack->setMargin(0);
      stack->setPadding(0);
      stack->currentWidgetChanged().connect([this](WWidget * w)
      {
        m_nav_bar->show_link(w->objectName() != "Home");
      });

      #ifdef WALI_FAKE_DATA
        data->mounts.boot_dev = "/dev/sda1";
        data->mounts.root_dev = "/dev/sda2";
        data->mounts.boot_fs = "vfat";
        data->mounts.root_fs = "ext4";
        data->mounts.home_target = HomeMountTarget::Root;
        data->accounts.user_username = "fake";
        data->accounts.user_pass = "nessie";

        #ifdef WALI_SKIP_VALIDATION
          m_btn_install->setEnabled(true);
        #endif
      #endif
    }
  }


private:
  WStackedWidget * m_stack{};
  WPushButton * m_btn_install{};
  NavBar * m_nav_bar{};
  InstallWidget * m_install_widget;
};


static std::unique_ptr<WApplication> create_app(const WEnvironment& env)
{
  return std::make_unique<WaliApplication>(env);
}


// TODO quite sure there should be a way to get this from WServer / WEnvironment _prior_
//      to create_app() being called
static std::pair<std::string_view, std::string_view> get_server_host(const int argc, char ** argv)
{
  using namespace std::string_view_literals;

  std::vector<std::string_view> args(argv, argv+argc);

  const auto it_address = rng::find(args, "--http-address"sv);
  const auto it_port = rng::find(args, "--http-port"sv);

  // +1 because the value is after the switch
  if (it_address == rng::end(args) || it_port == rng::end(args) ||
      it_address+1 == rng::end(args) || it_port+1 == rng::end(args))
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
