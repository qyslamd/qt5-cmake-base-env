#ifndef STYLE_SHEET_DEMO_DEMO_HELPERS_H_
#define STYLE_SHEET_DEMO_DEMO_HELPERS_H_

#include <QGroupBox>
#include <QLayout>
#include <QString>
#include <QVariant>


// 把 body 装进带标题的 QGroupBox，各页面共用，省去每处 new 两行。
inline QGroupBox* MakeGroup(const QString& title, QLayout* body) {
  auto* box = new QGroupBox(title);
  box->setLayout(body);
  return box;
}

#endif  // STYLE_SHEET_DEMO_DEMO_HELPERS_H_