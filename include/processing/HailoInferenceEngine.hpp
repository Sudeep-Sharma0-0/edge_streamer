#pragma once

#include "config/AppConfig.hpp"
#include "processing/IInferenceEngine.hpp"
#include "processing/InferenceTypes.hpp"
#include <hailo/hailort.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace processing {

class HailoInferenceEngine : public IInferenceEngine {
public:
  HailoInferenceEngine();
  explicit HailoInferenceEngine(const config::InferenceConfig &config);
  ~HailoInferenceEngine() override;

  bool Initialize(const config::InferenceConfig &config) override;
  bool Initialize(const std::string &model_path, float conf_thresh,
                  float iou_thresh) override;
  InferenceResult Infer(const FrameBuffer &frame) override;

  bool IsInitialized() const { return is_initialized_; }

private:
  bool ConfigureStreams();

  void DecodeScale(int grid_size, int stride, const float *bbox_dfl,
                   const float *cls_scores, const float *kpts,
                   std::vector<Detection> &proposals);

  void ApplyNMS(std::vector<Detection> &candidates,
                std::vector<Detection> &output);

  config::InferenceConfig config_;
  bool is_initialized_{false};

  std::unique_ptr<hailort::VDevice> vdevice_;
  std::shared_ptr<hailort::InferModel> infer_model_;
  std::unique_ptr<hailort::ConfiguredInferModel> configured_infer_model_;
  std::unique_ptr<hailort::ConfiguredInferModel::Bindings> bindings_;

  std::unordered_map<std::string, std::vector<uint8_t>> output_buffers_;
};

} // namespace processing
