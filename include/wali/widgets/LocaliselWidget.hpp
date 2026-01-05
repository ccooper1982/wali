#ifndef WALI_LOCALISEWIDGET_H
#define WALI_LOCALISEWIDGET_H

#include <Wt/WComboBox.h>
#include <wali/Common.hpp>
#include <wali/Commands.hpp>
#include <wali/widgets/Common.hpp>
#include <wali/widgets/WaliWidget.hpp>

struct LocaliseData
{
  std::string timezone;
  std::string locale;
  std::string keymap;
};

class LocaliseWidget : public WaliWidget<LocaliseData>
{
  inline static const std::vector<std::string> PriorityZones =
  {
    "Europe/London",  // not really, but convenient for me during testing
    "US/Central",
    "US/Eastern",
    "US/Mountain",
    "US/Pacific"
  };

  inline static const std::vector<std::string> PriorityLocales =
  {
    "en_GB.UTF-8",  // not really, but convenient for me during testing
    "en_US.UTF-8",
  };

  inline static const std::vector<std::string> PriorityKeymaps =
  {
    "uk",  // not really, but convenient for me during testing
    "us",
  };


public:
  LocaliseWidget()
  {
    auto layout = setLayout(make_wt<WVBoxLayout>());

    // timezones
    m_timezones = add_form_pair<WComboBox>(layout, "Timezone");
    m_timezones->setStyleClass("timezones");
    m_timezones->setNoSelectionEnabled(true);

    const auto zones = GetTimeZones{}();
    for_each(PriorityZones, [this](const auto zone){ m_timezones->addItem(zone); });
    for_each(zones, [this](const auto zone){ m_timezones->addItem(zone); });

    // locales
    m_locales = add_form_pair<WComboBox>(layout, "Locale");
    m_locales->setStyleClass("locales");
    m_locales->setNoSelectionEnabled(true);

    const auto locales = GetLocales{}();
    for_each(PriorityLocales, [this](const auto locale){ m_locales->addItem(locale); });
    for_each(locales, [this](const auto zone){ m_locales->addItem(zone); });

    // terminal key map
    m_keymap = add_form_pair<WComboBox>(layout, "Keymap");
    m_keymap->setStyleClass("keymaps");
    m_keymap->setNoSelectionEnabled(true);

    const auto keys = GetKeyMaps{}();
    for_each(PriorityKeymaps, [this](const auto zone){ m_keymap->addItem(zone); });
    for_each(keys, [this](const auto key){ m_keymap->addItem(key); });

    layout->addStretch(1);
  }

  virtual LocaliseData get_data() const override
  {
    return {
              .timezone = m_timezones->valueText().toUTF8(),
              .locale = m_locales->valueText().toUTF8(),
              .keymap = m_keymap->valueText().toUTF8()
           };
  }

private:
  WComboBox * m_timezones;
  WComboBox * m_locales;
  WComboBox * m_keymap;
};

#endif
