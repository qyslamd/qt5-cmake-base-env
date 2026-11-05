// 纯 OSG 渲染实现：本编译单元只包含 OSG 头与 QtCore，绝不包含
// Qt 的 GL 头（QOpenGLWidget / QQuickWindow / QOpenGL*），
// 避免 GL/gl.h 与 GLCORE OSG 的 glcorearb.h 冲突。
#include "osg_renderer.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <osg/Camera>
#include <osg/ComputeBoundsVisitor>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Group>
#include <osg/Material>
#include <osg/Math>
#include <osg/MatrixTransform>
#include <osg/Program>
#include <osg/Shader>
#include <osg/TexGen>
#include <osg/Texture2D>
#include <osg/Uniform>
#include <osg/Viewport>
#include <osgDB/ReadFile>
#include <osgGA/GUIEventHandler>
#include <osgGA/TrackballManipulator>
#include <osgText/Font>
#include <osgText/Text>
#include <osgUtil/IntersectionVisitor>
#include <osgUtil/LineSegmentIntersector>
#include <osgViewer/GraphicsWindow>
#include <osgViewer/Viewer>

#include "scene_assembler.h"

#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0 0x8CE0
#endif

namespace {

constexpr double kFovDegrees = 45.0;
constexpr int kClickMoveTolerancePx = 5;
constexpr int kAxesSizePx = 110;
constexpr int kAxesMarginPx = 16;
constexpr double kAxesViewExtent = 1.3;
constexpr double kAxesLength = 0.72;
constexpr double kAxesRadius = 0.035;
constexpr double kAxesConeLength = 0.20;
constexpr double kAxesConeRadius = 0.09;
constexpr double kAxesLabelOffset = 0.15;
constexpr float kAxesLabelSize = 0.32f;
constexpr const char* kInternalNodePrefix = "__";

// GL_TEXTURE_GEN_S/T/R/Q 与 GL_LIGHTING 均已从 Core Profile 删除
constexpr unsigned int kGlTextureGenS = 0x0C60;
constexpr unsigned int kGlTextureGenT = 0x0C61;
constexpr unsigned int kGlTextureGenR = 0x0C62;
constexpr unsigned int kGlTextureGenQ = 0x0C63;

unsigned int ToOsgButton(int qt_button) {
  switch (qt_button) {
    case 1:  // Qt::LeftButton
      return osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON;
    case 2:  // Qt::RightButton
      return osgGA::GUIEventAdapter::RIGHT_MOUSE_BUTTON;
    case 4:  // Qt::MiddleButton
      return osgGA::GUIEventAdapter::MIDDLE_MOUSE_BUTTON;
    default:
      return 0;
  }
}

void SetEyeAsOrbit(osgGA::TrackballManipulator* tb,
                   const osg::BoundingSphere& bs, double rx, double ry,
                   double rz) {
  const double r = bs.radius();
  const osg::Vec3d eye = bs.center() + osg::Vec3d(r * rx, r * ry, r * rz);
  tb->setHomePosition(eye, bs.center(), osg::Vec3d(0, 1, 0));
  tb->home(0.0);
}

// 自定义事件处理器：点击（非拖动）时拾取节点并回调
class PickHandler : public osgGA::GUIEventHandler {
 public:
  PickHandler(std::function<void(std::uintptr_t)> callback,
              osg::Camera* camera)
      : callback_(std::move(callback)), camera_(camera), mouse_down_(false) {}

  bool handle(const osgGA::GUIEventAdapter& ea,
              osgGA::GUIActionAdapter& aa) override {
    switch (ea.getEventType()) {
      case osgGA::GUIEventAdapter::PUSH:
        if (ea.getButton() == osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON) {
          mouse_down_x_ = ea.getX();
          mouse_down_y_ = ea.getY();
          mouse_down_ = true;
        }
        break;

      case osgGA::GUIEventAdapter::RELEASE:
        if (mouse_down_ &&
            ea.getButton() == osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON) {
          mouse_down_ = false;
          if (std::abs(ea.getX() - mouse_down_x_) < kClickMoveTolerancePx &&
              std::abs(ea.getY() - mouse_down_y_) < kClickMoveTolerancePx) {
            osg::Node* picked = Pick(ea.getX(), ea.getY());
            if (callback_) {
              callback_(reinterpret_cast<std::uintptr_t>(picked));
            }
          }
        }
        break;

      default:
        break;
    }
    return false;
  }

