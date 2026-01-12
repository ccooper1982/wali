#ifndef WALI_MOUNTSWIDGET_H
#define WALI_MOUNTSWIDGET_H

#include <functional>
#include <memory>

#include <Wt/WComboBox.h>
#include <Wt/WGlobal.h>
#include <Wt/WRadioButton.h>
#include <Wt/WButtonGroup.h>
#include <Wt/WTable.h>
#include <Wt/WVBoxLayout.h>

#include <wali/widgets/MessagesWidget.hpp>
#include <wali/widgets/WidgetData.hpp>
#include <wali/widgets/WaliWidget.hpp>
#include <wali/Common.hpp>
#include <wali/DiskUtils.hpp>


using Validate = std::function<void()>;


// Draws a Device dropdown and Filesystem dropdown (optional)
struct DeviceFilesytemWidget : public WContainerWidget
{
  DeviceFilesytemWidget(const std::shared_ptr<Partitions> parts, Validate validate, const StringViewVec filesystems = {}, const bool enable_fs = true)
  {
    auto layout = setLayout(make_wt<Wt::WHBoxLayout>());

    layout->addWidget(make_wt<WText>("Device"));
    m_device = layout->addWidget(make_wt<WComboBox>());
    m_device->setWidth(150);
    m_device->setNoSelectionEnabled(true);
    m_device->changed().connect(validate);

    if (!filesystems.empty())
    {
      layout->addWidget(make_wt<Wt::WText>("Filesystem"));
      m_fs = layout->addWidget(make_wt<Wt::WComboBox>());

      for_each(filesystems, [this](const auto& fs) { m_fs->addItem(fs.data()); });

      if (!enable_fs)
        m_fs->disable();
    }

    for_each(*parts, [this](const Partition& part) { m_device->addItem(part.dev); });

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


class MountsWidget : public WaliWidget<MountData>
{
  struct BootPartitionWidget : public WContainerWidget
  {
    BootPartitionWidget(const std::shared_ptr<Partitions> parts, Validate validate)
    {
      auto layout = setLayout(make_wt<WVBoxLayout>());

      layout->addWidget(make_wt<Wt::WText>("<h3>Boot</h3>"));

      m_dev_fs = layout->addWidget(make_wt<DeviceFilesytemWidget>(parts, std::move(validate), StringViewVec{"vfat"}, false));
    }

    std::string get_device() const { return m_dev_fs->get_device(); }
    std::string get_fs() const { return m_dev_fs->get_fs(); }

  private:
    DeviceFilesytemWidget * m_dev_fs;
  };

  struct RootPartitionWidget : public WContainerWidget
  {
    RootPartitionWidget(const std::shared_ptr<Partitions> parts, Validate validate)
    {
      auto layout = setLayout(make_wt<WVBoxLayout>());

      layout->addWidget(make_wt<Wt::WText>("<h3>Root</h3>"));

      m_dev_fs = layout->addWidget(make_wt<DeviceFilesytemWidget>(parts, std::move(validate), StringViewVec{"ext4"}));
    }

    std::string get_device() const { return m_dev_fs->get_device(); }
    std::string get_fs() const { return m_dev_fs->get_fs(); }

  private:
      DeviceFilesytemWidget * m_dev_fs;
  };

  struct HomePartitionWidget : public WContainerWidget
  {
    HomePartitionWidget(const std::shared_ptr<Partitions> parts, Validate validate)
    {
      auto layout = setLayout(make_wt<WVBoxLayout>());

      layout->addWidget(make_wt<Wt::WText>("<h3>Home</h3>"));

      m_btn_to_root = layout->addWidget(make_wt<Wt::WRadioButton>("Mount /home to /"));

      m_btn_to_new = layout->addWidget(make_wt<Wt::WRadioButton>("Mount /home to new partition"));
      m_devfs_to_new = layout->addWidget(make_wt<DeviceFilesytemWidget>(parts, validate, StringViewVec{"ext4"}));

      m_btn_to_existing = layout->addWidget(make_wt<Wt::WRadioButton>("Mount /home to existing partition"));
      m_devfs_to_existing = layout->addWidget(make_wt<DeviceFilesytemWidget>(parts, validate));

      m_btn_group = std::make_shared<WButtonGroup>();
      m_btn_group->addButton(m_btn_to_root);
      m_btn_group->addButton(m_btn_to_new);
      m_btn_group->addButton(m_btn_to_existing);

      m_btn_to_root->checked().connect([this]()
      {
        m_target = HomeMountTarget::Root;
        m_devfs_to_new->setDisabled(true);
        m_devfs_to_existing->setDisabled(true);
      });

      m_btn_to_new->checked().connect([this]()
      {
        m_target = HomeMountTarget::New;
        m_devfs_to_new->setDisabled(false);
        m_devfs_to_existing->setDisabled(true);
      });

      m_btn_to_existing->checked().connect([this]()
      {
        m_target = HomeMountTarget::Existing;
        m_devfs_to_existing->setDisabled(false);
        m_devfs_to_new->setDisabled(true);
      });

      m_target = HomeMountTarget::Root;
      m_devfs_to_new->setDisabled(true);
      m_devfs_to_existing->setDisabled(true);

      m_btn_group->setSelectedButtonIndex(0);
    }

    bool is_home_on_root() const { return m_target == HomeMountTarget::Root; }
    HomeMountTarget get_mount_target() const { return m_target; }

    std::string get_device() const
    {
      if (is_home_on_root())
        return ""; // caller must check
      else
        return m_target == HomeMountTarget::Existing ?  m_devfs_to_existing->get_device() :
                                                        m_devfs_to_new->get_device();
    }

    std::string get_fs() const
    {
      if (is_home_on_root())
        return ""; // caller must check
      else
        return m_btn_to_existing->isChecked() ? "" : m_devfs_to_new->get_fs();
    }

  private:
    WRadioButton * m_btn_to_root,
                  * m_btn_to_existing,
                  * m_btn_to_new;
    std::shared_ptr<WButtonGroup> m_btn_group;
    DeviceFilesytemWidget * m_devfs_to_existing;
    DeviceFilesytemWidget * m_devfs_to_new;
    HomeMountTarget m_target;
  };

public:
  MountsWidget();

  void validate_selection();

  MountData get_data() const override;

  bool is_valid() const override { return !m_messages->has_errors(); }

private:
  void create_table();

private:
  BootPartitionWidget * m_boot;
  RootPartitionWidget * m_root;
  HomePartitionWidget * m_home;
  MessageWidget * m_messages;
  Tree m_tree;
  std::shared_ptr<Partitions> m_partitions;
  WTable * m_table;
};

#endif
