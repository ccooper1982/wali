#ifndef WALI_INSTALLWIDGET_H
#define WALI_INSTALLWIDGET_H


#include <Wt/WApplication.h>
#include <Wt/WGlobal.h>
#include <Wt/WHBoxLayout.h>
#include <Wt/WMenu.h>
#include <Wt/WObject.h>
#include <Wt/WPanel.h>
#include <Wt/WPushButton.h>
#include <Wt/WServer.h>
#include <Wt/WSignal.h>
#include <Wt/WStringStream.h>
#include <Wt/WTextArea.h>
#include <functional>
#include <future>
#include <mutex>
#include <wali/Commands.hpp>
#include <wali/Common.hpp>
#include <wali/Install.hpp>
#include <wali/widgets/Common.hpp>
#include <wali/widgets/WaliWidget.hpp>
#include <Wt/WStackedWidget.h>

struct Widgets;
class SummaryWidget;


struct StageLog : public WContainerWidget
{
  // static const constexpr auto AutoScroll = R"(
  //   textArea.scrollTop = document.getElementById('stage_log').scrollHeight;
  // )";

  StageLog(const std::string_view name, const bool collapsed, const std::size_t init_size)
  {
    // layout?
    m_panel = addWidget(make_wt<WPanel>());
    m_panel->setTitle(name.data());
    m_panel->setCollapsible(true);
    m_panel->setCollapsed(collapsed);

    m_text = m_panel->setCentralWidget(make_wt<WTextArea>());
    m_text->setReadOnly(true);
    m_text->setStyleClass("stage_log");
    // m_text->setId("stage_log");
    // WApplication::instance()->doJavaScript(AutoScroll);

    m_log.reserve(init_size);
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


class InstallWidget : public WaliWidget
{
  using StageFunc = std::function<bool()>;

public:
  InstallWidget(WidgetDataPtr data) ;

  void update_data();

  Signal<InstallState>& install_state() { return m_on_install_state; }

private:

  void create_logs(WVBoxLayout * layout);
  void install ();

private:

  void on_stage_end(const std::string name, const StageStatus state, const std::string sid);
  void on_install_status(const InstallState state, const std::string sid);
  inline void on_log(const std::string msg, const InstallLogLevel level, const std::string sid);

  void set_install_status(const std::string_view stat, const std::string_view css_class);

private:
  Install m_install;
  Widgets * m_widgets;
  WPushButton * m_install_btn,
              * m_reboot_btn;
  WText * m_install_status;
  std::future<void> m_install_future;
  Signal<InstallState> m_on_install_state;

  std::size_t m_log{};
  std::vector<StageLog*> m_stage_logs;
  SummaryWidget * m_summary;
};

#endif
