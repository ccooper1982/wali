#ifndef WALI_DESKTOPWIDGET_H
#define WALI_DESKTOPWIDGET_H

#include "Wt/Json/Object.h"
#include "Wt/Json/Value.h"
#include <wali/Common.hpp>
#include <wali/widgets/WidgetData.hpp>
#include <Wt/WComboBox.h>
#include <Wt/WGlobal.h>
#include <Wt/WHBoxLayout.h>
#include <Wt/WRadioButton.h>
#include <Wt/WVBoxLayout.h>
#include <wali/widgets/Common.hpp>
#include <wali/widgets/VideoWidget.hpp>
#include <wali/widgets/WaliWidget.hpp>


class DesktopWidget : public WaliWidget
{
public:
  DesktopWidget(WidgetDataPtr data);

private:
  void read_profiles();
  void on_desktop_change();

private:
  std::map<std::string, Wt::Json::Object> m_profiles;
  WText * m_info;
  // WText * m_warning;
  WComboBox * m_desktops,
            * m_dm;
  VideoWidget * m_video_widget;
};

#endif
