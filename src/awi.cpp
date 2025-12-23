#include <algorithm>
#include <math.h>
#include <Wt/WApplication.h>
#include <Wt/WBootstrap5Theme.h>
#include <Wt/WBreak.h>
#include <Wt/WComboBox.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WHBoxLayout.h>
#include <Wt/WVBoxLayout.h>
#include <Wt/WGlobal.h>
#include <Wt/WLength.h>
#include <Wt/WLineEdit.h>
#include <Wt/WMenuItem.h>
#include <Wt/WPopupMenu.h>
#include <Wt/WPopupMenuItem.h>
#include <Wt/WPushButton.h>
#include <Wt/WMenu.h>
#include <Wt/WMessageBox.h>
#include <Wt/WNavigationBar.h>
#include <Wt/WStackedWidget.h>
#include <Wt/WTable.h>
#include <Wt/WText.h>
#include <Wt/WTextArea.h>
#include <Wt/WWidget.h>

#include <wali/disk_utils.hpp>


class IntroductionWidget;
class PartitionsWidget;


static std::string format_size(const int64_t size)
{
  static const char *sizeNames[] = {"B", "KB", "MB", "GB", "TB", "PB"};

  std::string result;

  const uint64_t i = (uint64_t) std::floor(std::log(size) / std::log(1024));
  const auto display_size = size / std::pow(1024, i);

  return std::format("{:.1f} {}", display_size, sizeNames[i]);
}

template<typename T, typename ... Args>
std::unique_ptr<T> make_widget (Args...args)
{
  return std::make_unique<T>(args...);
}


class HelloApplication : public Wt::WApplication
{
public:
    HelloApplication(const Wt::WEnvironment& env);


};

class IntroductionWidget : public Wt::WContainerWidget
{
  static constexpr auto text = R"(
    <h1>Web Arch Linux Installer</h1>
    <h2>No changes are applied until 'Install' is pressed in the final step.</h2>
    <ul>
      <li>This tool does yet not manage partitions, do so with fdisk, cfdisk, etc</li>
      <li>Something else useful</li>
      <li>Perhaps a third informative point</li>
    </ul>
    )";

public:
  IntroductionWidget()
  {
    addWidget(make_widget<Wt::WText>(text));
  }
};


class PartitionsWidget : public Wt::WContainerWidget
{
public:
  PartitionsWidget()
  {
    auto layout = setLayout(make_widget<Wt::WVBoxLayout>());

    if (!PartitionUtils::probe_for_install())
      std::cout << "Probe failed\n";
    else
    {
      if (!PartitionUtils::have_partitions())
        std::cout << "No partitions found\n";
      else
      {
        const auto parts = PartitionUtils::partitions();

        create_table(layout, parts, {"fat32"}, "Boot Partition", [](const Partition& p){ return p.is_fat32;});
        create_table(layout, parts, {"ext4", "btrfs"}, "OS Partition", [](const Partition& p){ return !p.is_fat32;});
        create_table(layout, parts, {"ext4", "btrfs"}, "Home Partition", [](const Partition& p){ return !p.is_fat32;});
      }
    }

    layout->addStretch(1);
  }

  void create_table (Wt::WVBoxLayout * layout, const Partitions& parts, const std::vector<std::string_view> filesystems,
                    const std::string_view title, std::function<bool(const Partition& part)>&& filter)
  {
    layout->addWidget(make_widget<Wt::WText>(std::format("<h2>{}</h2>", title)));

    auto table_boot = layout->addWidget(make_widget<Wt::WTable>());
    auto use_dev_container = layout->addWidget(make_widget<Wt::WContainerWidget>());

    auto use_dev_layout = use_dev_container->setLayout(make_widget<Wt::WHBoxLayout>());

    use_dev_layout->addWidget(make_widget<Wt::WText>("Device"));
    auto combo_part = use_dev_layout->addWidget(make_widget<Wt::WComboBox>());
    use_dev_layout->addWidget(make_widget<Wt::WText>("Filesystem"));
    auto combo_fs = use_dev_layout->addWidget(make_widget<Wt::WComboBox>());
    use_dev_layout->addStretch(1);

    std::for_each(filesystems.cbegin(), filesystems.cend(), [combo_fs](const auto& fs) { combo_fs->addItem(fs.data()); });

    table_boot->setWidth(300);
    table_boot->setHeaderCount(1);
    table_boot->elementAt(0, 0)->addNew<Wt::WText>("Device");
    table_boot->elementAt(0, 1)->addNew<Wt::WText>("Filesystem");
    table_boot->elementAt(0, 2)->addNew<Wt::WText>("Size");

    for(size_t i = 0; i < parts.size() ; ++i)
    {
      if (filter(parts[i]))
      {
        table_boot->elementAt(i+1,0)->addNew<Wt::WText>(parts[i].dev);
        table_boot->elementAt(i+1,1)->addNew<Wt::WText>(parts[i].fs_type);
        table_boot->elementAt(i+1,2)->addNew<Wt::WText>(format_size(parts[i].size));

        combo_part->addItem(parts[i].dev);
      }
    }

    table_boot->addStyleClass("table");
    combo_part->setWidth(150);
  }
};


HelloApplication::HelloApplication(const Wt::WEnvironment& env) : Wt::WApplication(env)
{
  setTitle("wali");

  // setCssTheme("bootstrap");
  useStyleSheet("wali.css");

  auto menu_contents = make_widget<Wt::WStackedWidget>();
  auto hbox = root()->setLayout(make_widget<Wt::WHBoxLayout>());

  auto menu_container = make_widget<Wt::WContainerWidget>();
  menu_container->setStyleClass("menu");

  auto menu = menu_container->addNew<Wt::WMenu>(menu_contents.get());
  menu->addItem("Introduction", make_widget<IntroductionWidget>());
  menu->addItem("Partitions",   make_widget<PartitionsWidget>());

  // menu on left, selected menu item content on right
  hbox->addWidget(std::move(menu_container));
  hbox->addWidget(std::move(menu_contents), 1);
}


int main(int argc, char **argv)
{
  return Wt::WRun(argc, argv, [](const Wt::WEnvironment& env)
  {
    return std::make_unique<HelloApplication>(env);
  });
}
