#ifndef FILE_TOUR_CATALOG_H_
#define FILE_TOUR_CATALOG_H_

#include <functional>
#include <variant>

#include <QList>
#include <QPixmap>
#include <QRect>
#include <QString>
#include <QWidget>

// —— 一步"指向什么"：三个类型，各自把语义与坐标口径写进类型名 ——
struct WidgetTarget {
  QWidget* widget = nullptr;  // 遮罩负责把它映射到自身坐标系
};
// **已经是全局屏幕坐标**（原点在屏幕左上）；类型名即约定
struct GlobalRect {
  QRect rect;
};
// 图示步：不挖洞，只在遮罩中央贴一张合成图。它不是靶，见 Target
struct SplashImage {
  QPixmap pixmap;
};

// 一步的锚点内容：三种互斥
using StepContent = std::variant<WidgetTarget, GlobalRect, SplashImage>;
// 靶：能"框住"的两种。额外靶只收这个——图片进不来是类型保证，不是注释约定
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
  // 除锚点外**还要一起框住**的靶。锚点是气泡与箭头的依托、只能有一个，
  // 其余的进这里，没有主次之分
  QList<Target> extra_targets_;
  std::function<void()> on_enter_;
  std::function<void()> on_exit_;

 public:
  TourStep(const StepContent& content)
      : content_(content),
        title_("未知"),
        description_("未知"),
        on_enter_(nullptr),
        on_exit_(nullptr) {}

  // getter 一律返回 const 引用：StepContent 是 variant（可能装 QPixmap）、
  // Func 是 std::function，按值返回时调用点一次要拷两遍
  inline const QString& title() const { return title_; }
  inline void setTitle(const QString& title) { title_ = title; }

  inline const QString& description() const { return description_; }
  inline void setDescription(const QString& desc) { description_ = desc; }

  // 锚点：框住它，气泡与三角箭头也挂在它旁边。多靶时它是**主靶**，
  // 其余的见 extraTargets()
  inline const StepContent& content() const { return content_; }

  // 允许在 enter 回调中延迟定位锚点
  inline void setContent(const StepContent& content) { content_ = content; }

  inline const QList<Target>& extraTargets() const { return extra_targets_; }
  // 链式，可调多次。**只收靶**（见 Target 的注释）——图片要走锚点那条路
  TourStep* withExtraTarget(const Target& target) {
    extra_targets_.append(target);
    return this;
  }

  inline const Func& enterFunc() const { return on_enter_; }
  inline void setEnterFunc(const Func& func) { on_enter_ = func; }

  inline const Func& exitFunc() const { return on_exit_; }
  inline void setExitFunc(const Func& func) { on_exit_ = func; }

  TourStep* withTitle(const QString& title) {
    title_ = title;
    return this;
  }
  TourStep* withDescription(const QString& desc) {
    description_ = desc;
    return this;
  }
  TourStep* withEnterFunc(const Func& func) {
    on_enter_ = func;
    return this;
  }
  TourStep* withExitFunc(const Func& func) {
    on_exit_ = func;
    return this;
  }
};

// 一条引导主题（Topic）：图标 + 名称/描述 + 一组 Step + 主题级进入/退出回调。
// 纯类，无 Q_OBJECT。
class TourTopic {
 private:
  QPixmap icon_;
  QString name_;
  QString description_;
  std::function<void()> on_enter_ = nullptr;
  std::function<void()> on_exit_ = nullptr;
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

  inline const std::function<void()>& enterFunc() const { return on_enter_; }
  inline void setEnterFunc(const std::function<void()>& func) {
    on_enter_ = func;
  }

  inline const std::function<void()>& exitFunc() const { return on_exit_; }
  inline void setExitFunc(const std::function<void()>& func) { on_exit_ = func; }

  TourTopic* withStep(TourStep* step) {
    addStep(step);
    return this;
  }
};

// 引导目录：持有全部主题。纯类，无 Q_OBJECT。
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
