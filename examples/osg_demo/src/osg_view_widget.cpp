// Qt-GL 编译单元：包含 QOpenGLWidget（进而包含 GL/gl.h），
// 不包含任何 OSG 头；OSG 全部在 osg_renderer.cpp 中。
#include "osg_view_widget.h"

#include <QDebug>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QWheelEvent>

#include "osg_renderer.h"

OsgViewWidget::OsgViewWidget(QWidget* parent) : QOpenGLWidget(parent) {
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
  renderer_.reset(new OsgRenderer);
}

OsgViewWidget::~OsgViewWidget() = default;

void OsgViewWidget::initializeGL() {
  qInfo() << "GL context ready:" << context()->format().majorVersion() << "."
          << context()->format().minorVersion()
          << (context()->format().profile() == QSurfaceFormat::CoreProfile
                  ? "Core"
                  : "Compatibility");
}

void OsgViewWidget::resizeGL(int w, int h) {
  Q_UNUSED(w);
  Q_UNUSED(h);
}

void OsgViewWidget::paintGL() {
  const qreal dpr = devicePixelRatioF();
  const int pixel_w = qMax(1, qRound(width() * dpr));
  const int pixel_h = qMax(1, qRound(height() * dpr));
  renderer_->Render(pixel_w, pixel_h);
}

bool OsgViewWidget::LoadModel(const QString& path, QString* error) {
  const bool ok = renderer_->LoadModel(path, error);
  update();
  return ok;
}

bool OsgViewWidget::LoadScene(const QString& path, QString* error) {
  const bool ok = renderer_->LoadScene(path, error);
  update();
  return ok;
}

QStringList OsgViewWidget::GetNodeNames() const {
  return renderer_->GetNodeNames();
}

QVector<SceneAnnotation> OsgViewWidget::SceneAnnotations() const {
  QVector<SceneAnnotation> result;
  for (const auto& a : renderer_->SceneAnnotations()) {
    result.push_back(a);
  }
  return result;
}

bool OsgViewWidget::Project(double x, double y, double z, double* screen_x,
                            double* screen_y) const {
  return renderer_->Project(x, y, z, screen_x, screen_y);
}

quintptr OsgViewWidget::PickNode(float x, float y) {
  return renderer_->Pick(x, y);
}

quintptr OsgViewWidget::SceneRoot() const { return renderer_->SceneRoot(); }

quintptr OsgViewWidget::FindNodeByName(const QString& name) const {
  return renderer_->FindNodeByName(name);
}

void OsgViewWidget::FocusNode(quintptr node) { renderer_->FocusNode(node); }

void OsgViewWidget::ResetView() { renderer_->ResetView(); }

void OsgViewWidget::SetAxesVisible(bool visible) {
  renderer_->SetAxesVisible(visible);
  update();
}

void OsgViewWidget::SetHighlightNode(quintptr node) {
  renderer_->SetHighlightNode(node);
  update();
}

void OsgViewWidget::ClearHighlight() {
  renderer_->ClearHighlight();
  update();
}

void OsgViewWidget::MousePress(float x, float y, int qt_button) {
  renderer_->MousePress(x, y, qt_button);
}

void OsgViewWidget::MouseMove(float x, float y) { renderer_->MouseMove(x, y); }

void OsgViewWidget::MouseRelease(float x, float y, int qt_button) {
  renderer_->MouseRelease(x, y, qt_button);
}

void OsgViewWidget::Wheel(float delta) { renderer_->Wheel(delta); }

void OsgViewWidget::SetNodePickedCallback(
    std::function<void(quintptr)> callback) {
  renderer_->SetNodePickedCallback(std::move(callback));
}

void OsgViewWidget::mousePressEvent(QMouseEvent* event) {
  const qreal dpr = devicePixelRatioF();
  renderer_->MousePress(static_cast<float>(event->localPos().x() * dpr),
                        static_cast<float>(event->localPos().y() * dpr),
                        static_cast<int>(event->button()));
  update();
}

void OsgViewWidget::mouseMoveEvent(QMouseEvent* event) {
  const qreal dpr = devicePixelRatioF();
  renderer_->MouseMove(static_cast<float>(event->localPos().x() * dpr),
                       static_cast<float>(event->localPos().y() * dpr));
  update();
}

void OsgViewWidget::mouseReleaseEvent(QMouseEvent* event) {
  const qreal dpr = devicePixelRatioF();
  renderer_->MouseRelease(static_cast<float>(event->localPos().x() * dpr),
                          static_cast<float>(event->localPos().y() * dpr),
                          static_cast<int>(event->button()));
  update();
}

void OsgViewWidget::wheelEvent(QWheelEvent* event) {
  renderer_->Wheel(static_cast<float>(event->angleDelta().y()));
  update();
}

std::unique_ptr<IOsgView> CreateOsgView() {
  return std::unique_ptr<IOsgView>(new OsgViewWidget);
}
