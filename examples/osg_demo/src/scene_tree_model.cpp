#include "scene_tree_model.h"

#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QString>

#include <osg/ComputeBoundsVisitor>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Group>
#include <osg/PrimitiveSet>
#include <osg/Transform>

namespace {

// 内部节点命名前缀（如高亮包围盒），不出现在场景树里
constexpr const char* kInternalPrefix = "__";

enum class NodeType { kGroup, kTransform, kGeode, kDrawable, kOther };

NodeType ClassifyNode(const osg::Node* node) {
  if (dynamic_cast<const osg::Geometry*>(node) != nullptr) {
    return NodeType::kDrawable;
  }
  if (dynamic_cast<const osg::Geode*>(node) != nullptr) {
    return NodeType::kGeode;
  }
  if (dynamic_cast<const osg::Transform*>(node) != nullptr) {
    return NodeType::kTransform;
  }
  if (dynamic_cast<const osg::Group*>(node) != nullptr) {
    return NodeType::kGroup;
  }
  return NodeType::kOther;
}

QString TypeName(NodeType type) {
  switch (type) {
    case NodeType::kGroup:
      return QStringLiteral("Group");
    case NodeType::kTransform:
      return QStringLiteral("Transform");
    case NodeType::kGeode:
      return QStringLiteral("Geode");
    case NodeType::kDrawable:
      return QStringLiteral("Drawable");
    default:
      return QStringLiteral("Node");
  }
}

QString DisplayNodeName(const osg::Node* node) {
  if (!node->getName().empty()) {
    return QString::fromStdString(node->getName());
  }
  return QStringLiteral("<%1>").arg(
      TypeName(ClassifyNode(node)));
}

// GLCORE 下仍可用的 16x16 单色类型图标（代码绘制，不引入资源文件）
QIcon TypeIcon(NodeType type) {
  static QHash<int, QIcon> cache;
  const int key = static_cast<int>(type);
  const auto it = cache.constFind(key);
  if (it != cache.constEnd()) {
    return it.value();
  }

  QPixmap pixmap(16, 16);
  pixmap.fill(Qt::transparent);
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);

  QPen pen(QColor(70, 78, 90));
  pen.setWidthF(1.2);
  painter.setPen(pen);
  QBrush brush;
  switch (type) {
    case NodeType::kGroup:  // 蓝色方块
      brush = QColor(88, 148, 236);
      painter.setBrush(brush);
      painter.drawRect(3, 3, 10, 10);
      break;
    case NodeType::kTransform:  // 橙色箭头
      brush = QColor(245, 158, 66);
      painter.setBrush(brush);
      {
        static const QPointF kArrow[4] = {{3, 3}, {13, 8}, {3, 13}, {6, 8}};
        painter.drawPolygon(kArrow, 4);
      }
      break;
    case NodeType::kGeode:  // 绿色圆
      brush = QColor(76, 175, 130);
      painter.setBrush(brush);
      painter.drawEllipse(3, 3, 10, 10);
      break;
    case NodeType::kDrawable:  // 灰色菱形
      brush = QColor(150, 156, 168);
      painter.setBrush(brush);
      {
        static const QPointF kDiamond[4] = {{8, 2}, {14, 8}, {8, 14}, {2, 8}};
        painter.drawPolygon(kDiamond, 4);
      }
      break;
    default:  // 空心圆
      painter.drawEllipse(4, 4, 8, 8);
      break;
  }
  painter.end();

  const QIcon icon(pixmap);
  cache.insert(key, icon);
  return icon;
}

unsigned long long CountTriangles(const osg::Drawable& drawable) {
  const osg::Geometry* geometry = drawable.asGeometry();
  if (geometry == nullptr) {
    return 0;
  }

  unsigned long long total = 0;
  for (unsigned int i = 0; i < geometry->getNumPrimitiveSets(); ++i) {
    const osg::PrimitiveSet* primitive_set = geometry->getPrimitiveSet(i);
    const unsigned int n = primitive_set->getNumIndices();
    switch (primitive_set->getMode()) {
      case osg::PrimitiveSet::TRIANGLES:
        total += n / 3;
        break;
      case osg::PrimitiveSet::TRIANGLE_STRIP:
      case osg::PrimitiveSet::TRIANGLE_FAN:
        total += (n >= 3) ? (n - 2) : 0;
        break;
      case osg::PrimitiveSet::QUADS:
        total += (n / 4) * 2;
        break;
      case osg::PrimitiveSet::QUAD_STRIP:
        total += (n >= 4) ? ((n - 2) / 2) : 0;
        break;
      default:
        break;  // 点 / 线不计入三角形
    }
  }
  return total;
}

