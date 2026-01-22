#ifndef WALI_LOCALISEWIDGET_H
#define WALI_LOCALISEWIDGET_H

#include <Wt/WComboBox.h>
#include <wali/Common.hpp>
#include <wali/Commands.hpp>
#include <wali/widgets/Common.hpp>
#include <wali/widgets/WaliWidget.hpp>
#include <wali/widgets/WidgetData.hpp>


class LocaliseWidget : public WaliWidget
{
public:
  LocaliseWidget(WidgetDataPtr data);

private:
  WComboBox * m_timezones;
  WComboBox * m_locales;
  WComboBox * m_keymap;
};

#endif
