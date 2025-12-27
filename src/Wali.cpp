#include <algorithm>
#include <iterator>
#include <math.h>

#include <Wt/WApplication.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WMenu.h>
#include <Wt/WMenuItem.h>
#include <Wt/WPanel.h>
#include <Wt/WPopupMenu.h>
#include <Wt/WPopupMenuItem.h>
#include <Wt/WStackedWidget.h>
#include <Wt/WTextArea.h>

#include <plog/Init.h>
#include <plog/Appenders/ColorConsoleAppender.h>

#include <wali/LogFormat.hpp>
#include <wali/widgets/AccountsWidget.hpp>
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


class HelloApplication : public Wt::WApplication
{
public:
  HelloApplication(const Wt::WEnvironment& env) : WApplication(env)
  {
    setTitle("wali");

    useStyleSheet("wali.css");

    // menu on left, selected menu displays on right
    auto hbox = root()->setLayout(make_wt<Wt::WHBoxLayout>());
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

    PLOGI << "Web server running on " << std::format("{}://{}", env.urlScheme() ,env.hostName());
  }
};


int main(int argc, char **argv)
{
  init_logger();

  PLOGI << "Starting webtoolkit";

  return Wt::WRun(argc, argv, [](const Wt::WEnvironment& env)
  {
    return std::make_unique<HelloApplication>(env);
  });
}
