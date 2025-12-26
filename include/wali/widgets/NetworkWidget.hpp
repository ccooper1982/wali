#ifndef WALI_NETWORKWIDGET_H
#define WALI_NETWORKWIDGET_H

#include <wali/Common.hpp>

class NetworkWidget : public Wt::WContainerWidget
{
  static constexpr auto text = R"(<ul><li>If using Wi-Fi, copy the iwd config to have internet connectivity in the live system</li></ul>)";

public:
  NetworkWidget()
  {
    auto layout = setLayout(make_wt<Wt::WVBoxLayout>());

    auto form_container = layout->addWidget(make_wt<Wt::WContainerWidget>());
    auto form_layout = form_container->setLayout(make_wt<Wt::WGridLayout>());

    form_container->setWidth(300);

    form_layout->addWidget<Wt::WText>(make_wt<Wt::WText>("Hostname"), 0, 0);
    m_hostname = form_layout->addWidget<Wt::WLineEdit>(make_wt<Wt::WLineEdit>("archlinux"), 0, 1);
    m_hostname->setWidth(100); // TODO does nothing

    form_layout->addWidget<Wt::WText>(make_wt<Wt::WText>("NTP"), 1, 0);
    m_ntp = form_layout->addWidget<Wt::WCheckBox>(make_wt<Wt::WCheckBox>(""), 1, 1);
    m_ntp->setCheckState(Wt::CheckState::Checked);

    form_layout->addWidget<Wt::WText>(make_wt<Wt::WText>("Copy Wi-Fi Config"), 2, 0);
    m_copy_config = form_layout->addWidget<Wt::WCheckBox>(make_wt<Wt::WCheckBox>(""), 2, 1);
    m_copy_config->setCheckState(Wt::CheckState::Checked);

    layout->addWidget<Wt::WText>(make_wt<Wt::WText>(text), 1);
  }

  bool ntp() const { return m_ntp->isChecked(); }
  bool copy_config() const { return m_copy_config->isChecked(); }

private:
  Wt::WLineEdit * m_hostname;
  Wt::WCheckBox * m_ntp;
  Wt::WCheckBox * m_copy_config;
};


#endif
