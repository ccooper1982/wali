#include <wali/widgets/PartitionWidget.hpp>


PartitionsWidget::PartitionsWidget()
{
  auto layout = setLayout(make_wt<Wt::WVBoxLayout>());

  if (!PartitionUtils::probe_for_install())
    PLOGE << "Probe failed\n";
  else
  {
    if (!PartitionUtils::have_devices())
      PLOGE << "No applicable devices found\n";
    else
    {
      auto has_partitions = [](const TreePair& pair){ return !pair.second.empty(); } ;

      m_tree = PartitionUtils::partitions();
      Partitions devices;

      auto table_parts = layout->addWidget(make_wt<Wt::WTable>());
      table_parts->setHeaderCount(1);
      table_parts->elementAt(0, 0)->addNew<Wt::WText>("Device");
      table_parts->elementAt(0, 1)->addNew<Wt::WText>("Filesystem");
      table_parts->elementAt(0, 2)->addNew<Wt::WText>("Size");
      table_parts->setStyleClass("table_partitions");

      size_t r{1}; // 1 because of the header row

      for (const auto& [parent, parts] : m_tree | std::views::filter(has_partitions))
      {
        devices.append_range(parts);

        table_parts->elementAt(r,0)->addNew<WText>(parent);
        table_parts->elementAt(r,0)->setColumnSpan(3);
        table_parts->rowAt(r)->setStyleClass("partitions_parent");

        ++r;

        for(size_t i = 0 ; i < parts.size() ; ++i)
        {
          table_parts->elementAt(r,0)->addNew<WText>(parts[i].dev);
          table_parts->elementAt(r,1)->addNew<WText>(parts[i].fs_type);
          table_parts->elementAt(r,2)->addNew<WText>(format_size(parts[i].size));
          table_parts->rowAt(r)->setStyleClass("partition");
          ++r;
        }
      }

      m_boot = layout->addWidget(make_wt<BootPartitionWidget>(devices, [this]{validate_selection();}));
      m_root = layout->addWidget(make_wt<RootPartitionWidget>(devices, [this]{validate_selection();}));
      m_home = layout->addWidget(make_wt<HomePartitionWidget>(devices, [this]{validate_selection();}));

      table_parts->addStyleClass("table");

      // TODO bootloader
    }

    m_messages = layout->addWidget(make_wt<MessageWidget>());
    validate_selection();
  }

  layout->addStretch(1);
}

void PartitionsWidget::validate_selection()
{
  const auto& boot_dev = m_boot->get_device();
  const auto& root_dev = m_root->get_device();

  m_messages->clear_messages();

  if (boot_dev == root_dev)
    m_messages->add("Boot and root must be on separate partitions", MessageWidget::Level::Error);
  else if (boot_dev.empty())
    m_messages->add("Boot not set", MessageWidget::Level::Error);
  else if (root_dev.empty())
    m_messages->add("Root not set", MessageWidget::Level::Error);

  const auto target = m_home->get_mount_target();
  if (target != HomeMountTarget::Root)
  {
    const auto& home_dev = m_home->get_device();
    if (home_dev == boot_dev || home_dev == root_dev)
      m_messages->add("/home is mounted to root or boot partition", MessageWidget::Level::Error);
  }
}


PartData PartitionsWidget::get_data() const
{
  const auto root_dev = m_root->get_device();
  const auto root_fs = m_root->get_fs();
  const auto home_dev = m_home->is_home_on_root() ? root_dev : m_home->get_device();
  const auto home_fs = m_home->is_home_on_root() ? root_fs : m_home->get_fs();

  return {
            .boot_dev = m_boot->get_device(),
            .boot_fs = m_boot->get_fs(),
            .root_dev = root_dev,
            .root_fs = root_fs,
            .home_dev = home_dev,
            .home_fs = home_fs,
            .home_target = m_home->get_mount_target()
         };
}
