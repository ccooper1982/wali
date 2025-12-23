#include <algorithm>
#include <math.h>
#include <Wt/WApplication.h>
#include <Wt/WBootstrap5Theme.h>
#include <Wt/WBreak.h>
#include <Wt/WCheckBox.h>
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

#include <string_view>
#include <wali/disk_utils.hpp>


class IntroductionWidget;
class PartitionsWidget;

enum class MountType
{
  None,
  Root,
  Boot,
  Home,
  Swap
};

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
  struct PartitionWidget : public Wt::WContainerWidget
  {
    PartitionWidget (const std::string_view title, std::vector<std::string_view> filesystems) :
      m_title(title),
      m_filesystems(std::move(filesystems))
    {
      m_layout = setLayout(make_widget<Wt::WVBoxLayout>());
    }

    std::string get_device() const
    {
      return m_device->currentText().toUTF8();
    }

    std::string get_fs() const
    {
      return m_fs->currentText().toUTF8();
    }

    virtual void create_table (const Partitions& parts)
    {
      m_layout->addWidget(make_widget<Wt::WText>(std::format("<h2>{}</h2>", m_title)));

      auto table_boot = m_layout->addWidget(make_widget<Wt::WTable>());
      auto use_dev_container = m_layout->addWidget(make_widget<Wt::WContainerWidget>());

      auto use_dev_layout = use_dev_container->setLayout(make_widget<Wt::WHBoxLayout>());

      use_dev_layout->addWidget(make_widget<Wt::WText>("Device"));
      m_device = use_dev_layout->addWidget(make_widget<Wt::WComboBox>());

      use_dev_layout->addWidget(make_widget<Wt::WText>("Filesystem"));
      m_fs = use_dev_layout->addWidget(make_widget<Wt::WComboBox>());

      use_dev_layout->addStretch(1);

      std::for_each(m_filesystems.cbegin(), m_filesystems.cend(), [this](const auto& fs) { m_fs->addItem(fs.data()); });

      table_boot->setWidth(300);
      table_boot->setHeaderCount(1);
      table_boot->elementAt(0, 0)->addNew<Wt::WText>("Device");
      table_boot->elementAt(0, 1)->addNew<Wt::WText>("Filesystem");
      table_boot->elementAt(0, 2)->addNew<Wt::WText>("Size");

      for(size_t i = 0; i < parts.size() ; ++i)
      {
        if (valid_for_install(parts[i]))
        {
          table_boot->elementAt(i+1,0)->addNew<Wt::WText>(parts[i].dev);
          table_boot->elementAt(i+1,1)->addNew<Wt::WText>(parts[i].fs_type);
          table_boot->elementAt(i+1,2)->addNew<Wt::WText>(format_size(parts[i].size));

          m_device->addItem(parts[i].dev);
        }
      }

      table_boot->addStyleClass("table");
      m_device->setWidth(150);
    }

  private:
    virtual bool valid_for_install (const Partition& part)
    {
      return true;
    }

    std::string_view m_title;
    std::vector<std::string_view> m_filesystems;

  protected:
    Wt::WVBoxLayout * m_layout;
    Wt::WComboBox * m_device, * m_fs;
  };

  struct BootPartitionWidget final : public PartitionWidget
  {
    BootPartitionWidget() : PartitionWidget("Boot", {"vfat32"})
    {}
  };

  struct RootPartitionWidget final : public PartitionWidget
  {
    RootPartitionWidget() : PartitionWidget("Root", {"ext4", "btrfs"})
    {}
  };

  struct HomePartitionWidget final : public PartitionWidget
  {
    HomePartitionWidget(std::function<void(bool)> on_mount_to_root) :
      PartitionWidget("Home", {"ext4", "btrfs"}),
      m_on_mount_to_root(on_mount_to_root)
    {

    }

    void create_table (const Partitions& parts) override
    {
      PartitionWidget::create_table(parts);

      auto check_home_to_root = m_layout->addWidget(make_widget<Wt::WCheckBox>("Mount /home to /"));
      check_home_to_root->changed().connect([this, check_home_to_root]()
      {
        enable_dev_fs(!check_home_to_root->isChecked());
        // m_device->setEnabled(!check_home_to_root->isChecked());
        // m_fs->setEnabled(!check_home_to_root->isChecked());

        m_on_mount_to_root(check_home_to_root->isChecked());
      });

      check_home_to_root->setCheckState(Wt::CheckState::Checked);
      enable_dev_fs(!check_home_to_root->isChecked());
    }

    void enable_dev_fs(const bool e)
    {
      m_device->setEnabled(e);
      m_fs->setEnabled(e);
    }
    // void enable(const bool e)
    // {
    //   m_device->setEnabled(e);
    //   m_fs->setEnabled(e);
    // }

    private:
      std::function<void(bool)> m_on_mount_to_root;
  };


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

        auto on_home_to_root = [](const bool checked)
        {
          std::cout << "Checked " << checked << '\n';

        };

        m_boot = layout->addWidget(make_widget<BootPartitionWidget>());
        m_boot->create_table(parts);

        m_root = layout->addWidget(make_widget<RootPartitionWidget>());
        m_root->create_table(parts);

        m_home = layout->addWidget(make_widget<HomePartitionWidget>(on_home_to_root));
        m_home->create_table(parts);
      }
    }

    layout->addStretch(1);
  }


private:
  BootPartitionWidget * m_boot;
  RootPartitionWidget * m_root;
  HomePartitionWidget * m_home;
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
