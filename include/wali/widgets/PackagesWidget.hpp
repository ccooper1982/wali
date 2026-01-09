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
#include <wali/Common.hpp>
#include <wali/widgets/Common.hpp>
#include <wali/widgets/WidgetData.hpp>


class PackagesWidget : public WaliWidget<PackagesData>
{
public:
  PackagesWidget();

  PackagesData get_data() const override { return data; }

private:
  void search ();
  void on_response(std::error_code rsp_err, const Http::Message& rsp);

private:
  WLineEdit * m_line_packages;
  WPushButton * m_btn_search;
  WSelectionBox * m_list_confirmed;
  PackageSet m_packages_pending;   // waiting on http response OR package does not exist
  PackageSet m_packages_confirmed; // have http response and package exists
  PackagesData data;
  std::size_t m_responses_expected{0};
};

#endif
