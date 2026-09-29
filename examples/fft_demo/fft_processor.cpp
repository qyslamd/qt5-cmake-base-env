#include "fft_processor.h"

#include <fftw3.h>

#include <algorithm>
#include <cmath>
#include <random>

#include <QElapsedTimer>

namespace {

// 不取 M_PI：MSVC 的 <cmath> 要先定义 _USE_MATH_DEFINES 才有，这里的常量更省事
constexpr double kTwoPi = 6.283185307179586476925286766559;

// 噪声用固定种子：每次重算得到同一条曲线，便于对照参数改动带来的差异
constexpr unsigned int kNoiseSeed = 20260929;

// PSD 取 dB 时的下限：功率可以小到 log 出 -inf，画不出来也没法比较
constexpr double kPsdFloorDb = -140.0;

// 判定"这根谱线是一个分量"的门槛，两条同时满足：
//   - 高出全谱中位数 kPeakFactor 倍 —— 有噪声时这条说了算
//   - 不低于最强谱线的 kPeakMinRatio —— 无噪声时中位数几乎是 0，只有上一条的话
//     矩形窗那些泄漏旁瓣（局部极大，一路衰减到 -190 dB）会全被报成分量
// 取中位数而不是均值，是因为有几个强分量在场时均值会被它们抬起来
constexpr double kPeakFactor = 8.0;
constexpr double kPeakMinRatio = 0.05;

// 两根峰隔得比这个近就只留强的。矩形窗的旁瓣就贴在主瓣旁边（第 10 根旁瓣还有
// 主瓣的 3%），间距给宽些才能把它们一并吃掉
constexpr int kPeakMinSeparationBins = 10;

// 谱减法：功率低于"噪声底 × 这个倍数"的谱线清掉（10 倍 = 10 dB）
constexpr double kDenoiseFactor = 10.0;

// FFTW 的缓冲要用 fftwf_malloc 分配（SIMD 内核要求 16 字节对齐）、fftwf_free
// 释放，plan 也要手工销毁。包一层 RAII 是为了中途
// return（比如分配失败）时不漏， 也顺手排掉"释放了还去读"这类顺序错误
template <typename T>
class FftwBuffer {
 public:
  explicit FftwBuffer(int count)
      : data_(static_cast<T*>(fftwf_malloc(sizeof(T) * count))) {}

  ~FftwBuffer() { fftwf_free(data_); }

  FftwBuffer(const FftwBuffer&) = delete;
  FftwBuffer& operator=(const FftwBuffer&) = delete;

  T* data() const { return data_; }
  bool valid() const { return data_ != nullptr; }

 private:
  T* data_;
};

class FftwPlan {
 public:
  explicit FftwPlan(fftwf_plan plan) : plan_(plan) {}

  ~FftwPlan() {
    if (plan_ != nullptr) {
      fftwf_destroy_plan(plan_);
    }
  }

  FftwPlan(const FftwPlan&) = delete;
  FftwPlan& operator=(const FftwPlan&) = delete;

  void Execute() const { fftwf_execute(plan_); }
  bool valid() const { return plan_ != nullptr; }

