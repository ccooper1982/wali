#ifndef WALI_WIDGETS_H
#define WALI_WIDGETS_H

#include <concepts>
#include <string_view>
#include <Wt/WMenu.h>
#include <Wt/WWidget.h>
#include <wali/widgets/Common.hpp>
#include <wali/widgets/AccountsWidget.hpp>
#include <wali/widgets/IntroductionWidget.hpp>
#include <wali/widgets/InstallWidget.hpp>
#include <wali/widgets/NetworkWidget.hpp>
#include <wali/widgets/PartitionWidget.hpp>

// TODO may a better way of doing this
//  - The InstallWidget requires access to all widgets, but they are children of the menu
//  - Could store values in the session variable, but seems to require a database, which is overkill
struct Widgets
{
  static void create_menu (WMenu * menu)
  {
    m_menu = menu;

    create_intro();
    create_partitions();
    create_network();
    create_accounts();
    create_install();
  }

  static IntroductionWidget * get_intro() { return get<IntroductionWidget>("Introduction"); }
  static PartitionsWidget * get_partitions() { return get<PartitionsWidget>("Partitions"); }
  static NetworkWidget * get_network() { return get<NetworkWidget>("Network"); }
  static AccountWidget * get_account() { return get<AccountWidget>("Accounts"); }
  static InstallWidget * get_install() { return get<InstallWidget>("Install"); }

private:

  static void create_intro() { add_menu_widget<IntroductionWidget>("Introduction"); }
  static void create_partitions() { add_menu_widget<PartitionsWidget>("Partitions"); }
  static void create_network() { add_menu_widget<NetworkWidget>("Network"); }
  static void create_accounts() { add_menu_widget<AccountWidget>("Accounts"); }
  static void create_install() { add_menu_widget<InstallWidget>("Install"); }

  template<class WidgetT, typename...Args> requires std::derived_from<WidgetT, WWidget>
  static void add_menu_widget(const std::string_view name, Args... args)
  {
    auto widget = m_menu->addItem(name.data(), make_wt<WidgetT>(args...));
    widget->setObjectName(name.data());
  }

  template<class WidgetT> requires std::derived_from<WidgetT, WWidget>
  static WidgetT * get (const std::string_view name)
  {
    const auto& items = m_menu->items();

    auto it_item = std::find_if(items.cbegin(), items.cend(), [name](const WMenuItem * item)
    {
      return item->objectName() == name;
    });

    return it_item == items.cend() ? nullptr : dynamic_cast<WidgetT*>((*it_item)->contents());
  }

private:
  inline static WMenu * m_menu;
};


#endif
