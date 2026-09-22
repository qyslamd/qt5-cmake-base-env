#include "guidance_controller.h"

#include <QDebug>

#include "guidance_presentation.h"
#include "guidance_style.h"

 

GuidanceController::GuidanceController(QObject* parent)
    : QObject(parent),
      presentation_(new GuidancePresentation(nullptr)),
      isRunning_(false),
      curFlowIndex_(0),
      curStepIndex_(0) {
  // 库自己把样式并入应用样式表（顺带完成静态库资源的初始化），宿主不必管。
  // 控制器是引导的入口，放这里能保证任何引导界面出来之前样式已就位
  GuidanceStyle::EnsureApplied();

  connect(presentation_, &GuidancePresentation::nextClicked, this,
          &GuidanceController::onNextClicked);
  connect(presentation_, &GuidancePresentation::prevClicked, this,
          &GuidanceController::onPrevClicked);
  connect(presentation_, &GuidancePresentation::skipClicked, this,
          &GuidanceController::onSkipClicked);
}

GuidanceController::~GuidanceController() {
  stop();
  // 演示层由控制器持有，销毁时回收（stop 后无父级，避免泄漏）。
  delete presentation_;
  presentation_ = nullptr;
  config_.clear();
}

void GuidanceController::setViewport(QWidget* viewport) {
  viewport_ = viewport;
}

GuidanceFlow* GuidanceController::addFlow(GuidanceFlow* flow) {
  return config_.addFlow(flow);
}

void GuidanceController::clearFlows() {
  config_.clear();
}

void GuidanceController::execute() {
  isRunning_ = true;

  // 先 show 再摆位：SetHost 内部要 raise()，对已映射的窗口才有效
  presentation_->show();
  presentation_->SetHost(viewport_);

  // 主题级 enterFunc 在 startOne/startAll 直启路径此前从不执行（只有
  // 跨主题前进/回退才调），上层挂在这上的状态编排会被静默跳过——统一
  // 收口到 execute()，两条路径行为一致
  GuidanceFlow* flow = config_.flows().at(curFlowIndex_);
  // 模式推送收口在 ApplyFlowOptions 里，避免像以前那样只有本路径记得推、
  // 跨主题那两条漏掉
  ApplyFlowOptions(flow);
  if (flow->enterFunc()) {
    flow->enterFunc()();
  }

  updateStep();

  emit started();
}

void GuidanceController::ApplyFlowOptions(GuidanceFlow* const flow) {
  if (flow == nullptr) {
    return;
  }
  // 实操式主题忽略自动倒计时：等用户动手优先于自动翻页。用"用户请求的自动
  // 模式"来算，而不是把 auto_mode_ 永久置 false——否则走过一个实操主题之后，
  // 后面即使全是讲解主题也不会再自动翻了
  auto_mode_ = requested_auto_mode_ && !flow->interactive();

  presentation_->setInteractive(flow->interactive());
  // 主题驱动时一定不是被动模式。被动模式是 funq 桥接那套（外部驱动、不带按钮、
  // 停倒计时），上一轮跑完留下的状态不能跟到这一轮来——踩过：跑完 funq 示例
  // （被测程序还开着）再点菜单开引导，跳过/上一步/下一步全没了
  presentation_->setPassiveMode(false);
  // 演示层的自动模式必须跟着一起推：refreshStepPresentation() 起不起倒计时看
  // 的是**演示层自己的** auto_mode_。跨主题前进/回退这两条路径以前只调了本
  // 函数、没推模式，于是两边分叉——讲解主题不再自动翻页，实操主题反而被自动
  // 翻页（用户正在动手，5 秒后页面自己跳走）
  presentation_->setIsAutoMode(auto_mode_);
}

void GuidanceController::startAll(bool autoMode) {
  if (config_.flows().isEmpty()) {
    return;
  }

  if (viewport_ == nullptr) {
    qWarning() << "Viewport must be set before start()";
    return;
  }

  // 占位主题（stepCount()==0）仅供首页卡片展示，不进入播放。
  const int firstPlayable = nextPlayableFlowIndex(0);
  if (firstPlayable < 0) {
    qWarning() << "No playable flow to start";
    return;
  }

  run_all_ = true;
  requested_auto_mode_ = autoMode;
  curFlowIndex_ = firstPlayable;
  curStepIndex_ = 0;
  execute();
}

void GuidanceController::startOne(GuidanceFlow* const flow, bool autoMode) {
  if (flow == nullptr) {
    return;
  }

  if (flow->stepCount() == 0) {
    qWarning() << "Placeholder flow can not be played";
    return;
  }

  if (!config_.flows().contains(flow)) {
    return;
  }

  if (viewport_ == nullptr) {
    qWarning() << "Viewport must be set before start()";
    return;
  }

  run_all_ = false;
  requested_auto_mode_ = autoMode;
  curFlowIndex_ = config_.flows().indexOf(flow);
  curStepIndex_ = 0;
  execute();
}

void GuidanceController::updateOverlayGeometry() {
  // 几何由演示层自己按宿主算（它不设 Qt 父子，坐标是屏幕坐标系）
  presentation_->SetHost(viewport_);
}

void GuidanceController::stop() {
  isRunning_ = false;

  // 本来就没有 Qt 父子关系（见 GuidancePresentation::SetHost），隐藏即可
  presentation_->hide();
}

