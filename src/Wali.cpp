#include "wali/Install.hpp"
#include "wali/widgets/AccountsWidget.hpp"
#include "wali/widgets/LocaliselWidget.hpp"
#include "wali/widgets/PackagesWidget.hpp"
#include "wali/widgets/WaliWidget.hpp"
#include "wali/widgets/WidgetData.hpp"
#include <Wt/WAnchor.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WEnvironment.h>
#include <Wt/WGlobal.h>
#include <Wt/WGridLayout.h>
#include <Wt/WBorderLayout.h>
#include <Wt/WHBoxLayout.h>
#include <Wt/WLength.h>
#include <Wt/WLink.h>
#include <Wt/WSignal.h>
#include <Wt/WText.h>
#include <algorithm>
#include <concepts>
#include <ios>
#include <math.h>

#include <Wt/WApplication.h>
#include <Wt/WMenu.h>
#include <Wt/WMenuItem.h>
#include <Wt/WDialog.h>
#include <Wt/WEvent.h>
#include <Wt/WServer.h>
#include <Wt/WStackedWidget.h>
#include <memory>
#include <plog/Init.h>
#include <plog/Appenders/ColorConsoleAppender.h>

#include <ranges>
#include <string>
#include <string_view>
#include <wali/Commands.hpp>
#include <wali/DiskUtils.hpp>
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
  ProgramExists check;
  for (const auto& prog : {"lshw"})
  {
    if (!check(prog))
      return false;
  }

  return true;
}

static bool sync_system_clock()
{
  ReadCommand cmd;
  return cmd.execute_read("timedatectl") == CmdSuccess;
}

static std::string startup_checks()
{
  if (!check_programs_exist()) // TODO show the missing command(s)
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


// class MenuWidget : public WContainerWidget
// {
//   inline static const constexpr std::string_view Names[] =
//   {
//     "Filesystems", "Network", "Accounts",
//     "Locale", "Video", "Packages"
//   };

// public:
//   MenuWidget()
//   {
//     resize(600, WLength::Auto);

//     auto btns_layout = setLayout(make_wt<WGridLayout>());

//     btns_layout->setVerticalSpacing(18);
//     btns_layout->setHorizontalSpacing(18);

//     for (int r = 0 ; r < 2 ; ++r)
//     {
//       for (int c = 0 ; c < 3 ; ++c)
//       {
//         auto btn = btns_layout->addWidget(make_wt<WPushButton>(Names[c+(r*3)].data()), r, c);
//         btn->setMinimumSize(100, 40);
//         btn->setMaximumSize(100, 40);
//         btn->clicked().connect([this](){ on_config_btn(); });
//       }
//     }

//     auto btn_install = btns_layout->addWidget(make_wt<WPushButton>("Install"), 1, 1);
//     btn_install->resize(100, 40);
//     btn_install->disable();
//     btn_install->clicked().connect([this](){ on_install_btn(); });
//   }

//   // Wt::Event<std::string_view> on_menu_click() { };

// private:

//   void on_config_btn ()
//   {
//     // show the widget
//   }

//   void on_install_btn ()
//   {
//     // show the install widget
//   }
// };


// class HomeWidget : public WContainerWidget
// {
// public:

//   HomeWidget()
//   {
//     setContentAlignment(AlignmentFlag::Center);
//     setMargin(0);
//     setPadding(0);

//     auto layout = setLayout(make_wt<WBorderLayout>());
//     layout->setContentsMargins(0,0,0,0);

//     auto north_cont = layout->addWidget(make_wt<WContainerWidget>(), LayoutPosition::North);
//     auto west_cont = layout->addWidget(make_wt<WContainerWidget>(), LayoutPosition::West);
//     auto central_cont = layout->addWidget(make_wt<WStackedWidget>(), LayoutPosition::Center);
//     auto east_cont = layout->addWidget(make_wt<WContainerWidget>(), LayoutPosition::East);
//     auto south_cont = layout->addWidget(make_wt<WContainerWidget>(), LayoutPosition::South);

//     //central_cont->setContentAlignment(AlignmentFlag::Center);
//     //central_cont->resize(600, WLength::Auto);

//     north_cont->setStyleClass("home_north");
//     north_cont->setVerticalAlignment(AlignmentFlag::Middle);
//     north_cont->addWidget(make_wt<WText>("Web Arch Linux Installer"));
//     south_cont->addWidget(make_wt<WText>(""));
//   }
// };


class NavBar : public WContainerWidget
{
public:
  NavBar(WStackedWidget * stack)
  {
    m_link = addWidget(make_wt<WAnchor>(WLink{}, "&#8592; Back"));
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
  "Locale", "Video", "Packages"
};


static WidgetDataPtr data;


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
        const auto name = Names[index];

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
    m_btn_install->disable();
    m_btn_install->clicked().connect([=]()
    {
      stack->disable();
    });

    // TODO investigate: doesn't work, something else must have focus
    // cont->keyPressed().connect([=](WKeyEvent ke)
    // {
    //   PLOGW << "oi oi";
    //   if (ke.key() == Key::Left)
    //     stack->setCurrentIndex(0);
    // });

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
    };

    stack->addWidget(create_home_widget(stack.get()));
    // ugly syntax to call the generic lambda because operator() is templated
    add_page.operator()<MountsWidget>();
    add_page.operator()<NetworkWidget>();
    add_page.operator()<AccountWidget>();
    add_page.operator()<LocaliseWidget>();
    add_page.operator()<VideoWidget>();
    add_page.operator()<PackagesWidget>();

    return stack;
  }

  void on_validity(const bool valid)
  {
    const auto& children = m_stack->children();

    const auto it = rng::find_if(children, [](const WWidget * w)
    {
      if (const auto wali_widget = dynamic_cast<const WaliWidget *>(w); wali_widget)
      {
        PLOGW << w->objectName() << " : " << std::boolalpha << wali_widget->is_data_valid();
        return !wali_widget->is_data_valid();
      }
      else
      {
        PLOGW << "Not a wali widget: " << w->objectName();
        return false;
      }
    });

    m_btn_install->setEnabled(it == rng::cend(children));
  }

