#include "tour_controller.h"

#include <QDebug>

#include "tour_overlay.h"
#include "tour_style.h"

TourController::TourController(QObject* parent)
    : QObject(parent),
      overlay_(new TourOverlay(nullptr)),
      is_running_(false),
      cur_topic_index_(0),
      cur_step_index_(0) {
  // 库自己把样式并入应用样式表（顺带完成静态库资源的初始化），宿主不必管。
  // 控制器是引导的入口，放这里能保证任何引导界面出来之前样式已就位
  TourStyle::ensureApplied();

  connect(overlay_, &TourOverlay::nextClicked, this,
          &TourController::onNextClicked);
  connect(overlay_, &TourOverlay::prevClicked, this,
          &TourController::onPrevClicked);
  connect(overlay_, &TourOverlay::skipClicked, this,
          &TourController::onSkipClicked);
}

TourController::~TourController() {
  stop();
  // 演示层由控制器持有，销毁时回收（stop 后无父级，避免泄漏）。
  delete overlay_;
  overlay_ = nullptr;
  catalog_.clear();
}

void TourController::setViewport(QWidget* viewport) { viewport_ = viewport; }

TourTopic* TourController::addTopic(TourTopic* topic) {
  return catalog_.addTopic(topic);
}

void TourController::clearTopics() { catalog_.clear(); }

void TourController::execute() {
  is_running_ = true;

  // 先 show 再摆位：setHost 内部要 raise()，对已映射的窗口才有效
  overlay_->show();
  overlay_->setHost(viewport_);

  // 主题级 enterFunc 与模式推送**收口在这里**：直启与跨主题前进/回退两条
  // 路径都得走同一套，不然挂在 enterFunc 上的状态编排会被静默跳过
  TourTopic* topic = catalog_.topics().at(cur_topic_index_);
  applyTopicOptions(topic);
  if (topic->enterFunc()) {
    topic->enterFunc()();
  }

  updateStep();

  emit started();
}

void TourController::applyTopicOptions(TourTopic* const topic) {
  if (topic == nullptr) {
    return;
  }
  // 实操式主题忽略自动倒计时（等用户动手优先于自动翻页），但**别把请求本身
  // 抹掉**：用 requested_auto_mode_ 现算，否则走过一个实操主题之后，后面即使
  // 全是讲解主题也不会再自动翻了
  auto_mode_ = requested_auto_mode_ && !topic->interactive();

  overlay_->setInteractive(topic->interactive());
  // 主题驱动时一定不是被动模式：那是"外部驱动"那套（不带按钮、停倒计时）的
  // 状态，上一轮留下的不能跟过来，否则主题播放只剩遮罩和气泡
  overlay_->setPassiveMode(false);
  // 演示层的自动模式要跟着一起推：起不起倒计时看的是**演示层自己的**那份
  overlay_->setIsAutoMode(auto_mode_);
}

void TourController::startAll(bool auto_mode) {
  if (catalog_.topics().isEmpty()) {
    return;
  }

  if (viewport_ == nullptr) {
    qWarning() << "Viewport must be set before start()";
    return;
  }

  // 占位主题（stepCount()==0）仅供首页卡片展示，不进入播放。
  const int first_playable = nextPlayableTopicIndex(0);
  if (first_playable < 0) {
    qWarning() << "No playable topic to start";
    return;
  }

  run_all_ = true;
  requested_auto_mode_ = auto_mode;
  cur_topic_index_ = first_playable;
  cur_step_index_ = 0;
  execute();
}

void TourController::startOne(TourTopic* const topic, bool auto_mode) {
  if (topic == nullptr) {
    return;
  }

  if (topic->stepCount() == 0) {
    qWarning() << "Placeholder topic can not be played";
    return;
  }

  if (!catalog_.topics().contains(topic)) {
    return;
  }

  if (viewport_ == nullptr) {
    qWarning() << "Viewport must be set before start()";
    return;
  }

  run_all_ = false;
  requested_auto_mode_ = auto_mode;
  cur_topic_index_ = catalog_.topics().indexOf(topic);
  cur_step_index_ = 0;
  execute();
}

void TourController::updateOverlayGeometry() {
  // 几何由演示层自己按宿主算（它不设 Qt 父子，坐标是屏幕坐标系）
  overlay_->setHost(viewport_);
}

void TourController::stop() {
  is_running_ = false;

  // 本来就没有 Qt 父子关系（见 TourOverlay::setHost），隐藏即可
  overlay_->hide();
}

void TourController::onNextClicked() {
  if (cur_topic_index_ >= catalog_.topics().size()) {
    stop();
    emit finished();
    return;
  }

  TourTopic* cur_topic = catalog_.topics().at(cur_topic_index_);
  if (cur_step_index_ >= cur_topic->stepCount()) {
    stop();
    emit finished();
    return;
  }

  TourStep* cur_step = cur_topic->steps().at(cur_step_index_);
  if (cur_step->exitFunc()) {
    cur_step->exitFunc()();
  }

  // ------------------------
  // 还在一个 topic 中
  if (cur_step_index_ < cur_topic->stepCount() - 1) {
    cur_step_index_++;
    updateStep();
    return;
  }

  // ------------------------
  // 一个主题结束
  // 先执行 退出函数
  if (cur_topic->exitFunc()) {
    cur_topic->exitFunc()();
  }

  // 单主题运行，或已无下一个可播放主题（跳过占位主题）时结束。
  if (!run_all_) {
    stop();
    emit finished();
    return;
  }

  const int next_index = nextPlayableTopicIndex(cur_topic_index_ + 1);
  if (next_index < 0) {
    stop();
    emit finished();
    return;
  }

  cur_topic_index_ = next_index;
  cur_step_index_ = 0;
  TourTopic* next_topic = catalog_.topics().at(cur_topic_index_);
  applyTopicOptions(next_topic);
  if (next_topic->enterFunc()) {
    next_topic->enterFunc()();
  }
  updateOverlayGeometry();
  emit topicChanged(next_topic->name());
  updateStep();
}