 private:
  fftwf_plan plan_;
};

// 复数谱。r2c 出来是 n/2+1 根（正频率那半边 + DC + Nyquist），逆变换要拿它回去
struct Spectrum {
  QVector<float> re;
  QVector<float> im;
  QVector<double> power;  // |X|²，省得每次重算
};

// 窗系数。顺带交出窗的和与平方和：**幅度谱按窗之和归一，PSD 按窗的平方和归一**
// （后者对应等效噪声带宽），两个口径不能混
QVector<double> MakeWindow(int kind, int n, double* sum, double* square_sum) {
  QVector<double> window(n);
  double total = 0.0;
  double total_sq = 0.0;
  for (int i = 0; i < n; ++i) {
    const double t = kTwoPi * i / (n - 1);
    double value = 1.0;
    if (kind == 1) {
      value = 0.5 * (1.0 - std::cos(t));  // Hann
    } else if (kind == 2) {
      value = 0.54 - 0.46 * std::cos(t);  // Hamming
    }
    window[i] = value;
    total += value;
    total_sq += value * value;
  }
  *sum = total;
  *square_sum = total_sq;
  return window;
}

// 合成信号：各音调叠加，再叠一层均匀噪声（固定种子，可复现）
QVector<double> SynthesizeSignal(const FftParams& params, int n) {
  std::mt19937 generator(kNoiseSeed);
  std::uniform_real_distribution<double> noise(-params.noise_amp,
                                               params.noise_amp);
  QVector<double> signal(n);
  for (int i = 0; i < n; ++i) {
    const double t = static_cast<double>(i) / params.sample_rate;
    double value = 0.0;
    for (int k = 0; k < params.tone_freqs.size(); ++k) {
      value += params.tone_amps.value(k) *
               std::sin(kTwoPi * params.tone_freqs.value(k) * t);
    }
    if (params.noise_amp > 0.0) {
      value += noise(generator);
    }
    signal[i] = value;
  }
  return signal;
}

// 实数到复数的正变换。samples 里的东西直接进 FFTW，加不加窗由调用方决定
bool ForwardR2c(const QVector<double>& samples, int n, int threads,
                double* elapsed_ms, Spectrum* spectrum) {
  const int bins = n / 2 + 1;
  FftwBuffer<float> in(n);
  FftwBuffer<fftwf_complex> out(bins);
  if (!in.valid() || !out.valid()) {
    return false;
  }
  for (int i = 0; i < n; ++i) {
    in.data()[i] = static_cast<float>(samples[i]);
  }

  // 多线程只影响此后新建的 plan；调用方没 init 过线程的话 FFTW 会忽略这一句
  if (threads > 1) {
    fftwf_plan_with_nthreads(threads);
  }
  // FFTW_ESTIMATE：规划瞬时完成、且不改动输入数组。换成 FFTW_MEASURE 重复执行
  // 时更快，但规划本身慢得多，而且**会覆盖 in**——那种写法得在规划之后再填数据
  FftwPlan plan(fftwf_plan_dft_r2c_1d(n, in.data(), out.data(), FFTW_ESTIMATE));
  if (!plan.valid()) {
    return false;
  }

  QElapsedTimer timer;
  timer.start();
  plan.Execute();
  if (elapsed_ms != nullptr) {
    *elapsed_ms = static_cast<double>(timer.nsecsElapsed()) / 1.0e6;
  }

  spectrum->re.resize(bins);
  spectrum->im.resize(bins);
  spectrum->power.resize(bins);
  for (int k = 0; k < bins; ++k) {
    const float re = out.data()[k][0];
    const float im = out.data()[k][1];
    spectrum->re[k] = re;
    spectrum->im[k] = im;
    spectrum->power[k] =
        static_cast<double>(re) * re + static_cast<double>(im) * im;
  }
  return true;
}

// 复数到实数的逆变换。既有的复数谱原样搬进 FFTW 的缓冲，**不**在这里清谱线
bool InverseC2r(const Spectrum& spectrum, int n, QVector<double>* signal_out) {
  const int bins = n / 2 + 1;
  FftwBuffer<fftwf_complex> spec(bins);
  FftwBuffer<float> out(n);
  if (!spec.valid() || !out.valid()) {
    return false;
  }
  for (int k = 0; k < bins; ++k) {
    spec.data()[k][0] = spectrum.re[k];
    spec.data()[k][1] = spectrum.im[k];
  }

  FftwPlan plan(
      fftwf_plan_dft_c2r_1d(n, spec.data(), out.data(), FFTW_ESTIMATE));
  if (!plan.valid()) {
    return false;
  }
  plan.Execute();

  signal_out->resize(n);
  // c2r **不做** 1/N 归一，这里自己除
  const double scale = 1.0 / n;
  for (int i = 0; i < n; ++i) {
    (*signal_out)[i] = static_cast<double>(out.data()[i]) * scale;
  }
  return true;
}

// 单边功率谱密度（功率/Hz）。除 DC 与 Nyquist 外翻倍——负频率那半折过来；
// 再除以采样率与窗的平方和
double PsdAt(int k, double power, int n, double sample_rate,
             double window_square_sum) {
  const double density = power / (sample_rate * window_square_sum);
  if (k == 0 || k == n / 2) {
    return density;
  }
  return 2.0 * density;
}

double ToDb(double power_ratio) {
  const double floored =
      std::max(power_ratio, std::pow(10.0, kPsdFloorDb / 10.0));
  return 10.0 * std::log10(floored);
}

QVector<double> SortedCopy(const QVector<double>& values) {
  QVector<double> sorted = values;
  std::sort(sorted.begin(), sorted.end());
  return sorted;
}

// 从单边幅度谱里找分量：比全谱中位数高出 kPeakFactor 倍、且是局部极大的 bin，
// 再用抛物线插值把频率细化到 bin 以内（FFT 本身只给到 bin 精度）
void FindPeaks(const QVector<double>& amp, double bin_hz,
               QVector<double>* peak_freqs, QVector<double>* peak_mags) {
  const int bins = amp.size();
  double strongest = 0.0;
  for (double value : amp) {
    strongest = std::max(strongest, value);
  }
  const double threshold = std::max(SortedCopy(amp)[bins / 2] * kPeakFactor,
                                    strongest * kPeakMinRatio);

  QVector<int> candidates;
  for (int k = 1; k + 1 < bins; ++k) {
    if (amp[k] < threshold) {
      continue;
    }
    if (amp[k] < amp[k - 1] || amp[k] < amp[k + 1]) {
      continue;
    }
    candidates.append(k);
  }
  // 从强到弱挑，与已挑中的隔得太近就丢——矩形窗的旁瓣也是局部极大
  std::sort(candidates.begin(), candidates.end(),
            [&amp](int lhs, int rhs) { return amp[lhs] > amp[rhs]; });
  QVector<int> kept;
  for (int k : candidates) {
    bool far_enough = true;
    for (int j : kept) {
      if (std::abs(k - j) < kPeakMinSeparationBins) {
        far_enough = false;
        break;
      }
    }
    if (far_enough) {
      kept.append(k);
    }
  }

  std::sort(kept.begin(), kept.end());
  for (int k : kept) {
    const double left = amp[k - 1];
    const double mid = amp[k];
    const double right = amp[k + 1];
    const double denominator = left - 2.0 * mid + right;
    double offset = 0.0;
    if (std::abs(denominator) > 1e-20) {
      // 抛物线顶点相对中间那根谱线的偏移，落在 -0.5~0.5 之间
      offset = 0.5 * (left - right) / denominator;
    }
    peak_freqs->append((k + offset) * bin_hz);
    peak_mags->append(mid);
  }
}

}  // namespace

