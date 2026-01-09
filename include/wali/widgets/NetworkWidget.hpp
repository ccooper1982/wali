#ifndef WALI_NETWORKWIDGET_H
#define WALI_NETWORKWIDGET_H

#include <wali/Common.hpp>
#include <wali/widgets/Common.hpp>
#include <wali/widgets/WaliWidget.hpp>
#include <wali/widgets/WidgetData.hpp>


class NetworkWidget : public WaliWidget<NetworkData>
{
  static constexpr auto text = "<ul><li>If using Wi-Fi, copy the iwd config to have "
                               "internet connectivity in the live system</li></ul>";

public:
  NetworkWidget();

  NetworkData get_data() const override { return m_data; }
  bool is_valid() const override { return !m_data.hostname.empty(); }

private:
  WLineEdit * m_hostname;
  WCheckBox * m_ntp;
  WCheckBox * m_copy_config;
  NetworkData m_data;
};


#endif
