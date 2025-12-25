#include <map>
#include <math.h>
#include <string_view>
#include <Wt/WApplication.h>
#include <Wt/WMenuItem.h>
#include <Wt/WPopupMenu.h>
#include <Wt/WPopupMenuItem.h>
#include <Wt/WMenu.h>
#include <Wt/WStackedWidget.h>
#include <Wt/WTextArea.h>
#include <Wt/WWidget.h>

#include <wali/AccountsWidget.hpp>
#include <wali/IntroductionWidget.hpp>
#include <wali/NetworkWidget.hpp>
#include <wali/PartitionWidget.hpp>

class MessageWidget : public Wt::WContainerWidget
{
public:
  enum class Level {Info, Warning, Error};

  MessageWidget()
  {
    create();
  }

  void clear_messages()
  {
    create();
  }

  void add(const std::string_view msg, const Level lvl)
  {
    static const std::map<const Level, const Wt::WString> LevelValueMap
    {
      {Level::Error,    "msg_error"},
      {Level::Warning,  "msg_warning"},
      {Level::Info,     "msg_info"}
    };

    if (LevelValueMap.contains(lvl))
    {
      auto txt = m_layout->addWidget(make_wt<Wt::WText>(msg.data()));
      txt->setStyleClass(LevelValueMap.at(lvl));
    }
  }

private:
  void create()
  {
    m_layout = setLayout(make_wt<Wt::WVBoxLayout>());
    // auto panel = make_wt<Wt::WPanel>();
    // panel->setTitle("Messages");
    // panel->setCentralWidget();
  }

private:
  Wt::WVBoxLayout * m_layout;
};


class HelloApplication : public Wt::WApplication
{
public:
  HelloApplication(const Wt::WEnvironment& env) : Wt::WApplication(env)
  {
    setTitle("wali");

    useStyleSheet("wali.css");

    // menu on left, selected menu displays on right
    auto hbox = root()->setLayout(make_wt<Wt::WHBoxLayout>());
    auto menu_container = hbox->addWidget(make_wt<Wt::WContainerWidget>());
    auto menu_contents = hbox->addWidget(make_wt<Wt::WStackedWidget>());
    hbox->addStretch(1);

    menu_container->setStyleClass("menu");

    auto menu = menu_container->addNew<Wt::WMenu>(menu_contents);
    menu->addItem("Introduction", make_wt<IntroductionWidget>());
    menu->addItem("Partitions",   make_wt<PartitionsWidget>());
    menu->addItem("Network",      make_wt<NetworkWidget>());
    menu->addItem("Accounts",     make_wt<AccountWidget>());
  }
};


int main(int argc, char **argv)
{
  return Wt::WRun(argc, argv, [](const Wt::WEnvironment& env)
  {
    return std::make_unique<HelloApplication>(env);
  });
}