// 统计子树的 Drawable 数与三角形数（三角面按 PrimitiveSet 模式折算）；
// 包围盒单独用 osg::ComputeBoundsVisitor 累计变换。
// 统计应与可见性无关，须强制遍历被 nodeMask 隐藏的子树
class SubtreeStatsVisitor : public osg::NodeVisitor {
 public:
  SubtreeStatsVisitor()
      : osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN) {
    setNodeMaskOverride(0xffffffffu);
  }

  void apply(osg::Drawable& drawable) override {
    ++drawable_count_;
    triangle_count_ += CountTriangles(drawable);
    traverse(drawable);
  }

  int drawable_count_ = 0;
  unsigned long long triangle_count_ = 0;
};

QString FormatBox(const osg::BoundingBox& box) {
  if (!box.valid()) {
    return QStringLiteral("-");
  }
  return QStringLiteral("%1 x %2 x %3")
      .arg(box.xMax() - box.xMin(), 0, 'f', 3)
      .arg(box.yMax() - box.yMin(), 0, 'f', 3)
      .arg(box.zMax() - box.zMin(), 0, 'f', 3);
}

bool IsInternal(const osg::Node* node) {
  return node->getName().rfind(kInternalPrefix, 0) == 0;
}

}  // namespace

struct SceneTreeModel::SceneTreeItem {
  osg::ref_ptr<osg::Node> node;
  SceneTreeItem* parent = nullptr;
  int row = 0;
  bool fetched = false;
  bool is_scene_root = false;
  bool stats_valid = false;
  int drawable_count = 0;
  unsigned long long triangle_count = 0;
  osg::BoundingBox box;
  std::vector<std::unique_ptr<SceneTreeItem>> children;
};

SceneTreeModel::SceneTreeModel(QObject* parent)
    : QAbstractItemModel(parent) {}

SceneTreeModel::~SceneTreeModel() = default;

void SceneTreeModel::SetSceneNode(osg::Node* scene) {
  beginResetModel();
  node_to_item_.clear();

  root_item_ = std::make_unique<SceneTreeItem>();
  root_item_->fetched = true;
  if (scene != nullptr) {
    auto scene_item = std::make_unique<SceneTreeItem>();
    scene_item->node = scene;
    scene_item->parent = root_item_.get();
    scene_item->is_scene_root = true;
    node_to_item_[scene] = scene_item.get();
    root_item_->children.emplace_back(std::move(scene_item));
  }
  endResetModel();
}

osg::Node* SceneTreeModel::SceneNode() const {
  if (root_item_ == nullptr || root_item_->children.empty()) {
    return nullptr;
  }
  return root_item_->children.front()->node.get();
}

SceneTreeModel::SceneTreeItem* SceneTreeModel::ItemForIndex(
    const QModelIndex& index) const {
  if (!index.isValid()) {
    return nullptr;
  }
  return static_cast<SceneTreeItem*>(index.internalPointer());
}

QModelIndex SceneTreeModel::ItemIndex(const SceneTreeItem* item) const {
  if (item == nullptr || item->parent == nullptr) {
    return {};
  }
  return createIndex(item->row, 0, const_cast<SceneTreeItem*>(item));
}

osg::Node* SceneTreeModel::NodeForIndex(const QModelIndex& index) const {
  SceneTreeItem* item = ItemForIndex(index);
  return item != nullptr ? item->node.get() : nullptr;
}

void SceneTreeModel::FetchChildren(SceneTreeItem* item) {
  if (item == nullptr || item->fetched) {
    return;
  }
  item->fetched = true;
  if (item->node == nullptr) {
    return;
  }

  osg::Group* group = item->node->asGroup();
  if (group == nullptr) {
    return;
  }

  // 内部节点（__ 前缀）不建行；先收集再插入，保证 begin/end 行数一致
  std::vector<osg::Node*> kids;
  for (unsigned int i = 0; i < group->getNumChildren(); ++i) {
    osg::Node* child = group->getChild(i);
    if (!IsInternal(child)) {
      kids.push_back(child);
    }
  }
  if (kids.empty()) {
    return;
  }

  const QModelIndex parent_index = ItemIndex(item);
  beginInsertRows(parent_index, 0, static_cast<int>(kids.size()) - 1);
  for (osg::Node* child : kids) {
    auto child_item = std::make_unique<SceneTreeItem>();
    child_item->node = child;
    child_item->parent = item;
    child_item->row = static_cast<int>(item->children.size());
    node_to_item_[child] = child_item.get();
    item->children.emplace_back(std::move(child_item));
  }
  endInsertRows();
}