 private:
  osg::Node* Pick(float x, float y) {
    if (!camera_.valid()) {
      return nullptr;
    }
    osg::ref_ptr<osgUtil::LineSegmentIntersector> intersector =
        new osgUtil::LineSegmentIntersector(osgUtil::Intersector::WINDOW, x, y);
    osgUtil::IntersectionVisitor iv(intersector.get());
    camera_->accept(iv);

    if (!intersector->containsIntersections()) {
      return nullptr;
    }
    const osg::NodePath node_path =
        intersector->getFirstIntersection().nodePath;
    for (auto it = node_path.rbegin(); it != node_path.rend(); ++it) {
      if (!(*it)->getName().empty()) {
        return *it;
      }
    }
    return node_path.back();
  }

  std::function<void(std::uintptr_t)> callback_;
  osg::ref_ptr<osg::Camera> camera_;
  float mouse_down_x_ = 0;
  float mouse_down_y_ = 0;
  bool mouse_down_;
};

class NodeNameCollector : public osg::NodeVisitor {
 public:
  NodeNameCollector()
      : osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN) {}

  void apply(osg::Geode& geode) override {
    if (!geode.getName().empty()) {
      names_ << QString::fromStdString(geode.getName());
    }
  }
  QStringList names_;
};

class NodeFinder : public osg::NodeVisitor {
 public:
  explicit NodeFinder(const std::string& target)
      : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN),
        target_(target),
        found_(nullptr) {}

  void apply(osg::Node& node) override {
    if (found_ != nullptr) return;
    if (node.getName() == target_) {
      found_ = &node;
      return;
    }
    traverse(node);
  }
  std::string target_;
  osg::Node* found_;
};

// 遍历模型，把固定管线材质转换为自定义 Shader 的 uniform
class MaterialEnhancer : public osg::NodeVisitor {
 public:
  MaterialEnhancer() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {
    program_ = new osg::Program;

    const QDir shader_dir(
        QDir(QCoreApplication::applicationDirPath())
            .absoluteFilePath(QStringLiteral("shaders")));
    const QString vert_path =
        shader_dir.filePath(QStringLiteral("basic.vert"));
    const QString frag_path =
        shader_dir.filePath(QStringLiteral("basic.frag"));
    if (!QFile::exists(vert_path) || !QFile::exists(frag_path)) {
      qWarning() << "Shader files missing under" << shader_dir.path()
                 << "- models will render without custom lighting";
    }

    auto* vert = new osg::Shader(osg::Shader::VERTEX);
    vert->loadShaderSourceFromFile(vert_path.toStdString());
    auto* frag = new osg::Shader(osg::Shader::FRAGMENT);
    frag->loadShaderSourceFromFile(frag_path.toStdString());
    program_->addShader(vert);
    program_->addShader(frag);
  }

  void apply(osg::Geode& geode) override {
    for (unsigned int i = 0; i < geode.getNumDrawables(); ++i) {
      osg::Drawable* drawable = geode.getDrawable(i);
      if (drawable != nullptr) {
        Enhance(drawable);
      }
    }
    traverse(geode);
  }

 private:
  void Enhance(osg::Drawable* drawable) {
    osg::StateSet* ss = drawable->getOrCreateStateSet();
    if (ss->getAttribute(osg::StateAttribute::PROGRAM) == program_.get()) {
      return;
    }
    osg::Material* mat = dynamic_cast<osg::Material*>(
        ss->getAttribute(osg::StateAttribute::MATERIAL));

    osg::Vec4 diffuse(0.8f, 0.8f, 0.8f, 1.0f);
    osg::Vec4 specular(0.5f, 0.5f, 0.5f, 1.0f);
    float shininess = 32.0f;

    if (mat != nullptr) {
      diffuse = mat->getDiffuse(osg::Material::FRONT);
      specular = mat->getSpecular(osg::Material::FRONT) * 2.0f;
      shininess = mat->getShininess(osg::Material::FRONT);
      ss->removeAttribute(osg::StateAttribute::MATERIAL);
    }

    ss->addUniform(new osg::Uniform("materialDiffuse", diffuse));
    ss->addUniform(new osg::Uniform("materialSpecular", specular));
    ss->addUniform(new osg::Uniform("materialShininess", shininess));

    bool use_sphere_map = false;
    for (unsigned int unit = 0; unit < 4; ++unit) {
      osg::TexGen* texgen = dynamic_cast<osg::TexGen*>(
          ss->getTextureAttribute(unit, osg::StateAttribute::TEXGEN));
      if (texgen != nullptr && texgen->getMode() == osg::TexGen::SPHERE_MAP) {
        use_sphere_map = true;
        ss->removeTextureAttribute(unit, osg::StateAttribute::TEXGEN);
        ss->removeTextureMode(unit, kGlTextureGenS);
        ss->removeTextureMode(unit, kGlTextureGenT);
        ss->removeTextureMode(unit, kGlTextureGenR);
        ss->removeTextureMode(unit, kGlTextureGenQ);
        break;
      }
    }
    ss->addUniform(new osg::Uniform("useSphereMap", use_sphere_map));

    bool found_texture = false;
    for (unsigned int unit = 0; unit < 4; ++unit) {
      osg::Texture2D* tex = dynamic_cast<osg::Texture2D*>(
          ss->getTextureAttribute(unit, osg::StateAttribute::TEXTURE));
      if (tex != nullptr && tex->getImage() != nullptr) {
        ss->addUniform(new osg::Uniform("diffuseMap", static_cast<int>(unit)));
        ss->addUniform(new osg::Uniform("hasTexture", true));
        found_texture = true;
        break;
      }
    }
    if (!found_texture) {
      ss->addUniform(new osg::Uniform("hasTexture", false));
    }

    ss->setAttributeAndModes(program_.get(), osg::StateAttribute::ON);
  }

