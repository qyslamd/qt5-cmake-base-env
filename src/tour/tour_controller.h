#ifndef FILE_TOUR_CONTROLLER_H_
#define FILE_TOUR_CONTROLLER_H_

#include <QObject>
#include <QWidget>

#include "tour_catalog.h"

class TourOverlay;

// 引导流程控制器：持有全部 Flow/Step 配置，驱动演示层（遮罩 + 高亮 + 气泡）
// 前进/后退/跳过，并把状态通过信号暴露给宿主。
class TourController : public QObject {
  Q_OBJECT

 public:
  explicit TourController(QObject* parent = nullptr);
  ~TourController() override;

  void setViewport(QWidget* viewport);
  TourTopic* addTopic(TourTopic* topic);
  void clearFlows();
  void startAll(bool autoMode = false);
  void startOne(TourTopic* const topic, bool autoMode = false);
  void stop();

  bool isRunning() const { return isRunning_; }
  int currentStepIndex() const { return curStepIndex_; }
  int totalSteps() const { return catalog_.totalSteps(); }

  // 首页构建主题卡片用：当前已注册的 Flow 列表。
  const QList<TourTopic*>& topics() const { return catalog_.topics(); }

  // 演示层由控制器持有。桥接层（TourBridge）要拿它响应外部高亮请求，
  // 所以这里开个口子，而不是让调用方自己 new 一个
  TourOverlay* overlay() const { return overlay_; }

  // 目标控件在 enter 回调中异步就绪（如编辑器 QtConcurrent 打开）后，
  // 重新定位当前步骤的高亮。
  void refreshCurrentStepHighlight();

 signals:
  void started();
  void finished();
  void finishedByUser();
  void stepChanged(int index, int total);
  void topicChanged(const QString& flowName);

 private slots:
  void onNextClicked();
  void onPrevClicked();
  void onSkipClicked();

 private:
  void execute();
  void updateOverlayGeometry();
  void updateStep();
  void refreshStepOverlay();
  // 把当前主题的演示选项套给演示层：实操式 → 遮罩穿透 + 气泡独立；
  // 实操主题忽略自动倒计时（等用户操作优先于自动翻页）
  void ApplyTopicOptions(TourTopic* const topic);
  // 自 fromIndex 起向后/向前找第一个可播放（stepCount()>0）的 Flow 下标，无则
  // -1。
  int nextPlayableTopicIndex(int fromIndex) const;
  int previousPlayableTopicIndex(int fromIndex) const;

 private:
  TourCatalog catalog_;
  QWidget* viewport_ = nullptr;
  TourOverlay* overlay_ = nullptr;

  bool run_all_ = false;  // 指示是否运行所有的 Flow
  // 用户请求的自动模式（startAll/startOne 的参数）。实操式主题会被强制降为
  // 手动，但"请求"要留着——否则走过一个实操主题后，后续讲解主题再也回不到
  // 自动。有效模式 auto_mode_ 由它和当前主题的 interactive 现算
  bool requested_auto_mode_ = false;
  bool auto_mode_ = false;  // 当前主题的有效模式
  bool isRunning_ = false;
  int curTopicIndex_ = 0;
  int curStepIndex_ = 0;
};
#endif  // FILE_TOUR_CONTROLLER_H_
