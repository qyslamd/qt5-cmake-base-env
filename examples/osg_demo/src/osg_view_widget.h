#ifndef OSG_QT_VIEWER_SRC_OSG_VIEW_WIDGET_H_
#define OSG_QT_VIEWER_SRC_OSG_VIEW_WIDGET_H_

#include <QOpenGLWidget>
#include <memory>

#include "osg_view.h"

class OsgRenderer;

// 唯一包含 Qt GL 头的类：OSG 渲染委托给独立编译单元的 OsgRenderer
class OsgViewWidget : public QOpenGLWidget, public IOsgView {
  Q_OBJECT

 public:
  explicit OsgViewWidget(QWidget* parent = nullptr);
  ~OsgViewWidget() override;

  QWidget* AsWidget() override { return this; }

  bool LoadModel(const QString& path, QString* error) override;
  bool LoadScene(const QString& path, QString* error) override;
  QStringList GetNodeNames() const override;
  QVector<SceneAnnotation> SceneAnnotations() const override;

  bool Project(double x, double y, double z, double* screen_x,
               double* screen_y) const override;
  quintptr PickNode(float x, float y) override;

  quintptr SceneRoot() const override;
  quintptr FindNodeByName(const QString& name) const override;
  void FocusNode(quintptr node) override;
  void ResetView() override;

  void SetAxesVisible(bool visible) override;
  void SetHighlightNode(quintptr node) override;
  void ClearHighlight() override;

  void MousePress(float x, float y, int qt_button) override;
  void MouseMove(float x, float y) override;
  void MouseRelease(float x, float y, int qt_button) override;
  void Wheel(float delta) override;

  void SetNodePickedCallback(
      std::function<void(quintptr)> callback) override;

 protected:
  void initializeGL() override;
  void resizeGL(int w, int h) override;
  void paintGL() override;

  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

 private:
  std::unique_ptr<OsgRenderer> renderer_;
};

#endif  // OSG_QT_VIEWER_SRC_OSG_VIEW_WIDGET_H_
