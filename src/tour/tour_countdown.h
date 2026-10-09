#ifndef FILE_TOUR_COUNTDOWN_H_
#define FILE_TOUR_COUNTDOWN_H_

#include <QObject>
#include <QTimer>

// 气泡上那条倒计时：一条进度条 + 一串剩余秒数。只管"还剩几秒"这一件事，
// 画在哪儿、什么时候露脸由气泡读它决定。
//
// 两种走法：自动挡（setAutoMode + startAuto）走到头发 finished()，意即
// "该翻页了"；静默趟（startSilent）**只显示、不推进**，走到头只收显示
class TourCountdown : public QObject {
  Q_OBJECT

 public:
  explicit TourCountdown(QObject* parent = nullptr);

  // 自动挡开关。开或关都复位并停表——上一趟的残留值不许续到下一趟
  void setAutoMode(bool auto_mode);
  bool autoMode() const { return auto_mode_; }

  // 自动挡起表。不是自动挡时什么也不做
  void startAuto();
  // 外部驱动的视觉倒计时：只显示、不推进（seconds <= 0 时忽略）
  void startSilent(int seconds);
  // 停表。**不复位**：按钮、点遮罩空白那几处只是要它别再走
  void stop();

  // 此刻该不该露脸：自动挡，或静默那趟还没走完（秒数与进度条同一口径）
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
  // 已过去的比例。起表必须复位成 0.0：给 1.0 会让进度条第一帧是空的
  qreal time_percent_ = 0.0;
  // 已走 / 本次总的 tick 数。起表都复位，避免续接上次的残留值
  int tick_ = 0;
  int max_ticks_ = 0;
  int remaining_ = 0;
};

#endif  // FILE_TOUR_COUNTDOWN_H_