void GuidanceController::onNextClicked() {
  if (curFlowIndex_ >= config_.flows().size()) {
    stop();
    emit finished();
    return;
  }

  GuidanceFlow* curFlow = config_.flows().at(curFlowIndex_);
  if (curStepIndex_ >= curFlow->stepCount()) {
    stop();
    emit finished();
    return;
  }

  GuidanceStep* curStep = curFlow->steps().at(curStepIndex_);
  if (curStep->exitFunc()) {
    curStep->exitFunc()();
  }

  // ------------------------
  // 还在一个 flow 中
  if (curStepIndex_ < curFlow->stepCount() - 1) {
    curStepIndex_++;
    updateStep();
    return;
  }

  // ------------------------
  // 一个 Flow 结束
  // 先执行 退出函数
  if (curFlow->exitFunc()) {
    curFlow->exitFunc()();
  }

  // 单主题运行，或已无下一个可播放主题（跳过占位主题）时结束。
  if (!run_all_) {
    stop();
    emit finished();
    return;
  }

  const int nextIndex = nextPlayableFlowIndex(curFlowIndex_ + 1);
  if (nextIndex < 0) {
    stop();
    emit finished();
    return;
  }

  curFlowIndex_ = nextIndex;
  curStepIndex_ = 0;
  GuidanceFlow* nextFlow = config_.flows().at(curFlowIndex_);
  ApplyFlowOptions(nextFlow);
  if (nextFlow->enterFunc()) {
    nextFlow->enterFunc()();
  }
  updateOverlayGeometry();
  emit flowChanged(nextFlow->name());
  updateStep();
}

void GuidanceController::onPrevClicked() {
  // 回退 step
  if (curStepIndex_ > 0) {
    curStepIndex_--;
    updateStep();
    return;
  }

  if (!run_all_) {
    return;
  }

  // 回退 flow（跳过占位主题）
  const int prevIndex = previousPlayableFlowIndex(curFlowIndex_ - 1);
  if (prevIndex < 0) {
    return;
  }

  GuidanceFlow* curFlow = config_.flows().at(curFlowIndex_);
  if (curFlow->exitFunc()) {
    curFlow->exitFunc()();
  }

  curFlowIndex_ = prevIndex;
  GuidanceFlow* prevFlow = config_.flows().at(curFlowIndex_);
  curStepIndex_ = prevFlow->stepCount() - 1;
  ApplyFlowOptions(prevFlow);
  if (prevFlow->enterFunc()) {
    prevFlow->enterFunc()();
  }
  emit flowChanged(prevFlow->name());
  // 原来这里只刷了几何、不刷内容：回退到上一主题后演示层仍显示旧步骤
  // 的标题与文案。改走 updateStep()（末步的 enterFunc + 刷新 + 信号）
  updateStep();
}

void GuidanceController::onSkipClicked() {
  stop();
  emit finishedByUser();
}

void GuidanceController::updateStep() {
  if (config_.flows().isEmpty()) {
    return;
  }
  if (curFlowIndex_ < 0 || curFlowIndex_ >= config_.flows().size()) {
    return;
  }

  GuidanceFlow* curFlow = config_.flows().at(curFlowIndex_);
  if (curStepIndex_ < 0 || curStepIndex_ >= curFlow->stepCount()) {
    return;
  }
  GuidanceStep* curStep = curFlow->steps().at(curStepIndex_);

  if (curStep->enterFunc()) {
    curStep->enterFunc()();
  }

  refreshStepPresentation();

  int globalIndex = 0;
  for (int i = 0; i < curFlowIndex_; ++i) {
    globalIndex += config_.flows().at(i)->stepCount();
  }
  globalIndex += curStepIndex_;

  emit stepChanged(globalIndex, config_.totalSteps());
}

void GuidanceController::refreshCurrentStepHighlight() {
  refreshStepPresentation();
}

// 刷新当前步骤的演示层（高亮/气泡/按钮态）。从 updateStep 抽出，
// 便于 enter 回调中目标控件异步就绪后重新定位（D12）。
void GuidanceController::refreshStepPresentation() {
  if (!isRunning_ || config_.flows().isEmpty()) {
    return;
  }
  if (curFlowIndex_ < 0 || curFlowIndex_ >= config_.flows().size()) {
    return;
  }

  GuidanceFlow* curFlow = config_.flows().at(curFlowIndex_);
  if (curStepIndex_ < 0 || curStepIndex_ >= curFlow->stepCount()) {
    return;
  }
  GuidanceStep* curStep = curFlow->steps().at(curStepIndex_);

  bool canPrev = true;
  bool canNext = true;
  if (run_all_) {
    if (curFlowIndex_ == 0 && curStepIndex_ == 0) {
      canPrev = false;
    }

    if (curFlowIndex_ == config_.flows().count() - 1 &&
        curStepIndex_ == curFlow->stepCount() - 1) {
      canNext = false;
    }
  } else {
    if (curStepIndex_ == 0) {
      canPrev = false;
    }

    if (curStepIndex_ == curFlow->stepCount() - 1) {
      canNext = false;
    }
  }
  presentation_->updateUi(curFlow, curStep, canPrev, canNext);
}

// 自 fromIndex 起向后找第一个可播放（stepCount()>0）的 Flow 下标，无则 -1。
int GuidanceController::nextPlayableFlowIndex(int fromIndex) const {
  for (int i = fromIndex; i < config_.flows().size(); ++i) {
    if (config_.flows().at(i)->stepCount() > 0) {
      return i;
    }
  }
  return -1;
}

// 自 fromIndex 起向前找第一个可播放（stepCount()>0）的 Flow 下标，无则 -1。
int GuidanceController::previousPlayableFlowIndex(int fromIndex) const {
  for (int i = fromIndex; i >= 0; --i) {
    if (config_.flows().at(i)->stepCount() > 0) {
      return i;
    }
  }
  return -1;
}

 
