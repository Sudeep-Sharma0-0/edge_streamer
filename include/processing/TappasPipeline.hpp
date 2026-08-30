#pragma once

#include "config/AppConfig.hpp"
#include "processing/IInferenceEngine.hpp"
#include <chrono>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <string>

namespace processing {

struct BenchmarkMetrics {
  double frame_latency_ms{0.0};
  size_t frame_count{0};
  double average_fps{0.0};
};

class TappasPipeline {
public:
  TappasPipeline(const config::CaptureConfig &capture_cfg,
                 const config::InferenceConfig &inf_cfg);
  ~TappasPipeline();

  bool Initialize();
  bool Start();
  void Stop();
  bool PushFrame(const FrameBuffer &frame);
  bool ProcessFrame(BenchmarkMetrics &metrics);

  GstAppSink *GetAppSink() const { return appsink_; }

private:
  std::string BuildPipelineString() const;

  config::CaptureConfig capture_cfg_;
  config::InferenceConfig inf_cfg_;

  GstElement *pipeline_{nullptr};
  GstAppSrc *appsrc_{nullptr};
  GstAppSink *appsink_{nullptr};

  std::chrono::high_resolution_clock::time_point start_time_;
  std::chrono::high_resolution_clock::time_point last_frame_time_;
  size_t frame_counter_{0};
};

} // namespace processing
