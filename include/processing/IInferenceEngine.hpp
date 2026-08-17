#pragma once

#include "config/AppConfig.hpp"
#include "processing/InferenceTypes.hpp"
#include <cstdint>
#include <string>

namespace processing {

struct FrameBuffer {
  const uint8_t *data = nullptr;
  size_t size = 0;
  int width = 0;
  int height = 0;
  uint64_t timestamp_ms = 0;
};

class IInferenceEngine {
public:
  virtual ~IInferenceEngine() = default;

  virtual bool Initialize(const config::InferenceConfig &config) = 0;
  virtual bool Initialize(const std::string &model_path, float conf_thresh,
                          float iou_thresh) = 0;
  virtual InferenceResult Infer(const FrameBuffer &frame) = 0;
};

} // namespace processing
