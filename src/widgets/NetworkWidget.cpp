#include <wali/widgets/NetworkWidget.hpp>


NetworkWidget::NetworkWidget()
{
  auto layout = setLayout(make_wt<WVBoxLayout>());

  auto form_container = layout->addWidget(make_wt<WContainerWidget>());
  auto form_layout = form_container->setLayout(make_wt<WGridLayout>());

  form_container->setWidth(300);

  form_layout->addWidget(make_wt<WText>("Hostname"), 0, 0);
  m_hostname = form_layout->addWidget(make_wt<WLineEdit>("archlinux"), 0, 1);
  m_hostname->changed().connect(this, [this]
  {
    m_data.hostname = m_hostname->text().toUTF8();
    m_data.ntp = m_ntp->isChecked();
    m_data.copy_config = m_copy_config->isChecked();
  });
  m_data.hostname = m_hostname->text().toUTF8();

  form_layout->addWidget(make_wt<WText>("NTP"), 1, 0);
  m_ntp = form_layout->addWidget(make_wt<WCheckBox>(""), 1, 1);
  m_ntp->setCheckState(Wt::CheckState::Checked);

  form_layout->addWidget(make_wt<WText>("Copy Wi-Fi Config"), 2, 0);
  m_copy_config = form_layout->addWidget(make_wt<WCheckBox>(""), 2, 1);
  m_copy_config->setCheckState(CheckState::Checked);

  layout->addWidget(make_wt<WText>(text), 1);
}