  osg::ref_ptr<osg::Program> program_;
};

void AppendAxisCylinder(osg::Vec3Array* vertices, osg::Vec4Array* colors,
                        const osg::Vec3& direction, const osg::Vec3& u,
                        const osg::Vec3& v, const osg::Vec4& color) {
  constexpr int kSegments = 12;
  const osg::Vec3 tip = direction * kAxesLength;
  for (int i = 0; i < kSegments; ++i) {
    const double a0 = 2.0 * osg::PI * i / kSegments;
    const double a1 = 2.0 * osg::PI * (i + 1) / kSegments;
    const osg::Vec3 p0 =
        u * (kAxesRadius * std::cos(a0)) + v * (kAxesRadius * std::sin(a0));
    const osg::Vec3 p1 =
        u * (kAxesRadius * std::cos(a1)) + v * (kAxesRadius * std::sin(a1));
    const osg::Vec3 q0 = p0 + tip;
    const osg::Vec3 q1 = p1 + tip;
    const osg::Vec3 triangles[] = {p0, p1, q1, p0, q1, q0, tip, q0, q1};
    for (const osg::Vec3& vertex : triangles) {
      vertices->push_back(vertex);
      colors->push_back(color);
    }
  }
}

void AppendAxisCone(osg::Vec3Array* vertices, osg::Vec4Array* colors,
                    const osg::Vec3& direction, const osg::Vec3& u,
                    const osg::Vec3& v, const osg::Vec4& color) {
  constexpr int kSegments = 12;
  const osg::Vec3 base = direction * kAxesLength;
  const osg::Vec3 apex = direction * (kAxesLength + kAxesConeLength);
  for (int i = 0; i < kSegments; ++i) {
    const double a0 = 2.0 * osg::PI * i / kSegments;
    const double a1 = 2.0 * osg::PI * (i + 1) / kSegments;
    const osg::Vec3 p0 = base + u * (kAxesConeRadius * std::cos(a0)) +
                         v * (kAxesConeRadius * std::sin(a0));
    const osg::Vec3 p1 = base + u * (kAxesConeRadius * std::cos(a1)) +
                         v * (kAxesConeRadius * std::sin(a1));
    const osg::Vec3 triangles[] = {apex, p1, p0, base, p0, p1};
    for (const osg::Vec3& vertex : triangles) {
      vertices->push_back(vertex);
      colors->push_back(color);
    }
  }
}

