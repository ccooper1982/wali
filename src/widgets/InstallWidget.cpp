
#include <Wt/WApplication.h>
#include <Wt/WComboBox.h>
#include <algorithm>
#include <functional>
#include <sstream>
#include <stop_token>
#include <string>
#include <chrono>
#include <Wt/WHBoxLayout.h>
#include <Wt/WGlobal.h>
#include <Wt/WLength.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPopupMenu.h>
#include <Wt/WText.h>
#include <Wt/WVBoxLayout.h>
#include <wali/Common.hpp>
#include <wali/widgets/Common.hpp>
#include <wali/Install.hpp>
#include <wali/widgets/InstallWidget.hpp>
#include <wali/widgets/WaliWidget.hpp>
#include <wali/widgets/WidgetData.hpp>


class SummaryWidget : public WContainerWidget
{
  // TODO 1. this probably use a table instead of labels
  //      2. maybe with a model for the data
  //      3. maybe Install::install() should set package count and disk space in WidgetData
public:
  SummaryWidget(WidgetDataPtr data) : m_data(data)
  {
    resize(400, WLength::Auto);

    auto layout = setLayout(make_wt<WVBoxLayout>());
    layout->setSpacing(0);
    layout->setContentsMargins(0,0,0,0);

    auto add_pair = [=](const std::string_view label, const std::string_view val = "")
    {
      auto pair_layout = layout->addLayout(make_wt<WHBoxLayout>());
      pair_layout->setSpacing(0);

      auto lbl_widget = pair_layout->addWidget(make_wt<WLabel>(label.data()), 0, Wt::AlignmentFlag::Middle);
      auto lbl_value = pair_layout->addWidget(make_wt<WLabel>(val.data()), 1, Wt::AlignmentFlag::Middle);

      lbl_widget->setWidth(120);
      lbl_widget->setStyleClass("install_summary_lbl");
      lbl_value->setStyleClass("install_summary_value");

      return lbl_value;
    };

    m_root = add_pair("Root Password");
    m_user_username = add_pair("Username");
    m_user_password = add_pair("Password");
    m_packages = add_pair("Packages");
    m_duration = add_pair("Duration");
    m_root_dev_space = add_pair("Root");

    layout->addStretch(1);
  }

  void update_data()
  {
    auto duration_string = [](const std::chrono::seconds d)
    {
      namespace chrono = std::chrono;

      // assume install is within 1 hour
      const auto m = chrono::duration_cast<chrono::minutes>(d);
      const auto s = d - chrono::duration_cast<chrono::seconds>(m);

      std::ostringstream ss;
      ss << m << ' ' << s;
      return ss.str();
    };

    m_root->setText(m_data->accounts.root_pass);
    m_user_username->setText(m_data->accounts.user_username);
    m_user_password->setText(m_data->accounts.user_pass);
    m_packages->setText(std::to_string(m_data->summary.package_count));
    m_duration->setText(duration_string(m_data->summary.duration));
    m_root_dev_space->setText(std::format("{} / {}", m_data->summary.root_used, m_data->summary.root_size));
  }

private:
  WidgetDataPtr m_data;
  WLabel *  m_root,
         *  m_user_username,
         *  m_user_password,
         *  m_packages,
         *  m_duration,
         *  m_root_dev_space;
};


InstallWidget::InstallWidget(WidgetDataPtr data) : WaliWidget(data, "Install")
{
  resize(800, WLength::Auto);

  auto layout = setLayout(make_wt<WVBoxLayout>());
  layout->setContentsMargins(10,0,0,0);

  // buttons
  auto controls_layout = layout->addLayout(make_wt<WHBoxLayout>());
  controls_layout->setSpacing(15);

  m_install_btn = controls_layout->addWidget(make_wt<WPushButton>("Install"));
  m_cancel_btn = controls_layout->addWidget(make_wt<WPushButton>("Cancel"));
  m_reboot_btn = controls_layout->addWidget(make_wt<WPushButton>("Reboot"));
  //m_savelog_btn = controls_layout->addWidget(make_wt<WSplitButton>("Save Log"));

  m_install_btn->clicked().connect([this]()
  {
    rng::for_each(m_stage_logs, [](StageLog * log){ log->reset(); });
    m_cancel_btn->enable();

    #ifndef WALI_DISABLE_INSTALL
      install();
    #else
      m_install_status->setText("Install disabled");
    #endif
  });

  m_cancel_btn->disable();
  m_cancel_btn->clicked().connect([this] { cancel(); });

  m_reboot_btn->enable();
  m_reboot_btn->clicked().connect([] { Reboot{}(); });

  // m_savelog_btn->disable();
  // auto savelog_options = make_wt<WPopupMenu>();
  // savelog_options->addItem("Save to single file");
  // savelog_options->addItem("Save file per stage");
  // savelog_options->itemSelected().connect([this, opts=savelog_options.get()]{ save_log(opts->currentIndex() == 0); });

  // m_savelog_btn->dropDownButton()->setMenu(std::move(savelog_options));


  controls_layout->addStretch(1);

  // status text and logs
  m_install_status = layout->addWidget(make_wt<WText>("Ready"),  0, AlignmentFlag::Center);
  m_install_status->setWidth(400);
  m_install_status->addStyleClass("install_status_ready");
  m_install_status->addStyleClass("install_status_running");
  m_install_status->addStyleClass("install_status_fail");
  m_install_status->addStyleClass("install_status_complete");
  m_install_status->addStyleClass("install_status_partial");
  m_install_status->setStyleClass("install_status_ready");

  m_summary = layout->addWidget(make_wt<SummaryWidget>(data),  0, AlignmentFlag::Center);
  m_summary->hide();

  auto logs_cont = layout->addWidget(make_wt<WContainerWidget>(),  1, AlignmentFlag::Center);
  auto log_layout = logs_cont->setLayout(make_wt<WVBoxLayout>());

  logs_cont->setWidth(600);

  create_log_widgets(log_layout);

  layout->addStretch(1);

  set_valid();
}


