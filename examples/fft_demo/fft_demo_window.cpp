#include "fft_demo_window.h"

#include <qcustomplot.h>

#include <cmath>

#include <QColor>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPen>
#include <QPushButton>
#include <QSpinBox>
#include <QStringList>
#include <QVBoxLayout>

namespace {

// 默认信号：三个分量，频率都不是 bin 宽的整数倍（8000/2048 = 3.90625 Hz），
// 所以默认状态下谱里能看见泄漏——把窗从矩形换成 Hann，泄漏会明显下去
constexpr int kDefaultSampleRate = 8000;
constexpr int kDefaultSampleCount = 2048;
constexpr double kDefaultToneFreqs[] = {440.0, 1200.0, 2500.0};
constexpr double kDefaultToneAmps[] = {0.8, 0.5, 0.3};
constexpr double kDefaultNoise = 0.05;

const char* const kSampleCountLabels[] = {"256",  "512",  "1024",
                                          "2048", "4096", "8192"};

}  // namespace

FftDemoWindow::FftDemoWindow(QWidget* parent) : QWidget(parent) {
  initUi();
  initSignals();
  onRecompute();  // 起来就有一幅图，不必先点按钮
}

void FftDemoWindow::initUi() {
  setObjectName(QStringLiteral("FftDemoWindow"));
  setWindowTitle(QStringLiteral("FFT 频谱演示"));
  resize(1180, 700);

  auto* signal_group = new QGroupBox(QStringLiteral("信号"), this);
  signal_group->setObjectName(QStringLiteral("signal_group"));
  auto* signal_form = new QFormLayout(signal_group);

  sample_rate_spin_ = new QSpinBox(signal_group);
  sample_rate_spin_->setObjectName(QStringLiteral("sample_rate_spin"));
  sample_rate_spin_->setRange(1000, 192000);
  sample_rate_spin_->setSingleStep(1000);
  sample_rate_spin_->setSuffix(QStringLiteral(" Hz"));
  sample_rate_spin_->setValue(kDefaultSampleRate);
  signal_form->addRow(QStringLiteral("采样率"), sample_rate_spin_);

  sample_count_combo_ = new QComboBox(signal_group);
  sample_count_combo_->setObjectName(QStringLiteral("sample_count_combo"));
  for (const char* label : kSampleCountLabels) {
    sample_count_combo_->addItem(QString::fromLatin1(label));
  }
  // 只给 2 的幂：kissfft 对任意偶数长度都行，这里收窄是为了让"谱线间隔 =
  // 采样率 / 点数"这条心算保持干净
  sample_count_combo_->setCurrentIndex(
      sample_count_combo_->findText(QString::number(kDefaultSampleCount)));
  signal_form->addRow(QStringLiteral("点数"), sample_count_combo_);

  // 三个分量：频率与幅度并排，比六个独立的行紧凑
  auto* tone_grid = new QGridLayout();
  tone_grid->addWidget(new QLabel(QStringLiteral("频率 (Hz)"), signal_group), 0,
                       0);
  tone_grid->addWidget(new QLabel(QStringLiteral("幅度"), signal_group), 0, 1);
  for (int i = 0; i < kToneCount; ++i) {
    tone_freq_spins_[i] =
        MakeToneSpin(20000.0, kDefaultToneFreqs[i], signal_group);
    tone_freq_spins_[i]->setObjectName(
        QStringLiteral("tone%1_freq").arg(i + 1));
    tone_amp_spins_[i] = MakeToneSpin(1.0, kDefaultToneAmps[i], signal_group);
    tone_amp_spins_[i]->setObjectName(QStringLiteral("tone%1_amp").arg(i + 1));
    tone_grid->addWidget(tone_freq_spins_[i], i + 1, 0);
    tone_grid->addWidget(tone_amp_spins_[i], i + 1, 1);
  }
  signal_form->addRow(QStringLiteral("分量"), tone_grid);

  noise_spin_ = new QDoubleSpinBox(signal_group);
  noise_spin_->setObjectName(QStringLiteral("noise_spin"));
  noise_spin_->setRange(0.0, 1.0);
  noise_spin_->setSingleStep(0.05);
  noise_spin_->setDecimals(2);
  noise_spin_->setValue(kDefaultNoise);
  signal_form->addRow(QStringLiteral("噪声幅度"), noise_spin_);

  auto* fft_group = new QGroupBox(QStringLiteral("FFT"), this);
  fft_group->setObjectName(QStringLiteral("fft_group"));
  auto* fft_form = new QFormLayout(fft_group);

  window_combo_ = new QComboBox(fft_group);
  window_combo_->setObjectName(QStringLiteral("window_combo"));
  window_combo_->addItem(QStringLiteral("矩形"));
  window_combo_->addItem(QStringLiteral("Hann"));
  window_combo_->addItem(QStringLiteral("Hamming"));
  fft_form->addRow(QStringLiteral("窗函数"), window_combo_);

  spectrum_combo_ = new QComboBox(fft_group);
  spectrum_combo_->setObjectName(QStringLiteral("spectrum_combo"));
  spectrum_combo_->addItem(QStringLiteral("单边幅度"));
  spectrum_combo_->addItem(QStringLiteral("双边幅度"));
  spectrum_combo_->addItem(QStringLiteral("单边 PSD (dB/Hz)"));
  fft_form->addRow(QStringLiteral("谱显示"), spectrum_combo_);

  recompute_button_ = new QPushButton(QStringLiteral("重新计算"), fft_group);
  recompute_button_->setObjectName(QStringLiteral("recompute_button"));
  fft_form->addRow(QString(), recompute_button_);

  status_label_ = new QLabel(fft_group);
  status_label_->setObjectName(QStringLiteral("status_label"));
  status_label_->setWordWrap(true);
  fft_form->addRow(status_label_);

  // 两块图各自套一层 QGroupBox 当标题，省得去动 QCustomPlot 自己的布局
  auto* time_group = new QGroupBox(QStringLiteral("时域波形"), this);
  time_group->setObjectName(QStringLiteral("time_group"));
  auto* time_layout = new QVBoxLayout(time_group);
  time_plot_ = new QCustomPlot(time_group);
  time_plot_->setObjectName(QStringLiteral("time_plot"));
  time_plot_->addGraph();  // 0：原始
  time_plot_->addGraph();  // 1：频域去噪后
  time_plot_->graph(0)->setName(QStringLiteral("原始（含噪声）"));
  time_plot_->graph(1)->setName(QStringLiteral("频域去噪后"));
  time_plot_->graph(1)->setPen(QPen(QColor(0, 114, 189)));
  time_plot_->legend->setVisible(true);
  time_plot_->xAxis->setLabel(QStringLiteral("时间 (ms)"));
  time_plot_->yAxis->setLabel(QStringLiteral("幅度"));
  time_layout->addWidget(time_plot_);

  auto* freq_group = new QGroupBox(QStringLiteral("幅度谱 / 功率谱"), this);
  freq_group->setObjectName(QStringLiteral("freq_group"));
  auto* freq_layout = new QVBoxLayout(freq_group);
  freq_plot_ = new QCustomPlot(freq_group);
  freq_plot_->setObjectName(QStringLiteral("freq_plot"));
  freq_plot_->addGraph();  // 0：谱线
  freq_plot_->addGraph();  // 1：检测到的分量（只画散点）
  freq_plot_->graph(1)->setLineStyle(QCPGraph::lsNone);
  freq_plot_->graph(1)->setScatterStyle(QCPScatterStyle(
      QCPScatterStyle::ssCircle, QPen(Qt::red, 1.5), Qt::NoBrush, 8));
  freq_plot_->xAxis->setLabel(QStringLiteral("频率 (Hz)"));
  freq_plot_->yAxis->setLabel(QStringLiteral("幅度"));
  // 谱只在 0..采样率/2 有值，但默认会把横轴拉到负半轴，看着空一半
  freq_plot_->xAxis->setRangeLower(0.0);
  freq_layout->addWidget(freq_plot_);

  result_label_ = new QLabel(this);
  result_label_->setObjectName(QStringLiteral("result_label"));
  result_label_->setWordWrap(true);

  auto* left_column = new QVBoxLayout();
  left_column->addWidget(signal_group);
  left_column->addWidget(fft_group);
  left_column->addStretch();

  auto* right_column = new QVBoxLayout();
  right_column->addWidget(time_group);
  right_column->addWidget(freq_group);
  right_column->addWidget(result_label_);

  auto* root = new QHBoxLayout(this);
  root->addLayout(left_column);
  root->addLayout(right_column, /*stretch=*/1);
}

