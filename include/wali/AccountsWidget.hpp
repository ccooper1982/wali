#ifndef WALI_ACCOUNTSIDGET_H
#define WALI_ACCOUNTSIDGET_H

#include <Wt/WGroupBox.h>
#include <wali/Common.hpp>


class AccountWidget : public Wt::WContainerWidget
{
public:
  AccountWidget ()
  {
    auto layout = setLayout(make_wt<Wt::WVBoxLayout>());

    auto gb_root = layout->addWidget(make_wt<Wt::WGroupBox>("Root"));
    auto gb_user = layout->addWidget(make_wt<Wt::WGroupBox>("User"));

    auto root_layout = gb_root->setLayout(make_wt<Wt::WHBoxLayout>());
    auto user_layout = gb_user->setLayout(make_wt<Wt::WGridLayout>());

    root_layout->addWidget(make_wt<Wt::WText>("Password"));
    m_root_password = root_layout->addWidget(make_wt<Wt::WLineEdit>("arch"));

    root_layout->addStretch(1);

    user_layout->addWidget(make_wt<Wt::WText>("Username"), 0, 0);
    m_user_username = user_layout->addWidget(make_wt<Wt::WLineEdit>(), 0, 1);

    user_layout->addWidget(make_wt<Wt::WText>("Password"), 1, 0);
    m_user_password = user_layout->addWidget(make_wt<Wt::WLineEdit>(), 1, 1);

    user_layout->addWidget(make_wt<Wt::WText>("Sudo"), 2, 0);
    m_user_sudo = user_layout->addWidget(make_wt<Wt::WCheckBox>(), 2, 1);
    m_user_sudo->setCheckState(Wt::CheckState::Checked);

    // use as a stretch
    user_layout->addWidget(make_wt<Wt::WText>(""),0,2);
    user_layout->setColumnStretch(2, 1);

    layout->addStretch(1);
  }

private:
  Wt::WLineEdit * m_root_password,
                * m_user_username,
                * m_user_password;
  Wt::WCheckBox * m_user_sudo;
};

#endif
