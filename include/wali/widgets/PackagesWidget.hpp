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
#include <wali/widgets/Common.hpp>
#include <wali/Common.hpp>
#include <ranges>

class PackagesWidget : public WContainerWidget
{
  static constexpr const auto IntroText = "Enter package names, separated by a space.<br/>"
                                          "Packages that don't exist, will remain in the textbox.";

public:
  enum class Level {Info, Warning, Error};

  PackagesWidget()
  {
    auto layout = setLayout(make_wt<WVBoxLayout>());
    layout->addWidget(make_wt<Wt::WText>(IntroText));

    auto cont_search = layout->addLayout(make_wt<WHBoxLayout>());
    m_line_packages = cont_search->addWidget(make_wt<WLineEdit>(), 1);
    m_line_packages->setStyleClass("packages");
    m_line_packages->setAttributeValue("maxlength", "100");
    m_line_packages->setPlaceholderText("nano code ...");
    m_line_packages->enterPressed().connect(this, &PackagesWidget::search);

    m_btn_search = cont_search->addWidget(make_wt<WPushButton>("Search"));
    m_btn_search->enterPressed().connect(this, &PackagesWidget::search);
    m_btn_search->clicked().connect(this, &PackagesWidget::search);

    m_list_confirmed = layout->addWidget(make_wt<WSelectionBox>());
    m_list_confirmed->setStyleClass("packages_confirmed");
    m_list_confirmed->setVerticalSize(10);

    layout->addStretch(1);
  }

private:
  void search ()
  {
    const auto package_names = m_line_packages->text().toUTF8();

    if (package_names.empty())
      return;

    m_btn_search->disable();
    m_packages_confirmed.clear();
    m_packages_pending.clear();
    m_responses_expected = 0;

    for (const auto it : package_names | std::ranges::views::split(' '))
    {
      const std::string package (std::string_view{it});

      if (m_packages_pending.contains(package) || m_packages_confirmed.contains(package) || m_packages_install.contains(package))
        continue;

      Http::Client * client = addChild(make_wt<Http::Client>());
      client->setMaxRedirects(10);
      client->setMaximumResponseSize(16 * 1024);
      client->done().connect(this, &PackagesWidget::on_response);

      // put 'any' arch to avoid missing packages, i.e. "reflector" is 'any'
      if (client->get(std::format("https://archlinux.org/packages/search/json/?arch=any&name={}", package)))
      {
        ++m_responses_expected;
        m_packages_pending.emplace(package);
      }
    }

    // if (m_packages_pending.size())
    //   WApplication::instance()->deferRendering();
  }

  void on_response(std::error_code err, const Http::Message& rsp)
  {
    // WApplication::instance()->resumeRendering();

    if (err || rsp.status() != 200)
    {
      // TODO diplay something on UI on err
      if (rsp.status() == 429)
        PLOGE << "Package search response: Too many requests";
      else
        PLOGE << "Package search response: " << rsp.status() << " : " << err.message();
    }
    else
    {
      // TODO should perhaps use the root_object["version"] (currently 2)
      Json::Value root;

      if (Json::ParseError err; !Json::parse(rsp.body(), root, err))
        PLOGE << "Response contains invalid JSON";
      else if (root.type() != Json::Type::Object)
        PLOGE << "Response JSON root is not an object";
      else if (Json::Object root_object = root; !root_object.contains("results") || root_object.get("results").type() != Json::Type::Array)
        PLOGE << "Response JSON 'results' does not exist or is not an array";
      else if (const Json::Array results = root_object.get("results"); !results.empty())
      {
        const Json::Object& result = results[0];
        const auto name = result.get("pkgname");

        m_packages_confirmed.emplace(name);
        m_packages_pending.erase(name);

        m_list_confirmed->addItem(name);
        m_packages_install.emplace(name);
      }
    }

    // when we have all resposnes
    if (m_packages_confirmed.size() + m_packages_pending.size() == m_responses_expected)
    {
      m_btn_search->enable();
      m_line_packages->setText(flatten(m_packages_pending));
    }

    WApplication::instance()->triggerUpdate();
  }

private:
  WLineEdit * m_line_packages;
  WPushButton * m_btn_search;
  WSelectionBox * m_list_confirmed;
  std::set<std::string> m_packages_pending;   // waiting on http response OR package does not exist
  std::set<std::string> m_packages_confirmed; // have http response and package exists
  std::set<std::string> m_packages_install;   // final set of packages to install
  std::size_t m_responses_expected{0};
};

#endif
