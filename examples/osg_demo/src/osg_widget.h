#ifndef OSG_QT_VIEWER_SRC_OSG_WIDGET_H_
#define OSG_QT_VIEWER_SRC_OSG_WIDGET_H_

#include <QImage>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>
#include <functional>
#include <memory>

#include <osg/Node>

class IOsgView;
class QLabel;
class QTimer;

// 查看器外壳（普通 QWidget，不含任何 Qt GL 头）：
// - 3D 渲染由 IOsgView（QOpenGLWidget 实现）承担
// - 标注（圆点/引线/方框/文字）全部是 Qt 2D：QPainter 子控件 + QLabel
// - 每个零件一个标注球，点击球展开该零件标签，点击空白收起全部
class OsgWidget : public QWidget {
  Q_OBJECT

 public:
  explicit OsgWidget(QWidget* parent = nullptr);
  ~OsgWidget() override;

  bool LoadModel(const QString& path, QString* error = nullptr);
  bool LoadScene(const QString& config_path, QString* error = nullptr);
  QStringList GetModelNodeNames() const;
  // 场景图根节点（含高亮包围盒等内部节点），供场景树模型包装
  osg::Node* GetSceneRoot() const;
  void FocusOnNode(const QString& name);
  void FocusOnNode(osg::Node* node);
  void ResetView();
  void SetAxesVisible(bool visible);
  void SetAnnotationsVisible(bool visible);
  bool AreAnnotationsVisible() const { return annotations_visible_; }
  void SetHighlightNode(osg::Node* node);
  void ClearHighlight();
  QImage GrabFramebuffer();

  // 3D 点击拾取回调：空白处回调 nullptr
  std::function<void(osg::Node*)> on_node_picked_;

 protected:
  void resizeEvent(QResizeEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  struct PartAnnotation {
    double anchor[3] = {0.0, 0.0, 0.0};
    QLabel* label = nullptr;
    bool open = false;
  };

  void RebuildAnnotations();
  void UpdateAnnotations();
  void CloseAllAnnotations();

  std::unique_ptr<IOsgView> view_;
  QWidget* view_widget_ = nullptr;
  QTimer* timer_ = nullptr;

  QWidget* line_layer_ = nullptr;
  QVector<PartAnnotation> annotations_;
  bool annotations_visible_ = true;
};

#endif  // OSG_QT_VIEWER_SRC_OSG_WIDGET_H_
