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

class VideoWidget : public WaliWidget<VideoData>
{
public:
  VideoWidget();

  VideoData get_data() const override;

private:
  void init_combos();
  void set_driver(WRadioButton * btn);
  void set_active(WRadioButton * btn);

private:
  std::shared_ptr<WButtonGroup> m_group_company;
  WComboBox * m_amd_driver,
            * m_nvidia_driver,
            * m_intel_driver;
  WText * m_waffle;
};

#endif
