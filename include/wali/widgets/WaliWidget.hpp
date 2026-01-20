#ifndef WALI_WALIWIDGET_H
#define WALI_WALIWIDGET_H

#include <Wt/WContainerWidget.h>
#include <Wt/WEvent.h>
#include <wali/widgets/WidgetData.hpp>



class WaliWidget : public Wt::WContainerWidget
{
private:
  WaliWidget() = delete;

public:
  WaliWidget (WidgetDataPtr data, const std::string_view name) : m_data(data)
  {
    setObjectName(name.data());
  }

  virtual ~WaliWidget() = default;

  Wt::Signal<bool>& data_valid() { return m_evt_valid; }

  bool is_data_valid() const { return m_valid; }

protected:
  void set_valid (const bool valid = true)
  {
    m_valid = valid;
    data_valid()(valid);
  }

  void set_invalid () { set_valid(false); }

protected:
  WidgetDataPtr m_data;
  Wt::Signal<bool> m_evt_valid;
  bool m_valid{true};
};


#endif
