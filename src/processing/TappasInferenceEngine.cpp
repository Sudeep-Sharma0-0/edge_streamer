#include "processing/TappasInferenceEngine.hpp"
#include <iostream>

namespace processing {

bool TappasInferenceEngine::Initialize(const config::AppConfig &app_config) {
  pipeline_ = std::make_unique<TappasPipeline>(app_config.capture,
                                               app_config.inference);
  if (!pipeline_->Initialize()) {
    std::cerr
        << "[TappasInferenceEngine] Failed to initialize TappasPipeline.\n";
    return false;
  }
  return pipeline_->Start();
}

bool TappasInferenceEngine::Initialize(const config::InferenceConfig &config) {
  config::AppConfig app_config;
  app_config.inference = config;
  return Initialize(app_config);
}

bool TappasInferenceEngine::Initialize(const std::string &model_path,
                                       float conf_thresh,
                                       float keypoint_thresh) {
  config::AppConfig app_config;
  app_config.inference.model_path = model_path;
  app_config.inference.confidence_threshold = conf_thresh;
  app_config.inference.keypoint_threshold = keypoint_thresh;
  return Initialize(app_config);
}

InferenceResult TappasInferenceEngine::Infer(const FrameBuffer &frame) {
  InferenceResult result{};

  if (!pipeline_)
    return result;

  pipeline_->PushFrame(frame);

  if (pipeline_->ProcessFrame(last_metrics_)) {
    result.npu_time_ms = static_cast<float>(last_metrics_.frame_latency_ms);
    result.postprocess_time_ms = 0.0f;
  }

  return result;
}

} // namespace processing
