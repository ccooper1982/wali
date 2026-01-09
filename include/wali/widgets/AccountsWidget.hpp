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
  AccountWidget ();

  void update_data();

  AccountsData get_data() const override { return m_data; }

  bool is_valid() const override;

private:
  Wt::WLineEdit * m_root_password,
                * m_user_username,
                * m_user_password;
  Wt::WCheckBox * m_user_sudo;
  MessageWidget * m_messages;
  AccountsData m_data;
};


#endif
