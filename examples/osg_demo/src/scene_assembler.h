#ifndef OSG_QT_VIEWER_SRC_SCENE_ASSEMBLER_H_
#define OSG_QT_VIEWER_SRC_SCENE_ASSEMBLER_H_

#include <QString>
#include <QStringList>
#include <osg/Group>
#include <osg/Vec3d>
#include <osg/ref_ptr>

class SceneAssembler {
 public:
  SceneAssembler() = default;
  ~SceneAssembler() = default;

  bool Load(const QString& config_path, QString* error);

  osg::Group* GetRoot() const { return root_.get(); }
  const QString& GetSceneName() const { return scene_name_; }
  QStringList GetComponentNames() const;

 private:
  struct PartSpec {
    QString file_path;
    osg::Vec3d position{0.0, 0.0, 0.0};
    osg::Vec3d rotation_deg{0.0, 0.0, 0.0};
  };

  bool LoadPart(const PartSpec& spec, QString* error);

  osg::ref_ptr<osg::Group> root_;
  QString scene_name_;
};

#endif  // OSG_QT_VIEWER_SRC_SCENE_ASSEMBLER_H_
