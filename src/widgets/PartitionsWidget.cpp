#include <Wt/WCheckBox.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WGlobal.h>
#include <cstdlib>
#include <exception>
#include <ranges>
#include <wali/Common.hpp>
#include <wali/Commands.hpp>
#include <wali/DiskUtils.hpp>
#include <wali/widgets/Common.hpp>
#include <Wt/WApplication.h>
#include <Wt/WComboBox.h>
#include <Wt/WCompositeWidget.h>
#include <Wt/WHBoxLayout.h>
#include <Wt/WPushButton.h>
#include <Wt/WSpinBox.h>
#include <Wt/WServer.h>
#include <Wt/WText.h>
#include <Wt/WVBoxLayout.h>
#include <wali/widgets/PartitionsWidget.hpp>


static const constexpr auto waffle_info = R"(
  This will delete all partitions and create new partitions as configured.
  <br/>
  To create a dedicated partition for /home, set "Home" to "Use remaining".
  <ul>
    <li>
      <b>Boot:</b> 1GB is preferred
    </li>
    <li>
      <b>Root:</b> At least 32GB
    </li>
    <li>
      <b>Home:</b> At least 64GB.
                   To use a dedicated partition, select "Use remaining". Otherwise
                   /home must mount to the root partition.
    </li>
  </ul>
  )";

static const constexpr auto waffle_warning = R"(
  <p style="color: red;">When you press 'Create', all data on the selected device is lost.</p>
  )";

// TODO user defined literal

static const constexpr int64_t B_MB = 1024*1024;
static const constexpr int64_t B_GB = 1024*1024*1024;
static const constexpr int64_t MB_GB = 1024;

static constexpr int64_t b_to_gb(const int64_t n) { return n / B_GB; }
static constexpr int64_t mb_to_b(const int64_t n) { return n * B_MB; }
static constexpr int64_t gb_to_b(const int64_t n) { return n * B_GB; }
static constexpr int64_t gb_to_mb(const int64_t n) { return n * MB_GB; }

static const int64_t BootSizeMin = mb_to_b(512);
static const int64_t RootSizeMin = gb_to_b(5);


PartitionsWidget::PartitionsWidget()
{
  auto layout = setLayout(make_wt<WVBoxLayout>());
  layout->setSpacing(20);
  layout->addWidget(make_wt<WText>(waffle_info));

  auto disk_layout = layout->addLayout(make_wt<WHBoxLayout>());
  auto metrics_layout = layout->addLayout(make_wt<WHBoxLayout>());
  auto boot_layout = layout->addLayout(make_wt<WHBoxLayout>());
  auto root_layout = layout->addLayout(make_wt<WHBoxLayout>());
  auto home_layout = layout->addLayout(make_wt<WHBoxLayout>());


  auto lbl_disk = disk_layout->addWidget(make_wt<WLabel>("Disk"));
  lbl_disk->setWidth(100);
  m_disk = disk_layout->addWidget(make_wt<WComboBox>());
  m_disk->changed().connect(this, &PartitionsWidget::calculate_sizes);
  disk_layout->addStretch(1);

  m_total = metrics_layout->addWidget(make_wt<WText>());
  m_remaining = metrics_layout->addWidget(make_wt<WText>());
  metrics_layout->addStretch(1);

  auto lbl_boot = boot_layout->addWidget(make_wt<WLabel>("Boot/EFI"));
  lbl_boot->setWidth(100);
  m_boot = boot_layout->addWidget(make_wt<WComboBox>());
  m_boot->changed().connect(this, &PartitionsWidget::calculate_sizes);
  m_boot->addItem("1024");
  m_boot->addItem("512");
  boot_layout->addWidget(make_wt<WLabel>("MB"));
  boot_layout->addStretch(1);

  auto lbl_root = root_layout->addWidget(make_wt<WLabel>("Root"));
  lbl_root->setWidth(100);
  m_root = root_layout->addWidget(make_wt<WSpinBox>());
  m_root->changed().connect(this, &PartitionsWidget::calculate_sizes);
  m_root->setValue(b_to_gb(RootSizeMin));
  m_root->setSingleStep(100);
  m_root->setStyleClass("partition_size");
  root_layout->addWidget(make_wt<WLabel>("GB"));
  root_layout->addStretch(1);

  auto lbl_home = home_layout->addWidget(make_wt<WLabel>("Home"));
  lbl_home->setWidth(100);
  m_home = home_layout->addWidget(make_wt<WComboBox>());
  m_home->addItem("Use remaining");
  m_home->addItem("Do nothing");
  home_layout->addStretch(1);

  layout->addWidget(make_wt<WText>(waffle_warning));

  m_create = layout->addWidget(make_wt<WPushButton>("Create"));
  m_create->clicked().connect(this, &PartitionsWidget::create);

  layout->addStretch(1);

  read_partitions();
  calculate_sizes();
}