osg::Geode* CreateAxesGeode() {
  const osg::Vec4 kRed(0.95f, 0.25f, 0.25f, 1.0f);
  const osg::Vec4 kGreen(0.35f, 0.90f, 0.35f, 1.0f);
  const osg::Vec4 kBlue(0.35f, 0.55f, 0.95f, 1.0f);

  struct AxisSpec {
    osg::Vec3 direction;
    osg::Vec3 u;
    osg::Vec3 v;
    osg::Vec4 color;
    const char* label;
  };
  const AxisSpec axes[] = {
      {osg::Vec3(1.0, 0.0, 0.0), osg::Vec3(0.0, 1.0, 0.0),
       osg::Vec3(0.0, 0.0, 1.0), kRed, "X"},
      {osg::Vec3(0.0, 1.0, 0.0), osg::Vec3(1.0, 0.0, 0.0),
       osg::Vec3(0.0, 0.0, 1.0), kGreen, "Y"},
      {osg::Vec3(0.0, 0.0, 1.0), osg::Vec3(1.0, 0.0, 0.0),
       osg::Vec3(0.0, 1.0, 0.0), kBlue, "Z"},
  };

  osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
  osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array;
  for (const AxisSpec& axis : axes) {
    AppendAxisCylinder(vertices, colors, axis.direction, axis.u, axis.v,
                       axis.color);
    AppendAxisCone(vertices, colors, axis.direction, axis.u, axis.v,
                   axis.color);
  }

  osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
  geometry->setVertexArray(vertices);
  geometry->setColorArray(colors, osg::Array::BIND_PER_VERTEX);
  geometry->addPrimitiveSet(new osg::DrawArrays(
      osg::PrimitiveSet::TRIANGLES, 0, static_cast<int>(vertices->size())));

  osg::ref_ptr<osg::Geode> geode = new osg::Geode;
  geode->addDrawable(geometry);

  osg::ref_ptr<osgText::Font> font =
      osgText::readFontFile("C:/Windows/Fonts/arial.ttf");
  if (!font.valid()) {
    qWarning() << "Axes label font not found";
  }
  for (const AxisSpec& axis : axes) {
    const osg::Vec3 label_position =
        axis.direction * (kAxesLength + kAxesConeLength + kAxesLabelOffset);
    osg::ref_ptr<osgText::Text> label = new osgText::Text;
    label->setText(axis.label);
    label->setColor(axis.color);
    label->setCharacterSize(kAxesLabelSize);
    label->setPosition(label_position);
    label->setAlignment(osgText::Text::CENTER_CENTER);
    label->setAxisAlignment(osgText::Text::SCREEN);
    if (font.valid()) {
      label->setFont(font.get());
    }
    geode->addDrawable(label);
  }

  osg::StateSet* stateset = geode->getOrCreateStateSet();
  // Core Profile 无 GL_LIGHTING，不能 setMode（会报 GL_INVALID_ENUM）
  stateset->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
  return geode.release();
}

class AxesViewCallback : public osg::Camera::DrawCallback {
 public:
  AxesViewCallback(osg::Camera* main_camera, osg::Camera* axes_camera)
      : main_camera_(main_camera), axes_camera_(axes_camera) {}

  void operator()(osg::RenderInfo&) const override {
    osg::Matrix view = main_camera_->getViewMatrix();
    view.setTrans(0.0, 0.0, 0.0);
    axes_camera_->setViewMatrix(view);
  }

 private:
  osg::Camera* main_camera_;
  osg::Camera* axes_camera_;
};

}  // namespace

struct OsgRenderer::Impl {
  osg::ref_ptr<osgViewer::GraphicsWindowEmbedded> graphics_window;
  osg::ref_ptr<osgViewer::Viewer> viewer;
  osg::ref_ptr<osg::Group> root;
  osg::ref_ptr<osg::Camera> axes_camera;
  osg::ref_ptr<osg::Geode> highlight_geode;
  std::function<void(std::uintptr_t)> picked_callback;
  QVector<SceneAnnotation> annotations;

  int width = 1;
  int height = 1;
  double near_clip = 1.0;
  double far_clip = 1.0e5;
  bool realized = false;
  bool axes_visible = true;
};

OsgRenderer::OsgRenderer() : impl_(new Impl) {
  impl_->root = new osg::Group;

  CreateHighlightNode();
  impl_->highlight_geode->setNodeMask(0);
  impl_->root->addChild(impl_->highlight_geode);

  impl_->graphics_window = new osgViewer::GraphicsWindowEmbedded(0, 0, 1, 1);

  impl_->viewer = new osgViewer::Viewer;
  impl_->viewer->setThreadingModel(osgViewer::Viewer::SingleThreaded);
  impl_->viewer->setCameraManipulator(new osgGA::TrackballManipulator);
  impl_->viewer->setKeyEventSetsDone(0);
  impl_->viewer->setLightingMode(osgViewer::Viewer::HEADLIGHT);
  impl_->viewer->setSceneData(impl_->root.get());

  osg::Camera* camera = impl_->viewer->getCamera();
  camera->setGraphicsContext(impl_->graphics_window.get());
  camera->setViewport(new osg::Viewport(0, 0, 1, 1));
  camera->setClearColor(osg::Vec4(0.2f, 0.2f, 0.25f, 1.0f));
  camera->setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);
  camera->setProjectionMatrixAsPerspective(kFovDegrees, 1.0, impl_->near_clip,
                                           impl_->far_clip);
  // Qt 的 QOpenGLWidget 渲染到自己的 FBO，OSG 必须写其颜色附件
  camera->setDrawBuffer(GL_COLOR_ATTACHMENT0);
  camera->setReadBuffer(GL_COLOR_ATTACHMENT0);

  impl_->viewer->addEventHandler(new PickHandler(
      [this](std::uintptr_t node) {
        if (impl_->picked_callback) {
          impl_->picked_callback(node);
        }
      },
      camera));

  CreateAxesCamera();
  UpdateAxesViewport();
}

