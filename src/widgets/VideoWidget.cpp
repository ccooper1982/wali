#include "wali/Commands.hpp"
#include "wali/Common.hpp"
#include "wali/widgets/WaliWidget.hpp"
#include "wali/widgets/WidgetData.hpp"
#include <Wt/WApplication.h>
#include <Wt/WComboBox.h>
#include <wali/widgets/VideoWidget.hpp>
#include <ranges>

static constexpr const auto none_waffle = R"(
  <b>None</b>
  <br/>
  <ul>
    <li>Use the default modesetting driver</li>
    <li>Not ideal for desktop environments</li>
  </ul>
)";

static constexpr const  auto amd_waffle = R"(
  <b>AMDGPU</b>
  <ul>
    <li>Radeon 7000+ (released in 2012 and newer)</li>
  </ul>
  <br/>
  <b>ATI</b>
  <ul>
    <li>Precedes Radeon 7000 (released before 2012)</li>
  </ul>
)";

static constexpr const  auto nvidia_waffle = R"(
  <b>Nvidia Open</b>
  <ul>
    <li>Partially open source</li>
    <li>Turing (Titan RTX, GeForce RTX20xx, RTX16xx) onwards</li>
    <li>
        If your GPU predates the above, you will need to install via the AUR after install.
        See <a href="https://wiki.archlinux.org/title/Graphics_processing_unit#NVIDIA">guide</a>
    </li>
  </ul>
  <br/>
  <b>Nouveau</b>
  <ul>
    <li>Open source</li>
    <li>No Vulkan driver for cards prior to Kepler (GTX 660 / Quadro K3000)</li>
  </ul>
)";

static constexpr const auto intel_waffle = R"(
  <b>Gen 8+</b>
  <ul>
    <li>Cards from HD400 (released 2015) onwards</li>
  </ul>
  <br/>
  <b>Gen 3 - Gen 7.5</b>
  <ul>
    <li>Cards from GMA900 (released 2004) to Iris 5200 (released 2013)</li>
  </ul>
  <br/>
  See Intel GPU <a href="https://en.wikipedia.org/wiki/List_of_Intel_graphics_processing_units">families</a>.
)";

static constexpr const auto vm_waffle = R"(
  <b>Virtual Machine</b>
  <ul>
    <li>Installs <code>xf86-video-vmware</code></li>
  </ul>
)";

static const PackageSet AmdGpuPackages = {"mesa", "vulkan-radeon", "xf86-video-amdgpu"};
static const PackageSet AtiPackages = {"mesa-amber", "xf86-video-ati"};

static const PackageSet NouveaPackages = {"mesa", "vulkan-nouveau", "xf86-video-nouveau"};
static const PackageSet NvidiaOpenPackages = {"nvidia-open", "nvidia-utils"};

// Intel Gen2 requires mesa-amber, but was released in 2002 so unlikely to be a problem
static const PackageSet IntelGen8Packages = {"mesa", "vulkan-intel", "xf86-video-intel", "intel-media-driver"};
static const PackageSet IntelPreGen8Packages = {"mesa", "vulkan-intel", "xf86-video-intel", "libva-media-driver"};

static const PackageSet VmPackages = {"xf86-video-vmware"};

static const std::map<std::string, PackageSet> AmdDriverMap =
{
  {"AMDGPU", AmdGpuPackages},
  {"ATI", AtiPackages}
};

static const std::map<std::string, PackageSet> NvidiaDriverMap =
{
  {"Nouvea", NouveaPackages},
  {"Nvidia Open", NvidiaOpenPackages}
};

static const std::map<std::string, PackageSet> IntelDriverMap =
{
  {"Gen 8+", IntelGen8Packages},
  {"Gen 3 - Gen 7.5", IntelPreGen8Packages}
};

static const std::map<std::string, PackageSet> VmDriverMap =
{
  {"VM", VmPackages}
};


