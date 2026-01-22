#ifndef WALI_WIDGETSCOMMON_H
#define WALI_WIDGETSCOMMON_H


#include <Wt/WHBoxLayout.h>
#include <Wt/WGlobal.h>
#include <Wt/WLabel.h>
#include <Wt/WLineEdit.h>
#include <Wt/WText.h>
#include <Wt/WVBoxLayout.h>
#include <wali/widgets/WaliWidget.hpp>

using namespace Wt;


template<typename T, typename ... Args>
std::unique_ptr<T> make_wt (Args...args)
{
  return std::make_unique<T>(args...);
}


template<typename WidgetT, typename ...WidgetArgs>
WidgetT* add_form_pair (WVBoxLayout * parent_layout, const std::string_view label_text, const int label_width, WidgetArgs&&... widget_args)
{
  auto layout = parent_layout->addLayout(make_wt<WHBoxLayout>());

  auto label = layout->addWidget(make_wt<WLabel>(label_text.data()));
  auto widget = layout->addWidget(make_wt<WidgetT>(widget_args...));

  label->setWidth(label_width);
  label->setBuddy(widget);
  label->setStyleClass("form_label");

  layout->addStretch(1);

  return widget;
}

#endif
