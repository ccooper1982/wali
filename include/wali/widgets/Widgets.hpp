#ifndef WALI_WIDGETS_H
#define WALI_WIDGETS_H

#include <Wt/WMenu.h>
#include <Wt/WMenuItem.h>
#include <Wt/WServer.h>
#include <Wt/WWidget.h>
#include <wali/widgets/WaliWidget.hpp>
#include <wali/widgets/Common.hpp>
#include <wali/widgets/WidgetData.hpp>
#include <wali/widgets/AccountsWidget.hpp>
#include <wali/widgets/IntroductionWidget.hpp>
#include <wali/widgets/InstallWidget.hpp>
#include <wali/widgets/LocaliselWidget.hpp>
#include <wali/widgets/MountsWidget.hpp>
#include <wali/widgets/NetworkWidget.hpp>
#include <wali/widgets/PackagesWidget.hpp>
#include <wali/widgets/PartitionsWidget.hpp>
#include <wali/widgets/VideoWidget.hpp>
#include <wali/widgets/WaliWidget.hpp>


// TODO may a better way of doing this
//  - Install.cpp requires access to all widgets, but they are children of the menu
//  - Could store values in the session variable, but seems to require a database, which is overkill
struct Widgets
{
  void create_menu (WMenu * menu)
  {
    menu->setInternalPathEnabled();

    m_widgets.insert(add_menu_widget<IntroductionWidget>(menu, "Introduction"));
    // m_widgets.insert(add_menu_widget<PartitionsWidget>(menu, "Partitions"));
    m_widgets.insert(add_menu_widget<MountsWidget>(menu, "Mounts"));
    m_widgets.insert(add_menu_widget<NetworkWidget>(menu, "Network"));
    m_widgets.insert(add_menu_widget<AccountWidget>(menu, "Accounts"));
    m_widgets.insert(add_menu_widget<LocaliseWidget>(menu, "Locale"));
    m_widgets.insert(add_menu_widget<VideoWidget>(menu, "Video"));
    m_widgets.insert(add_menu_widget<PackagesWidget>(menu, "Packages"));
    m_widgets.insert(add_menu_widget<InstallWidget>(menu, "Install", this));
  }

  IntroductionWidget * get_intro() const { return get<IntroductionWidget>("Introduction"); }
  // PartitionsWidget * get_partitions() const { return get<PartitionsWidget>("Partitions"); }
  MountsWidget * get_mounts() const { return get<MountsWidget>("Mounts"); }
  NetworkWidget * get_network() const { return get<NetworkWidget>("Network"); }
  AccountWidget * get_account() const { return get<AccountWidget>("Accounts"); }
  LocaliseWidget * get_localise() const { return get<LocaliseWidget>("Locale"); }
  VideoWidget * get_video() const { return get<VideoWidget>("Video"); }
  PackagesWidget * get_packages() const { return get<PackagesWidget>("Packages"); }
  InstallWidget * get_install() const { return get<InstallWidget>("Install"); }

  const WidgetsMap& get_all() { return m_widgets; }

  WidgetData get_data() const
  {
    return {
              .accounts = get_account()->get_data(),
              .mounts = get_mounts()->get_data(),
              .packages = get_packages()->get_data(),
              .network = get_network()->get_data(),
              .localise = get_localise()->get_data(),
              .video = get_video()->get_data()
            };
  }

private:

  template<class WidgetT, typename...Args> requires std::derived_from<WidgetT, WWidget>
  std::pair<std::string, WidgetT*> add_menu_widget (WMenu * menu, const std::string& name, Args... args)
  {
    auto item = menu->addItem(name.data(), make_wt<WidgetT>(std::forward<Args>(args)...));
    item->setObjectName(name.data());
    item->setPathComponent(name.data());
    return {name, dynamic_cast<WidgetT *>(item->contents())};
  }

  template<class WidgetT> requires std::derived_from<WidgetT, WWidget>
  WidgetT * get (const std::string& name) const
  {
    return m_widgets.contains(name) ? std::get<WidgetT*>(m_widgets.at(name)) : nullptr;
  }

private:
  WidgetsMap m_widgets;
};


#endif
