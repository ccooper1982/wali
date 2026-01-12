#include <wali/widgets/PackagesWidget.hpp>

static constexpr const auto IntroText = "Enter package names, separated by a space.<br/>"
                                        "Packages that don't exist, will remain in the textbox.";

enum class Level
{
  Info,
  Warning,
  Error
};


PackagesWidget::PackagesWidget()
{
  auto layout = setLayout(make_wt<WVBoxLayout>());
  layout->addWidget(make_wt<Wt::WText>(IntroText));

  auto layout_search = layout->addLayout(make_wt<WHBoxLayout>());
  m_line_packages = layout_search->addWidget(make_wt<WLineEdit>(), 1);
  m_line_packages->setStyleClass("packages");
  m_line_packages->setAttributeValue("maxlength", "100");
  m_line_packages->setPlaceholderText("nano git btop ...");
  m_line_packages->enterPressed().connect(this, &PackagesWidget::search);

  m_btn_search = layout_search->addWidget(make_wt<WPushButton>("Search"));
  m_btn_search->enterPressed().connect(this, &PackagesWidget::search);
  m_btn_search->clicked().connect(this, &PackagesWidget::search);
  m_btn_search->setStyleClass("packages");

  // list of packages to install, control buttons
  auto layout_results = layout->addLayout(make_wt<WHBoxLayout>());

  m_list_confirmed = layout_results->addWidget(make_wt<WSelectionBox>(), 2);
  m_list_confirmed->setStyleClass("packages_confirmed");
  m_list_confirmed->setVerticalSize(10);
  m_list_confirmed->setHeight(200);
  m_list_confirmed->setSelectionMode(SelectionMode::Extended);

  auto layout_control = layout_results->addLayout(make_wt<WVBoxLayout>());

  auto btn_rmv = layout_control->addWidget(make_wt<WPushButton>("Remove"));
  auto btn_clear = layout_control->addWidget(make_wt<WPushButton>("Clear"));
  layout_control->addStretch(1);

  btn_rmv->setStyleClass("packages");
  btn_clear->setStyleClass("packages");

  btn_rmv->clicked().connect([this]
  {
    const auto& selected = m_list_confirmed->selectedIndexes();

    if (selected.empty())
      return;

    for_each(selected, [this](const auto i)
    {
      data.additional.erase(m_list_confirmed->itemText(i).toUTF8());
    });

    // repopulate list with what remains
    m_list_confirmed->clear();

    for_each(data.additional, [this](const auto name)
    {
      m_list_confirmed->addItem(name);
    });
  });

  btn_clear->clicked().connect([this]
  {
    data.additional.clear();
    m_list_confirmed->clear();
  });

  layout->addStretch(1);
}


void PackagesWidget::search ()
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
    static const constexpr auto search_string = "https://archlinux.org/packages/search/json/?"
                                                "arch=x86_64&arch=any&"
                                                "repo=Core&repo=Extra&name={}";

    const std::string package (std::string_view{it});

    if (m_packages_pending.contains(package) || m_packages_confirmed.contains(package) || data.additional.contains(package))
      continue;

    Http::Client * client = addChild(make_wt<Http::Client>());
    client->setMaxRedirects(10);
    client->setMaximumResponseSize(16 * 1024);
    client->done().connect(this, &PackagesWidget::on_response);

    // put 'any' arch to avoid missing packages, i.e. "reflector" is 'any'
    if (client->get(std::format(search_string, package)))
    {
      ++m_responses_expected;
      m_packages_pending.emplace(package);
    }
  }

  // if (m_packages_pending.size())
  //   WApplication::instance()->deferRendering();
}


void PackagesWidget::on_response(std::error_code rsp_err, const Http::Message& rsp)
{
  // WApplication::instance()->resumeRendering();

  if (rsp_err || rsp.status() != 200)
  {
    // TODO diplay something on UI on err
    if (rsp.status() == 429)
      PLOGE << "Package search response: Too many requests";
    else
      PLOGE << "Package search response: " << rsp.status() << " : " << rsp_err.message();
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
      data.additional.emplace(name);
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
