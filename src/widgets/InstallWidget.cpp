#include "wali/Common.hpp"
#include <functional>
#include <mutex>
#include <wali/widgets/WidgetData.hpp>
#include <wali/widgets/InstallWidget.hpp>
#include <wali/widgets/ValidateWidgets.hpp>
#include <wali/widgets/Widgets.hpp>


InstallWidget::InstallWidget(Widgets * widgets) : m_widgets(widgets)
{
  auto layout = setLayout(make_wt<WVBoxLayout>());

  // buttons
  auto controls_layout = layout->addLayout(make_wt<WHBoxLayout>());

  m_install_btn = controls_layout->addWidget(make_wt<WPushButton>("Install"));
  m_install_btn->clicked().connect([this]()
  {
    if (is_config_valid())
      install();
  });

  m_reboot_btn = controls_layout->addWidget(make_wt<WPushButton>("Reboot"));
  m_reboot_btn->enable();
  m_reboot_btn->clicked().connect([] { Reboot{}(); });

  controls_layout->addStretch(1);

  // status text and logs
  m_install_status = layout->addWidget(make_wt<WText>());
  m_install_status->addStyleClass("install_status_ready");
  m_install_status->addStyleClass("install_status_running");
  m_install_status->addStyleClass("install_status_fail");
  m_install_status->addStyleClass("install_status_complete");
  m_install_status->addStyleClass("install_status_partial");
  m_install_status->setStyleClass("install_status_ready");

  m_stage_logs.push_back(layout->addWidget(make_wt<StageLog>(STAGE_FS)));
  m_stage_logs.push_back(layout->addWidget(make_wt<StageLog>(STAGE_MOUNT)));
  m_stage_logs.push_back(layout->addWidget(make_wt<StageLog>(STAGE_PACSTRAP)));
  m_stage_logs.push_back(layout->addWidget(make_wt<StageLog>(STAGE_FSTAB)));
  m_stage_logs.push_back(layout->addWidget(make_wt<StageLog>(STAGE_ROOT_ACC)));
  m_stage_logs.push_back(layout->addWidget(make_wt<StageLog>(STAGE_BOOT_LOADER)));
  m_stage_logs.push_back(layout->addWidget(make_wt<StageLog>(STAGE_USER_ACC)));
  m_stage_logs.push_back(layout->addWidget(make_wt<StageLog>(STAGE_LOCALISE)));
  m_stage_logs.push_back(layout->addWidget(make_wt<StageLog>(STAGE_VIDEO)));
  m_stage_logs.push_back(layout->addWidget(make_wt<StageLog>(STAGE_NETWORK)));
  m_stage_logs.push_back(layout->addWidget(make_wt<StageLog>(STAGE_PACKAGES)));
  m_stage_logs.push_back(layout->addWidget(make_wt<StageLog>(STAGE_UNMOUNT)));

  layout->addStretch(1);
}

void InstallWidget::install ()
{
  try
  {
    m_log = 0;
    m_install_btn->disable();
    m_stage_logs[0]->start();

    const auto sessionId = WApplication::instance()->sessionId();

    m_install_future = std::async(std::launch::async, [this, sessionId]
    {
      auto stage_change = [this, sessionId](const std::string name, const StageStatus state) {
        on_stage_end(name, state, sessionId);
      };

      auto log = [this, sessionId](const std::string msg, const InstallLogLevel level) {
        on_log(msg, level, sessionId);
      };

      auto complete = [this, sessionId](const InstallState state) {
        on_install_status(state, sessionId);
      };

      m_install.install(  { .stage_change = stage_change,
                            .log = log,
                            .complete = complete
                          },
                          m_widgets->get_data());
                        });
  }
  catch (const std::exception& ex)
  {
    PLOGE << "Exception during install:\n" << ex.what();
    m_install_btn->enable();
  }
}


bool InstallWidget::is_config_valid()
{
  PLOGW << "is_config_valid()";

  const auto& err_stage = validate_widgets(m_widgets->get_all());
  if (err_stage)
  {
    PLOGW << "is_config_valid(): problem";
    m_install_status->setText(std::format("Problem in: {}", *err_stage));
    m_install_status->setStyleClass("install_status_ready_err");
  }
  else
  {
    PLOGW << "is_config_valid(): ok";

    m_install_status->setText("Ready to install");
    m_install_status->setStyleClass("install_status_ready");
  }

  return !err_stage.has_value();
}

void InstallWidget::on_log(const std::string msg, const InstallLogLevel level, const std::string sid)
{
  WServer::instance()->post(sid, [=, this]()
  {
    std::scoped_lock lck{m_post_lock};

    if (m_log < m_stage_logs.size())
    {
      m_stage_logs[m_log]->add(msg, level);
      WApplication::instance()->triggerUpdate();
    }
  });
}


void InstallWidget::on_stage_end(const std::string name, const StageStatus state, const std::string sid)
{
  WServer::instance()->post(sid, [=, this]()
  {
    if (state == StageStatus::Complete)
    {
      {
        std::scoped_lock lck{m_post_lock};

        m_stage_logs[m_log]->end();

        if (++m_log < m_stage_logs.size())
          m_stage_logs[m_log]->start();
      }

      WApplication::instance()->triggerUpdate();
    }
  });
}


void InstallWidget::on_install_status(const InstallState state, const std::string sid)
{
  bool allow_install{false};
  std::string_view status, css_class{"install_status"};

  switch (state)
  {
  case InstallState::Running:
    status = "Installing ...";
    css_class = "install_status_running";
  break;

  case InstallState::Fail:
    status = "Failed: system is not bootable";
    css_class = "install_status_fail";
    allow_install = true;
  break;

  case InstallState::Complete:
    status = "Complete: ready to reboot";
    css_class = "install_status_complete";
  break;

  case InstallState::Bootable:
    status = "Bootable: minimal steps successful";
    css_class = "install_status_bootable";
  break;

  case InstallState::Partial:
    status = "Partial: completed minimal, but a subsequent step failed";
    css_class = "install_status_partial";
    allow_install = true;
  break;

  case InstallState::None:
    status = "Press Install to start";
    allow_install = true;
  break;
  }

  WServer::instance()->post(sid, [=, this]()
  {
    std::scoped_lock lck{m_post_lock};

    m_on_install_state(state);
    set_install_status(status, css_class);

    m_install_btn->setEnabled(allow_install);

    WApplication::instance()->triggerUpdate();

    if ((state == InstallState::Complete || state == InstallState::Fail) && m_install_future.valid())
      m_install_future.wait(); // wait_for()?
  });
}
