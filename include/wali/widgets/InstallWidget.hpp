#ifndef WALI_INSTALLWIDGET_H
#define WALI_INSTALLWIDGET_H

#include <Wt/WApplication.h>
#include <Wt/WGlobal.h>
#include <Wt/WHBoxLayout.h>
#include <Wt/WMenu.h>
#include <Wt/WPanel.h>
#include <Wt/WPushButton.h>
#include <Wt/WServer.h>
#include <Wt/WTextArea.h>
#include <thread>
#include <wali/Common.hpp>
#include <wali/Install.hpp>
#include <wali/widgets/Common.hpp>

class InstallWidget : public WContainerWidget
{
  using StageFunc = std::function<bool()>;

  struct StageLog : public WContainerWidget
  {
    StageLog(const std::string_view name, const bool collapsed = true)
    {
      // layout?
      m_panel = addWidget(make_wt<WPanel>());
      m_panel->setTitle(name.data());
      m_panel->setCollapsible(true);
      m_panel->setCollapsed(collapsed);

      m_text = m_panel->setCentralWidget(make_wt<WTextArea>());
      m_text->setReadOnly(true);
      m_text->setStyleClass("stage_log");

      m_log.reserve(200);
    }

    void add(const std::string_view msg, const InstallLogLevel level)
    {
      m_log += msg;
      m_log += '\n';
      m_text->setText(m_log);
    }

    void start()
    {
      m_panel->setCollapsed(false);
    }

    void end()
    {
      m_panel->setCollapsed(true);
    }

    private:
      WPanel * m_panel;
      WTextArea * m_text;
      std::string m_log;
  };

public:
  InstallWidget(WServer * server) : m_server(server)
  {
    auto layout = setLayout(make_wt<WVBoxLayout>());

    // buttons
    auto controls_layout = layout->addLayout(make_wt<WHBoxLayout>());

    m_install_btn = controls_layout->addWidget(make_wt<WPushButton>("Install"));
    m_install_btn->setStyleClass("install");
    m_install_btn->clicked().connect([this]() { install(); });

    m_reboot_btn = controls_layout->addWidget(make_wt<WPushButton>("Reboot"));
    m_reboot_btn->setStyleClass("install");
    m_reboot_btn->disable();
    // m_reboot_btn->clicked().connect([this]() { install(); });
    controls_layout->addStretch(1);

    // status text and logs
    m_install_status = layout->addWidget(make_wt<WText>());
    m_stage_status = layout->addWidget(make_wt<WText>());

    m_fs_log = layout->addWidget(make_wt<StageLog>("Create Filesystem"));
    m_mount_log = layout->addWidget(make_wt<StageLog>("Mount Partitions"));
    m_pacstrap_log = layout->addWidget(make_wt<StageLog>("Pacstrap"));
    m_fstab_log = layout->addWidget(make_wt<StageLog>("Filesystem Table"));
    m_root_account_log = layout->addWidget(make_wt<StageLog>("Root Account"));
    m_bootloader_log = layout->addWidget(make_wt<StageLog>("Boot Loader"));
    m_user_account_log = layout->addWidget(make_wt<StageLog>("User Account"));
    m_localise_log = layout->addWidget(make_wt<StageLog>("Localise"));
    m_network_log = layout->addWidget(make_wt<StageLog>("Network"));

    m_log = m_fs_log;

    layout->addStretch(1);
  }

private:
  void install ()
  {
    try
    {
      m_install_btn->disable();
      m_fs_log->start();

      const auto sessionId = WApplication::instance()->sessionId();

      m_install_thread = std::jthread([this, sessionId]
      {
        auto stage_change = [this, sessionId](const std::string name, const StageStatus state)
        {
          on_stage(name, state, sessionId);
        };

        auto log = [this, sessionId](const std::string msg, const InstallLogLevel level)
        {
          on_log(msg, level, sessionId);
        };

        auto complete = [this, sessionId](const InstallState state)
        {
          on_complete(state, sessionId);
        };

        m_install.install({ .stage_change = stage_change,
                            .log = log,
                            .complete = complete });
      });
    }
    catch (const std::exception& ex)
    {
      PLOGE << "Exception during install:\n" << ex.what();
      m_install_btn->enable();
    }
  }

private:

  void on_stage(const std::string name, const StageStatus state, const std::string sid)
  {
    std::string msg;
    bool next{false};
    switch (state)
    {
      case StageStatus::Start:
        msg = "Start";
      break;

      case StageStatus::Complete:
        msg = "Complete";
        next = true;
      break;

      case StageStatus::Fail:
        msg = "Failed";
      break;
    }

    m_server->post(sid, [=, this]()
    {
      if (next && m_log)
      {
        m_log->end();

        if (name == "Create Filesystems")
          m_log = m_mount_log;
        else if (name == "Mounting")
          m_log = m_pacstrap_log;
        else if (name == "Pacstrap")
          m_log = m_fstab_log;
        else if (name == "Generate fstab")
          m_log = m_root_account_log;
        else if (name == "Root Account")
          m_log = m_bootloader_log;
        else if (name == "Boot loader")
          m_log = m_user_account_log;
        else if (name == "User Account")
          m_log = m_localise_log;
        else if (name == "Localise")
          m_log = m_network_log;
        else
          m_log = nullptr;

        if (m_log)
          m_log->start();
      }

      m_stage_status->setText(std::format("Stage {}: {} ", msg, name));
      WApplication::instance()->triggerUpdate();
    });
  }

  void on_log(const std::string msg, const InstallLogLevel level, const std::string sid)
  {
    m_server->post(sid, [=, this]()
    {
      if (m_log)
      {
        m_log->add(msg, level);
        WApplication::instance()->triggerUpdate();
      }
    });
  }

  void on_complete(const InstallState state, const std::string sid)
  {
    bool allow_install{false};
    std::string_view bootable_msg;

    switch (state)
    {
    case InstallState::Running:
      bootable_msg = "Running";
    break;

    case InstallState::Complete:
      bootable_msg = "Complete - ready to reboot";
    break;

    case InstallState::Bootable:
      bootable_msg = "Bootable - minimal steps successful";
    break;

    case InstallState::Partial:
      bootable_msg = "Partial - completed minimal, but a subsequent step failed";
      allow_install = true;
    break;

    case InstallState::Fail:
      bootable_msg = "Failed - system is not bootable";
      allow_install = true;
    break;

    case InstallState::None:
      bootable_msg = "Press Install to start";
      allow_install = true;
    break;
    }

    const auto msg = std::format("Install Status: {}", bootable_msg);
    m_server->post(sid, [=, this]()
    {
      if (allow_install)
      {
        m_install_status->setText(msg);
        m_install_btn->setEnabled(allow_install);
      }
      else
      {
        m_stage_status->hide();
      }
    });
  }


private:
  Install m_install;
  WPushButton * m_install_btn,
              * m_reboot_btn;
  WServer * m_server;
  WText * m_stage_status;
  WText * m_install_status;
  std::jthread m_install_thread;

  StageLog *  m_fs_log,
           *  m_mount_log,
           *  m_pacstrap_log,
           *  m_fstab_log,
           *  m_root_account_log,
           *  m_bootloader_log,
           *  m_user_account_log,
           *  m_localise_log,
           *  m_network_log;
  StageLog * m_log{}; // current log
};

#endif