OsgRenderer::~OsgRenderer() {
  if (impl_->viewer.valid()) {
    impl_->viewer->setDone(true);
    impl_->viewer->setSceneData(nullptr);
    if (impl_->viewer->getCamera() != nullptr) {
      impl_->viewer->getCamera()->setGraphicsContext(nullptr);
    }
  }
}

void OsgRenderer::CreateHighlightNode() {
  impl_->highlight_geode = new osg::Geode;
  impl_->highlight_geode->setName(std::string(kInternalNodePrefix) +
                                  "highlight");

  osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array(8);
  vertices->setDataVariance(osg::Object::DYNAMIC);
  osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array;
  colors->push_back(osg::Vec4(1.0f, 0.85f, 0.1f, 1.0f));

  osg::ref_ptr<osg::DrawElementsUByte> edges =
      new osg::DrawElementsUByte(osg::PrimitiveSet::LINES);
  const unsigned char kEdges[24] = {0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6,
                                    6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7};
  for (unsigned char v : kEdges) {
    edges->push_back(v);
  }

  osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
  // 每帧改顶点：必须 DYNAMIC，否则 VAO 缓存导致高亮盒不更新
  geometry->setDataVariance(osg::Object::DYNAMIC);
  geometry->setVertexArray(vertices);
  geometry->setColorArray(colors, osg::Array::BIND_OVERALL);
  geometry->addPrimitiveSet(edges);
  geometry->setUseDisplayList(false);
  geometry->setUseVertexBufferObjects(false);

  impl_->highlight_geode->addDrawable(geometry);
}

void OsgRenderer::SetHighlightBox(double x0, double y0, double z0, double x1,
                                  double y1, double z1) {
  if (!impl_->highlight_geode.valid() ||
      impl_->highlight_geode->getNumDrawables() == 0) {
    return;
  }
  auto* geometry = impl_->highlight_geode->getDrawable(0)->asGeometry();
  if (geometry == nullptr) {
    return;
  }
  auto* vertices = static_cast<osg::Vec3Array*>(geometry->getVertexArray());
  if (vertices == nullptr || vertices->size() < 8) {
    return;
  }

  const float fx0 = static_cast<float>(x0);
  const float fy0 = static_cast<float>(y0);
  const float fz0 = static_cast<float>(z0);
  const float fx1 = static_cast<float>(x1);
  const float fy1 = static_cast<float>(y1);
  const float fz1 = static_cast<float>(z1);
  (*vertices)[0].set(fx0, fy0, fz0);
  (*vertices)[1].set(fx1, fy0, fz0);
  (*vertices)[2].set(fx1, fy1, fz0);
  (*vertices)[3].set(fx0, fy1, fz0);
  (*vertices)[4].set(fx0, fy0, fz1);
  (*vertices)[5].set(fx1, fy0, fz1);
  (*vertices)[6].set(fx1, fy1, fz1);
  (*vertices)[7].set(fx0, fy1, fz1);
  vertices->dirty();
  geometry->dirtyBound();
}

