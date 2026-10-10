#ifndef EXAMPLES_WIDGET_H_
#define EXAMPLES_WIDGET_H_

#include <QWidget>

class Widget : public QWidget {
  Q_OBJECT
 public:
  explicit Widget(QWidget* parent = nullptr);
  ~Widget() override;
};

#endif  // EXAMPLES_WIDGET_H_