#ifndef FILE_EXAMPLES_FFT_DEMO_FFT_PROCESSOR_H_
#define FILE_EXAMPLES_FFT_DEMO_FFT_PROCESSOR_H_

#include <QString>
#include <QVector>

// 谱的三种摆法。它们都是同一份 FFT 结果的呈现，差别在归一化口径：
//   kSingleAmplitude —— 把负半轴折起来，bin 上的分量峰值 = 该分量的幅度
//   kDoubleAmplitude —— 不折，能看见实数信号谱的共轭对称（峰值是一半）
//   kSinglePsdDb     —— 功率/Hz 取 dB。噪声底不随点数变，看宽带信号用这个
enum class SpectrumView {
  kSingleAmplitude = 0,
  kDoubleAmplitude = 1,
  kSinglePsdDb = 2,
};

// FFT 的输入参数。这一层刻意不碰任何 Qt 部件：界面那半只负责把控件值搬进来、
// 把结果画出去，算法与显示分开，便于替换与验证
struct FftParams {
  int sample_count = 2048;  // 采样点数，取 2 的幂（谱线间隔 =
                            // 采样率/点数，好心算；kissfft 对任意偶数长度都行）
  double sample_rate = 8000.0;  // 采样率，Hz
  QVector<double> tone_freqs;   // 各分量频率，Hz
  QVector<double> tone_amps;    // 各分量幅度
  double noise_amp = 0.0;       // 均匀噪声幅度，0 = 不加
  int window = 0;               // 0 矩形 / 1 Hann / 2 Hamming
  SpectrumView view = SpectrumView::kSingleAmplitude;  // 谱怎么摆
};

struct FftResult {
  QVector<double> time_x;   // 时域横轴，毫秒
  QVector<double> time_y;   // 时域波形（合成后、未加窗）
  QVector<double> clean_y;  // 频域阈值去噪 + 逆变换后的波形，与 time_y 同长

  // 谱：横轴与纵轴的含义随 params.view 变（见 SpectrumView）
  QVector<double> freq_x;
  QVector<double> mag_y;

  QVector<double> peak_freqs;  // 检测到的分量频率（抛物线插值细化过）
  QVector<double> peak_mags;   // 对应幅度

  int kept_bins = 0;  // 去噪保留了几根谱线（单边，共 sample_count/2+1）
  double noise_floor_db = 0.0;  // 估出的噪声底，dB/Hz（单边 PSD）

  double fft_ms = 0.0;  // 正变换耗时，不含规划与缓冲区分配
  QString error;        // 空 = 成功
  bool ok() const { return error.isEmpty(); }
};

// 合成信号 → 正变换（显示谱 + 找峰）→ 频域阈值去噪 → 逆变换。
//
// 用两套变换：显示那份**加窗**（不然演示不了泄漏），去噪那份**不加窗**——窗在
// 两端趋近 0，除回去会把噪声放大成尖刺，所以谱减法一律在未加窗的信号上做
FftResult RunFft(const FftParams& params);

#endif  // FILE_EXAMPLES_FFT_DEMO_FFT_PROCESSOR_H_