void SceneTreeModel::EnsureFetched(SceneTreeItem* item) {
  if (item != nullptr && !item->fetched) {
    FetchChildren(item);
  }
}

QModelIndex SceneTreeModel::IndexForNode(osg::Node* node) {
  if (node == nullptr || root_item_ == nullptr ||
      root_item_->children.empty()) {
    return {};
  }

  SceneTreeItem* scene_item = root_item_->children.front().get();
  if (node == scene_item->node.get()) {
    return ItemIndex(scene_item);
  }

  // 从目标节点向上收集到场景根的链，再自上而下补建行
  std::vector<osg::Node*> chain;
  for (osg::Node* current = node; current != nullptr;
       current = current->getNumParents() > 0 ? current->getParent(0)
                                              : nullptr) {
    chain.push_back(current);
    if (current == scene_item->node.get()) {
      break;
    }
  }
  if (chain.empty() || chain.back() != scene_item->node.get()) {
    return {};
  }

  SceneTreeItem* item = scene_item;
  for (auto it = chain.rbegin() + 1; it != chain.rend(); ++it) {
    EnsureFetched(item);
    SceneTreeItem* next = nullptr;
    for (auto& child : item->children) {
      if (child->node.get() == *it) {
        next = child.get();
        break;
      }
    }
    if (next == nullptr) {
      return {};
    }
    item = next;
  }
  return ItemIndex(item);
}

void SceneTreeModel::FetchAll() {
  if (root_item_ == nullptr) {
    return;
  }
  std::vector<SceneTreeItem*> stack;
  stack.push_back(root_item_.get());
  while (!stack.empty()) {
    SceneTreeItem* item = stack.back();
    stack.pop_back();
    EnsureFetched(item);
    for (auto& child : item->children) {
      stack.push_back(child.get());
    }
  }
}

QModelIndex SceneTreeModel::index(int row, int column,
                                  const QModelIndex& parent) const {
  SceneTreeItem* item =
      parent.isValid() ? ItemForIndex(parent) : root_item_.get();
  if (item == nullptr || row < 0 || column < 0 ||
      column >= kColumnCount || row >= static_cast<int>(item->children.size())) {
    return {};
  }
  return createIndex(row, column, item->children[static_cast<size_t>(row)].get());
}

QModelIndex SceneTreeModel::parent(const QModelIndex& child) const {
  SceneTreeItem* item = ItemForIndex(child);
  if (item == nullptr || item->parent == nullptr ||
      item->parent->parent == nullptr) {
    return {};
  }
  return createIndex(item->parent->row, 0, item->parent);
}

int SceneTreeModel::rowCount(const QModelIndex& parent) const {
  if (root_item_ == nullptr) {
    return 0;
  }
  SceneTreeItem* item =
      parent.isValid() ? ItemForIndex(parent) : root_item_.get();
  if (item == nullptr) {
    return 0;
  }
  // 未展开的行由 hasChildren/canFetchMore 告知可展开，rowCount 保持 0
  return static_cast<int>(item->children.size());
}

int SceneTreeModel::columnCount(const QModelIndex& /*parent*/) const {
  return kColumnCount;
}

void SceneTreeModel::ComputeStats(const SceneTreeItem* item) const {
  if (item->stats_valid) {
    return;
  }
  SubtreeStatsVisitor visitor;
  item->node->accept(visitor);
  osg::ComputeBoundsVisitor bounds_visitor;
  // 隐藏的子树同样计入统计
  bounds_visitor.setNodeMaskOverride(0xffffffffu);
  item->node->accept(bounds_visitor);
  auto* mutable_item = const_cast<SceneTreeItem*>(item);
  mutable_item->drawable_count = visitor.drawable_count_;
  mutable_item->triangle_count = visitor.triangle_count_;
  mutable_item->box = bounds_visitor.getBoundingBox();
  mutable_item->stats_valid = true;
}

Qt::CheckState SceneTreeModel::VisibilityState(
    const SceneTreeItem* item) const {
  if (item->node->getNodeMask() == 0) {
    return Qt::Unchecked;
  }
  // 自身可见但被某个祖先隐藏：半选
  for (const SceneTreeItem* parent_item = item->parent;
       parent_item != nullptr && parent_item->node != nullptr;
       parent_item = parent_item->parent) {
    if (parent_item->node->getNodeMask() == 0) {
      return Qt::PartiallyChecked;
    }
  }
  return Qt::Checked;
}

void SceneTreeModel::EmitCheckStateDeep(const QModelIndex& index) {
  if (!index.isValid()) {
    return;
  }
  emit dataChanged(index.sibling(index.row(), kColumnName),
                   index.sibling(index.row(), kColumnName));
  SceneTreeItem* item = ItemForIndex(index);
  if (item != nullptr) {
    for (auto& child : item->children) {
      EmitCheckStateDeep(ItemIndex(child.get()));
    }
  }
}