void OsgRenderer::SetHighlightNode(std::uintptr_t node_handle) {
  auto* node = reinterpret_cast<osg::Node*>(node_handle);
  if (!impl_->highlight_geode.valid()) {
    return;
  }
  if (node == nullptr) {
    ClearHighlight();
    return;
  }

  osg::ComputeBoundsVisitor visitor;
  // 隐藏的零件同样要能看得到包围盒
  visitor.setNodeMaskOverride(0xffffffffu);
  node->accept(visitor);
  osg::BoundingBox box = visitor.getBoundingBox();
  if (!box.valid()) {
    impl_->highlight_geode->setNodeMask(0);
    return;
  }

  // ComputeBoundsVisitor 已含 node 自身变换；祖先补偿从父节点开始
  osg::Matrix world = osg::Matrix::identity();
  bool has_transform = false;
  for (osg::Node* current =
           node->getNumParents() > 0 ? node->getParent(0) : nullptr;
       current != nullptr && current != impl_->root.get();
       current = current->getNumParents() > 0 ? current->getParent(0)
                                              : nullptr) {
    auto* transform = dynamic_cast<osg::Transform*>(current);
    if (transform != nullptr) {
      has_transform = true;
      osg::Matrix local;
      transform->computeLocalToWorldMatrix(local, nullptr);
      world.postMult(local);
    }
  }
  if (has_transform) {
    osg::BoundingBox world_box;
    for (int corner_index = 0; corner_index < 8; ++corner_index) {
      world_box.expandBy(box.corner(corner_index) * world);
    }
    box = world_box;
  }

  // expandBy(Vec3) 是“扩到包含该点”，等量外扩需对 min/max 分别 expand
  const float inflate = 0.01f * box.radius();
  const osg::Vec3 margin(inflate, inflate, inflate);
  box.expandBy(box._min - margin);
  box.expandBy(box._max + margin);
  SetHighlightBox(box.xMin(), box.yMin(), box.zMin(), box.xMax(), box.yMax(),
                  box.zMax());
  impl_->highlight_geode->setNodeMask(0xffffffffu);
}

void OsgRenderer::ClearHighlight() {
  if (impl_->highlight_geode.valid()) {
    impl_->highlight_geode->setNodeMask(0);
  }
}

bool OsgRenderer::LoadModel(const QString& path, QString* error) {
  if (!QFile::exists(path)) {
    if (error) *error = QStringLiteral("File not found: %1").arg(path);
    return false;
  }

  osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(path.toUtf8().constData());
  if (!node.valid()) {
    if (error) *error = QStringLiteral("OSG failed to read: %1").arg(path);
    return false;
  }

  osg::ref_ptr<osg::MatrixTransform> mt = new osg::MatrixTransform;
  mt->setMatrix(osg::Matrix::rotate(osg::PI, osg::Vec3(1, 0, 0)));
  // 命名后既便于场景树识别，也作为整体标注的标题
  mt->setName(QFileInfo(path).completeBaseName().toStdString());
  mt->addChild(node);

  ClearModelNodes();
  impl_->root->addChild(mt);
  RebuildAnnotations();

  MaterialEnhancer enhancer;
  mt->accept(enhancer);

  if (impl_->realized) {
    ResetView();
  }
  return true;
}

bool OsgRenderer::LoadScene(const QString& config_path, QString* error) {
  SceneAssembler assembler;
  if (!assembler.Load(config_path, error)) {
    return false;
  }

  ClearModelNodes();
  impl_->root->addChild(assembler.GetRoot());
  RebuildAnnotations();

  MaterialEnhancer enhancer;
  assembler.GetRoot()->accept(enhancer);

  if (impl_->realized) {
    ResetView();
  }
  return true;
}

void OsgRenderer::ClearModelNodes() {
  // 高亮盒是常驻子节点，先摘下再清空场景，最后挂回
  if (impl_->highlight_geode.valid() &&
      impl_->highlight_geode->getNumParents() > 0) {
    impl_->root->removeChild(impl_->highlight_geode);
  }
  if (impl_->root->getNumChildren() > 0) {
    impl_->root->removeChildren(0, impl_->root->getNumChildren());
  }
  if (impl_->highlight_geode.valid()) {
    impl_->highlight_geode->setNodeMask(0);
    impl_->root->addChild(impl_->highlight_geode);
  }
  impl_->annotations.clear();
}

QStringList OsgRenderer::GetNodeNames() const {
  NodeNameCollector collector;
  impl_->root->accept(collector);
  QStringList result;
  for (const QString& name : collector.names_) {
    if (!name.startsWith(QStringLiteral("__"))) {
      result << name;
    }
  }
  return result;
}

QVector<SceneAnnotation> OsgRenderer::SceneAnnotations() const {
  return impl_->annotations;
}

void OsgRenderer::RebuildAnnotations() {
  impl_->annotations.clear();
  for (unsigned int i = 0; i < impl_->root->getNumChildren(); ++i) {
    osg::Node* child = impl_->root->getChild(i);
    if (child == impl_->highlight_geode.get()) {
      continue;
    }
    if (child->getName().rfind(kInternalNodePrefix, 0) == 0) {
      continue;
    }

    // 装配场景的根 Group 未命名，其子节点才是零件；单一模型的
    // MatrixTransform 已在 LoadModel 中命名，直接作为一个整体
    osg::Group* group = child->asGroup();
    if (child->getName().empty() && group != nullptr &&
        group->getNumChildren() > 0) {
      for (unsigned int j = 0; j < group->getNumChildren(); ++j) {
        AddAnnotationForNode(group->getChild(j));
      }
    } else {
      AddAnnotationForNode(child);
    }
  }
}

