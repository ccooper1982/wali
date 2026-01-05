#ifndef WALI_MESSAGESWIDGET_H
#define WALI_MESSAGESWIDGET_H

#include <wali/widgets/Common.hpp>
#include <wali/Common.hpp>

class MessageWidget : public WContainerWidget
{
public:
  enum class Level {Info, Warning, Error};

  MessageWidget()
  {
    create();
  }

  void clear_messages()
  {
    create();
  }

  void add(const std::string_view msg, const Level lvl)
  {
    static const std::map<const Level, const Wt::WString> LevelValueMap
    {
      {Level::Error,    "msg_error"},
      {Level::Warning,  "msg_warning"},
      {Level::Info,     "msg_info"}
    };

    auto container = m_layout->addWidget(make_wt<Wt::WContainerWidget>());
    container->setStyleClass(LevelValueMap.at(lvl));
    container->addWidget(make_wt<Wt::WText>(msg.data()));

    if (lvl == Level::Error)
      ++m_error_count;
  }

  bool has_errors() const { return m_error_count > 0; }

private:
  void create()
  {
    m_error_count = 0;
    m_layout = setLayout(make_wt<Wt::WVBoxLayout>());
  }

private:
  Wt::WVBoxLayout * m_layout;
  std::size_t m_error_count{};
};

#endif