QVariant SceneTreeModel::data(const QModelIndex& index, int role) const {
  SceneTreeItem* item = ItemForIndex(index);
  if (item == nullptr || item->node == nullptr) {
    return {};
  }
  osg::Node* node = item->node.get();

  switch (role) {
    case Qt::DisplayRole: {
      switch (index.column()) {
        case kColumnName:
          return item->is_scene_root ? QStringLiteral("Scene")
                                     : DisplayNodeName(node);
        case kColumnType:
          return TypeName(ClassifyNode(node));
        case kColumnDrawables:
        case kColumnTriangles:
        case kColumnSize:
          ComputeStats(item);
          if (index.column() == kColumnDrawables) {
            return item->drawable_count;
          }
          if (index.column() == kColumnTriangles) {
            return static_cast<qulonglong>(item->triangle_count);
          }
          return FormatBox(item->box);
        default:
          return {};
      }
    }
    case Qt::CheckStateRole:
      if (index.column() == kColumnName) {
        return VisibilityState(item);
      }
      return {};
    case Qt::DecorationRole:
      if (index.column() == kColumnName) {
        return TypeIcon(ClassifyNode(node));
      }
      return {};
    case Qt::ToolTipRole:
      if (index.column() == kColumnName) {
        return node->getName().empty()
                   ? QStringLiteral("(unnamed)")
                   : QString::fromStdString(node->getName());
      }
      return {};
    case kNodePtrRole:
      return qulonglong(node);
    default:
      return {};
  }
}

bool SceneTreeModel::setData(const QModelIndex& index, const QVariant& value,
                             int role) {
  if (role != Qt::CheckStateRole || !index.isValid()) {
    return false;
  }
  SceneTreeItem* item = ItemForIndex(index);
  if (item == nullptr || item->node == nullptr) {
    return false;
  }

  const bool visible = value.toInt() == static_cast<int>(Qt::Checked);
  item->node->setNodeMask(visible ? 0xffffffffu : 0u);
  // 子节点的半选状态随祖先变化，一并刷新
  EmitCheckStateDeep(index);
  return true;
}

Qt::ItemFlags SceneTreeModel::flags(const QModelIndex& index) const {
  Qt::ItemFlags base =
      Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
  if (index.column() == kColumnName) {
    return base;
  }
  return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant SceneTreeModel::headerData(int section, Qt::Orientation orientation,
                                    int role) const {
  if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
    return {};
  }
  switch (section) {
    case kColumnName:
      return QStringLiteral("Name");
    case kColumnType:
      return QStringLiteral("Type");
    case kColumnDrawables:
      return QStringLiteral("Drawables");
    case kColumnTriangles:
      return QStringLiteral("Triangles");
    case kColumnSize:
      return QStringLiteral("Size (m)");
    default:
      return {};
  }
}

bool SceneTreeModel::hasChildren(const QModelIndex& parent) const {
  SceneTreeItem* item =
      parent.isValid() ? ItemForIndex(parent) : root_item_.get();
  if (item == nullptr) {
    return false;
  }
  if (item->fetched) {
    return !item->children.empty();
  }
  if (item->node == nullptr) {
    return false;
  }
  const osg::Group* group = item->node->asGroup();
  return group != nullptr && group->getNumChildren() > 0;
}

bool SceneTreeModel::canFetchMore(const QModelIndex& parent) const {
  SceneTreeItem* item =
      parent.isValid() ? ItemForIndex(parent) : root_item_.get();
  return item != nullptr && !item->fetched && hasChildren(parent);
}

void SceneTreeModel::fetchMore(const QModelIndex& parent) {
  FetchChildren(parent.isValid() ? ItemForIndex(parent) : root_item_.get());
}

SceneTreeFilterProxy::SceneTreeFilterProxy(QObject* parent)
    : QSortFilterProxyModel(parent) {}

bool SceneTreeFilterProxy::filterAcceptsRow(
    int source_row, const QModelIndex& source_parent) const {
  const QModelIndex index =
      sourceModel()->index(source_row, SceneTreeModel::kColumnName,
                           source_parent);
  if (index.data(Qt::DisplayRole)
          .toString()
          .contains(filterRegularExpression())) {
    return true;
  }
  const int child_count = sourceModel()->rowCount(index);
  for (int i = 0; i < child_count; ++i) {
    if (filterAcceptsRow(i, index)) {
      return true;
    }
  }
  return false;
}