void FftDemoWindow::initSignals() {
  connect(recompute_button_, &QPushButton::clicked, this,
          &FftDemoWindow::onRecompute);
  // 换谱的摆法不改信号，所以立刻重算——它只是个视图开关。
  // QComboBox::currentIndexChanged 在 Qt5 里是重载的（还有 QString 那个已废弃的
  // 版本），取地址必须指明哪一个
  connect(spectrum_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) { onRecompute(); });
}

void FftDemoWindow::onRecompute() {
  const FftParams params = CollectParams();
  const FftResult result = RunFft(params);
  if (!result.ok()) {
    status_label_->setText(QStringLiteral("计算失败：%1").arg(result.error));
    result_label_->clear();
    return;
  }

  PlotResult(result);
  status_label_->setText(QStringLiteral("%1 点 · %2 · FFT %3 ms")
                             .arg(params.sample_count)
                             .arg(window_combo_->currentText())
                             .arg(result.fft_ms, 0, 'f', 3));

  QStringList peak_texts;
  for (int i = 0; i < result.peak_freqs.size(); ++i) {
    peak_texts << QStringLiteral("%1 Hz @ %2")
                      .arg(result.peak_freqs.at(i), 0, 'f', 1)
                      .arg(result.peak_mags.at(i), 0, 'f', 3);
  }
  result_label_->setText(
      QStringLiteral("检测到分量：%1\n噪声底 %2 dB/Hz · 去噪保留 %3/%4 根谱线")
          .arg(peak_texts.isEmpty() ? QStringLiteral("（无）")
                                    : peak_texts.join(QStringLiteral("，")))
          .arg(result.noise_floor_db, 0, 'f', 1)
          .arg(result.kept_bins)
          .arg(params.sample_count / 2 + 1));
}

