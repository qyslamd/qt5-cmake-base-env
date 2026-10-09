#ifndef FILE_TOUR_COUNTDOWN_H_
#define FILE_TOUR_COUNTDOWN_H_

#include <QObject>
#include <QTimer>

// 气泡上那条倒计时：一条进度条 + 一串剩余秒数。
//
// 从 TourOverlay 里抽出来的——原先散在 setIsAutoMode / updateUi / showCountdown
// 三处，靠 auto_mode_ 与 countdown_silent_ 两个布尔互相制约。它只管"还剩几秒"
// 这一件事：timer、已走比例、剩余秒数都在这里；画在哪儿、什么时候露脸由气泡
// 从这个对象读。
//
// 两种走法：
//   - 自动挡（setAutoMode(true) 之后 startAuto）：讲解式主题那 5 秒，走到头
//     发 finished()，意思是"该翻页了"
//   - 静默趟（startSilent(seconds)）：外部驱动的视觉倒计时，走到头**只把显示
//     收掉、不发信号**——下一步做什么由脚本决定，这里只让"马上要切页了"看得见
class TourCountdown : public QObject {
  Q_OBJECT

 public:
  explicit TourCountdown(QObject* parent = nullptr);

  // 自动挡开关。开或关都复位（时长回到自动挡那 5 秒、静默标记清掉、停表），
  // 免得上一趟的残留值续到下一趟
  void setAutoMode(bool auto_mode);
  bool autoMode() const { return auto_mode_; }

  // 自动挡起表。不是自动挡时什么也不做
  void startAuto();
  // 外部驱动的视觉倒计时：只显示、不推进（seconds <= 0 时忽略）
  void startSilent(int seconds);
  // 停表。**不复位**：按钮、点遮罩空白那几处只是要它别再走
  void stop();

  // 此刻该不该露脸（自动挡，或静默那趟还没走完）：秒数与进度条同一个口径
  bool showing() const { return auto_mode_ || silent_; }
  // 已过去的比例，0.0 = 刚开始。进度条长度用它（1.0 时条子是空的）
  qreal elapsed() const { return time_percent_; }
  // 剩余秒数，界面上那个数字
  int remaining() const { return remaining_; }

 signals:
  void changed();   // 数值变了，显示方重画
  void finished();  // 自动挡走到头：该翻页了

 private:
  void onTick();

  QTimer timer_;
  bool auto_mode_ = false;
  // 本次是不是"只显示、不推进"那一趟（见 startSilent）
  bool silent_ = false;
  // 本次倒计时已过去的比例。起表时必须复位成 0.0——它是已过去不是剩余，
  // 给 1.0 会让进度条第一帧是空的
  qreal time_percent_ = 0.0;
  // 已走的 tick 数与本次的总 tick 数。起表都复位，避免续接上次的残留值
  int tick_ = 0;
  int max_ticks_ = 0;
  int remaining_ = 0;
};

#endif  // FILE_TOUR_COUNTDOWN_H_
