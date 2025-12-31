#ifndef WALI_PARTITIONWIDGET_H
#define WALI_PARTITIONWIDGET_H

#include <Wt/WComboBox.h>
#include <Wt/WGlobal.h>
#include <Wt/WRadioButton.h>
#include <Wt/WButtonGroup.h>
#include <Wt/WTable.h>

#include <Wt/WVBoxLayout.h>
#include <memory>
#include <wali/widgets/MessagesWidget.hpp>
#include <wali/Common.hpp>
#include <wali/DiskUtils.hpp>


// Draws a Device dropdown and Filesystem dropdown (optional)
struct DeviceFilesytemWidget : public WContainerWidget
{
  DeviceFilesytemWidget() = default;

  DeviceFilesytemWidget(const Partitions& parts, const StringViewVec filesystems = {})
  {
    auto layout = setLayout(make_wt<Wt::WHBoxLayout>());

    layout->addWidget(make_wt<WText>("Device"));
    m_device = layout->addWidget(make_wt<WComboBox>());
    m_device->setWidth(150);

    if (!filesystems.empty())
    {
      layout->addWidget(make_wt<Wt::WText>("Filesystem"));
      m_fs = layout->addWidget(make_wt<Wt::WComboBox>());

      for_each(filesystems, [this](const auto& fs) { m_fs->addItem(fs.data()); });
    }

    for_each(parts, [this](const Partition& part) { m_device->addItem(part.dev); });

    layout->addStretch(1);
  }

  virtual ~DeviceFilesytemWidget() = default;

  std::string get_device() const
  {
    return m_device->currentText().toUTF8();
  }

  std::string get_fs() const
  {
    return m_fs->currentText().toUTF8();
  }

private:
  WComboBox * m_device;
  WComboBox * m_fs{};
};

// TODO
//  - Allow /home mount to existing partition (no wipe, no create filesystem)
class PartitionsWidget : public WContainerWidget
{
  using DeviceChange = std::function<void(const std::string_view)>;

  struct DeviceFilesystemProvider
  {
    virtual std::string get_device() const = 0;
    virtual std::string get_fs() const = 0;
  };

  struct BootPartitionWidget : public WContainerWidget
  {
    BootPartitionWidget(const Partitions& parts)
    {
      auto layout = setLayout(make_wt<WVBoxLayout>());

      layout->addWidget(make_wt<Wt::WText>("<h2>Boot</h2>"));

      m_dev_fs = layout->addWidget(make_wt<DeviceFilesytemWidget>(parts, StringViewVec{"vfat"}));
    }

    std::string get_device() const
    {
      return m_dev_fs->get_device();
    }

    std::string get_fs() const
    {
      return m_dev_fs->get_fs();
    }

  private:
    DeviceFilesytemWidget * m_dev_fs;
  };

  struct RootPartitionWidget : public WContainerWidget
  {
    RootPartitionWidget(const Partitions& parts)
    {
      auto layout = setLayout(make_wt<WVBoxLayout>());

      layout->addWidget(make_wt<Wt::WText>("<h2>Root</h2>"));

      m_dev_fs = layout->addWidget(make_wt<DeviceFilesytemWidget>(parts, StringViewVec{"ext4"}));
    }

    std::string get_device() const
    {
      return m_dev_fs->get_device();
    }

    std::string get_fs() const
    {
      return m_dev_fs->get_fs();
    }

  private:
      DeviceFilesytemWidget * m_dev_fs;
  };