void PartitionsWidget::read_partitions()
{
  m_tree = DiskUtils::probe();
  set_devices();
}

void PartitionsWidget::set_devices()
{
  static const constexpr int64_t MinDevSize = BootSizeMin + RootSizeMin;

  for (const auto& disk : m_tree | std::views::keys)
  {
    if (disk.size >= MinDevSize)
    {
      PLOGI << disk.dev << " : " << format_size(disk.size);
      m_disk->addItem(disk.dev);
      m_disk_sizes[disk.dev] = disk.size;
    }
  }
}

void PartitionsWidget::calculate_sizes()
{
  const auto& disk = m_disk->currentText().toUTF8();

  if (!m_tree.contains(disk) || !m_disk_sizes.contains(disk))
    PLOGE << "Disk not present in tree or disk sizes"; // shouldn't happen
  else
  {
    const auto disk_size = m_disk_sizes.at(disk);
    const auto boot_size = mb_to_b(std::strtoll(m_boot->currentText().toUTF8().data(), nullptr, 10));
    const int64_t max_root_size = disk_size - boot_size;

    m_total->setText(std::format("<b>Size:</b> {}", format_size(disk_size)));
    m_root->setRange(b_to_gb(RootSizeMin), b_to_gb(max_root_size));
    //m_root->setValue(std::clamp(m_root->value(), m_root->minimum(), m_root->maximum()));

    const auto root_size = gb_to_b(std::strtoll(m_root->text().toUTF8().data(), nullptr, 10));
    m_remaining->setText(std::format("<b>Remaining:</b> {}", format_size(disk_size - boot_size - root_size)));
  }
}

void PartitionsWidget::create()
{
  auto log = [this](const std::string_view m)
  {
    PLOGI << m;
  };

  m_evt_busy(true);
  m_create->disable();

  WServer::instance()->post(WApplication::instance()->sessionId(), [=, this]()
  {
    try
    {
      const auto& disk =  m_disk->currentText().toUTF8();

      if (!CreatePartitionTable{}(disk, log))
      {
        // TODO something
        PLOGE << "Failed to create parition table";
      }
      else
      {
        const int64_t boot_size = std::strtoll(m_boot->currentText().toUTF8().data(), nullptr, 10);
        const int64_t root_size = gb_to_mb(std::strtoll(m_root->text().toUTF8().data(), nullptr, 10));

        PLOGW << "Boot: " << m_boot->currentText().toUTF8().data() << "MB, " << boot_size << "MB";
        PLOGW << "Root: " << m_root->text().toUTF8().data() << "GB, " << root_size << "MB";

        CreatePartition{}(disk, 1, boot_size, log);
        CreatePartition{}(disk, 2, root_size, log);

        SetPartitionType{}(disk, 1, PartTypeEfi);
        SetPartitionType{}(disk, 2, PartTypeRoot);

        if (m_home->currentIndex() == 0)
        {
          CreatePartition{}(disk, 3, log);
          SetPartitionType{}(disk, 3, PartTypeHome);
        }
      }

      m_changed = true;
    }
    catch (const std::exception& ex)
    {
      PLOGE << "PartitionsWidget::create() exception: " <<ex.what();
    }

    m_evt_busy(false);
    m_create->enable();
    WApplication::instance()->triggerUpdate();
  });
}
