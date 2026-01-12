#ifndef WALI_PARTITIONSWIDGET_H
#define WALI_PARTITIONSWIDGET_H

#include <Wt/WCheckBox.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WGlobal.h>
#include <Wt/WLayout.h>
#include <cstdlib>
#include <wali/DiskUtils.hpp>
#include <wali/widgets/Common.hpp>
#include <wali/widgets/WaliWidget.hpp>

class PartitionsWidget  : public WaliWidget<void>
{
public:
  PartitionsWidget();

  bool is_changed() const { return m_changed; };

private:
  void read_partitions();
  void set_devices();

  void calculate_sizes();
  void create();

private:
  Tree m_tree;
  std::map<std::string, int64_t> m_disk_sizes;

  WComboBox * m_disk,
            * m_boot,
            * m_home;
  WSpinBox  * m_root;
  WText * m_remaining;
  WPushButton * m_create;
  WText * m_total;
  bool m_changed{};
};

#endif
