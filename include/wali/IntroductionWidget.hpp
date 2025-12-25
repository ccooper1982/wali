#ifndef WALI_INTROWIDGET_H
#define WALI_INTROWIDGET_H

#include <wali/MessagesWidget.hpp>

class IntroductionWidget : public Wt::WContainerWidget
{
  static constexpr auto text = R"(
    <h1>Web Arch Linux Installer</h1>
    <h2>No changes are applied until 'Install' is pressed in the final step.</h2>
    <ul>
      <li>This tool does yet not manage partitions, do so with fdisk, cfdisk, etc</li>
      <li>Something else useful</li>
      <li>Perhaps a third informative point</li>
    </ul>
    )";

public:
  IntroductionWidget()
  {
    addWidget(make_wt<Wt::WText>(text));

    // auto msg = addWidget(make_wt<MessageWidget>());
    // msg->add("Warning", MessageWidget::Level::Warning);
    // msg->add("Error", MessageWidget::Level::Error);
    // msg->add("Info", MessageWidget::Level::Info);
  }
};

#endif
