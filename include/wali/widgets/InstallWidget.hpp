#ifndef WALI_INSTALLWIDGET_H
#define WALI_INSTALLWIDGET_H

#include <Wt/WMenu.h>
#include <Wt/WPushButton.h>
#include <exception>
#include <wali/Common.hpp>
#include <wali/Install.hpp>
#include <wali/widgets/Common.hpp>

class InstallWidget : public WContainerWidget
{
public:
  InstallWidget()
  {
    auto layout = setLayout(make_wt<WVBoxLayout>());

    m_install_btn = layout->addWidget(make_wt<WPushButton>("Install"));
    m_install_btn->setStyleClass("install");
    m_install_btn->clicked().connect(std::bind(&InstallWidget::install, std::ref(*this)));
    m_install_btn->enterPressed().connect(std::bind(&InstallWidget::install, std::ref(*this)));

    layout->addStretch(1);
  }

private:
  void install ()
  {
    PLOGI << "Starting install";

    // TODO should the install run in a separate thread? It did with Qt to avoid blocking
    //      the UI thread

    auto on_state = [](const std::string_view name, const StageStatus state)
    {
      // std::string msg;
      // switch (state)
      // {
      //   case StageStatus::Start:
      //     msg = "started";
      //   break;

      //   case StageStatus::Complete:
      //     msg = "complete";
      //   break;

      //   case StageStatus::Fail:
      //     msg = "failed";
      //   break;
      // }

      // PLOGI << name << msg;

      // TODO update UI
    };

    auto on_log = [](const std::string_view msg, const InstallLogLevel level)
    {
      // TODO append to UI log
      // switch (level)
      // {
      //   using enum InstallLogLevel;

      // case Info:
      //   PLOGI << msg;
      // break;

      // case Warning:
      //   PLOGW << msg;
      // break;

      // case Error:
      //   PLOGE << msg;
      // break;
      // }
    };

    auto on_complete = [](const InstallState state)
    {
      std::string bootable_msg;
      switch (state)
      {
      case InstallState::Bootable:
      case InstallState::Partial:
      case InstallState::Complete:
        bootable_msg = "Installed system is bootable";
      break;

      case InstallState::Fail:
        bootable_msg = "Installed system is not bootable";
      break;

      case InstallState::Running:
        bootable_msg = "Install running";
      break;

      case InstallState::None:
        bootable_msg = "Install not started";
      break;
      }

      const auto msg = std::format("{}", bootable_msg);
      PLOGI << msg;
    };

    try
    {
      m_install_btn->disable();

      Install install;
      install.install({.stage_change = on_state, .complete = on_complete, .log = on_log});
    }
    catch (const std::exception& ex)
    {
      PLOGE << "Exception during install:\n" << ex.what();
      m_install_btn->enable();
    }
  }

private:
  WPushButton * m_install_btn;
};

#endif