FftParams FftDemoWindow::CollectParams() const {
  FftParams params;
  params.sample_rate = sample_rate_spin_->value();
  params.sample_count = sample_count_combo_->currentText().toInt();
  params.noise_amp = noise_spin_->value();
  params.window = window_combo_->currentIndex();
  params.view = static_cast<SpectrumView>(spectrum_combo_->currentIndex());
  for (int i = 0; i < kToneCount; ++i) {
    params.tone_freqs.append(tone_freq_spins_[i]->value());
    params.tone_amps.append(tone_amp_spins_[i]->value());
  }
  return params;
}

void FftDemoWindow::PlotResult(const FftResult& result) {
  time_plot_->graph(0)->setData(result.time_x, result.time_y);
  time_plot_->graph(1)->setData(result.time_x, result.clean_y);
  time_plot_->rescaleAxes();
  time_plot_->replot();

  freq_plot_->graph(0)->setData(result.freq_x, result.mag_y);
  freq_plot_->rescaleAxes();
  // rescaleAxes 会把下界拉回负值，重新钉到
  // 0（双边谱视图除外，那时本就该有负轴）
  if (result.freq_x.isEmpty() || result.freq_x.first() >= 0.0) {
    freq_plot_->xAxis->setRangeLower(0.0);
  }
  PlotPeakMarkers(result);
  freq_plot_->replot();
}

void FftDemoWindow::PlotPeakMarkers(const FftResult& result) {
  QVector<double> marker_x;
  QVector<double> marker_y;
  for (int i = 0; i < result.peak_freqs.size(); ++i) {
    // 标记要落在当前这条曲线上：双边谱视图里同一根分量的纵坐标只有单边的一半，
    // 直接拿 peak_mags 画会浮在曲线上面
    int nearest = -1;
    double best = 0.0;
    for (int k = 0; k < result.freq_x.size(); ++k) {
      const double distance =
          std::abs(result.freq_x.at(k) - result.peak_freqs.at(i));
      if (nearest < 0 || distance < best) {
        nearest = k;
        best = distance;
      }
    }
    if (nearest >= 0) {
      marker_x.append(result.freq_x.at(nearest));
      marker_y.append(result.mag_y.at(nearest));
    }
  }
  freq_plot_->graph(1)->setData(marker_x, marker_y);
}

QDoubleSpinBox* FftDemoWindow::MakeToneSpin(double max_value,
                                            double default_value,
                                            QWidget* parent) const {
  auto* spin = new QDoubleSpinBox(parent);
  spin->setRange(0.0, max_value);
  spin->setDecimals(2);
  spin->setSingleStep(max_value > 1.0 ? 10.0 : 0.1);
  spin->setValue(default_value);
  return spin;
}
