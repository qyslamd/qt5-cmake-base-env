// QWidget 外壳编译单元：包含 OSG（仅为 osg::Node* 公开 API）与 Qt Widgets，
// 不包含任何 Qt GL 头，因此可与 scene_tree_model 等 OSG 代码共存。
#include "osg_widget.h"

#include <QDebug>
#include <QMouseEvent>
#include <QLabel>
#include <QPainter>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <cmath>

#include "osg_view.h"

namespace {

constexpr int kTimerIntervalMs = 16;
constexpr int kBallRadiusPx = 5;
constexpr int kBallHitRadiusPx = 12;
constexpr int kLabelGapXPx = 56;
constexpr int kLabelGapYPx = 42;
constexpr int kEdgeMarginPx = 8;

struct BallLineItem {
  QPointF ball;
  QPointF target;
  bool line_visible = false;
};

// 透明引线层：圆点 + 引线，鼠标事件留给 3D 视图
class LeaderLineLayer : public QWidget {
 public:
  explicit LeaderLineLayer(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
  }

  void SetItems(const QVector<BallLineItem>& items) {
    items_ = items;
    update();
  }

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(255, 220, 70), 1.5));
    for (const BallLineItem& item : items_) {
      if (item.line_visible) {
        painter.setBrush(Qt::NoBrush);
        painter.drawLine(item.ball, item.target);
      }
      painter.setBrush(QColor(255, 220, 70));
      painter.drawEllipse(item.ball, kBallRadiusPx, kBallRadiusPx);
    }
  }

 private:
  QVector<BallLineItem> items_;
};

QLabel* CreateAnnotationLabel(QWidget* parent, const QString& text) {
  auto* label = new QLabel(parent);
  label->setText(text);
  label->setWordWrap(true);
  label->setAttribute(Qt::WA_Hover);
  label->setProperty("role", "annotation");
  label->adjustSize();
  label->setVisible(false);  // 点击对应的球才显示
  return label;
}

}  // namespace

OsgWidget::OsgWidget(QWidget* parent) : QWidget(parent) {
  view_ = CreateOsgView();

  view_widget_ = view_->AsWidget();
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(view_widget_);
  view_widget_->installEventFilter(this);

  // 引线层先创建（在底层），QSS 文字标签在加载后创建并盖在上面
  line_layer_ = new LeaderLineLayer(this);
  line_layer_->setGeometry(rect());

  view_->SetNodePickedCallback([this](quintptr handle) {
    // 点空白处收起所有标注
    if (handle == 0) {
      CloseAllAnnotations();
    }
    if (on_node_picked_) {
      on_node_picked_(reinterpret_cast<osg::Node*>(handle));
    }
  });

  timer_ = new QTimer(this);
  timer_->setInterval(kTimerIntervalMs);
  connect(timer_, &QTimer::timeout, this, [this]() {
    UpdateAnnotations();
    // 持续驱动 3D 重绘：OSG 的鼠标事件在 frame() 中消费
    if (QWidget* render_widget = view_->AsWidget()) {
      render_widget->update();
    }
  });
  timer_->start();
}

OsgWidget::~OsgWidget() = default;

void OsgWidget::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  if (line_layer_ != nullptr) {
    line_layer_->setGeometry(rect());
  }
  UpdateAnnotations();
}

bool OsgWidget::LoadModel(const QString& path, QString* error) {
  if (!view_->LoadModel(path, error)) {
    return false;
  }
  RebuildAnnotations();
  UpdateAnnotations();
  return true;
}

bool OsgWidget::LoadScene(const QString& config_path, QString* error) {
  if (!view_->LoadScene(config_path, error)) {
    return false;
  }
  RebuildAnnotations();
  UpdateAnnotations();
  return true;
}

QStringList OsgWidget::GetModelNodeNames() const {
  return view_->GetNodeNames();
}

osg::Node* OsgWidget::GetSceneRoot() const {
  return reinterpret_cast<osg::Node*>(view_->SceneRoot());
}

void OsgWidget::FocusOnNode(const QString& name) {
  view_->FocusNode(view_->FindNodeByName(name));
}

void OsgWidget::FocusOnNode(osg::Node* node) {
  view_->FocusNode(reinterpret_cast<quintptr>(node));
}

void OsgWidget::ResetView() { view_->ResetView(); }

void OsgWidget::SetAxesVisible(bool visible) { view_->SetAxesVisible(visible); }

void OsgWidget::SetAnnotationsVisible(bool visible) {
  annotations_visible_ = visible;
  UpdateAnnotations();
}

