#include <wali/widgets/NetworkWidget.hpp>
#include <Wt/WCheckBox.h>

NetworkWidget::NetworkWidget(WidgetDataPtr data) : WaliWidget(data, "Network")
{
  auto layout = setLayout(make_wt<WVBoxLayout>());

  auto form_container = layout->addWidget(make_wt<WContainerWidget>());
  auto form_layout = form_container->setLayout(make_wt<WGridLayout>());

  form_container->setWidth(300);

  form_layout->addWidget(make_wt<WText>("Hostname"), 0, 0);
  m_hostname = form_layout->addWidget(make_wt<WLineEdit>("archlinux"), 0, 1);
  m_hostname->changed().connect(this, [this]
  {
    m_data->network.hostname = m_hostname->text().toUTF8();
    set_valid(!m_data->network.hostname.empty());
  });


  form_layout->addWidget(make_wt<WText>("NTP"), 1, 0);
  m_ntp = form_layout->addWidget(make_wt<WCheckBox>(""), 1, 1);
  m_ntp->setCheckState(Wt::CheckState::Checked);
  m_ntp->changed().connect(this, [this]
  {
    m_data->network.ntp = m_ntp->isChecked();
  });

  form_layout->addWidget(make_wt<WText>("Copy Wi-Fi Config"), 2, 0);
  m_copy_config = form_layout->addWidget(make_wt<WCheckBox>(""), 2, 1);
  m_copy_config->setCheckState(CheckState::Checked);
  m_copy_config->changed().connect(this, [this]
  {
    m_data->network.copy_config = m_copy_config->isChecked();
  });

  m_data->network.hostname = m_hostname->text().toUTF8();
  m_data->network.ntp = m_ntp->isChecked();
  m_data->network.copy_config = m_copy_config->isChecked();

  layout->addWidget(make_wt<WText>(text), 1);

  set_valid(!m_data->network.hostname.empty());
}

// bool NetworkWidget::is_valid() const
// {
//   return !m_data.hostname.empty();
// }