FftResult RunFft(const FftParams& params) {
  FftResult result;

  const int n = params.sample_count;
  if (n < 2) {
    result.error = QStringLiteral("采样点数太小");
    return result;
  }

  double window_sum = 0.0;
  double window_square_sum = 0.0;
  const QVector<double> window =
      MakeWindow(params.window, n, &window_sum, &window_square_sum);
  if (window_sum <= 0.0 || window_square_sum <= 0.0) {
    result.error = QStringLiteral("窗系数之和为 0，无法归一");
    return result;
  }

  const QVector<double> signal = SynthesizeSignal(params, n);
  const double bin_hz = params.sample_rate / n;
  const int bins = n / 2 + 1;

  // ---- 显示那条路：加窗之后正变换（加窗才能演示泄漏）----
  QVector<double> windowed(n);
  for (int i = 0; i < n; ++i) {
    windowed[i] = signal[i] * window[i];
  }
  Spectrum display_spectrum;
  if (!ForwardR2c(windowed, n, params.threads, &result.fft_ms,
                  &display_spectrum)) {
    result.error = QStringLiteral("正变换失败（缓冲区或规划）");
    return result;
  }

  // 三种视图都从原始幅度换算
  QVector<double> amplitude(bins);
  for (int k = 0; k < bins; ++k) {
    amplitude[k] = std::sqrt(display_spectrum.power[k]);
  }
  // 单边幅度：负半轴的功率折过来，除 DC 与 Nyquist（它们没有"另一半"）外 ×2。
  // 找峰也在这份**归一之后**的幅度上做——拿未归一的 |X| 去报幅度会差 N/2 倍，
  // 而峰的位置不变，所以这个错很容易只错一半、看不出来
  QVector<double> single_amp(bins);
  for (int k = 0; k < bins; ++k) {
    const bool two_sided_bin = (k != 0 && k != n / 2);
    const double scale = two_sided_bin ? 2.0 / window_sum : 1.0 / window_sum;
    single_amp[k] = amplitude[k] * scale;
  }
  FindPeaks(single_amp, bin_hz, &result.peak_freqs, &result.peak_mags);

  // ---- 去噪那条路：不加窗正变换 → 按噪声底清谱线 → 逆变换回时域 ----
  // 用未加窗的信号：窗在两端趋近 0，除回去会把噪声放大成尖刺
  Spectrum raw_spectrum;
  if (!ForwardR2c(signal, n, params.threads, nullptr, &raw_spectrum)) {
    result.error = QStringLiteral("正变换失败（去噪那条路）");
    return result;
  }
  const double raw_floor = SortedCopy(raw_spectrum.power)[bins / 2];
  const double threshold = raw_floor * kDenoiseFactor;
  for (int k = 0; k < bins; ++k) {
    if (raw_spectrum.power[k] >= threshold) {
      ++result.kept_bins;
    } else {
      // 清掉的谱线实虚部都置 0；保留的谱线**相位照旧**——只留幅度、把相位抹平
      // 会让逆变换出来的波形整个错位，那不是去噪
      raw_spectrum.re[k] = 0.0f;
      raw_spectrum.im[k] = 0.0f;
    }
  }
  if (!InverseC2r(raw_spectrum, n, &result.clean_y)) {
    result.error = QStringLiteral("逆变换失败");
    return result;
  }

  // 报出来的噪声底跟画出来的 PSD 用同一个口径，不然图上报的和算的对不上
  const double display_floor = SortedCopy(display_spectrum.power)[bins / 2];
  result.noise_floor_db = ToDb(
      PsdAt(bins / 2, display_floor, n, params.sample_rate, window_square_sum));

  // ---- 谱的三种摆法 ----
  result.freq_x.reserve(bins);
  result.mag_y.reserve(bins);
  if (params.view == SpectrumView::kDoubleAmplitude) {
    // 负半轴：|X[n-k]| = |X[k]|（实数信号的共轭对称），幅度不翻倍——两个半边
    // 各带一半，正负加起来才是分量幅度
    for (int k = bins - 1; k >= 1; --k) {
      result.freq_x.append(-static_cast<double>(k) * bin_hz);
      result.mag_y.append(amplitude[k] / window_sum);
    }
    for (int k = 0; k < bins; ++k) {
      result.freq_x.append(static_cast<double>(k) * bin_hz);
      result.mag_y.append(amplitude[k] / window_sum);
    }
  } else if (params.view == SpectrumView::kSinglePsdDb) {
    for (int k = 0; k < bins; ++k) {
      result.freq_x.append(static_cast<double>(k) * bin_hz);
      result.mag_y.append(ToDb(PsdAt(k, display_spectrum.power[k], n,
                                     params.sample_rate, window_square_sum)));
    }
  } else {
    for (int k = 0; k < bins; ++k) {
      result.freq_x.append(static_cast<double>(k) * bin_hz);
      result.mag_y.append(single_amp[k]);
    }
  }

  // ---- 时域 ----
  result.time_x.reserve(n);
  result.time_y.reserve(n);
  for (int i = 0; i < n; ++i) {
    result.time_x.append(static_cast<double>(i) * 1000.0 / params.sample_rate);
    result.time_y.append(signal[i]);
  }

  return result;
}