void OsgRenderer::AddAnnotationForNode(osg::Node* node) {
  osg::ComputeBoundsVisitor visitor;
  visitor.setNodeMaskOverride(0xffffffffu);
  node->accept(visitor);
  const osg::BoundingBox box = visitor.getBoundingBox();
  if (!box.valid()) {
    return;
  }

  SceneAnnotation annotation;
  annotation.x = box.center().x();
  // 锚点放在零件包围盒顶面之上，留出随尺度缩放的间距
  annotation.y = box.yMax() + std::max(box.radius() * 0.08, 0.01);
  annotation.z = box.center().z();
  const std::string name = node->getName();
  annotation.title =
      name.empty()
          ? QStringLiteral("Part %1").arg(impl_->annotations.size() + 1)
          : QString::fromStdString(name);
  impl_->annotations.push_back(annotation);
}

void OsgRenderer::ResetView() {
  osg::BoundingSphere bs;
  for (unsigned i = 0; i < impl_->root->getNumChildren(); ++i) {
    osg::Node* child = impl_->root->getChild(i);
    if (child == impl_->highlight_geode.get()) {
      continue;
    }
    bs.expandBy(child->getBound());
  }
  if (!bs.valid()) return;

  auto* tb = dynamic_cast<osgGA::TrackballManipulator*>(
      impl_->viewer->getCameraManipulator());
  if (tb == nullptr) return;

  // 裁剪面随场景尺度缩放，保证 HUD 投影稳定且近距离模型不被裁掉
  impl_->near_clip = std::max(bs.radius() * 0.01, 0.001);
  impl_->far_clip = std::max(bs.radius() * 1000.0, 100.0);
  impl_->viewer->getCamera()->setProjectionMatrixAsPerspective(
      kFovDegrees,
      static_cast<double>(impl_->width) / static_cast<double>(impl_->height),
      impl_->near_clip, impl_->far_clip);

  SetEyeAsOrbit(tb, bs, 1.5, 1.5, 2.5);
}

void OsgRenderer::FocusNode(std::uintptr_t node_handle) {
  auto* node = reinterpret_cast<osg::Node*>(node_handle);
  if (node == nullptr || !impl_->viewer.valid()) {
    return;
  }
  const osg::BoundingSphere bs = node->getBound();
  if (!bs.valid()) return;

  auto* tb = dynamic_cast<osgGA::TrackballManipulator*>(
      impl_->viewer->getCameraManipulator());
  if (tb == nullptr) return;
  SetEyeAsOrbit(tb, bs, 2.0, 2.0, 3.0);
}

std::uintptr_t OsgRenderer::SceneRoot() const {
  return reinterpret_cast<std::uintptr_t>(impl_->root.get());
}

std::uintptr_t OsgRenderer::FindNodeByName(const QString& name) const {
  NodeFinder finder(name.toStdString());
  impl_->root->accept(finder);
  return reinterpret_cast<std::uintptr_t>(finder.found_);
}

void OsgRenderer::Render(int width_px, int height_px) {
  impl_->width = width_px > 0 ? width_px : 1;
  impl_->height = height_px > 0 ? height_px : 1;

  osg::Camera* camera = impl_->viewer->getCamera();
  const osg::Viewport* vp = camera->getViewport();
  if (vp == nullptr || static_cast<int>(vp->width()) != impl_->width ||
      static_cast<int>(vp->height()) != impl_->height) {
    impl_->graphics_window->resized(0, 0, impl_->width, impl_->height);
    impl_->graphics_window->getEventQueue()->windowResize(
        0, 0, impl_->width, impl_->height);
    camera->setViewport(new osg::Viewport(0, 0, impl_->width, impl_->height));
    camera->setProjectionMatrixAsPerspective(
        kFovDegrees,
        static_cast<double>(impl_->width) / static_cast<double>(impl_->height),
        impl_->near_clip, impl_->far_clip);
    UpdateAxesViewport();
  }

  if (!impl_->realized) {
    impl_->viewer->realize();
    impl_->realized = impl_->viewer->isRealized();
    if (impl_->realized) {
      ResetView();
    }
  }
  impl_->viewer->frame();


}

