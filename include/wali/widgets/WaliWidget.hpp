#ifndef WALI_WALIWIDGET_H
#define WALI_WALIWIDGET_H

#include <Wt/WContainerWidget.h>

template<typename DataT>
struct WaliWidget :  public Wt::WContainerWidget
{
  virtual DataT get_data() const = 0;
  virtual bool is_valid() const { return true; }
};

template<>
struct WaliWidget<void> :  public Wt::WContainerWidget
{
  virtual void get_data() const { }
  virtual bool is_valid() const { return true; }
};

#endif
