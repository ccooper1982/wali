#ifndef WALI_WIDGETS_H
#define WALI_WIDGETS_H

#include <concepts>
#include <Wt/WMenu.h>
#include <Wt/WWidget.h>
#include <utility>
#include <wali/Common.hpp>
#include <wali/widgets/AccountsWidget.hpp>
#include <wali/widgets/IntroductionWidget.hpp>
#include <wali/widgets/InstallWidget.hpp>
#include <wali/widgets/NetworkWidget.hpp>
#include <wali/widgets/PartitionWidget.hpp>

// TODO probably a better way of doing this
//  - The InstallWidget requires access to all widgets
//  - Could store values in the session variable (assuming that's possible)
//    - This may be better long term if more pages/sections are added
struct Widgets
{
  static void create_intro(WMenu * menu)
  {
    add_menu_widget<IntroductionWidget>(menu, "Introduction");
  }

  static void create_partitions(WMenu * menu)
  {
    add_menu_widget<PartitionsWidget>(menu, "Partitions");
  }

  static void create_network(WMenu * menu)
  {
    add_menu_widget<NetworkWidget>(menu, "Network");
  }

  static void create_accounts(WMenu * menu)
  {
    add_menu_widget<AccountWidget>(menu, "Accounts");
  }

  static void create_install(WMenu * menu)
  {
    // InstallWidget requires the menu
    add_menu_widget<InstallWidget>(menu, "Install", menu);
  }

  //
  IntroductionWidget * get_intro(WMenu * menu) { return get<IntroductionWidget>(menu, "Introduction"); }
  PartitionsWidget * get_partitions(WMenu * menu) { return get<PartitionsWidget>(menu, "Partitions"); }
  NetworkWidget * get_network(WMenu * menu) { return get<NetworkWidget>(menu, "Network"); }
  AccountWidget * get_account(WMenu * menu) { return get<AccountWidget>(menu, "Accounts"); }
  InstallWidget * get_install(WMenu * menu) { return get<InstallWidget>(menu, "Install"); }

private:
  template<class WidgetT, typename...Args> requires std::derived_from<WidgetT, WWidget>
  static void add_menu_widget(WMenu * menu, const std::string_view name, Args... args)
  {
    auto widget = menu->addItem(name.data(), make_wt<WidgetT>(args...));
    widget->setObjectName(name.data());
  }

  template<class WidgetT> requires std::derived_from<WidgetT, WWidget>
  static WidgetT * get (WMenu * parent, const std::string_view name)
  {
    const auto& items = parent->items();

    auto it_item = std::find_if(items.cbegin(), items.cend(), [name](const WMenuItem * item)
    {
      return item->objectName() == name;
    });

    return it_item == items.cend() ? nullptr : dynamic_cast<WidgetT*>((*it_item)->contents());
  }
};


#endif
