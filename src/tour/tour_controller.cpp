#include "tour_controller.h"

#include <QDebug>

#include "tour_overlay.h"
#include "tour_style.h"

 

TourController::TourController(QObject* parent)
    : QObject(parent),
      overlay_(new TourOverlay(nullptr)),
      isRunning_(false),
      curTopicIndex_(0),
      curStepIndex_(0) {
  // 库自己把样式并入应用样式表（顺带完成静态库资源的初始化），宿主不必管。
  // 控制器是引导的入口，放这里能保证任何引导界面出来之前样式已就位
  TourStyle::EnsureApplied();

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

void TourController::setViewport(QWidget* viewport) {
  viewport_ = viewport;
}

TourTopic* TourController::addTopic(TourTopic* topic) {
  return catalog_.addTopic(topic);
}

void TourController::clearFlows() {
  catalog_.clear();
}

void TourController::execute() {
  isRunning_ = true;

  // 先 show 再摆位：SetHost 内部要 raise()，对已映射的窗口才有效
  overlay_->show();
  overlay_->SetHost(viewport_);

  // 主题级 enterFunc 在 startOne/startAll 直启路径此前从不执行（只有
  // 跨主题前进/回退才调），上层挂在这上的状态编排会被静默跳过——统一
  // 收口到 execute()，两条路径行为一致
  TourTopic* topic = catalog_.topics().at(curTopicIndex_);
  // 模式推送收口在 ApplyTopicOptions 里，避免像以前那样只有本路径记得推、
  // 跨主题那两条漏掉
  ApplyTopicOptions(topic);
  if (topic->enterFunc()) {
    topic->enterFunc()();
  }

  updateStep();

  emit started();
}

void TourController::ApplyTopicOptions(TourTopic* const topic) {
  if (topic == nullptr) {
    return;
  }
  // 实操式主题忽略自动倒计时：等用户动手优先于自动翻页。用"用户请求的自动
  // 模式"来算，而不是把 auto_mode_ 永久置 false——否则走过一个实操主题之后，
  // 后面即使全是讲解主题也不会再自动翻了
  auto_mode_ = requested_auto_mode_ && !topic->interactive();

  overlay_->setInteractive(topic->interactive());
  // 主题驱动时一定不是被动模式。被动模式属于"外部驱动"那套（不带按钮、停倒计时），
  // 上一轮留给它的状态不能跟到这一轮来——否则外部驱动跑过一次之后，再走主题播放
  // 就只剩遮罩和气泡，三个按钮全是隐的
  overlay_->setPassiveMode(false);
  // 演示层的自动模式必须跟着一起推：refreshStepOverlay() 起不起倒计时看
  // 的是**演示层自己的** auto_mode_。跨主题前进/回退这两条路径以前只调了本
  // 函数、没推模式，于是两边分叉——讲解主题不再自动翻页，实操主题反而被自动
  // 翻页（用户正在动手，5 秒后页面自己跳走）
  overlay_->setIsAutoMode(auto_mode_);
}

void TourController::startAll(bool autoMode) {
  if (catalog_.topics().isEmpty()) {
    return;
  }

  if (viewport_ == nullptr) {
    qWarning() << "Viewport must be set before start()";
    return;
  }

  // 占位主题（stepCount()==0）仅供首页卡片展示，不进入播放。
  const int firstPlayable = nextPlayableTopicIndex(0);
  if (firstPlayable < 0) {
    qWarning() << "No playable topic to start";
    return;
  }

  run_all_ = true;
  requested_auto_mode_ = autoMode;
  curTopicIndex_ = firstPlayable;
  curStepIndex_ = 0;
  execute();
}

void TourController::startOne(TourTopic* const topic, bool autoMode) {
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
  requested_auto_mode_ = autoMode;
  curTopicIndex_ = catalog_.topics().indexOf(topic);
  curStepIndex_ = 0;
  execute();
}

void TourController::updateOverlayGeometry() {
  // 几何由演示层自己按宿主算（它不设 Qt 父子，坐标是屏幕坐标系）
  overlay_->SetHost(viewport_);
}

void TourController::stop() {
  isRunning_ = false;

  // 本来就没有 Qt 父子关系（见 TourOverlay::SetHost），隐藏即可
  overlay_->hide();
}

void TourController::onNextClicked() {
  if (curTopicIndex_ >= catalog_.topics().size()) {
    stop();
    emit finished();
    return;
  }

  TourTopic* curTopic = catalog_.topics().at(curTopicIndex_);
  if (curStepIndex_ >= curTopic->stepCount()) {
    stop();
    emit finished();
    return;
  }

  TourStep* curStep = curTopic->steps().at(curStepIndex_);
  if (curStep->exitFunc()) {
    curStep->exitFunc()();
  }

  // ------------------------
  // 还在一个 topic 中
  if (curStepIndex_ < curTopic->stepCount() - 1) {
    curStepIndex_++;
    updateStep();
    return;
  }

  // ------------------------
  // 一个 Flow 结束
  // 先执行 退出函数
  if (curTopic->exitFunc()) {
    curTopic->exitFunc()();
  }

  // 单主题运行，或已无下一个可播放主题（跳过占位主题）时结束。
  if (!run_all_) {
    stop();
    emit finished();
    return;
  }

  const int nextIndex = nextPlayableTopicIndex(curTopicIndex_ + 1);
  if (nextIndex < 0) {
    stop();
    emit finished();
    return;
  }

  curTopicIndex_ = nextIndex;
  curStepIndex_ = 0;
  TourTopic* nextTopic = catalog_.topics().at(curTopicIndex_);
  ApplyTopicOptions(nextTopic);
  if (nextTopic->enterFunc()) {
    nextTopic->enterFunc()();
  }
  updateOverlayGeometry();
  emit topicChanged(nextTopic->name());
  updateStep();
}

void TourController::onPrevClicked() {
  // 回退 step
  if (curStepIndex_ > 0) {
    curStepIndex_--;
    updateStep();
    return;
  }

  if (!run_all_) {
    return;
  }

  // 回退 topic（跳过占位主题）
  const int prevIndex = previousPlayableTopicIndex(curTopicIndex_ - 1);
  if (prevIndex < 0) {
    return;
  }

  TourTopic* curTopic = catalog_.topics().at(curTopicIndex_);
  if (curTopic->exitFunc()) {
    curTopic->exitFunc()();
  }

  curTopicIndex_ = prevIndex;
  TourTopic* prevTopic = catalog_.topics().at(curTopicIndex_);
  curStepIndex_ = prevTopic->stepCount() - 1;
  ApplyTopicOptions(prevTopic);
  if (prevTopic->enterFunc()) {
    prevTopic->enterFunc()();
  }
  emit topicChanged(prevTopic->name());
  // 原来这里只刷了几何、不刷内容：回退到上一主题后演示层仍显示旧步骤
  // 的标题与文案。改走 updateStep()（末步的 enterFunc + 刷新 + 信号）
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
  if (curTopicIndex_ < 0 || curTopicIndex_ >= catalog_.topics().size()) {
    return;
  }

  TourTopic* curTopic = catalog_.topics().at(curTopicIndex_);
  if (curStepIndex_ < 0 || curStepIndex_ >= curTopic->stepCount()) {
    return;
  }
  TourStep* curStep = curTopic->steps().at(curStepIndex_);

  if (curStep->enterFunc()) {
    curStep->enterFunc()();
  }

  refreshStepOverlay();

  int globalIndex = 0;
  for (int i = 0; i < curTopicIndex_; ++i) {
    globalIndex += catalog_.topics().at(i)->stepCount();
  }
  globalIndex += curStepIndex_;

  emit stepChanged(globalIndex, catalog_.totalSteps());
}

void TourController::refreshCurrentStepHighlight() {
  refreshStepOverlay();
}

// 刷新当前步骤的演示层（高亮/气泡/按钮态）。从 updateStep 抽出，
// 便于 enter 回调中目标控件异步就绪后重新定位。
void TourController::refreshStepOverlay() {
  if (!isRunning_ || catalog_.topics().isEmpty()) {
    return;
  }
  if (curTopicIndex_ < 0 || curTopicIndex_ >= catalog_.topics().size()) {
    return;
  }

  TourTopic* curTopic = catalog_.topics().at(curTopicIndex_);
  if (curStepIndex_ < 0 || curStepIndex_ >= curTopic->stepCount()) {
    return;
  }
  TourStep* curStep = curTopic->steps().at(curStepIndex_);

  // 两个按钮能不能按，口径必须与 onNextClicked / onPrevClicked 的落点一致：
  // 它们跳的是**下一个/上一个可播放的主题**（首页会给 stepCount()==0 的占位
  // 主题建卡，所以 topics 里允许有它）。原先这里拿 topics().count()-1 判"最后
  // 一条"，是数组末尾而不是最后一个可播放的——末尾是占位主题时，最后一步的
  // 按钮永远显示"下一步"而不是"完成！"；首端是占位主题时，"上一步"亮着却点
  // 不动。同一件"还有没有下一站"，只留这两个谓词一个出处
  bool canPrev = true;
  bool canNext = true;
  if (run_all_) {
    const bool has_prev_flow =
        previousPlayableTopicIndex(curTopicIndex_ - 1) >= 0;
    if (curStepIndex_ == 0 && !has_prev_flow) {
      canPrev = false;
    }

    const bool has_next_flow = nextPlayableTopicIndex(curTopicIndex_ + 1) >= 0;
    if (curStepIndex_ == curTopic->stepCount() - 1 && !has_next_flow) {
      canNext = false;
    }
  } else {
    if (curStepIndex_ == 0) {
      canPrev = false;
    }

    if (curStepIndex_ == curTopic->stepCount() - 1) {
      canNext = false;
    }
  }
  overlay_->updateUi(curTopic, curStep, canPrev, canNext);
}

// 自 fromIndex 起向后找第一个可播放（stepCount()>0）的 Flow 下标，无则 -1。
int TourController::nextPlayableTopicIndex(int fromIndex) const {
  for (int i = fromIndex; i < catalog_.topics().size(); ++i) {
    if (catalog_.topics().at(i)->stepCount() > 0) {
      return i;
    }
  }
  return -1;
}

// 自 fromIndex 起向前找第一个可播放（stepCount()>0）的 Flow 下标，无则 -1。
int TourController::previousPlayableTopicIndex(int fromIndex) const {
  for (int i = fromIndex; i >= 0; --i) {
    if (catalog_.topics().at(i)->stepCount() > 0) {
      return i;
    }
  }
  return -1;
}

 
