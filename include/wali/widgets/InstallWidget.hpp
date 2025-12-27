#ifndef WALI_INSTALLWIDGET_H
#define WALI_INSTALLWIDGET_H

#include <Wt/WMenu.h>
#include <Wt/WPushButton.h>
#include <wali/Common.hpp>

class InstallWidget : public WContainerWidget
{
public:
  InstallWidget(WMenu * menu) : m_menu(menu)
  {
    auto layout = setLayout(make_wt<WVBoxLayout>());

    auto btn_install = layout->addWidget(make_wt<WPushButton>("Install"));
    btn_install->setStyleClass("install");
    btn_install->clicked().connect(std::bind(&InstallWidget::install, std::ref(*this)));
    btn_install->enterPressed().connect(std::bind(&InstallWidget::install, std::ref(*this)));

    layout->addStretch(1);
  }

private:
  void install ()
  {
    PLOGI << "Starting install";
  }

private:
  WMenu * m_menu;
};

#endif
