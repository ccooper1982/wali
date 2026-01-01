#ifndef WALI_NETWORKWIDGET_H
#define WALI_NETWORKWIDGET_H

#include <wali/Common.hpp>
#include <wali/widgets/Common.hpp>

class NetworkWidget : public WContainerWidget
{
  static constexpr auto text = R"(<ul><li>If using Wi-Fi, copy the iwd config to have internet connectivity in the live system</li></ul>)";

public:
  NetworkWidget()
  {
    auto layout = setLayout(make_wt<WVBoxLayout>());

    auto form_container = layout->addWidget(make_wt<WContainerWidget>());
    auto form_layout = form_container->setLayout(make_wt<WGridLayout>());

    form_container->setWidth(300);

    form_layout->addWidget(make_wt<WText>("Hostname"), 0, 0);
    m_hostname = form_layout->addWidget(make_wt<WLineEdit>("archlinux"), 0, 1);
    m_hostname->setWidth(100); // TODO does nothing

    form_layout->addWidget(make_wt<WText>("NTP"), 1, 0);
    m_ntp = form_layout->addWidget(make_wt<WCheckBox>(""), 1, 1);
    m_ntp->setCheckState(Wt::CheckState::Checked);

    form_layout->addWidget(make_wt<WText>("Copy Wi-Fi Config"), 2, 0);
    m_copy_config = form_layout->addWidget(make_wt<WCheckBox>(""), 2, 1);
    m_copy_config->setCheckState(CheckState::Checked);

    layout->addWidget(make_wt<WText>(text), 1);
  }

  bool ntp() const { return m_ntp->isChecked(); }
  bool copy_config() const { return m_copy_config->isChecked(); }
  std::string hostname () const { return m_hostname->valueText().toUTF8(); }

private:
  WLineEdit * m_hostname;
  WCheckBox * m_ntp;
  WCheckBox * m_copy_config;
};


#endif
