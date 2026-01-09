#include <wali/widgets/LocaliselWidget.hpp>


LocaliseWidget::LocaliseWidget()
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

LocaliseData LocaliseWidget::get_data() const
{
  return {
            .timezone = m_timezones->currentText().toUTF8(),
            .locale = m_locales->currentText().toUTF8(),
            .keymap = m_keymap->currentText().toUTF8()
         };
}
