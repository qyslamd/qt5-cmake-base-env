#ifndef FILE_TOUR_CATALOG_H_
#define FILE_TOUR_CATALOG_H_

#include <functional>
#include <variant>

#include <QList>
#include <QPixmap>
#include <QRect>
#include <QString>
#include <QWidget>

// —— 一步"指向什么"：三个类型，各自把语义与坐标口径写进类型 ——
//
// 原先是一个裸的 `std::variant<QWidget*, QRect, QPixmap>`：三种东西的坐标口径
// 完全不同，而"传进来的矩形必须是**全局**坐标"只能写在注释里、靠调用方记得
// ——注释里那个"必须假定"本身就是警告。现在类型自己回答：
struct WidgetTarget {
  QWidget* widget = nullptr;  // 遮罩负责把它映射到自身坐标系
};
// 这个矩形**已经是全局屏幕坐标**（原点在屏幕左上）。类型名即约定
struct GlobalRect {
  QRect rect;
};
// 一步不挖洞、只在遮罩中央贴一张合成图（图示步）。**它不是靶**：与"框住某处"
// 互斥，所以 `withExtraTarget` 收不了它——靶那个 variant 里没有这一支
struct SplashImage {
  QPixmap pixmap;
};

// 一步的锚点内容：三种互斥
using StepContent = std::variant<WidgetTarget, GlobalRect, SplashImage>;
// 靶：能"框住"的两种。**额外靶只收这个** —— SplashImage 进不来是类型保证，
// 不再是注释里的约定（原先那个分支要 qWarning 一句"额外靶不支持图片"）
using Target = std::variant<WidgetTarget, GlobalRect>;

// 单个引导步骤：锚点内容 + 标题/描述 + 进入/退出回调。
// 纯类，无 Q_OBJECT（不依赖元对象系统）。
class TourStep {
 public:
  using Func = std::function<void()>;

 private:
  QString title_;
  QString description_;
  StepContent content_;
  // 除锚点外**还要一起框住**的靶（《一步多靶设计》）。空 = 单靶，与从前一致。
  // 锚点是气泡与三角箭头的依托，所以只能有一个；其余的进这里，没有主次之分
  QList<Target> extra_targets_;
  std::function<void()> onEnter;
  std::function<void()> onExit;

 public:
  TourStep(const StepContent& content)
      : content_(content),
        title_("未知"),
        description_("未知"),
        onEnter(nullptr),
        onExit(nullptr) {}

  // getter 一律返回 const 引用。StepContent 是 variant（可能装着 QPixmap）、Func 是
  // std::function（按值拷可能堆分配），而调用点普遍长成
  // `if (x->enterFunc()) { x->enterFunc()(); }`——按值返回时，一次调用就拷两遍
  inline const QString& title() const { return title_; }
  inline void setTitle(const QString& title) { title_ = title; }

  inline const QString& description() const { return description_; }
  inline void setDescription(const QString& desc) { description_ = desc; }

  // 锚点：框住它，气泡与三角箭头也挂在它旁边。"一个步骤讲一件事"曾经靠它就够
  // 了，现在它多一层含义——多靶时它是**主靶**，其余的见 extraTargets()
  inline const StepContent& content() const { return content_; }

  // 允许在 enter 回调中延迟定位锚点
  inline void setContent(const StepContent& content) { content_ = content; }

  inline const QList<Target>& extraTargets() const { return extra_targets_; }
  // 链式，可调多次。**只收靶**（见 Target 的注释）——图片要走锚点那条路
  TourStep* withExtraTarget(const Target& target) {
    extra_targets_.append(target);
    return this;
  }

  inline const Func& enterFunc() const { return onEnter; }
  inline void setEnterFunc(const Func& func) { onEnter = func; }

  inline const Func& exitFunc() const { return onExit; }
  inline void setExitFunc(const Func& func) { onExit = func; }

  TourStep* withTitle(const QString& title) {
    title_ = title;
    return this;
  }
  TourStep* withDescription(const QString& desc) {
    description_ = desc;
    return this;
  }
  TourStep* withEnterFunc(const Func& func) {
    onEnter = func;
    return this;
  }
  TourStep* withExitFunc(const Func& func) {
    onExit = func;
    return this;
  }
};

// 一条引导主题（Flow）：图标 + 名称/描述 + 一组 Step + 主题级进入/退出回调。
// 纯类，无 Q_OBJECT。
class TourTopic {
 private:
  QPixmap icon_;
  QString name_;
  QString description_;
  std::function<void()> onEnter = nullptr;
  std::function<void()> onExit = nullptr;
  QList<TourStep*> steps_;
  // 实操式主题：遮罩对鼠标穿透，用户边看边动手（自发自收演示用）。
  // 默认关闭 = 传统讲解式（目标不响应点击，点遮罩空白退出）
  bool interactive_ = false;

 public:
  TourTopic(const QString& name, const QString& desc)
      : name_(name), description_(desc) {}

  TourTopic(const QPixmap& icon, const QString& name, const QString& desc)
      : icon_(icon), name_(name), description_(desc) {}

  TourTopic(const QPixmap& icon) : icon_(icon) {}

  TourTopic() = default;
  ~TourTopic();

  void addStep(TourStep* step);
  void clear();

  inline const QString& name() const { return name_; }
  inline void setName(const QString& name) { name_ = name; }

  inline const QString& description() const { return description_; }
  inline void setDescription(const QString& desc) { description_ = desc; }

  // QPixmap 隐式共享，按值返回不至于深拷，但仍是一次原子引用计数；返回引用是零成本
  inline const QPixmap& icon() const { return icon_; }
  inline void setIcon(const QPixmap& icon) { icon_ = icon; }

  inline const QList<TourStep*>& steps() const { return steps_; }
  inline int stepCount() const { return steps_.count(); }

  inline bool interactive() const { return interactive_; }
  inline void setInteractive(bool on) { interactive_ = on; }
  TourTopic* withInteractive(bool on) {
    interactive_ = on;
    return this;
  }

  inline const std::function<void()>& enterFunc() const { return onEnter; }
  inline void setEnterFunc(const std::function<void()>& func) {
    onEnter = func;
  }

  inline const std::function<void()>& exitFunc() const { return onExit; }
  inline void setExitFunc(const std::function<void()>& func) { onExit = func; }

  TourTopic* withStep(TourStep* step) {
    addStep(step);
    return this;
  }
};

// 引导配置：持有全部 Flow。纯类，无 Q_OBJECT。
class TourCatalog {
 public:
  TourCatalog();
  ~TourCatalog();

  inline const QList<TourTopic*>& topics() const { return topics_; }

  TourTopic* addTopic(TourTopic* topic);
  void clear();
  int totalSteps() const;

 private:
  QList<TourTopic*> topics_;
};

Q_DECLARE_METATYPE(TourTopic*)
#endif  // FILE_TOUR_CATALOG_H_
