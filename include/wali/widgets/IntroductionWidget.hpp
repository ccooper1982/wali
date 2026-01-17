#ifndef WALI_INTROWIDGET_H
#define WALI_INTROWIDGET_H

#include <Wt/WVBoxLayout.h>
#include <wali/widgets/WaliWidget.hpp>
#include <wali/widgets/MessagesWidget.hpp>

class IntroductionWidget : public WaliWidget<void>
{
  static constexpr auto text = R"(
    <h1>Web Arch Linux Installer</h1>
    <h3>Notes</h3>
    <ul>
      <li>Apart from creating partitions, no changes are applied until 'Install' is pressed in the final step</li>
      <li>WALI does not offer 'profiles' yet</li>
    </ul>
    <p>If in doubt, check the Arch install <a href="https://wiki.archlinux.org/title/Installation_guide" target="_blank">guide</a>.</p>
    <h3>Useful Links</h3>
    <ul>
      <li><a href="https://wiki.archlinux.org/title/Installation_guide#Partition_the_disks" target="_blank">Partitioning</a></li>
      <li><a href="https://wiki.archlinux.org/title/Graphics_processing_unit#Installation" target="_blank">GPU</a></li>
    </ul>
    )";

public:
  IntroductionWidget()
  {
    auto layout = setLayout(make_wt<WVBoxLayout>());
    layout->addWidget(make_wt<Wt::WText>(text));
    layout->addStretch(1);

    // TODO show version
  }
};

#endif
