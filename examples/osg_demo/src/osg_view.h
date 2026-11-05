#ifndef OSG_QT_VIEWER_SRC_OSG_VIEW_H_
#define OSG_QT_VIEWER_SRC_OSG_VIEW_H_

#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>
#include <functional>
#include <memory>

class QWidget;

// 场景标注：零件锚点（世界坐标）与标题
struct SceneAnnotation {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
  QString title;
};

// Qt 与 OSG 之间的唯一接口：只使用原生 / QtCore 类型与不透明句柄
// （quintptr 即 osg::Node*），保证调用方（QWidget 外壳）不必包含
// 任何 Qt GL 头，OSG 实现也不必包含 Qt GL 头。
class IOsgView {
 public:
  virtual ~IOsgView() = default;

  virtual QWidget* AsWidget() = 0;

  virtual bool LoadModel(const QString& path, QString* error) = 0;
  virtual bool LoadScene(const QString& path, QString* error) = 0;
  virtual QStringList GetNodeNames() const = 0;
  // 每个零件的标注锚点（装配场景按零件，单一模型整体一个）
  virtual QVector<SceneAnnotation> SceneAnnotations() const = 0;

  // 世界坐标 -> 视图内屏幕坐标（物理像素，左上原点）
  virtual bool Project(double x, double y, double z, double* screen_x,
                       double* screen_y) const = 0;
  // 屏幕坐标（物理像素，左上原点）拾取，返回节点句柄；未命中返回 0
  virtual quintptr PickNode(float x, float y) = 0;

  virtual quintptr SceneRoot() const = 0;
  virtual quintptr FindNodeByName(const QString& name) const = 0;
  virtual void FocusNode(quintptr node) = 0;
  virtual void ResetView() = 0;

  virtual void SetAxesVisible(bool visible) = 0;
  virtual void SetHighlightNode(quintptr node) = 0;
  virtual void ClearHighlight() = 0;

  virtual void MousePress(float x, float y, int qt_button) = 0;
  virtual void MouseMove(float x, float y) = 0;
  virtual void MouseRelease(float x, float y, int qt_button) = 0;
  virtual void Wheel(float delta) = 0;

  virtual void SetNodePickedCallback(
      std::function<void(quintptr)> callback) = 0;
};

// 工厂：实现在 Qt-GL 编译单元中，返回可放入 Qt 布局的视图
std::unique_ptr<IOsgView> CreateOsgView();

#endif  // OSG_QT_VIEWER_SRC_OSG_VIEW_H_
