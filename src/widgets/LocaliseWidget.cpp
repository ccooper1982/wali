#include "wali/widgets/WaliWidget.hpp"
#include <algorithm>
#include <wali/widgets/LocaliselWidget.hpp>


static const std::vector<std::string> PriorityZones =
{
  "Europe/London",  // convenient during testing
  "US/Central",
  "US/Eastern",
  "US/Mountain",
  "US/Pacific"
};

static const std::vector<std::string> PriorityLocales =
{
  "en_GB.UTF-8",  // convenient during testing
  "en_US.UTF-8",
};

static const std::vector<std::string> PriorityKeymaps =
{
  "uk",  // convenient during testing
  "us",
};


LocaliseWidget::LocaliseWidget(WidgetDataPtr data) : WaliWidget(data, "Locale")
{
  auto layout = setLayout(make_wt<WVBoxLayout>());

  // timezones
  m_timezones = add_form_pair<WComboBox>(layout, "Timezone", 100);
  m_timezones->setStyleClass("localise");
  m_timezones->setNoSelectionEnabled(true);
  m_timezones->changed().connect([this](){ m_data->localise.timezone = m_timezones->currentText().toUTF8(); });

  const auto zones = GetTimeZones{}();
  rng::for_each(PriorityZones, [this](const auto zone){ m_timezones->addItem(zone); });
  rng::for_each(zones, [this](const auto zone){ m_timezones->addItem(zone); });

  // locales
  m_locales = add_form_pair<WComboBox>(layout, "Locale", 100);
  m_locales->setStyleClass("localise");
  m_locales->setNoSelectionEnabled(true);
  m_locales->changed().connect([this](){ m_data->localise.locale = m_locales->currentText().toUTF8(); });

  const auto locales = GetLocales{}();
  rng::for_each(PriorityLocales, [this](const auto locale){ m_locales->addItem(locale); });
  rng::for_each(locales, [this](const auto zone){ m_locales->addItem(zone); });

  // terminal key map
  m_keymap = add_form_pair<WComboBox>(layout, "Keymap", 100);
  m_keymap->setStyleClass("localise");
  m_keymap->setNoSelectionEnabled(true);
  m_keymap->changed().connect([this](){ m_data->localise.keymap = m_keymap->currentText().toUTF8(); });

  const auto keys = GetKeyMaps{}();
  rng::for_each(PriorityKeymaps, [this](const auto zone){ m_keymap->addItem(zone); });
  rng::for_each(keys, [this](const auto key){ m_keymap->addItem(key); });

  layout->addStretch(1);

  // locale optional
  set_valid(true);
}
