#ifndef WALI_WIDGETSCOMMON_H
#define WALI_WIDGETSCOMMON_H

#include <Wt/WCheckBox.h>
#include <Wt/WComboBox.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WHBoxLayout.h>
#include <Wt/WVBoxLayout.h>
#include <Wt/WGridLayout.h>
#include <Wt/WLabel.h>
#include <Wt/WLineEdit.h>
#include <Wt/WText.h>

using namespace Wt;


template<typename T, typename ... Args>
std::unique_ptr<T> make_wt (Args...args)
{
  return std::make_unique<T>(args...);
}

#endif