void InstallWidget::cancel()
{
  m_cancel_btn->disable();
  m_install_status->setText("Cancelling...");
  m_install.stop();
}


void InstallWidget::create_log_widgets(WVBoxLayout * layout)
{
  for (const auto& name : Stages)
  {
    std::size_t size = 100;

    if (name == STAGE_PACSTRAP)
      size = 25'000;
    else if (name == STAGE_BOOT_LOADER)
      size = 2'000;
    else if (name == STAGE_DESKTOP || name == STAGE_PACKAGES)
      size = 50'000;

    m_stage_logs.push_back(layout->addWidget(make_wt<StageLog>(name, true, size)));
  }

  layout->addStretch(1);
}


void InstallWidget::update_data()
{
  m_summary->update_data();
}


void InstallWidget::install()
{
  try
  {
    m_log = 0;
    m_install_btn->disable();
    m_stage_logs[0]->start();

    const auto sessionId = WApplication::instance()->sessionId();

    m_install_future = std::async(std::launch::async, [this, sessionId]
    {
      auto stage_change = [=, this](const std::string name, const StageStatus state) {
        on_stage_end(name, state, sessionId);
      };

      auto log = [=, this](const std::string msg, const InstallLogLevel level) {
        on_log(msg, level, sessionId);
      };

      auto complete = [=, this](const InstallState state) {
        on_install_status(state, sessionId);
      };

      m_install.install(  { .stage_change = stage_change,
                            .log = log,
                            .complete = complete
                          },
                          m_data);
    });
  }
  catch (const std::exception& ex)
  {
    PLOGE << "Exception during install:\n" << ex.what();
    m_install_btn->enable();
  }
}


// void InstallWidget::save_log(const bool single_file)
// {
//   static const fs::path SuggestedFileName = "wali_log.txt";

//   PLOGI << "Saving logs to " << (single_file ? " single file" : "multiple files");

//   // TODO send logs in memory to zip
// }


void InstallWidget::set_install_status(const std::string_view stat, const std::string_view css_class)
{
  m_install_status->setText(stat.data());
  m_install_status->setStyleClass(css_class.data());
}


void InstallWidget::on_log(const std::string msg, const InstallLogLevel level, const std::string sid)
{
  WServer::instance()->post(sid, [=, this]()
  {
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
      m_stage_logs[m_log]->end();

      if (++m_log < m_stage_logs.size())
        m_stage_logs[m_log]->start();

      WApplication::instance()->triggerUpdate();
    }
  });
}


void InstallWidget::on_install_status(const InstallState state, const std::string sid)
{
  bool allow_install{};
  std::string_view status, css_class{"install_status"};

  switch (state)
  {
  case InstallState::Running:
    status = "Installing...";
    css_class = "install_status_running";
  break;

  case InstallState::Cancelled:
    status  = "Cancelled";
    css_class = "install_status_fail";
    allow_install = true;
  break;

  case InstallState::Fail:
    status  = "Failed: system is not bootable. Check logs.";
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
    status = "Partial: a non-essential step failed. Check logs.";
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
    // the on_install_status() is called from the install thread, but we don't
    // need to worry about data races: post() serialises widget access
    m_on_install_state(state);
    set_install_status(status, css_class);

    m_install_btn->setEnabled(allow_install);

    const auto finished = state == InstallState::Complete || state == InstallState::Fail || state == InstallState::Cancelled;
    m_cancel_btn->setDisabled(finished);
    // m_savelog_btn->setDisabled(finished == false);

    if (state == InstallState::Complete)
    {
      update_data();
      m_summary->show();
    }

    WApplication::instance()->triggerUpdate();
  });
}
