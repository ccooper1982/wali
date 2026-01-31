#ifndef WALI_MOUNTSWIDGET_H
#define WALI_MOUNTSWIDGET_H

#include <Wt/WCheckBox.h>
#include <algorithm>
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
    : m_parts(parts)
  {
    auto layout = setLayout(make_wt<Wt::WHBoxLayout>());
    layout->setContentsMargins(0,0,0,0);

    layout->addWidget(make_wt<WText>("Device"));
    m_device = layout->addWidget(make_wt<WComboBox>());
    m_device->setWidth(150);
    m_device->setNoSelectionEnabled(true);
    m_device->changed().connect(validate);

    if (!filesystems.empty())
    {

      layout->addWidget(make_wt<Wt::WText>("Filesystem"));
      m_fs = layout->addWidget(make_wt<Wt::WComboBox>());
      m_fs->changed().connect([=]{ validate(); });

      rng::for_each(filesystems, [this](const auto& fs) { m_fs->addItem(fs.data()); });

      if (!enable_fs)
        m_fs->disable();
    }

    refresh_partitions();

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

  void refresh_partitions()
  {
    m_device->clear();
    rng::for_each(*m_parts, [this](const Partition& part) { m_device->addItem(part.dev); });
  }

private:
  WComboBox * m_device;
  WComboBox * m_fs{};
  std::shared_ptr<Partitions> m_parts;
};


class MountsWidget : public WaliWidget
{
  struct BootPartitionWidget : public WContainerWidget
  {
    BootPartitionWidget(const std::shared_ptr<Partitions> parts, Validate validate)
    {
      auto layout = setLayout(make_wt<WVBoxLayout>());
      layout->setContentsMargins(0,0,0,0);

      layout->addWidget(make_wt<Wt::WText>("<h3>Boot</h3>"));

      m_dev_fs = layout->addWidget(make_wt<DeviceFilesytemWidget>(parts, std::move(validate), StringViewVec{"vfat"}, false));
    }

    std::string get_device() const { return m_dev_fs->get_device(); }
    std::string get_fs() const { return m_dev_fs->get_fs(); }

    void refresh_partitions()
    {
      m_dev_fs->refresh_partitions();
    }

  private:
    DeviceFilesytemWidget * m_dev_fs;
  };

  struct RootPartitionWidget : public WContainerWidget
  {
    RootPartitionWidget(const std::shared_ptr<Partitions> parts, Validate validate)
    {
      auto layout = setLayout(make_wt<WVBoxLayout>());
      layout->setContentsMargins(0,0,0,0);

      layout->addWidget(make_wt<Wt::WText>("<h3>Root</h3>"));

      m_dev_fs = layout->addWidget(make_wt<DeviceFilesytemWidget>(parts, std::move(validate), StringViewVec{"ext4", "btrfs"}));
    }

    std::string get_device() const { return m_dev_fs->get_device(); }
    std::string get_fs() const { return m_dev_fs->get_fs(); }
    void refresh_partitions() { m_dev_fs->refresh_partitions(); }

  private:
      DeviceFilesytemWidget * m_dev_fs;
  };

  struct HomePartitionWidget : public WContainerWidget
  {
    HomePartitionWidget(const std::shared_ptr<Partitions> parts, Validate validate)
    {
      auto layout = setLayout(make_wt<WVBoxLayout>());
      layout->setContentsMargins(0,0,0,0);

      layout->addWidget(make_wt<Wt::WText>("<h3>Home</h3>"));

      m_btn_to_root = layout->addWidget(make_wt<Wt::WRadioButton>("Mount /home to root partition"));

      m_btn_to_new = layout->addWidget(make_wt<Wt::WRadioButton>("Mount /home to new partition"));
      m_devfs_to_new = layout->addWidget(make_wt<DeviceFilesytemWidget>(parts, validate, StringViewVec{"ext4", "btrfs"}));

      m_btn_to_existing = layout->addWidget(make_wt<Wt::WRadioButton>("Mount /home to existing partition"));
      m_devfs_to_existing = layout->addWidget(make_wt<DeviceFilesytemWidget>(parts, validate));

      m_btn_group = std::make_shared<WButtonGroup>();
      m_btn_group->addButton(m_btn_to_root);
      m_btn_group->addButton(m_btn_to_new);
      m_btn_group->addButton(m_btn_to_existing);

      m_btn_to_root->checked().connect([validate, this]()
      {
        m_target = HomeMountTarget::Root;
        m_devfs_to_new->setDisabled(true);
        m_devfs_to_existing->setDisabled(true);
        validate();
      });

      m_btn_to_new->checked().connect([validate, this]()
      {
        m_target = HomeMountTarget::New;
        m_devfs_to_new->setDisabled(false);
        m_devfs_to_existing->setDisabled(true);
        validate();
      });

      m_btn_to_existing->checked().connect([validate, this]()
      {
        m_target = HomeMountTarget::Existing;
        m_devfs_to_existing->setDisabled(false);
        m_devfs_to_new->setDisabled(true);
        validate();
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

    void refresh_partitions()
    {
      m_devfs_to_new->refresh_partitions();
      m_devfs_to_existing->refresh_partitions();
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
  MountsWidget(WidgetDataPtr data);

  void validate_selection();
  void set_data();

private:
  void refresh_data();

private:
  BootPartitionWidget * m_boot;
  RootPartitionWidget * m_root;
  HomePartitionWidget * m_home;
  WComboBox * m_boot_loader;
  WCheckBox * m_zram;
  MessageWidget * m_messages;
  Tree m_tree;
  std::shared_ptr<Partitions> m_partitions;
  WTable * m_table;
};

#endif
