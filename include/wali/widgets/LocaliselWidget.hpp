#ifndef WALI_LOCALISEWIDGET_H
#define WALI_LOCALISEWIDGET_H

#include <Wt/WComboBox.h>
#include <wali/Common.hpp>
#include <wali/Commands.hpp>
#include <wali/widgets/Common.hpp>
#include <wali/widgets/WaliWidget.hpp>
#include <wali/widgets/WidgetData.hpp>


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
  LocaliseWidget();

  LocaliseData get_data() const override;

private:
  WComboBox * m_timezones;
  WComboBox * m_locales;
  WComboBox * m_keymap;
};

#endif
