#ifndef OSG_QT_VIEWER_SRC_SCENE_TREE_MODEL_H_
#define OSG_QT_VIEWER_SRC_SCENE_TREE_MODEL_H_

#include <QAbstractItemModel>
#include <QHash>
#include <QSortFilterProxyModel>
#include <memory>
#include <vector>

#include <osg/Node>
#include <osg/ref_ptr>

// 把 osg 场景图包装成 QAbstractItemModel：
// - 子节点懒展开（fetchMore），场景含数千 Geode 时不卡加载
// - 每行显示类型图标 / Drawable 数 / 三角形数 / 包围盒尺寸（首次取数据时
//   计算并缓存）
// - 名称列复选框直通 setNodeMask；父节点隐藏时子节点显示半选状态
class SceneTreeModel : public QAbstractItemModel {
  Q_OBJECT

 public:
  enum Column {
    kColumnName = 0,
    kColumnType,
    kColumnDrawables,
    kColumnTriangles,
    kColumnSize,
    kColumnCount
  };

  // data() 通过该角色返回节点指针（qulonglong），供 3D 联动取用
  static constexpr int kNodePtrRole = Qt::UserRole + 1;

  explicit SceneTreeModel(QObject* parent = nullptr);
  ~SceneTreeModel() override;

  // 替换整棵树的数据源（场景重载时调用）
  void SetSceneNode(osg::Node* scene);
  osg::Node* SceneNode() const;

  osg::Node* NodeForIndex(const QModelIndex& index) const;
  // 返回该节点的行索引；沿途父级未展开时按需补建行
  QModelIndex IndexForNode(osg::Node* node);
  // 递归展开行数据（名称过滤前调用，保证全树可匹配）
  void FetchAll();

  QModelIndex index(int row, int column,
                    const QModelIndex& parent) const override;
  QModelIndex parent(const QModelIndex& child) const override;
  int rowCount(const QModelIndex& parent) const override;
  int columnCount(const QModelIndex& parent) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  bool setData(const QModelIndex& index, const QVariant& value,
               int role) override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role) const override;
  bool hasChildren(const QModelIndex& parent) const override;
  bool canFetchMore(const QModelIndex& parent) const override;
  void fetchMore(const QModelIndex& parent) override;

 private:
  struct SceneTreeItem;

  SceneTreeItem* ItemForIndex(const QModelIndex& index) const;
  QModelIndex ItemIndex(const SceneTreeItem* item) const;
  void FetchChildren(SceneTreeItem* item);
  void EnsureFetched(SceneTreeItem* item);
  void ComputeStats(const SceneTreeItem* item) const;
  Qt::CheckState VisibilityState(const SceneTreeItem* item) const;
  void EmitCheckStateDeep(const QModelIndex& index);

  // 不可见的树根；唯一子项是场景根节点本身（显示为 "Scene" 行）
  std::unique_ptr<SceneTreeItem> root_item_;
  QHash<osg::Node*, SceneTreeItem*> node_to_item_;
};

// 名称过滤：自身或任一后代命中即保留该行
class SceneTreeFilterProxy : public QSortFilterProxyModel {
  Q_OBJECT

 public:
  explicit SceneTreeFilterProxy(QObject* parent = nullptr);

 protected:
  bool filterAcceptsRow(int source_row,
                        const QModelIndex& source_parent) const override;
};

#endif  // OSG_QT_VIEWER_SRC_SCENE_TREE_MODEL_H_
