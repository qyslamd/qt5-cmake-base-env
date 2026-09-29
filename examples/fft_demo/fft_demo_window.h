#ifndef FILE_EXAMPLES_FFT_DEMO_FFT_DEMO_WINDOW_H_
#define FILE_EXAMPLES_FFT_DEMO_FFT_DEMO_WINDOW_H_

#include <array>

#include <QWidget>

#include "fft_processor.h"

class QComboBox;
class QCustomPlot;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;

// 频谱演示窗口：左边一列参数，右边上下两块图——时域（原始 + 去噪后）与谱
// （三种摆法切换），谱上标出检测到的分量。
//
// 信号参数改动不自动重算（点了「重新计算」才跑），便于逐项对照；谱的显示方式
// 是**视图**切换而不是信号改动，所以改了下拉立刻重算
class FftDemoWindow : public QWidget {
  Q_OBJECT

 public:
  // threads_available：调用方是否成功初始化了 FFTW 的线程支持（见 main.cpp）。
  // 没成功就不摆线程数控件——摆一个改不动的控件只会误导
  explicit FftDemoWindow(bool threads_available, QWidget* parent = nullptr);

 private slots:
  void onRecompute();

 private:
  void initUi(bool threads_available);
  void initSignals();
  FftParams CollectParams() const;
  void PlotResult(const FftResult& result);
  void PlotPeakMarkers(const FftResult& result);

  QDoubleSpinBox* MakeToneSpin(double max_value, double default_value,
                               QWidget* parent) const;

  // 三个音调的频率与幅度固定三组：这一版只演示"多分量 + 加窗"，不做任意个分量
  static constexpr int kToneCount = 3;
  std::array<QDoubleSpinBox*, kToneCount> tone_freq_spins_ = {};
  std::array<QDoubleSpinBox*, kToneCount> tone_amp_spins_ = {};

  QSpinBox* sample_rate_spin_ = nullptr;
  QComboBox* sample_count_combo_ = nullptr;
  QDoubleSpinBox* noise_spin_ = nullptr;
  QComboBox* window_combo_ = nullptr;
  QComboBox* spectrum_combo_ = nullptr;
  QSpinBox* threads_spin_ = nullptr;
  QPushButton* recompute_button_ = nullptr;
  QLabel* status_label_ = nullptr;
  QLabel* result_label_ = nullptr;
  QCustomPlot* time_plot_ = nullptr;
  QCustomPlot* freq_plot_ = nullptr;
};

#endif  // FILE_EXAMPLES_FFT_DEMO_FFT_DEMO_WINDOW_H_
