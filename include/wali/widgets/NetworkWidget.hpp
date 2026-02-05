#ifndef WALI_NETWORKWIDGET_H
#define WALI_NETWORKWIDGET_H

#include <wali/Common.hpp>
#include <wali/widgets/Common.hpp>
#include <wali/widgets/WaliWidget.hpp>
#include <wali/widgets/WidgetData.hpp>


class NetworkWidget : public WaliWidget
{
public:
  NetworkWidget(WidgetDataPtr data);

  // NetworkData get_data() const override { return m_data; }
  // bool is_valid() const override;

private:
  WLineEdit * m_hostname;
  WCheckBox * m_ntp;
  WCheckBox * m_copy_config;
};


#endif
