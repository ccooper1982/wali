#include <Wt/WApplication.h>
#include <Wt/WGlobal.h>
#include <Wt/WHBoxLayout.h>
#include <Wt/WServer.h>
#include <algorithm>
#include <chrono>
#include <wali/widgets/PackagesWidget.hpp>

static constexpr const auto IntroText = R"(
  <ul>
    <li>Enter package names, separated by a space</li>
    <li>Packages that don't exist, remain in the textbox</li>
    <li>This only checks the Arch repo, not the AUR</li>
  </ul>
  )";

enum class Level
{
  Info,
  Warning,
  Error
};

/// This is more complex than it should because:
///   1. The Arch search API only allows one package per request
///   2. Sending requests at a high rate results in a HTTP 429 response or connection close
///   3. If a package does not exist, the response does not include the name
/// For (1) and (2), package searches are staggered:
///   - Sent in groups of 3 requests, 200ms apart
///   - Only when all responses are received, is the next group sent
/// For (3):
///   - A package that does not exist, remains in m_packages_pending

PackagesWidget::PackagesWidget(WidgetDataPtr data) : WaliWidget(data, "Packages")
{
  auto layout = setLayout(make_wt<WVBoxLayout>());
  layout->setSpacing(10);
  layout->addWidget(make_wt<Wt::WText>(IntroText));

  auto search_cont = layout->addWidget(make_wt<WContainerWidget>());
  auto layout_search = search_cont->setLayout(make_wt<WHBoxLayout>());

  m_line_packages = layout_search->addWidget(make_wt<WLineEdit>(), 1);
  m_line_packages->setStyleClass("packages");
  m_line_packages->setAttributeValue("maxlength", "300");
  m_line_packages->setPlaceholderText("firefox git btop ...");
  m_line_packages->enterPressed().connect(this, &PackagesWidget::search);

  m_btn_search = layout_search->addWidget(make_wt<WPushButton>("Search"));
  m_btn_search->enterPressed().connect(this, &PackagesWidget::search);
  m_btn_search->clicked().connect(this, &PackagesWidget::search);
  m_btn_search->setStyleClass("packages");

  // list of packages to install, control buttons
  auto results_cont = layout->addWidget(make_wt<WContainerWidget>());
  auto layout_results = results_cont->setLayout(make_wt<WHBoxLayout>());

  m_list_confirmed = layout_results->addWidget(make_wt<WSelectionBox>(), 1);
  m_list_confirmed->setStyleClass("packages_confirmed");
  m_list_confirmed->setVerticalSize(10);
  m_list_confirmed->setHeight(300);
  m_list_confirmed->setSelectionMode(SelectionMode::Extended);

  auto layout_control = layout_results->addLayout(make_wt<WVBoxLayout>());
  m_btn_rmv = layout_control->addWidget(make_wt<WPushButton>("Remove"));
  m_btn_clear = layout_control->addWidget(make_wt<WPushButton>("Clear"));
  layout_control->addStretch(1);

  m_btn_rmv->setStyleClass("packages");
  m_btn_clear->setStyleClass("packages");

  m_btn_rmv->clicked().connect([this]
  {
    const auto& selected = m_list_confirmed->selectedIndexes();

    if (selected.empty())
      return;

    rng::for_each(selected, [this](const auto i)
    {
      m_data->packages.additional.erase(m_list_confirmed->itemText(i).toUTF8());
    });

    // repopulate list with what remains
    m_list_confirmed->clear();

    rng::for_each(m_data->packages.additional, [this](const auto name)
    {
      m_list_confirmed->addItem(name);
    });
  });

  m_btn_clear->clicked().connect([this]
  {
    m_data->packages.additional.clear();
    m_list_confirmed->clear();
  });

  layout->addStretch(1);

  // this can't be invalid, ignore packages that are not found
  set_valid();
}


void PackagesWidget::search ()
{
  const auto package_names = m_line_packages->text().toUTF8();

  if (package_names.empty())
    return;

  enable_search(false);
  m_packages_confirmed.clear();
  m_packages_pending.clear();
  m_queue.clear();
  m_sent = m_rcvd = 0;

  for (const auto it : package_names | view::split(' '))
  {
    const std::string package (std::string_view{it});

    if (package.empty() || m_packages_confirmed.contains(package) || m_data->packages.additional.contains(package))
      continue;

    m_queue.add(package);
  }

  send_next();
}


void PackagesWidget::send_next()
{
  using milliseconds = std::chrono::milliseconds;

  static const std::size_t MaxActiveRequests = 3;
  // put 'any' arch to avoid missing packages, i.e. "reflector" is 'any'
  static const constexpr auto SearchString =  "https://archlinux.org/packages/search/json/?"
                                              "arch=x86_64&arch=any&repo=Core&repo=Extra&name={}";

  const auto session_id = WApplication::instance()->sessionId();
  milliseconds delay {200};

  std::size_t i{};
  while (!m_queue.empty() && i++ < MaxActiveRequests)
  {
    const auto package = m_queue.next();

    WServer::instance()->schedule(delay, session_id, [this, package]()
    {
      // TODO should be removing each child when done?
      Http::Client * client = addChild(make_wt<Http::Client>());
      client->setMaxRedirects(10);
      client->setMaximumResponseSize(16 * 1024);
      client->done().connect(this, &PackagesWidget::on_response);

      if (client->get(std::format(SearchString, package)))
      {
        ++m_sent;
        m_packages_pending.emplace(package);
      }
      else
        removeChild(client);
    });

    // need this because the calls to schedule() are microseconds apart
    delay += milliseconds(200);
  }
}


void PackagesWidget::on_response(std::error_code rsp_err, const Http::Message& rsp)
{
  // note: the response does not include the name of a package that does not exist, only
  //       an empty results array, so the entry remains in m_packages_pending

  ++m_rcvd;

  const bool error = rsp.status() != 200;

  // TODO display something on UI
  if (error)
    PLOGE << "Package search response: " << rsp.status() << " : " << rsp_err.message();
  else if (const auto results = parse_response(rsp.body()); !results.empty())
  {
    const Json::Object& result = results[0];
    const auto name = result.get("pkgname");

    m_packages_confirmed.insert(m_packages_pending.extract(name));

    m_list_confirmed->addItem(name);
    m_data->packages.additional.emplace(name);
  }

  if (m_rcvd == m_sent)
  {
    m_line_packages->setText(flatten(m_packages_pending));

    if (error)
    {
      m_packages_pending.clear();
      m_queue.clear();
    }

    if (m_queue.empty())
      enable_search(true);
    else
      send_next();
  }

  WApplication::instance()->triggerUpdate();
}


Json::Array PackagesWidget::parse_response(const std::string body)
{
  Json::Array results{};

  Json::Value root;
  if (Json::ParseError err; !Json::parse(body, root, err))
    PLOGE << "Response contains invalid JSON";
  else if (root.type() != Json::Type::Object)
    PLOGE << "Response JSON root is not an object";
  else if (Json::Object root_object = root; !(root_object.contains("results") && root_object.get("results").type() == Json::Type::Array))
    PLOGE << "Response JSON 'results' does not exist or is not an array";
  else
    results = root_object.get("results");

  return results;
}


void PackagesWidget::enable_search(const bool enable)
{
  m_line_packages->setEnabled(enable);
  m_btn_search->setEnabled(enable);
  m_btn_clear->setEnabled(enable);
  m_btn_rmv->setEnabled(enable);
}
