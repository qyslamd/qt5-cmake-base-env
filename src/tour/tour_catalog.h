#ifndef FILE_TOUR_CONFIG_H_
#define FILE_TOUR_CONFIG_H_

#include <functional>
#include <variant>

#include <QList>
#include <QPixmap>
#include <QRect>
#include <QString>
#include <QWidget>

// 单个引导步骤：目标元素（控件/矩形/图片）+ 标题/描述 + 进入/退出回调。
// 纯类，无 Q_OBJECT（不依赖元对象系统）。
class TourStep {
 public:
  using Target = std::variant<QWidget*, QRect, QPixmap>;
  using Func = std::function<void()>;

 private:
  QString title_;
  QString description_;
  Target target_;
  std::function<void()> onEnter;
  std::function<void()> onExit;

 public:
  TourStep(const Target& target)
      : target_(target),
        title_("未知"),
        description_("未知"),
        onEnter(nullptr),
        onExit(nullptr) {}

  // getter 一律返回 const 引用。Target 是 variant（可能装着 QPixmap）、Func 是
  // std::function（按值拷可能堆分配），而调用点普遍长成
  // `if (x->enterFunc()) { x->enterFunc()(); }`——按值返回时，一次调用就拷两遍
  inline const QString& title() const { return title_; }
  inline void setTitle(const QString& title) { title_ = title; }

  inline const QString& description() const { return description_; }
  inline void setDescription(const QString& desc) { description_ = desc; }

  inline const Target& target() const { return target_; }

  // 允许在 enter 回调中延迟定位目标
  inline void setTarget(const Target& target) { target_ = target; }

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
class GuideTopic {
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
  GuideTopic(const QString& name, const QString& desc)
      : name_(name), description_(desc) {}

  GuideTopic(const QPixmap& icon, const QString& name, const QString& desc)
      : icon_(icon), name_(name), description_(desc) {}

  GuideTopic(const QPixmap& icon) : icon_(icon) {}

  GuideTopic() = default;
  ~GuideTopic();

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
  GuideTopic* withInteractive(bool on) {
    interactive_ = on;
    return this;
  }

  inline const std::function<void()>& enterFunc() const { return onEnter; }
  inline void setEnterFunc(const std::function<void()>& func) {
    onEnter = func;
  }

  inline const std::function<void()>& exitFunc() const { return onExit; }
  inline void setExitFunc(const std::function<void()>& func) { onExit = func; }

  GuideTopic* withStep(TourStep* step) {
    addStep(step);
    return this;
  }
};

// 引导配置：持有全部 Flow。纯类，无 Q_OBJECT。
class TourCatalog {
 public:
  TourCatalog();
  ~TourCatalog();

  inline const QList<GuideTopic*>& topics() const { return topics_; }

  GuideTopic* addTopic(GuideTopic* topic);
  void clear();
  int totalSteps() const;

 private:
  QList<GuideTopic*> topics_;
};

Q_DECLARE_METATYPE(GuideTopic*)
#endif  // FILE_TOUR_CONFIG_H_
