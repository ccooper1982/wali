
#include "wali/DiskUtils.hpp"
#include <Wt/WApplication.h>
#include <Wt/WDialog.h>
#include <Wt/WGlobal.h>
#include <Wt/WPushButton.h>
#include <Wt/WTable.h>
#include <wali/widgets/MountsWidget.hpp>
#include <wali/widgets/PartitionsWidget.hpp>
#include <wali/widgets/WidgetData.hpp>


MountsWidget::MountsWidget()
{
  auto layout = setLayout(make_wt<WVBoxLayout>());
  m_partitions = std::make_shared<Partitions>();

  m_table = layout->addWidget(make_wt<WTable>());

  auto btn_manage = layout->addWidget(make_wt<WPushButton>("Manage Partitions"), 0, Wt::AlignmentFlag::Center);
  btn_manage->setWidth(160);
  btn_manage->clicked().connect(this, [=,this]()
  {
    auto dialog = btn_manage->addChild(make_wt<WDialog>("Partitions"));
    auto partitions = dialog->contents()->addWidget(make_wt<PartitionsWidget>());
    auto btn_close = dialog->footer()->addWidget(make_wt<WPushButton>("Close"));

    dialog->rejectWhenEscapePressed();
    dialog->titleBar()->setStyleClass("partitions_dialog_title");
    dialog->contents()->setStyleClass("partitions_dialog_contents");
    dialog->footer()->setStyleClass("partitions_dialog_foot");
    dialog->titleBar()->setPadding(0);
    dialog->finished().connect([=, this]()
    {
      if (partitions->is_changed())
        refresh_data();
    });
    partitions->busy().connect([=](bool busy)
    {
      dialog->rejectWhenEscapePressed(!busy);
      btn_close->setEnabled(!busy);
    });

    btn_close->clicked().connect(dialog, &WDialog::reject);

    dialog->show();
  });

  m_boot = layout->addWidget(make_wt<BootPartitionWidget>(m_partitions, [this]{validate_selection();}));
  m_root = layout->addWidget(make_wt<RootPartitionWidget>(m_partitions, [this]{validate_selection();}));
  m_home = layout->addWidget(make_wt<HomePartitionWidget>(m_partitions, [this]{validate_selection();}));

  refresh_data();

  // TODO bootloader

  m_messages = layout->addWidget(make_wt<MessageWidget>());
  validate_selection();

  layout->addStretch(1);
}

void MountsWidget::refresh_data()
{
  auto valid_disk = [](const TreePair& pair){ return !pair.second.empty() && pair.first.is_gpt; } ;
  auto valid_partition = [](const Partition& part) { return !part.is_mounted; };

  m_tree = DiskUtils::probe();
  m_partitions->clear();
  m_table->clear();

  m_table->setHeaderCount(1);
  m_table->elementAt(0, 0)->addNew<Wt::WText>("Device");
  m_table->elementAt(0, 1)->addNew<Wt::WText>("Filesystem");
  m_table->elementAt(0, 2)->addNew<Wt::WText>("Size");
  m_table->setStyleClass("table_partitions");

  size_t r{1}; // 1 because of the header row

  for (const auto& [parent, parts] : m_tree | std::views::filter(valid_disk))
  {
    m_partitions->append_range(parts | std::views::filter(valid_partition));

    m_table->elementAt(r,0)->addNew<WText>(parent.dev);
    m_table->elementAt(r,0)->setColumnSpan(3);
    m_table->rowAt(r)->setStyleClass("partitions_parent");

    ++r;

    for(size_t i = 0 ; i < parts.size() ; ++i)
    {
      PLOGI << parts[i].dev << " = " << format_size(parts[i].size);
      m_table->elementAt(r,0)->addNew<WText>(parts[i].dev);
      m_table->elementAt(r,1)->addNew<WText>(parts[i].fs_type);
      m_table->elementAt(r,2)->addNew<WText>(format_size(parts[i].size));
      m_table->rowAt(r)->setStyleClass("partition");
      ++r;
    }
  }

  m_boot->refresh_partitions();
  m_root->refresh_partitions();
  m_home->refresh_partitions();

  WApplication::instance()->triggerUpdate();

  m_table->addStyleClass("table");
}

void MountsWidget::validate_selection()
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


MountData MountsWidget::get_data() const
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
