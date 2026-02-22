#ifndef WALI_VIDEOWIDGET_H
#define WALI_VIDEOWIDGET_H

#include <wali/Common.hpp>
#include <wali/widgets/WidgetData.hpp>
#include <Wt/WComboBox.h>
#include <Wt/WGlobal.h>
#include <Wt/WHBoxLayout.h>
#include <Wt/WRadioButton.h>
#include <Wt/WVBoxLayout.h>
#include <wali/widgets/Common.hpp>
#include <wali/widgets/WaliWidget.hpp>

class VideoWidget : public WaliWidget
{
public:
  VideoWidget(WidgetDataPtr data);

private:
  void init_combos();
  void set_default_driver();
  void on_vendor_change(WRadioButton * btn);
  void on_driver_change(WComboBox * btn);
  void set_waffle();
  bool check_validity() const;
  void set_data();

private:
  std::shared_ptr<WButtonGroup> m_group_vendor;
  WComboBox * m_amd_driver,
            * m_nvidia_driver,
            * m_intel_driver;
  WText * m_waffle;
};

#endif
