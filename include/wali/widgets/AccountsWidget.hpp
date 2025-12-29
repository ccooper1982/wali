#ifndef WALI_ACCOUNTSIDGET_H
#define WALI_ACCOUNTSIDGET_H

#include <Wt/WCheckBox.h>
#include <Wt/WGlobal.h>
#include <string>
#include <wali/Common.hpp>
#include <wali/widgets/Common.hpp>
#include <wali/widgets/MessagesWidget.hpp>

class AccountWidget : public WContainerWidget
{
  template<typename WidgetT, typename ...WidgetArgs>
  WidgetT* add_form_pair (WLayout * parent_layout, const std::string_view label_text, WidgetArgs&&... widget_args)
  {
    auto cont = make_wt<WContainerWidget>();
    auto layout = cont->setLayout(make_wt<WHBoxLayout>());

    auto label = layout->addWidget(make_wt<WLabel>(label_text.data()));
    auto widget = layout->addWidget(make_wt<WidgetT>(widget_args...));
    label->setBuddy(widget);

    layout->addStretch(1);

    parent_layout->addWidget(std::move(cont));

    return widget;
  }

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

      cont_layout->addWidget(make_wt<WLabel>("<h2>User</h2>"));
      m_user_username = add_form_pair<WLineEdit>(cont_layout, "Username");
      m_user_password = add_form_pair<WLineEdit>(cont_layout, "Password");

      m_user_sudo = add_form_pair<WCheckBox>(cont_layout, "Sudo");
      m_user_sudo->setCheckState(CheckState::Checked);
      m_user_sudo->changed().connect([this]
      {
        if (m_user_sudo->isChecked())
          m_messages->clear_messages();
        else
          m_messages->add("The user cannot run sudo commands. Do you mean this?", MessageWidget::Level::Warning);
      });
    }

    layout->addWidget(std::move(messages));

    layout->addStretch(1);
  }

  const std::string get_root_password() const
  {
    return m_root_password->valueText().toUTF8();
  }

  const std::string get_user_username() const
  {
    return m_user_username->valueText().toUTF8();
  }

  const std::string get_user_password() const
  {
    return m_user_password->valueText().toUTF8();
  }

  const bool get_user_can_sudo() const
  {
    return m_user_sudo->isChecked();
  }


private:
  Wt::WLineEdit * m_root_password,
                * m_user_username,
                * m_user_password;
  Wt::WCheckBox * m_user_sudo;
  MessageWidget * m_messages;
};

#endif
