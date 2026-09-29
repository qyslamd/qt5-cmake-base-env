#include <fftw3.h>

#include <QApplication>

#include "fft_demo_window.h"

namespace {

// FFTW 的线程支持是进程级的：用之前初始化一次、退出前清理一次。做成 RAII 是
// 为了不管从哪条路径返回都能收尾。
//
// 它与 Qt 的 QThread、std::thread 互不干扰——FFTW 只是在自己内部开几个 worker
// 来并行**单次**变换，不改变调用方的线程模型。真正的两条约束在 FFTW 那边：
//   1. 不初始化就调 fftwf_plan_with_nthreads，设置会被忽略
//   2. **建 plan 不是线程安全的**（多线程同时 fftwf_plan_* 会坏），要并发建就得
//      自己加锁，或者调 fftwf_make_planner_thread_safe()。而 fftwf_execute 在
//      不同线程上跑不同数组是安全的
class FftwThreadGuard {
 public:
  FftwThreadGuard() : ready_(fftwf_init_threads() != 0) {}

  ~FftwThreadGuard() {
    if (ready_) {
      fftwf_cleanup_threads();
    }
  }

  FftwThreadGuard(const FftwThreadGuard&) = delete;
  FftwThreadGuard& operator=(const FftwThreadGuard&) = delete;

  bool ready() const { return ready_; }

 private:
  bool ready_;
};

}  // namespace

int main(int argc, char** argv) {
  QApplication app(argc, argv);

  // 要在任何一次 FFT 之前；失败也不拦启动，窗口会把线程数那项收起来
  FftwThreadGuard fftw_threads;

  FftDemoWindow window(fftw_threads.ready());
  window.show();

  return app.exec();
}
