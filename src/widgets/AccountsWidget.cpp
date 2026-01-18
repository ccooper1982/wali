#include <wali/widgets/AccountsWidget.hpp>


AccountWidget::AccountWidget ()
{
  auto messages = make_wt<MessageWidget>();
  messages->setWidth(600);
  m_messages = messages.get();

  auto layout = setLayout(make_wt<WVBoxLayout>());
  auto cont_form = layout->addWidget(make_wt<WContainerWidget>());
  auto cont_layout = cont_form->setLayout(make_wt<WVBoxLayout>());

  cont_layout->addWidget(make_wt<WLabel>("<h2>Root</h2>"));
  m_root_password = add_form_pair<WLineEdit>(cont_layout, "Password", 100, "arch");
  m_root_password->changed().connect(this, &AccountWidget::update_data);
  m_data.root_pass = m_root_password->text().toUTF8();

  cont_layout->addWidget(make_wt<WLabel>("<h2>User</h2>"));
  m_user_username = add_form_pair<WLineEdit>(cont_layout, "Username", 100);
  m_user_password = add_form_pair<WLineEdit>(cont_layout, "Password", 100);
  m_user_username->changed().connect(this, &AccountWidget::update_data);
  m_user_password->changed().connect(this, &AccountWidget::update_data);
  m_user_username->setMaxLength(32);
  m_user_password->setMaxLength(32);

  m_user_sudo = add_form_pair<WCheckBox>(cont_layout, "Sudo", 100);
  m_user_sudo->setCheckState(CheckState::Checked);
  m_user_sudo->changed().connect([this]
  {
    if (m_user_sudo->isChecked())
      m_messages->clear_messages();
    else
      m_messages->add("The user cannot run sudo commands. Do you mean this?", MessageWidget::Level::Warning);

    update_data();
  });


  layout->addWidget(std::move(messages));

  layout->addStretch(1);
}


void AccountWidget::update_data()
{
  m_data.root_pass = m_root_password->text().toUTF8();
  m_data.user_username = m_user_username->text().toUTF8();
  m_data.user_pass = m_user_password->text().toUTF8();
  m_data.user_sudo = m_user_sudo->isChecked();
}


bool AccountWidget::is_valid() const
{
  return  !m_data.root_pass.empty() &&
          !m_data.user_username.empty() &&
          !m_data.user_pass.empty();
}
