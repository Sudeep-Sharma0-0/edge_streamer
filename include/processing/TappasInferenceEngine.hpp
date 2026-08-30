#pragma once

#include "config/AppConfig.hpp"
#include "processing/IInferenceEngine.hpp"
#include "processing/TappasPipeline.hpp"
#include <memory>

namespace processing {

class TappasInferenceEngine : public IInferenceEngine {
public:
  TappasInferenceEngine() = default;
  ~TappasInferenceEngine() override = default;

  bool Initialize(const config::AppConfig &app_config) override;
  bool Initialize(const config::InferenceConfig &config) override;
  bool Initialize(const std::string &model_path, float conf_thresh,
                  float keypoint_thresh) override;
  InferenceResult Infer(const FrameBuffer &frame) override;

private:
  std::unique_ptr<TappasPipeline> pipeline_;
  BenchmarkMetrics last_metrics_;
};

} // namespace processing
