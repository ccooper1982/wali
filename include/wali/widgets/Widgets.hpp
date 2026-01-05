#ifndef WALI_WIDGETS_H
#define WALI_WIDGETS_H

#include <concepts>
#include <string_view>
#include <Wt/WMenu.h>
#include <Wt/WMenuItem.h>
#include <Wt/WServer.h>
#include <Wt/WWidget.h>
#include <wali/widgets/WaliWidget.hpp>
#include <wali/widgets/Common.hpp>
#include <wali/widgets/AccountsWidget.hpp>
#include <wali/widgets/IntroductionWidget.hpp>
#include <wali/widgets/InstallWidget.hpp>
#include <wali/widgets/LocaliselWidget.hpp>
#include <wali/widgets/NetworkWidget.hpp>
#include <wali/widgets/PackagesWidget.hpp>
#include <wali/widgets/PartitionWidget.hpp>
#include <wali/widgets/WaliWidget.hpp>


// TODO may a better way of doing this
//  - Install.cpp requires access to all widgets, but they are children of the menu
//  - Could store values in the session variable, but seems to require a database, which is overkill
struct Widgets
{
  static void create_menu (WMenu * menu, WServer * server)
  {
    m_menu = menu;
    m_menu->setInternalPathEnabled();

    add_menu_widget<IntroductionWidget>("Introduction");
    add_menu_widget<PartitionsWidget>("Partitions");
    add_menu_widget<NetworkWidget>("Network");
    add_menu_widget<AccountWidget>("Accounts");
    add_menu_widget<LocaliseWidget>("Locale");
    add_menu_widget<PackagesWidget>("Packages");
    add_menu_widget<InstallWidget>("Install");
  }

  static IntroductionWidget * get_intro() { return get<IntroductionWidget>("Introduction"); }
  static PartitionsWidget * get_partitions() { return get<PartitionsWidget>("Partitions"); }
  static NetworkWidget * get_network() { return get<NetworkWidget>("Network"); }
  static AccountWidget * get_account() { return get<AccountWidget>("Accounts"); }
  static LocaliseWidget * get_localise() { return get<LocaliseWidget>("Locale"); }
  static PackagesWidget * get_packages() { return get<PackagesWidget>("Packages"); }
  static InstallWidget * get_install() { return get<InstallWidget>("Install"); }

private:

  template<class WidgetT, typename...Args> requires std::derived_from<WidgetT, WWidget>
  static void add_menu_widget(const std::string_view name, Args... args)
  {
    auto item = m_menu->addItem(name.data(), make_wt<WidgetT>(args...));
    item->setObjectName(name.data());
    item->setPathComponent(name.data());
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
