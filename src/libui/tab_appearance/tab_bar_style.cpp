#include "tab_bar_style.h"

#include <QPainter>
#include <QPixmap>
#include <QSize>
#include <QWidget>

TabBarStyle::TabBarStyle(QObject* parent) : QProxyStyle(nullptr) {
  setParent(parent);
}

TabBarStyle::TabBarStyle(TabAppearance* appearance, QObject* parent)
    : QProxyStyle(nullptr), appearance_(appearance) {
  setParent(parent);
}

void TabBarStyle::setAppearance(TabAppearance* appearance) {
  if (appearance_ == appearance) {
    return;
  }
  delete appearance_;
  appearance_ = appearance;
  // 触发所属 QTabBar 重绘
  if (auto* w = qobject_cast<QWidget*>(parent())) {
    w->update();
  }
}

QSize TabBarStyle::sizeFromContents(ContentsType type, const QStyleOption* opt,
                                    const QSize& size, const QWidget* w) const {
  QSize base = QProxyStyle::sizeFromContents(type, opt, size, w);
  if (type == CT_TabBarTab && appearance_) {
    if (const auto* tabOpt = qstyleoption_cast<const QStyleOptionTab*>(opt)) {
      base = appearance_->adjustSize(tabOpt, base);
      base.setHeight(theme_.tabHeight);
    }
  }
  return base;
}

void TabBarStyle::drawControl(ControlElement element, const QStyleOption* opt,
                              QPainter* p, const QWidget* w) const {
  // 标签文字按主题配色绘制，TabTheme 的文字色在此生效。基类对异形 tab
  // 没有可依赖的布局，故自行排布：无图标时居中，有图标时图标与文字
  // 整体居中，超宽省略
  if (element == CE_TabBarTabLabel && appearance_) {
    if (const auto* tabOpt = qstyleoption_cast<const QStyleOptionTab*>(opt)) {
      const bool selected = tabOpt->state & QStyle::State_Selected;
      p->save();
      p->setPen(selected ? theme_.selectedTextColor : theme_.normalTextColor);

      const QRect rect = tabOpt->rect;
      const QSize icon_size =
          tabOpt->iconSize.isValid() ? tabOpt->iconSize : QSize(16, 16);
      const bool has_icon = !tabOpt->icon.isNull();
      const QPixmap icon =
          has_icon ? tabOpt->icon.pixmap(icon_size) : QPixmap();
      constexpr int kIconGap = 4;
      const int icon_width = has_icon ? icon.width() + kIconGap : 0;
      const QString text = p->fontMetrics().elidedText(
          tabOpt->text, Qt::ElideRight, qMax(0, rect.width() - icon_width));

      if (!has_icon) {
        p->drawText(rect, Qt::AlignCenter | Qt::TextShowMnemonic, text);
      } else {
        const int total =
            icon.width() + kIconGap + p->fontMetrics().horizontalAdvance(text);
        const int x0 = rect.center().x() - (total / 2);
        p->drawPixmap(x0, rect.center().y() - (icon.height() / 2), icon);
        p->drawText(QRect(x0 + icon.width() + kIconGap, rect.y(),
                          rect.right() - x0 - icon.width() - kIconGap + 1,
                          rect.height()),
                    Qt::AlignVCenter | Qt::TextShowMnemonic, text);
      }
      p->restore();
      return;
    }
  }

  if (element != CE_TabBarTabShape || !appearance_) {
    QProxyStyle::drawControl(element, opt, p, w);
    return;
  }

  const auto* tabOpt = qstyleoption_cast<const QStyleOptionTab*>(opt);
  if (!tabOpt) {
    QProxyStyle::drawControl(element, opt, p, w);
    return;
  }

  const auto state = tabOpt->state;

  if (state & QStyle::State_Selected) {
    const QPainterPath path = appearance_->shape(tabOpt, theme_);
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->setPen(Qt::NoPen);
    p->setBrush(theme_.selectedBackground);
    p->drawPath(path);
    p->restore();
  } else if (state & QStyle::State_MouseOver) {
    const QPainterPath path = appearance_->shape(tabOpt, theme_);
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->setPen(Qt::NoPen);
    p->setBrush(theme_.hoverBackground);
    p->drawPath(path);
    p->restore();
  } else {
    // 未选中未 hover 时，视外观决定是否画分隔线
    if (appearance_->wantsSeparator(tabOpt)) {
      const QLineF line = appearance_->separatorLine(tabOpt);
      if (!line.isNull()) {
        p->save();
        p->setPen(QPen(theme_.separatorColor, 1));
        p->drawLine(line);
        p->restore();
      }
    }
  }
}
