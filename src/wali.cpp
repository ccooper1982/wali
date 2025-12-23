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
std::unique_ptr<T> make_wt (Args...args)
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
    addWidget(make_wt<Wt::WText>(text));
  }
};


class PartitionsWidget : public Wt::WContainerWidget
{
  using DeviceChange = std::function<void(const std::string_view)>;

  struct PartitionWidget : public Wt::WContainerWidget
  {
    PartitionWidget (const std::string_view title, std::vector<std::string_view> filesystems) :
      m_title(title),
      m_filesystems(std::move(filesystems))
    {
      m_layout = setLayout(make_wt<Wt::WVBoxLayout>());
    }

    std::string get_device() const
    {
      return m_device->currentText().toUTF8();
    }

    std::string get_fs() const
    {
      return m_fs->currentText().toUTF8();
    }

    void set_device(const std::string_view dev)
    {
      m_device->setValueText(dev.data());
    }

    virtual void create (const Partitions& parts, DeviceChange&& on_change)
    {
      m_on_device = on_change;

      m_layout->addWidget(make_wt<Wt::WText>(std::format("<h2>{}</h2>", m_title)));

      // holds the widgets for selecting device and filesystem
      auto container_dev_fs = m_layout->addWidget(make_wt<Wt::WContainerWidget>());

      auto layout_dev_fs = container_dev_fs->setLayout(make_wt<Wt::WHBoxLayout>());

      layout_dev_fs->addWidget(make_wt<Wt::WText>("Device"));
      m_device = layout_dev_fs->addWidget(make_wt<Wt::WComboBox>());
      m_device->setWidth(150);

      m_device->changed().connect([this]()
      {
        if (m_on_device)
          m_on_device(m_device->currentText().toUTF8());
      });

      layout_dev_fs->addWidget(make_wt<Wt::WText>("Filesystem"));

      m_fs = layout_dev_fs->addWidget(make_wt<Wt::WComboBox>());
      layout_dev_fs->addStretch(1);

      std::for_each(parts.cbegin(), parts.cend(), [this](const Partition& part) { m_device->addItem(part.dev); });
      std::for_each(m_filesystems.cbegin(), m_filesystems.cend(), [this](const auto& fs) { m_fs->addItem(fs.data()); });
    }

  private:
    virtual bool valid_for_install (const Partition& part)
    {
      return true;
    }

    std::string_view m_title;
    std::vector<std::string_view> m_filesystems;
    DeviceChange m_on_device;

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

    void create (const Partitions& parts, DeviceChange&& on_change) override
    {
      PartitionWidget::create(parts, std::move(on_change));

      m_home_to_root = m_layout->addWidget(make_wt<Wt::WCheckBox>("Mount /home to /"));
      m_home_to_root->changed().connect([this]()
      {
        enable_dev_fs(!m_home_to_root->isChecked());
        m_on_mount_to_root(m_home_to_root->isChecked());
      });

      m_home_to_root->setCheckState(Wt::CheckState::Checked);
      enable_dev_fs(!m_home_to_root->isChecked());
    }

    void enable_dev_fs(const bool e)
    {
      m_device->setEnabled(e);
      m_fs->setEnabled(e);
    }

    bool is_home_on_root() const
    {
      return m_home_to_root->isChecked();
    }

    private:
      std::function<void(bool)> m_on_mount_to_root;
      Wt::WCheckBox * m_home_to_root;
  };


public:
  PartitionsWidget()
  {
    auto layout = setLayout(make_wt<Wt::WVBoxLayout>());

    if (!PartitionUtils::probe_for_install())
      std::cout << "Probe failed\n";
    else
    {
      if (!PartitionUtils::have_partitions())
        std::cout << "No partitions found\n";
      else
      {
        const auto parts = PartitionUtils::partitions();

        auto table_parts = layout->addWidget(make_wt<Wt::WTable>());
        table_parts->setHeaderCount(1);
        table_parts->elementAt(0, 0)->addNew<Wt::WText>("Device");
        table_parts->elementAt(0, 1)->addNew<Wt::WText>("Filesystem");
        table_parts->elementAt(0, 2)->addNew<Wt::WText>("Size");
        table_parts->setStyleClass("table_partitions");

        for(size_t i = 0; i < parts.size() ; ++i)
        {
          table_parts->elementAt(i+1,0)->addNew<Wt::WText>(parts[i].dev);
          table_parts->elementAt(i+1,1)->addNew<Wt::WText>(parts[i].fs_type);
          table_parts->elementAt(i+1,2)->addNew<Wt::WText>(format_size(parts[i].size));
        }

        table_parts->addStyleClass("table");

        m_boot = layout->addWidget(make_wt<BootPartitionWidget>());
        m_boot->create(parts, std::bind(&PartitionsWidget::validate_selection, std::ref(*this)));

        m_root = layout->addWidget(make_wt<RootPartitionWidget>());
        m_root->create(parts, [this](const std::string_view dev)
        {
          if (m_home->is_home_on_root())
            m_home->set_device(dev);

          validate_selection();
        });

        m_home = layout->addWidget(make_wt<HomePartitionWidget>(std::bind_front(&PartitionsWidget::on_home_to_home, std::ref(*this))));
        m_home->create(parts, std::bind(&PartitionsWidget::validate_selection, std::ref(*this)));
      }
    }

    layout->addStretch(1);
  }

  void on_home_to_home(const bool checked)
  {
    if (checked)
      m_home->set_device(m_root->get_device());
  }

  void validate_selection()
  {
    const auto& boot_dev = m_boot->get_device();
    const auto& root_dev = m_root->get_device();
    const auto& home_dev = m_home->get_device();

    if (boot_dev == root_dev || boot_dev == home_dev)
      std::cout << "INVALID\n";
    else
      std::cout << "VALID\n";
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

  auto menu_contents = make_wt<Wt::WStackedWidget>();
  auto hbox = root()->setLayout(make_wt<Wt::WHBoxLayout>());

  auto menu_container = make_wt<Wt::WContainerWidget>();
  menu_container->setStyleClass("menu");

  auto menu = menu_container->addNew<Wt::WMenu>(menu_contents.get());
  menu->addItem("Introduction", make_wt<IntroductionWidget>());
  menu->addItem("Partitions",   make_wt<PartitionsWidget>());

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
