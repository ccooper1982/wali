#include "Wt/Json/Array.h"
#include "Wt/Json/Object.h"
#include "Wt/Json/Parser.h"
#include "Wt/Json/Serializer.h"
#include "Wt/WApplication.h"
#include <Wt/WCheckBox.h>
#include <Wt/WComboBox.h>
#include <Wt/WHBoxLayout.h>
#include <Wt/WLabel.h>
#include <Wt/WText.h>
#include <Wt/WVBoxLayout.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <sstream>
#include <string_view>
#include <wali/widgets/DesktopWidget.hpp>
#include <wali/Common.hpp>


static constexpr const auto KeyName = "name";
static constexpr const auto KeyInfo = "info";
static constexpr const auto KeyIwd = "iwd";
static constexpr const auto KeyNetManager = "netmanager";
static constexpr const auto KeyPackagesRequired = "packages_required";
static constexpr const auto KeyPackagesOptional = "packages_optional";
static constexpr const auto KeyServicesEnable   = "services_enable";


static constexpr const auto NoneJson = R"({
  "name": "None",
  "info": "No desktop, tty only.",
  "iwd": true,
  "netmanager":false,
  "packages_required": [],
  "packages_optional": [],
  "services_enable": []
})";



DesktopWidget::DesktopWidget(WidgetDataPtr data) : WaliWidget(data, "Desktop")
{
  resize(600, WLength::Auto);

  auto layout = setLayout(make_wt<WVBoxLayout>());
  layout->setContentsMargins(10,0,10,0);
  layout->setSpacing(10);

  // video drivers
  layout->addWidget(make_wt<WText>("<h3>Video Drivers</h3>"));
  m_video_widget = layout->addWidget(make_wt<VideoWidget>(m_data));
  m_video_widget->data_valid().connect([this](const bool valid){ set_valid(valid); });

  // desktop
  layout->addWidget(make_wt<WText>("<h3>Desktop</h3>"));
  layout->addWidget(make_wt<WText>("All desktops, except None, require the root partition is at least 12Gb."));

  // desktop combo
  auto de_layout = layout->addLayout(make_wt<WHBoxLayout>());

  de_layout->addWidget(make_wt<WLabel>("Desktop"));
  m_desktops = de_layout->addWidget(make_wt<WComboBox>());
  m_desktops->changed().connect([this]{ on_desktop_change(); });
  de_layout->addStretch(1);

  auto wm_layout = layout->addLayout(make_wt<WHBoxLayout>());
  wm_layout->addWidget(make_wt<WLabel>("Login"));
  m_dm = wm_layout->addWidget(make_wt<WComboBox>());
  m_dm->addItem("sddm");
  m_dm->changed().connect([this]{ m_data->desktop.dm = PackageSet{{m_dm->currentText().toUTF8()}}; });
  wm_layout->addStretch(1);

  read_profiles();

  layout->addSpacing(20);

  m_info = layout->addWidget(make_wt<WText>());

  layout->addStretch(1);

  on_desktop_change();
}


void DesktopWidget::read_profiles()
{
  static const fs::path ProfileDir {"profiles"};
  static const std::set<std::string> RequiredKeys {KeyName, KeyPackagesRequired, KeyInfo, KeyIwd, KeyNetManager};

  auto object_valid = [](const std::set<std::string>& keys)
  {
    // TODO validate key's value type
    const auto has_keys = rng::includes(keys, RequiredKeys);
    if (!has_keys)
    {
      PLOGE << "DesktopWidget: invalid JSON";
    }
    return has_keys;
  };

  auto read_file = [=, this](const std::string& name, const std::string& json) -> std::optional<std::string>
  {
    Wt::Json::ParseError error;
    if (Wt::Json::Object root; !Wt::Json::parse(json, root, error))
    {
      PLOGE << "JSON error whilst parsing: " << name << " - " << error.what();
      return {};
    }
    else if (const auto& keys = root.names(); object_valid(keys))
    {
      const std::string name = root["name"];
      m_profiles.emplace(std::move(name), std::move(root));
      return name;
    }
    return {};
  };


  // "None" is default
  if (const auto name = read_file("None", NoneJson); name)
    m_desktops->addItem(*name);

  const auto path = WApplication::instance()->docRoot() / ProfileDir;

  if (!fs::exists(path))
  {
    PLOGE << "Profile path does not exist: " << path.string();
    return;
  }

  for(const auto& entry : fs::directory_iterator{path} | view::filter([](const fs::directory_entry& e){ return e.path().extension() == ".json"; }))
  {
    PLOGI << "Reading profile: " << entry.path().string();

    std::stringstream buff;
    {
      std::ifstream stream {entry.path()};
      buff << stream.rdbuf();
    }

    if (const auto name = read_file(entry.path().string(), buff.str()); name)
      m_desktops->addItem(*name);
  }
}


void DesktopWidget::on_desktop_change()
{
  const auto name = m_desktops->currentText().toUTF8();

  if (!m_profiles.contains(name))
    return;

  const auto& profile = m_profiles.at(name);

  m_info->setText((std::string)profile.get(KeyInfo));

  m_data->desktop.iwd = profile.get(KeyIwd);
  m_data->desktop.netmanager = profile.get(KeyNetManager);

  m_data->desktop.desktop.clear();
  m_data->desktop.dm.clear();
  m_data->desktop.services.clear();
  //m_warning->setText("");

  for (const auto& value : (Json::Array)profile.get(KeyPackagesRequired))
    m_data->desktop.desktop.emplace((PackageSet::value_type)value);

  for (const auto& value : (Json::Array)profile.get(KeyPackagesOptional))
    m_data->desktop.desktop.emplace((PackageSet::value_type)value);

  for (const auto& value : (Json::Array)profile.get(KeyServicesEnable))
    m_data->desktop.services.emplace((PackageSet::value_type)value);

  m_dm->setEnabled(m_desktops->currentIndex() != 0);

  if (m_desktops->currentIndex() != 0)
  {
    m_data->desktop.dm.emplace("sddm");
    m_data->desktop.services.emplace("sddm.service");

    // if (const auto size = DiskUtils::get_disk_size(m_data->mounts.root_dev); size)
    // {
    //   PLOGI << "Root dev size: " << *size;

    //   if (size < gb_to_b(12))
    //     m_warning->setText("<span style='color: red;'>This profile requires the root partition is at least 12Gb.</span>");
    // }
  }
}