void TourController::onPrevClicked() {
  // 回退 step
  if (cur_step_index_ > 0) {
    cur_step_index_--;
    updateStep();
    return;
  }

  if (!run_all_) {
    return;
  }

  // 回退 topic（跳过占位主题）
  const int prev_index = previousPlayableTopicIndex(cur_topic_index_ - 1);
  if (prev_index < 0) {
    return;
  }

  TourTopic* cur_topic = catalog_.topics().at(cur_topic_index_);
  if (cur_topic->exitFunc()) {
    cur_topic->exitFunc()();
  }

  cur_topic_index_ = prev_index;
  TourTopic* prev_topic = catalog_.topics().at(cur_topic_index_);
  cur_step_index_ = prev_topic->stepCount() - 1;
  applyTopicOptions(prev_topic);
  if (prev_topic->enterFunc()) {
    prev_topic->enterFunc()();
  }
  emit topicChanged(prev_topic->name());
  updateStep();
}

void TourController::onSkipClicked() {
  stop();
  emit finishedByUser();
}

void TourController::updateStep() {
  if (catalog_.topics().isEmpty()) {
    return;
  }
  if (cur_topic_index_ < 0 || cur_topic_index_ >= catalog_.topics().size()) {
    return;
  }

  TourTopic* cur_topic = catalog_.topics().at(cur_topic_index_);
  if (cur_step_index_ < 0 || cur_step_index_ >= cur_topic->stepCount()) {
    return;
  }
  TourStep* cur_step = cur_topic->steps().at(cur_step_index_);

  if (cur_step->enterFunc()) {
    cur_step->enterFunc()();
  }

  refreshStepOverlay();

  int global_index = 0;
  for (int i = 0; i < cur_topic_index_; ++i) {
    global_index += catalog_.topics().at(i)->stepCount();
  }
  global_index += cur_step_index_;

  emit stepChanged(global_index, catalog_.totalSteps());
}

void TourController::refreshCurrentStepHighlight() { refreshStepOverlay(); }

// 刷新当前步骤的演示层（高亮/气泡/按钮态）。从 updateStep 抽出，
// 便于 enter 回调中目标控件异步就绪后重新定位。
void TourController::refreshStepOverlay() {
  if (!is_running_ || catalog_.topics().isEmpty()) {
    return;
  }
  if (cur_topic_index_ < 0 || cur_topic_index_ >= catalog_.topics().size()) {
    return;
  }

  TourTopic* cur_topic = catalog_.topics().at(cur_topic_index_);
  if (cur_step_index_ < 0 || cur_step_index_ >= cur_topic->stepCount()) {
    return;
  }
  TourStep* cur_step = cur_topic->steps().at(cur_step_index_);

  // 两个按钮能不能按，口径必须与 onNextClicked / onPrevClicked 的落点一致：
  // 它们跳的是**下一个 / 上一个可播放的主题**（首页会给 stepCount()==0 的占位
  // 主题建卡，topics 里允许有它），所以判"还有没有下一站"只能用这两个谓词
  bool can_prev = true;
  bool can_next = true;
  if (run_all_) {
    const bool has_prev_topic =
        previousPlayableTopicIndex(cur_topic_index_ - 1) >= 0;
    if (cur_step_index_ == 0 && !has_prev_topic) {
      can_prev = false;
    }

    const bool has_next_topic =
        nextPlayableTopicIndex(cur_topic_index_ + 1) >= 0;
    if (cur_step_index_ == cur_topic->stepCount() - 1 && !has_next_topic) {
      can_next = false;
    }
  } else {
    if (cur_step_index_ == 0) {
      can_prev = false;
    }

    if (cur_step_index_ == cur_topic->stepCount() - 1) {
      can_next = false;
    }
  }
  overlay_->updateUi(cur_topic, cur_step, can_prev, can_next);
}

// 自 from_index 起向后找第一个可播放（stepCount()>0）的主题下标，无则 -1。
int TourController::nextPlayableTopicIndex(int from_index) const {
  for (int i = from_index; i < catalog_.topics().size(); ++i) {
    if (catalog_.topics().at(i)->stepCount() > 0) {
      return i;
    }
  }
  return -1;
}

// 自 from_index 起向前找第一个可播放（stepCount()>0）的主题下标，无则 -1。
int TourController::previousPlayableTopicIndex(int from_index) const {
  for (int i = from_index; i >= 0; --i) {
    if (catalog_.topics().at(i)->stepCount() > 0) {
      return i;
    }
  }
  return -1;
}
