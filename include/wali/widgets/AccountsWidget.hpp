#ifndef WALI_ACCOUNTSIDGET_H
#define WALI_ACCOUNTSIDGET_H

#include <Wt/WCheckBox.h>
#include <Wt/WGlobal.h>
#include <wali/widgets/WidgetData.hpp>
#include <wali/Common.hpp>
#include <wali/widgets/MessagesWidget.hpp>
#include <wali/widgets/WaliWidget.hpp>


class AccountWidget : public WaliWidget
{
public:
  AccountWidget (WidgetDataPtr data);

  bool check_validity() const;

private:
  void update_data();

private:
  WLineEdit * m_root_password,
                * m_user_username,
                * m_user_password;
  WCheckBox * m_user_sudo;
  WComboBox * m_user_shell;
  MessageWidget * m_messages;
};


#endif