VideoWidget::VideoWidget(WidgetDataPtr data) : WaliWidget(data, "Video")
{
  auto layout = setLayout(make_wt<WVBoxLayout>());

  auto layout_company = layout->addLayout(make_wt<WHBoxLayout>());
  layout_company->setSpacing(20);

  auto layout_none = layout_company->addLayout(make_wt<WVBoxLayout>());
  auto layout_vm = layout_company->addLayout(make_wt<WVBoxLayout>());
  auto layout_amd = layout_company->addLayout(make_wt<WVBoxLayout>());
  auto layout_nvidia = layout_company->addLayout(make_wt<WVBoxLayout>());
  auto layout_intel = layout_company->addLayout(make_wt<WVBoxLayout>());

  m_group_vendor = std::make_shared<WButtonGroup>();
  m_group_vendor->addButton(layout_none->addWidget(make_wt<WRadioButton>("None")));
  m_group_vendor->addButton(layout_vm->addWidget(make_wt<WRadioButton>("VM")));
  m_group_vendor->addButton(layout_amd->addWidget(make_wt<WRadioButton>("AMD")));
  m_group_vendor->addButton(layout_nvidia->addWidget(make_wt<WRadioButton>("Nvidia")));
  m_group_vendor->addButton(layout_intel->addWidget(make_wt<WRadioButton>("Intel")));
  m_group_vendor->setSelectedButtonIndex(0);
  m_group_vendor->checkedChanged().connect(this, [this](WRadioButton * btn)
  {
    set_selected_driver(btn);
  });

  layout_none->addStretch(1);
  layout_vm->addStretch(1);

  m_amd_driver = layout_amd->addWidget(make_wt<WComboBox>());
  m_amd_driver->changed().connect([this](){ set_valid(true); });
  layout_amd->addStretch(1);

  m_nvidia_driver = layout_nvidia->addWidget(make_wt<WComboBox>());
  m_nvidia_driver->changed().connect([this](){ set_valid(true); });
  layout_nvidia->addStretch(1);

  m_intel_driver = layout_intel->addWidget(make_wt<WComboBox>());
  m_intel_driver->changed().connect([this](){ set_valid(true); });
  layout_intel->addStretch(1);

  m_amd_driver->setNoSelectionEnabled(true);
  m_nvidia_driver->setNoSelectionEnabled(true);
  m_intel_driver->setNoSelectionEnabled(true);

  m_waffle = layout->addWidget(make_wt<WText>());

  init_combos();
  set_default_driver();

  layout_company->addStretch(1);
  layout->addStretch(1);

  set_valid(check_validity());
}

void VideoWidget::init_combos()
{
  auto add = [](const std::map<std::string, PackageSet>& m, WComboBox * combo)
  {
    for (const auto& driver : m | std::views::keys)
      combo->addItem(driver);
  };

  add(AmdDriverMap, m_amd_driver);
  add(NvidiaDriverMap, m_nvidia_driver);
  add(IntelDriverMap, m_intel_driver);
}

void VideoWidget::set_default_driver()
{
  switch (GetGpuVendor{}())
  {
    case GpuVendor::Vm:
      m_group_vendor->setSelectedButtonIndex(1);
    break;

    case GpuVendor::Amd:
      m_group_vendor->setSelectedButtonIndex(2);
    break;

    case GpuVendor::Nvidia:
      m_group_vendor->setSelectedButtonIndex(3);
    break;

    case GpuVendor::Intel:
      m_group_vendor->setSelectedButtonIndex(4);
    break;

    default:
      m_group_vendor->setSelectedButtonIndex(0);
    break;
  }

  set_selected_driver(m_group_vendor->checkedButton());
}

void VideoWidget::set_waffle()
{
  const auto& selected = m_group_vendor->checkedButton()->text().toUTF8();

  std::string_view waffle{none_waffle};

  if (selected == "AMD")
    waffle = amd_waffle;
  else if (selected == "Nvidia")
    waffle = nvidia_waffle;
  else if (selected == "Intel")
    waffle = intel_waffle;
  else if (selected == "VM")
    waffle = vm_waffle;

  m_waffle->setText(waffle.data());
}

void VideoWidget::set_selected_driver(WRadioButton * btn)
{
  WApplication::instance()->deferRendering();

  m_amd_driver->disable();
  m_nvidia_driver->disable(),
  m_intel_driver->disable();

  const auto& selected = btn->text().toUTF8();

  if (selected == "AMD")
    m_amd_driver->enable();
  else if (selected == "Nvidia")
    m_nvidia_driver->enable();
  else if (selected == "Intel")
    m_intel_driver->enable();

  set_waffle();

  set_valid(check_validity());

  WApplication::instance()->resumeRendering();
}


void VideoWidget::set_data()
{
  const auto& selected = m_group_vendor->checkedButton()->text().toUTF8();

  WString name;

  if (selected == "AMD")
  {
    name = m_amd_driver->currentText();
    m_data->video.drivers = AmdDriverMap.at(name.toUTF8());
  }
  else if (selected == "Nvidia")
  {
    name = m_nvidia_driver->currentText();
    m_data->video.drivers = NvidiaDriverMap.at(name.toUTF8());
  }
  else if (selected == "Intel")
  {
    name = m_intel_driver->currentText();
    m_data->video.drivers = IntelDriverMap.at(name.toUTF8());
  }
  else if (selected == "VM")
    m_data->video.drivers = VmDriverMap.at("VM");
  else
  {
    PLOGE << "VideoWidget::get_data() should not be here";
    m_data->video.drivers.clear();
  }
}


bool VideoWidget::check_validity() const
{
  // None and VM don't have options
  if (const auto index = m_group_vendor->selectedButtonIndex(); index == 0 || index == 1)
    return true;
  else if (index == 2)
    return !m_amd_driver->currentText().empty();
  else if (index == 3)
    return !m_nvidia_driver->currentText().empty();
  else if (index == 4)
    return !m_intel_driver->currentText().empty();
  else
  {
    PLOGE << "VideoWidget::is_valid() : shouldn't reach here";
    return false;
  }
}
