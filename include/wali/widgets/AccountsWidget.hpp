#ifndef WALI_ACCOUNTSIDGET_H
#define WALI_ACCOUNTSIDGET_H

#include <Wt/WCheckBox.h>
#include <Wt/WGlobal.h>
#include <wali/widgets/WidgetData.hpp>
#include <wali/Common.hpp>
#include <wali/widgets/MessagesWidget.hpp>
#include <wali/widgets/WaliWidget.hpp>


class AccountWidget : public WaliWidget<AccountsData>
{
public:
  AccountWidget ()
  {
    auto messages = make_wt<MessageWidget>();
    messages->setWidth(600);
    m_messages = messages.get();

    auto layout = setLayout(make_wt<WVBoxLayout>());
    auto cont_form = layout->addWidget(make_wt<WContainerWidget>());
    auto cont_layout = cont_form->setLayout(make_wt<WVBoxLayout>());

    {
      cont_layout->addWidget(make_wt<WLabel>("<h2>Root</h2>"));
      m_root_password = add_form_pair<WLineEdit>(cont_layout, "Password", "arch");
      m_root_password->changed().connect(this, &AccountWidget::update_data);
      m_data.root_pass = m_root_password->text().toUTF8();

      cont_layout->addWidget(make_wt<WLabel>("<h2>User</h2>"));
      m_user_username = add_form_pair<WLineEdit>(cont_layout, "Username");
      m_user_password = add_form_pair<WLineEdit>(cont_layout, "Password");
      m_user_username->changed().connect(this, &AccountWidget::update_data);
      m_user_password->changed().connect(this, &AccountWidget::update_data);
      m_user_username->setMaxLength(32);
      m_user_password->setMaxLength(32);

      m_user_sudo = add_form_pair<WCheckBox>(cont_layout, "Sudo");
      m_user_sudo->setCheckState(CheckState::Checked);
      m_user_sudo->changed().connect([this]
      {
        if (m_user_sudo->isChecked())
          m_messages->clear_messages();
        else
          m_messages->add("The user cannot run sudo commands. Do you mean this?", MessageWidget::Level::Warning);

        update_data();
      });
    }

    layout->addWidget(std::move(messages));

    layout->addStretch(1);
  }

  void update_data()
  {
    m_data.root_pass = m_root_password->text().toUTF8();
    m_data.user_username = m_user_username->text().toUTF8();
    m_data.user_pass = m_user_password->text().toUTF8();
    m_data.user_sudo = m_user_sudo->isChecked();
  }

  AccountsData get_data() const override
  {
    return m_data;
  }

  bool is_valid() const override
  {
    return  !m_data.root_pass.empty() &&
            !m_data.user_username.empty() &&
            !m_data.user_pass.empty();
  }

private:
  Wt::WLineEdit * m_root_password,
                * m_user_username,
                * m_user_password;
  Wt::WCheckBox * m_user_sudo;
  MessageWidget * m_messages;
  AccountsData m_data;
};


#endif
