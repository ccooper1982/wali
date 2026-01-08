#ifndef WALI_WIDGETSCOMMON_H
#define WALI_WIDGETSCOMMON_H


#include <Wt/WGlobal.h>
#include <variant>
#include <Wt/WCheckBox.h>
#include <Wt/WComboBox.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WHBoxLayout.h>
#include <Wt/WVBoxLayout.h>
#include <Wt/WGridLayout.h>
#include <Wt/WLabel.h>
#include <Wt/WLineEdit.h>
#include <Wt/WText.h>
#include <wali/widgets/WaliWidget.hpp>

using namespace Wt;

class IntroductionWidget;
class PartitionsWidget;
class NetworkWidget;
class AccountWidget;
class LocaliseWidget;
class InstallWidget;
class PackagesWidget;
class VideoWidget;

using WidgetVariant = std::variant< IntroductionWidget*, PartitionsWidget*,
                                    NetworkWidget*, AccountWidget*,
                                    LocaliseWidget*, InstallWidget*,
                                    PackagesWidget*, VideoWidget*>;
using WidgetsMap = std::map<std::string, WidgetVariant>;


template<typename T, typename ... Args>
std::unique_ptr<T> make_wt (Args...args)
{
  return std::make_unique<T>(args...);
}


template<typename WidgetT, typename ...WidgetArgs>
WidgetT* add_form_pair (WLayout * parent_layout, const std::string_view label_text, WidgetArgs&&... widget_args)
{
  auto cont = make_wt<WContainerWidget>();
  auto layout = cont->setLayout(make_wt<WHBoxLayout>());

  auto label = layout->addWidget(make_wt<WLabel>(label_text.data()));
  auto widget = layout->addWidget(make_wt<WidgetT>(widget_args...));

  label->setBuddy(widget);
  label->setStyleClass("form_label");

  layout->addStretch(1);

  parent_layout->addWidget(std::move(cont));

  return widget;
}

#endif