void OsgWidget::SetHighlightNode(osg::Node* node) {
  if (node == nullptr) {
    view_->ClearHighlight();
    return;
  }
  view_->SetHighlightNode(reinterpret_cast<quintptr>(node));
}

void OsgWidget::ClearHighlight() { view_->ClearHighlight(); }

QImage OsgWidget::GrabFramebuffer() {
  // QWidget::grab 同时包含 3D 内容与 QWidget 标注
  return grab().toImage();
}

bool OsgWidget::eventFilter(QObject* watched, QEvent* event) {
  if (watched == view_widget_ && event->type() == QEvent::MouseButtonPress &&
      annotations_visible_) {
    auto* mouse_event = static_cast<QMouseEvent*>(event);
    if (mouse_event->button() == Qt::LeftButton) {
      const qreal dpr = devicePixelRatioF();
      const QPointF pos = mouse_event->localPos();
      for (int i = 0; i < annotations_.size(); ++i) {
        PartAnnotation& annotation = annotations_[i];
        double sx = 0.0;
        double sy = 0.0;
        if (!view_->Project(annotation.anchor[0], annotation.anchor[1],
                            annotation.anchor[2], &sx, &sy)) {
          continue;
        }
        const QPointF ball(sx / dpr, sy / dpr);
        const double dx = pos.x() - ball.x();
        const double dy = pos.y() - ball.y();
        if (dx * dx + dy * dy <= kBallHitRadiusPx * kBallHitRadiusPx) {
          annotation.open = !annotation.open;
          UpdateAnnotations();
          return true;  // 消费事件，不触发旋转与拾取
        }
      }
    }
  }
  return QWidget::eventFilter(watched, event);
}

void OsgWidget::RebuildAnnotations() {
  for (PartAnnotation& annotation : annotations_) {
    delete annotation.label;
  }
  annotations_.clear();

  const QVector<SceneAnnotation> parts = view_->SceneAnnotations();
  annotations_.reserve(parts.size());
  for (const SceneAnnotation& part : parts) {
    PartAnnotation annotation;
    annotation.anchor[0] = part.x;
    annotation.anchor[1] = part.y;
    annotation.anchor[2] = part.z;
    annotation.label = CreateAnnotationLabel(this, part.title);
    annotations_.push_back(annotation);
  }
}

void OsgWidget::CloseAllAnnotations() {
  bool changed = false;
  for (PartAnnotation& annotation : annotations_) {
    if (annotation.open) {
      annotation.open = false;
      changed = true;
    }
  }
  if (changed) {
    UpdateAnnotations();
  }
}

void OsgWidget::UpdateAnnotations() {
  if (line_layer_ == nullptr) {
    return;
  }
  auto* layer = static_cast<LeaderLineLayer*>(line_layer_);

  if (!annotations_visible_) {
    layer->SetItems({});
    layer->setVisible(false);
    for (PartAnnotation& annotation : annotations_) {
      if (annotation.label != nullptr) {
        annotation.label->setVisible(false);
      }
    }
    return;
  }

  const qreal dpr = devicePixelRatioF();
  QVector<BallLineItem> items;
  items.reserve(annotations_.size());
  for (PartAnnotation& annotation : annotations_) {
    double sx = 0.0;
    double sy = 0.0;
    if (!view_->Project(annotation.anchor[0], annotation.anchor[1],
                        annotation.anchor[2], &sx, &sy)) {
      if (annotation.label != nullptr) {
        annotation.label->setVisible(false);
      }
      continue;
    }

    BallLineItem item;
    item.ball = QPointF(sx / dpr, sy / dpr);
    QPointF target(item.ball.x() + kLabelGapXPx,
                   item.ball.y() - kLabelGapYPx);
    if (annotation.label != nullptr) {
      const QSize hint = annotation.label->sizeHint();
      if (target.x() + hint.width() > width() - kEdgeMarginPx) {
        target.setX(item.ball.x() - kLabelGapXPx - hint.width());
      }
      target.setY(qBound(static_cast<double>(kEdgeMarginPx), target.y(),
                         static_cast<double>(height() - kEdgeMarginPx)));
      annotation.label->resize(hint);
      annotation.label->move(
          static_cast<int>(target.x()),
          static_cast<int>(target.y() - hint.height() * 0.5));
      annotation.label->setVisible(annotation.open);
      if (annotation.open) {
        annotation.label->raise();
      }
    }

    item.target = target;
    item.line_visible = annotation.open;
    items.push_back(item);
  }

  layer->SetItems(items);
  layer->setVisible(!items.isEmpty());
}
