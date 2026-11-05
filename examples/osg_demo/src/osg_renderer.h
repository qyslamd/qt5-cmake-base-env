#ifndef OSG_QT_VIEWER_SRC_OSG_RENDERER_H_
#define OSG_QT_VIEWER_SRC_OSG_RENDERER_H_

#include <QString>
#include <QStringList>
#include <QVector>
#include <cstdint>
#include <functional>
#include <memory>

#include "osg_view.h"

namespace osg {
class Node;
}

// 纯 OSG 渲染实现（独立编译单元，不包含 Qt GL 头）

class OsgRenderer {
 public:
  OsgRenderer();
  ~OsgRenderer();

  bool LoadModel(const QString& path, QString* error);
  bool LoadScene(const QString& path, QString* error);
  QStringList GetNodeNames() const;
  QVector<SceneAnnotation> SceneAnnotations() const;

  // 在 Qt 当前上下文与 FBO 下渲染；尺寸为物理像素
  void Render(int width_px, int height_px);

  // 世界坐标 -> 屏幕坐标（物理像素，左上原点）
  bool Project(double x, double y, double z, double* screen_x,
               double* screen_y) const;
  // 屏幕坐标（物理像素，左上原点）拾取，返回 osg::Node* 句柄
  std::uintptr_t Pick(float x, float y) const;

  std::uintptr_t SceneRoot() const;
  std::uintptr_t FindNodeByName(const QString& name) const;
  void FocusNode(std::uintptr_t node);
  void ResetView();

  void SetAxesVisible(bool visible);
  void SetHighlightNode(std::uintptr_t node);
  void ClearHighlight();

  void MousePress(float x, float y, int qt_button);
  void MouseMove(float x, float y);
  void MouseRelease(float x, float y, int qt_button);
  void Wheel(float delta);

  void SetNodePickedCallback(std::function<void(std::uintptr_t)> callback);

 private:
  void CreateHighlightNode();
  void SetHighlightBox(double x0, double y0, double z0, double x1, double y1,
                       double z1);
  void ClearModelNodes();
  void RebuildAnnotations();
  void AddAnnotationForNode(osg::Node* node);
  void CreateAxesCamera();
  void UpdateAxesViewport();

  struct Impl;
  std::unique_ptr<Impl> impl_;
};

#endif  // OSG_QT_VIEWER_SRC_OSG_RENDERER_H_