bool OsgRenderer::Project(double x, double y, double z, double* screen_x,
                          double* screen_y) const {
  const osg::Camera* camera = impl_->viewer->getCamera();
  const osg::Viewport* vp = camera != nullptr ? camera->getViewport() : nullptr;
  if (vp == nullptr) {
    return false;
  }

  const osg::Matrix mvp =
      camera->getViewMatrix() * camera->getProjectionMatrix();
  const osg::Vec4 clip = osg::Vec4(x, y, z, 1.0) * mvp;
  if (clip.w() <= 0.0) {
    return false;
  }

  const double ndc_x = clip.x() / clip.w();
  const double ndc_y = clip.y() / clip.w();
  *screen_x = vp->x() + (ndc_x + 1.0) * 0.5 * vp->width();
  // 转成左上原点（Qt 约定）
  *screen_y = impl_->height - (vp->y() + (ndc_y + 1.0) * 0.5 * vp->height());
  return true;
}

std::uintptr_t OsgRenderer::Pick(float x, float y) const {
  osg::ref_ptr<osgUtil::LineSegmentIntersector> intersector =
      new osgUtil::LineSegmentIntersector(osgUtil::Intersector::WINDOW, x,
                                          impl_->height - y);
  osgUtil::IntersectionVisitor iv(intersector.get());
  impl_->viewer->getCamera()->accept(iv);
  if (!intersector->containsIntersections()) {
    return 0;
  }
  const osg::NodePath node_path = intersector->getFirstIntersection().nodePath;
  for (auto it = node_path.rbegin(); it != node_path.rend(); ++it) {
    if (!(*it)->getName().empty()) {
      return reinterpret_cast<std::uintptr_t>(*it);
    }
  }
  return reinterpret_cast<std::uintptr_t>(node_path.back());
}

void OsgRenderer::SetNodePickedCallback(
    std::function<void(std::uintptr_t)> callback) {
  impl_->picked_callback = std::move(callback);
}

void OsgRenderer::SetAxesVisible(bool visible) {
  impl_->axes_visible = visible;
  if (impl_->axes_camera.valid()) {
    impl_->axes_camera->setNodeMask(visible ? 0xffffffffu : 0u);
  }
}

void OsgRenderer::CreateAxesCamera() {
  if (!impl_->viewer.valid() || impl_->viewer->getCamera() == nullptr) {
    return;
  }
  osg::ref_ptr<osg::Camera> axes_camera = new osg::Camera;
  axes_camera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
  axes_camera->setProjectionMatrixAsOrtho(-kAxesViewExtent, kAxesViewExtent,
                                          -kAxesViewExtent, kAxesViewExtent,
                                          -10.0, 10.0);
  axes_camera->setRenderOrder(osg::Camera::POST_RENDER);
  axes_camera->setClearMask(0);
  axes_camera->setComputeNearFarMode(
      osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);
  axes_camera->addChild(CreateAxesGeode());
  axes_camera->setNodeMask(impl_->axes_visible ? 0xffffffffu : 0u);
  axes_camera->setPreDrawCallback(new AxesViewCallback(
      impl_->viewer->getCamera(), axes_camera.get()));
  impl_->viewer->getCamera()->addChild(axes_camera);
  impl_->axes_camera = axes_camera;
}

void OsgRenderer::UpdateAxesViewport() {
  if (!impl_->axes_camera.valid()) {
    return;
  }
  const int size = kAxesSizePx;
  const int margin = kAxesMarginPx;
  impl_->axes_camera->setViewport(
      impl_->width - size - margin, margin, size, size);
}

// 鼠标坐标由调用方传入物理像素、左上原点；EventQueue 会按自身的
// Y 轴方向（默认向上）翻转一次，这里不能再手动翻转，否则双重翻转
void OsgRenderer::MousePress(float x, float y, int qt_button) {
  impl_->graphics_window->getEventQueue()->mouseButtonPress(
      x, y, ToOsgButton(qt_button));
}

void OsgRenderer::MouseMove(float x, float y) {
  impl_->graphics_window->getEventQueue()->mouseMotion(x, y);
}

void OsgRenderer::MouseRelease(float x, float y, int qt_button) {
  impl_->graphics_window->getEventQueue()->mouseButtonRelease(
      x, y, ToOsgButton(qt_button));
}

void OsgRenderer::Wheel(float delta) {
  impl_->graphics_window->getEventQueue()->mouseScroll(
      delta > 0.0f ? osgGA::GUIEventAdapter::SCROLL_UP
                   : osgGA::GUIEventAdapter::SCROLL_DOWN);
}
