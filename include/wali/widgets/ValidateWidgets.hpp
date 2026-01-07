#ifndef WALI_VALIDATEWIDGETS_H
#define WALI_VALIDATEWIDGETS_H

#include <optional>
#include <string>
#include <wali/widgets/Widgets.hpp>


template<class... Ts>
struct overload : Ts... { using Ts::operator()...; };

inline std::optional<std::string> is_config_valid(const WidgetsMap& widgets)
{
  auto check = [](const std::string_view name, const auto * w)
  {
    return w->is_valid();
  };

  for (const auto& [name, widget] : widgets)
  {
    const bool valid = std::visit(overload{
                          [&](IntroductionWidget * w){ return check(name, w);  },
                          [&](PartitionsWidget * w){ return check(name, w);  },
                          [&](NetworkWidget * w){ return check(name, w);  },
                          [&](AccountWidget * w){ return check(name, w);  },
                          [&](LocaliseWidget * w){ return check(name, w);  },
                          [&](PackagesWidget * w){ return check(name, w);  },
                          [&](InstallWidget * w){ return check(name, w);  }
                        },
                        widget);

    if (!valid)
      return {name};
  }

  return {};
}


#endif