public:
  WaliApplication(const Wt::WEnvironment& env) : WApplication(env)
  {
    setTitle("wali");

    useStyleSheet("wali.css");

    enableUpdates(true);

    data = std::make_shared<WidgetData>();

    root()->setMargin(0);
    root()->setPadding(0);

    auto layout = root()->setLayout(make_wt<WVBoxLayout>());
    layout->setSpacing(0);
    layout->setContentsMargins(0,0,0,0);


    auto header = layout->addWidget(make_wt<WContainerWidget>());
    auto title = layout->addWidget(make_wt<WContainerWidget>(), 0, AlignmentFlag::Center);
    auto stack = layout->addWidget(create_stack(), 1, AlignmentFlag::Center);
    layout->addStretch(2);

    header->setStyleClass("home_north");
    header->addWidget(make_wt<WText>("Web Arch Linux Installer"));

    title->setStyleClass("nav_bar");
    title->setHeight(50);
    // title->setPadding(20, Side::Bottom);

    auto nav = title->addWidget(make_wt<NavBar>(stack));

    stack->setMargin(0);
    stack->setPadding(0);
    stack->currentWidgetChanged().connect([=](WWidget * w)
    {
      nav->show_link(w->objectName() != "Home");
    });

    // auto hbox = root()->setLayout(make_wt<Wt::WHBoxLayout>());
    // hbox->setSpacing(0);
    // hbox->setContentsMargins(0, 0, 0, 0);

    // if (const auto err = startup_checks(); !err.empty())
    // {
    //   hbox->addWidget(make_wt<WText>(std::format("<h1>Startup checks failed</h1> <br/> <h2>{}</h2>", err)));
    //   hbox->addStretch(1);
    // }
    // else
    // {
    //   m_widgets = new Widgets;

    //   // menu on left, selected menu displays on right
    //   auto lhs = hbox->addWidget(make_wt<WContainerWidget>());
    //   auto rhs = hbox->addLayout(make_wt<WVBoxLayout>());

    //   lhs->setStyleClass("menu");
    //   rhs->setSpacing(0);

    //   // auto wali_widget_text = rhs->addWidget(make_wt<WText>("hello"));
    //   // wali_widget_text->setStyleClass("page_title");
    //   // wali_widget_text->setInline(true);

    //   auto wali_widgets = rhs->addWidget(make_wt<WStackedWidget>());
    //   hbox->addStretch(1);

    //   wali_widgets->setStyleClass("menu_content");
    //   wali_widgets->setPadding(0);

    //   auto menu = lhs->addNew<WMenu>(wali_widgets);
    //   menu->setStyleClass("menu");

    //   m_widgets->create_home_widget(menu);
    //   m_widgets->get_install()->install_state().connect([menu](InstallState state)
    //   {
    //     if (state == InstallState::Fail || state == InstallState::Partial)
    //     {
    //       menu->enable();
    //       menu->show();
    //     }
    //     else
    //     {
    //       menu->disable();
    //       menu->hide();
    //     }
    //   });
    // }
  }


private:
  WStackedWidget * m_stack{};
  WPushButton * m_btn_install{};
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
