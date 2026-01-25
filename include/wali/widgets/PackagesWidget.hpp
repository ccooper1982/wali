#ifndef WALI_PACKAGESSWIDGET_H
#define WALI_PACKAGESSWIDGET_H


#include <Wt/Json/Value.h>
#include <Wt/WApplication.h>
#include <Wt/WGlobal.h>
#include <Wt/WPushButton.h>
#include <Wt/WSelectionBox.h>
#include <Wt/WTextEdit.h>
#include <Wt/Http/Client.h>
#include <Wt/Json/Array.h>
#include <Wt/Json/Object.h>
#include <Wt/Json/Parser.h>
#include <Wt/WVBoxLayout.h>
#include <queue>
#include <sys/mount.h>
#include <wali/Common.hpp>
#include <wali/widgets/Common.hpp>
#include <wali/widgets/WidgetData.hpp>


class PackageQueue
{
public:

  void add(const std::string& name)
  {
    m_q.push(name);
  }

  std::string next()
  {
    const auto name = m_q.front();
    m_q.pop();
    return name;
  }

  void clear()
  {
    m_q = std::queue<std::string>{};
  }

  std::size_t size() const { return m_q.size();  }

  bool empty() const { return m_q.empty(); }

private:
  std::queue<std::string> m_q;
};


class PackagesWidget : public WaliWidget
{
public:
  PackagesWidget(WidgetDataPtr data);

private:
  void search ();
  void on_response(std::error_code rsp_err, const Http::Message& rsp);
  void send_next();
  void enable_search(const bool enable);
  Json::Array parse_response(const std::string body);

private:
  WLineEdit * m_line_packages;
  WPushButton * m_btn_search,
              * m_btn_clear,
              * m_btn_rmv;
  WSelectionBox * m_list_confirmed;
  PackageSet m_packages_pending;   // waiting on http response OR package does not exist
  PackageSet m_packages_confirmed; // have http response and package exists
  PackagesData data;
  std::size_t m_sent{}, m_rcvd{};
  PackageQueue m_queue; // used to limit the rate of requests to Arch package server
};

#endif