  struct HomePartitionWidget : public WContainerWidget //final : public PartitionWidget
  {
    HomePartitionWidget(const Partitions& parts)
    {
      auto layout = setLayout(make_wt<WVBoxLayout>());

      layout->addWidget(make_wt<Wt::WText>("<h2>Home</h2>"));

      m_btn_to_root = layout->addWidget(make_wt<Wt::WRadioButton>("Mount /home to /"));

      m_btn_to_new = layout->addWidget(make_wt<Wt::WRadioButton>("Mount /home to new partition"));
      m_to_new = layout->addWidget(make_wt<DeviceFilesytemWidget>(parts, StringViewVec{"ext4"}));

      m_btn_to_existing = layout->addWidget(make_wt<Wt::WRadioButton>("Mount /home to existing partition"));
      m_to_existing = layout->addWidget(make_wt<DeviceFilesytemWidget>(parts));

      m_btn_group = std::make_shared<WButtonGroup>();
      m_btn_group->addButton(m_btn_to_root);
      m_btn_group->addButton(m_btn_to_new);
      m_btn_group->addButton(m_btn_to_existing);

      m_btn_to_root->checked().connect([this]()
      {
        m_target = HomeMountTarget::Root;
        m_to_new->setDisabled(true);
        m_to_existing->setDisabled(true);
      });

      m_btn_to_new->checked().connect([this]()
      {
        m_target = HomeMountTarget::New;
        m_to_new->setDisabled(false);
        m_to_existing->setDisabled(true);
      });

      m_btn_to_existing->checked().connect([this]()
      {
        m_target = HomeMountTarget::Existing;
        m_to_existing->setDisabled(false);
        m_to_new->setDisabled(true);
      });

      m_btn_group->setSelectedButtonIndex(0);
      m_to_new->setDisabled(true);
      m_to_existing->setDisabled(true);
    }

    bool is_home_on_root() const
    {
      return m_target == HomeMountTarget::Root;
    }

    HomeMountTarget get_mount_target() const
    {
      return m_target;
    }

    std::string get_device() const
    {
      if (is_home_on_root())
        return ""; // caller must check
      else
        return m_target == HomeMountTarget::Existing ? m_to_existing->get_device() :
                                                   m_to_new->get_device();
    }

    std::string get_fs() const
    {
      if (is_home_on_root())
        return ""; // caller must check
      else
        return m_btn_to_existing->isChecked() ? "" : m_to_new->get_fs();
    }

    private:
      WRadioButton * m_btn_to_root,
                   * m_btn_to_existing,
                   * m_btn_to_new;
      std::shared_ptr<WButtonGroup> m_btn_group;
      DeviceFilesytemWidget * m_to_existing;
      DeviceFilesytemWidget * m_to_new;
      HomeMountTarget m_target;
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

        m_boot = layout->addWidget(make_wt<BootPartitionWidget>(parts));
        m_root = layout->addWidget(make_wt<RootPartitionWidget>(parts));
        m_home = layout->addWidget(make_wt<HomePartitionWidget>(parts));

        m_messages = layout->addWidget(make_wt<MessageWidget>());

        // TODO bootloader
      }
    }

    layout->addStretch(1);
  }

  // void on_home_to_home(const bool checked)
  // {
  //   if (checked)
  //     m_home->set_device(m_root->get_device());

  //   validate_selection();
  // }

  void validate_selection()
  {
    // const auto& boot_dev = m_boot->get_device();
    // const auto& root_dev = m_root->get_device();
    // const auto& home_dev = m_home->get_device();
    // const bool home_on_root = m_home->is_home_on_root();

    // m_messages->clear_messages();

    // if (boot_dev == root_dev)
    //   m_messages->add("/boot cannot mount on the root partition", MessageWidget::Level::Error);
    // if (boot_dev == home_dev)
    //   m_messages->add("/boot cannot mount on the home partition", MessageWidget::Level::Error);
    // if (!home_on_root && home_dev == root_dev)
    //   m_messages->add("/home is mounted on the root partition", MessageWidget::Level::Warning);
  }

  const BootPartitionWidget * get_boot() const
  {
    return m_boot;
  }

  const RootPartitionWidget * get_root() const
  {
    return m_root;
  }

  const HomePartitionWidget * get_home() const
  {
    return m_home;
  }
private:
  BootPartitionWidget * m_boot;
  RootPartitionWidget * m_root;
  HomePartitionWidget * m_home;
  MessageWidget * m_messages;
};

#endif
